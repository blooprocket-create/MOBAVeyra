package chat

import (
	"context"
	"errors"
	"slices"
	"time"
)

// Message is one stored message.
type Message struct {
	// Seq orders every message; a later commit always has a greater Seq.
	Seq  int64
	Kind Kind
	Key  string
	// SenderName is the sender's display name when it sent.
	SenderID   string
	SenderName string
	// RecipientID is the other account of a direct message; empty otherwise.
	RecipientID string
	Text        string
	SentAt      time.Time
	// ClientID is the sender's own ID for the message, unique per sender.
	ClientID string
}

// PostMatchMember is an account's part in a match's post-match chat
// (ADR-046 §5).
type PostMatchMember struct {
	MatchID   string
	AccountID string
	// JoinSeq is the sequence of the account's first message; 0 when it left
	// without sending one.
	JoinSeq int64
	Left    bool
}

// Room is a party, select or post-match conversation a poll reads, from its
// floor.
type Room struct {
	Kind Kind
	Key  string
	// After is the sequence the room's messages must follow; Since the time
	// they must not precede.
	After int64
	Since time.Time
}

// Query asks a store for messages.
type Query struct {
	Rooms []Room
	// Direct is the account whose direct messages to include, "" for none.
	Direct string
	// After is the sequence every message must follow; Since the time none
	// may precede.
	After int64
	Since time.Time
	Limit int
	// Newest asks for the newest Limit messages instead of the oldest after
	// After. Either way the answer is oldest first.
	Newest bool
}

// Tx is one storage transaction.
type Tx interface {
	// LockSends serializes sends until the transaction ends, so sequence
	// order is commit order and a poll never skips a message that committed
	// late.
	LockSends() error
	// ByClientID returns the sender's message with that client ID, or
	// ErrMessageNotFound.
	ByClientID(senderID, clientID string) (Message, error)
	// CountSentSince counts the sender's messages sent at or after a time.
	CountSentSince(senderID string, since time.Time) (int, error)
	// Add stores a message and returns it with its sequence.
	Add(m Message) (Message, error)
	// Prune removes messages sent before a time.
	Prune(before time.Time) error
	// PostMatchMember returns an account's post-match participation, or
	// ErrNotJoined.
	PostMatchMember(matchID, accountID string) (PostMatchMember, error)
	SavePostMatchMember(m PostMatchMember) error
	SetMute(matchID, muterID, mutedID string, muted bool) error
}

// Store persists chat. InTx passes its callback a ctx carrying the
// transaction, so other domains' stores called with it join the transaction.
type Store interface {
	InTx(ctx context.Context, fn func(context.Context, Tx) error) error
	Messages(ctx context.Context, q Query) ([]Message, error)
	// OpenPostMatch returns the account's post-match participations it
	// joined and has not left.
	OpenPostMatch(ctx context.Context, accountID string) ([]PostMatchMember, error)
	// Mutes returns the accounts the muter muted in a match's post-match chat.
	Mutes(ctx context.Context, matchID, muterID string) ([]string, error)
	// LastSeq returns the newest message's sequence, 0 when there is none.
	LastSeq(ctx context.Context) (int64, error)
}

// Membership is an account's party, as chat needs it.
type Membership struct {
	PartyID  string
	JoinedAt time.Time
}

// Parties is what chat needs of the party domain.
type Parties interface {
	// PartyOf returns the account's party; ok is false when it is in none.
	PartyOf(ctx context.Context, accountID string) (m Membership, ok bool, err error)
}

// Friends is what chat needs of the social domain.
type Friends interface {
	AreFriends(ctx context.Context, a, b string) (bool, error)
	// BlockedWithAny reports a block either way between the account and any
	// of others.
	BlockedWithAny(ctx context.Context, account string, others []string) (bool, error)
}

// SelectTeam is an account's side in its active champion select.
type SelectTeam struct {
	SelectID string
	Side     string
}

// Selects is what chat needs of the selection domain.
type Selects interface {
	// TeamOf returns the account's side in its active select; ok is false
	// when it is in none.
	TeamOf(ctx context.Context, accountID string) (t SelectTeam, ok bool, err error)
}

// PlayedMatch is a match an account played, as post-match chat needs it.
type PlayedMatch struct {
	// EndedAt is zero while the match is not over.
	EndedAt time.Time
	// Participants are the match's human participants.
	Participants []string
}

// Matches is what chat needs of the match domain.
type Matches interface {
	// Played returns a match the account played, or ErrNotParticipant.
	Played(ctx context.Context, accountID, matchID string) (PlayedMatch, error)
	// Live returns the account's match that has not ended; ok is false when
	// there is none.
	Live(ctx context.Context, accountID string) (matchID string, ok bool, err error)
}

// Preferences is what chat needs of the settings domain.
type Preferences interface {
	// AllChatOn reports the account's All Chat preference.
	AllChatOn(ctx context.Context, accountID string) (bool, error)
}

// Names answers accounts' display names.
type Names interface {
	DisplayNames(ctx context.Context, ids []string) (map[string]string, error)
}

// Domains are the other domains chat asks about readers.
type Domains struct {
	Parties     Parties
	Friends     Friends
	Selects     Selects
	Matches     Matches
	Preferences Preferences
	Names       Names
}

// Service applies the chat rules.
type Service struct {
	store   Store
	domains Domains
	tuning  Tuning
	now     func() time.Time
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, domains Domains, tuning Tuning, now func() time.Time) *Service {
	return &Service{store: store, domains: domains, tuning: tuning, now: now}
}

// Send is one message a client sends.
type Send struct {
	Kind Kind
	// Target is the friend of a direct message or the match of a post-match
	// message; empty for the others.
	Target   string
	ClientID string
	Text     string
}

// Send stores a message after checking its sender may send it (ADR-046 §3).
// A resend with a client ID the sender already used returns the first
// message, so a lost answer never sends twice. A post-match sender's first
// message opts it in.
func (s *Service) Send(ctx context.Context, actor string, req Send) (Message, error) {
	if !ValidClientID(req.ClientID) {
		return Message{}, ErrInvalidMessage
	}
	text := Clean(req.Text)
	if err := CheckText(s.tuning, text); err != nil {
		return Message{}, err
	}
	now := s.now()
	var out Message
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		if err := tx.LockSends(); err != nil {
			return err
		}
		prior, err := tx.ByClientID(actor, req.ClientID)
		if err == nil {
			if prior.Kind != req.Kind {
				return ErrClientIDConflict
			}
			out = prior
			return nil
		}
		if !errors.Is(err, ErrMessageNotFound) {
			return err
		}
		key, recipient, err := s.authorizeSend(ctx, tx, actor, req, now)
		if err != nil {
			return err
		}
		sent, err := tx.CountSentSince(actor, now.Add(-s.tuning.Window))
		if err != nil {
			return err
		}
		if sent >= s.tuning.MaxPerWindow {
			return ErrRateLimited
		}
		if err := tx.Prune(now.Add(-s.tuning.Retention)); err != nil {
			return err
		}
		names, err := s.domains.Names.DisplayNames(ctx, []string{actor})
		if err != nil {
			return err
		}
		out, err = tx.Add(Message{Kind: req.Kind, Key: key, SenderID: actor, SenderName: names[actor], RecipientID: recipient,
			Text: text, SentAt: now, ClientID: req.ClientID})
		if err != nil {
			return err
		}
		if req.Kind != KindPostMatch {
			return nil
		}
		if _, err := tx.PostMatchMember(key, actor); err == nil {
			return nil
		} else if !errors.Is(err, ErrNotJoined) {
			return err
		}
		return tx.SavePostMatchMember(PostMatchMember{MatchID: key, AccountID: actor, JoinSeq: out.Seq})
	})
	if err != nil {
		return Message{}, err
	}
	return out, nil
}

// authorizeSend returns the conversation a message goes to, and a direct
// message's recipient, or why the actor may not send it (ADR-046 §2).
func (s *Service) authorizeSend(ctx context.Context, tx Tx, actor string, req Send, now time.Time) (key, recipient string, err error) {
	switch req.Kind {
	case KindParty:
		m, ok, err := s.domains.Parties.PartyOf(ctx, actor)
		if err != nil {
			return "", "", err
		}
		if !ok {
			return "", "", ErrNotInParty
		}
		return m.PartyID, "", nil
	case KindDirect:
		if req.Target == "" || req.Target == actor {
			return "", "", ErrInvalidMessage
		}
		if err := s.checkFriend(ctx, actor, req.Target); err != nil {
			return "", "", err
		}
		return DirectKey(actor, req.Target), req.Target, nil
	case KindSelect:
		t, ok, err := s.domains.Selects.TeamOf(ctx, actor)
		if err != nil {
			return "", "", err
		}
		if !ok {
			return "", "", ErrNoSelect
		}
		return SelectKey(t.SelectID, t.Side), "", nil
	case KindPostMatch:
		if _, err := s.postMatchOpen(ctx, actor, req.Target, now); err != nil {
			return "", "", err
		}
		member, err := tx.PostMatchMember(req.Target, actor)
		if err == nil && member.Left {
			return "", "", ErrPostMatchClosed
		}
		if err != nil && !errors.Is(err, ErrNotJoined) {
			return "", "", err
		}
		on, err := s.domains.Preferences.AllChatOn(ctx, actor)
		if err != nil {
			return "", "", err
		}
		if !on {
			return "", "", ErrAllChatOff
		}
		return req.Target, "", nil
	}
	return "", "", ErrInvalidMessage
}

// checkFriend returns why two accounts may not exchange direct messages, or
// nil.
func (s *Service) checkFriend(ctx context.Context, actor, other string) error {
	blocked, err := s.domains.Friends.BlockedWithAny(ctx, actor, []string{other})
	if err != nil {
		return err
	}
	if blocked {
		return ErrBlocked
	}
	friends, err := s.domains.Friends.AreFriends(ctx, actor, other)
	if err != nil {
		return err
	}
	if !friends {
		return ErrNotFriends
	}
	return nil
}

// postMatchOpen returns the match if its post-match chat is open to the
// actor: it played the match, the match ended within the window, and the
// actor has entered no select or match since (UX-60).
func (s *Service) postMatchOpen(ctx context.Context, actor, matchID string, now time.Time) (PlayedMatch, error) {
	played, err := s.domains.Matches.Played(ctx, actor, matchID)
	if err != nil {
		return PlayedMatch{}, err
	}
	if !PostMatchOpen(s.tuning, played.EndedAt, now) {
		return PlayedMatch{}, ErrPostMatchClosed
	}
	if _, inSelect, err := s.domains.Selects.TeamOf(ctx, actor); err != nil {
		return PlayedMatch{}, err
	} else if inSelect {
		return PlayedMatch{}, ErrPostMatchClosed
	}
	if _, live, err := s.domains.Matches.Live(ctx, actor); err != nil {
		return PlayedMatch{}, err
	} else if live {
		return PlayedMatch{}, ErrPostMatchClosed
	}
	return played, nil
}

// LeavePostMatch ends the actor's part in a match's post-match chat: it
// leaves the results screen (UX-60). It cannot rejoin.
func (s *Service) LeavePostMatch(ctx context.Context, actor, matchID string) error {
	if _, err := s.domains.Matches.Played(ctx, actor, matchID); err != nil {
		return err
	}
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		member, err := tx.PostMatchMember(matchID, actor)
		if errors.Is(err, ErrNotJoined) {
			member = PostMatchMember{MatchID: matchID, AccountID: actor}
		} else if err != nil {
			return err
		}
		if member.Left {
			return nil
		}
		member.Left = true
		return tx.SavePostMatchMember(member)
	})
}

// MutePostMatch mutes or unmutes another participant in a match's
// post-match chat, for the actor only (ADR-046 §5).
func (s *Service) MutePostMatch(ctx context.Context, actor, matchID, target string, muted bool) error {
	played, err := s.domains.Matches.Played(ctx, actor, matchID)
	if err != nil {
		return err
	}
	if target == actor || !slices.Contains(played.Participants, target) {
		return ErrNotParticipant
	}
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		return tx.SetMute(matchID, actor, target, muted)
	})
}

// Page is one poll's answer.
type Page struct {
	// Messages the actor may read, oldest first.
	Messages []Message
	// Next is the cursor for the next poll.
	Next int64
	// More is true when the page was full: the next poll should come at once.
	More bool
}

// Poll returns the messages the actor may read now after a cursor, or, with
// no cursor, its newest messages (ADR-046 §4). Readers are checked again at
// delivery, so a message the actor may no longer read is never returned.
func (s *Service) Poll(ctx context.Context, actor string, after int64, hasCursor bool) (Page, error) {
	now := s.now()
	since := now.Add(-s.tuning.Retention)
	rooms, mutes, err := s.rooms(ctx, actor, now, since)
	if err != nil {
		return Page{}, err
	}
	q := Query{Rooms: rooms, Direct: actor, After: after, Since: since, Limit: s.tuning.PageSize}
	if !hasCursor {
		q.After, q.Limit, q.Newest = 0, s.tuning.HistoryMessages, true
	}
	raw, err := s.store.Messages(ctx, q)
	if err != nil {
		return Page{}, err
	}
	page := Page{Next: q.After, More: hasCursor && len(raw) >= q.Limit}
	for _, m := range raw {
		page.Next = max(page.Next, m.Seq)
	}
	if !hasCursor && len(raw) == 0 {
		if page.Next, err = s.store.LastSeq(ctx); err != nil {
			return Page{}, err
		}
	}
	readable := map[string]bool{}
	for _, m := range raw {
		ok, err := s.deliverable(ctx, actor, m, mutes, readable)
		if err != nil {
			return Page{}, err
		}
		if ok {
			page.Messages = append(page.Messages, m)
		}
	}
	return page, nil
}

// rooms returns the party, select and post-match conversations the actor may
// read now, and its mutes in each post-match chat.
func (s *Service) rooms(ctx context.Context, actor string, now, since time.Time) ([]Room, map[string][]string, error) {
	var rooms []Room
	mutes := map[string][]string{}
	if m, ok, err := s.domains.Parties.PartyOf(ctx, actor); err != nil {
		return nil, nil, err
	} else if ok {
		// A member reads only what was sent since it joined (Bible §3).
		rooms = append(rooms, Room{Kind: KindParty, Key: m.PartyID, Since: later(since, m.JoinedAt)})
	}
	if t, ok, err := s.domains.Selects.TeamOf(ctx, actor); err != nil {
		return nil, nil, err
	} else if ok {
		// In a select, every earlier post-match chat is over (UX-60).
		return append(rooms, Room{Kind: KindSelect, Key: SelectKey(t.SelectID, t.Side), Since: since}), mutes, nil
	}
	members, err := s.store.OpenPostMatch(ctx, actor)
	if err != nil || len(members) == 0 {
		return rooms, mutes, err
	}
	on, err := s.domains.Preferences.AllChatOn(ctx, actor)
	if err != nil || !on {
		return rooms, mutes, err
	}
	for _, member := range members {
		if _, err := s.postMatchOpen(ctx, actor, member.MatchID, now); errors.Is(err, ErrPostMatchClosed) || errors.Is(err, ErrNotParticipant) {
			continue
		} else if err != nil {
			return nil, nil, err
		}
		muted, err := s.store.Mutes(ctx, member.MatchID, actor)
		if err != nil {
			return nil, nil, err
		}
		mutes[member.MatchID] = muted
		// The first message is the actor's own, and nothing before it is read.
		rooms = append(rooms, Room{Kind: KindPostMatch, Key: member.MatchID, After: member.JoinSeq - 1, Since: since})
	}
	return rooms, mutes, nil
}

// deliverable reports whether the actor may read a message now: no block
// either way with its sender, a direct message's two accounts still friends,
// and a post-match sender not muted. readable caches each account's answer.
func (s *Service) deliverable(ctx context.Context, actor string, m Message, mutes map[string][]string, readable map[string]bool) (bool, error) {
	other := m.SenderID
	if m.Kind == KindDirect && other == actor {
		other = m.RecipientID
	}
	if other != actor {
		// A direct message also needs the friendship, so its answer is cached apart.
		cacheKey := other
		if m.Kind == KindDirect {
			cacheKey = "direct|" + other
		}
		ok, cached := readable[cacheKey]
		if !cached {
			var err error
			if m.Kind == KindDirect {
				err = s.checkFriend(ctx, actor, other)
			} else {
				var blocked bool
				blocked, err = s.domains.Friends.BlockedWithAny(ctx, actor, []string{other})
				if err == nil && blocked {
					err = ErrBlocked
				}
			}
			if err != nil && !errors.Is(err, ErrBlocked) && !errors.Is(err, ErrNotFriends) {
				return false, err
			}
			ok = err == nil
			readable[cacheKey] = ok
		}
		if !ok {
			return false, nil
		}
	}
	if m.Kind == KindPostMatch && slices.Contains(mutes[m.Key], m.SenderID) {
		return false, nil
	}
	return true, nil
}

func later(a, b time.Time) time.Time {
	if b.After(a) {
		return b
	}
	return a
}
