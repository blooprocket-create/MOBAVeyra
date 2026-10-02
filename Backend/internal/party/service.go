package party

import (
	"context"
	"crypto/rand"
	"errors"
	"fmt"
	"slices"
	"sort"
	"time"
)

// Invite is a pending party invitation.
type Invite struct {
	ID        string
	PartyID   string
	InviterID string
	InviteeID string
	CreatedAt time.Time
	ExpiresAt time.Time
}

// Tx is one storage transaction. Parties are locked when loaded and stay
// locked until the transaction ends, so a rule check and its write can never
// interleave with another change to the same party.
type Tx interface {
	// PartyIDOf returns the party the account belongs to, or ErrNotInParty.
	PartyIDOf(accountID string) (string, error)
	// LockParty loads and locks a party, or returns ErrPartyNotFound.
	LockParty(id string) (Party, error)
	CreateParty(p Party) error
	SaveParty(p Party) error
	DeleteParty(id string) error
	// PutInvite stores an invite, replacing any pending invite from the same
	// party to the same invitee.
	PutInvite(inv Invite) error
	// Invite returns an unexpired invite, or ErrInviteNotFound.
	Invite(id string, now time.Time) (Invite, error)
	DeleteInvite(id string) error
	// DeleteInvitesBetween removes pending invites where either account
	// invited the other.
	DeleteInvitesBetween(a, b string) error
	// DeleteInvitesInto removes pending invites into a party for an invitee.
	DeleteInvitesInto(partyID, invitee string) error
	// LockQueued loads and locks the parties queued for a mode, oldest first.
	// Parties another transaction has locked are skipped, so two matchmaker
	// passes never both take a party.
	LockQueued(mode string) ([]Party, error)
}

// Store persists parties and invites.
type Store interface {
	// InTx runs fn in one transaction. The ctx passed to fn carries that
	// transaction, so SocialGraph queries made with it read through the same
	// connection instead of taking a second one from the pool, and an
	// enclosing unit of work (see httpapi.Deps.Atomic) is joined, not nested.
	InTx(ctx context.Context, fn func(ctx context.Context, tx Tx) error) error
	// PartyOf returns the account's current party, or ErrNotInParty.
	PartyOf(ctx context.Context, accountID string) (Party, error)
	// PartiesOf returns the current party of each of the accounts that has
	// one, keyed by account, in a bounded number of reads whatever the count.
	PartiesOf(ctx context.Context, accountIDs []string) (map[string]Party, error)
	// InvitesFor lists unexpired invites addressed to the account.
	InvitesFor(ctx context.Context, accountID string, now time.Time) ([]Invite, error)
}

// SocialGraph answers the friendship and block questions party rules need.
// It is implemented by the social package; party never writes social state.
type SocialGraph interface {
	AreFriends(ctx context.Context, a, b string) (bool, error)
	// FriendOfAny reports whether account is friends with any of others.
	FriendOfAny(ctx context.Context, account string, others []string) (bool, error)
	// BlockedWithAny reports whether account and any of others block each
	// other in either direction.
	BlockedWithAny(ctx context.Context, account string, others []string) (bool, error)
	// BlockedWith returns those of others that block, or are blocked by,
	// account, in one read.
	BlockedWith(ctx context.Context, account string, others []string) (map[string]bool, error)
}

// Activity answers whether any of accounts is in a match or a champion
// select. The main package joins the match and selection packages to
// implement it; they in turn read parties, so it is set after both exist.
type Activity interface {
	Busy(ctx context.Context, accounts []string) (bool, error)
}

// Settings are the validated party settings.
type Settings struct {
	Rules          Rules
	InviteLifetime time.Duration
	// DefaultPrivacy applies to newly created parties.
	DefaultPrivacy Privacy
}

// Service applies party rules for an acting account.
type Service struct {
	store    Store
	social   SocialGraph
	activity Activity
	settings Settings
	now      func() time.Time
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, social SocialGraph, settings Settings, now func() time.Time) *Service {
	return &Service{store: store, social: social, settings: settings, now: now}
}

// SetActivity connects what says whether a player is in a match or champion
// select, which StartQueue checks. Until it is set, StartQueue checks nothing
// more than the party's own rules.
func (s *Service) SetActivity(a Activity) { s.activity = a }

// Get returns the actor's party.
func (s *Service) Get(ctx context.Context, actor string) (Party, error) {
	return s.store.PartyOf(ctx, actor)
}

// QueuedFor returns how long the party has been in matchmaking, by the
// service's clock; zero while it is idle.
func (s *Service) QueuedFor(p Party) time.Duration {
	if p.QueuedAt.IsZero() {
		return 0
	}
	return max(s.now().Sub(p.QueuedAt), 0)
}

// Invites lists the actor's pending invitations.
func (s *Service) Invites(ctx context.Context, actor string) ([]Invite, error) {
	return s.store.InvitesFor(ctx, actor, s.now())
}

// SelectMode creates a one-person party with the mode, or changes the mode
// of the party the actor leads (UX-2, UX-6).
func (s *Service) SelectMode(ctx context.Context, actor, modeID string) (Party, error) {
	var out Party
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		p, err := s.ownOrNewParty(tx, actor)
		if err != nil {
			return err
		}
		if err := p.SetMode(actor, modeID, s.settings.Rules); err != nil {
			return err
		}
		out = p
		return s.save(tx, p)
	})
	return out, err
}

// SetReady, SetPrivacy, TransferLeader, Kick, StartQueue and CancelQueue act
// on the actor's own party.

func (s *Service) SetReady(ctx context.Context, actor string, ready bool) (Party, error) {
	return s.mutate(ctx, actor, func(p *Party) error { return p.SetReady(actor, ready) })
}

func (s *Service) SetPrivacy(ctx context.Context, actor string, privacy Privacy) (Party, error) {
	return s.mutate(ctx, actor, func(p *Party) error { return p.SetPrivacy(actor, privacy) })
}

func (s *Service) TransferLeader(ctx context.Context, actor, target string) (Party, error) {
	return s.mutate(ctx, actor, func(p *Party) error { return p.TransferLeader(actor, target) })
}

func (s *Service) Kick(ctx context.Context, actor, target string) (Party, error) {
	return s.mutate(ctx, actor, func(p *Party) error { return p.Kick(actor, target) })
}

// StartQueue also refuses a party one of whose members is in a match or
// champion select already: matchmaking could never make their next match, and
// the other players would wait on it for nothing.
func (s *Service) StartQueue(ctx context.Context, actor string) (Party, error) {
	return s.mutateIn(ctx, actor, func(ctx context.Context, p *Party) error {
		if err := p.StartQueue(actor, s.settings.Rules, s.now()); err != nil {
			return err
		}
		if s.activity == nil {
			return nil
		}
		members := make([]string, len(p.Members))
		for i, m := range p.Members {
			members[i] = m.AccountID
		}
		busy, err := s.activity.Busy(ctx, members)
		if err != nil {
			return err
		}
		if busy {
			return ErrMemberBusy
		}
		return nil
	})
}

// The matchmaking package moves parties through a proposed match and its
// champion select with these, inside its own unit of work.

// LockQueued returns a mode's queued parties, oldest first, locked until the
// caller's unit of work ends.
func (s *Service) LockQueued(ctx context.Context, mode string) ([]Party, error) {
	var out []Party
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		queued, err := tx.LockQueued(mode)
		out = queued
		return err
	})
	return out, err
}

// MarkFound puts queued parties into a proposed match.
func (s *Service) MarkFound(ctx context.Context, ids []string) error {
	return s.changeByID(ctx, ids, (*Party).MarkFound)
}

// MarkSelecting moves parties whose match everyone accepted into its select.
func (s *Service) MarkSelecting(ctx context.Context, ids []string) error {
	return s.changeByID(ctx, ids, (*Party).MarkSelecting)
}

// Requeue returns parties to the queue with their places kept. A party that
// has since left matchmaking, or no longer exists, stays as it is.
func (s *Service) Requeue(ctx context.Context, ids []string) error {
	return s.changeByID(ctx, ids, func(p *Party) error {
		if err := p.Requeue(); !errors.Is(err, ErrNotInQueue) {
			return err
		}
		return nil
	})
}

// ReturnToIdle takes parties out of matchmaking, Not Ready.
func (s *Service) ReturnToIdle(ctx context.Context, ids []string) error {
	return s.changeByID(ctx, ids, func(p *Party) error {
		p.ReturnToIdle()
		return nil
	})
}

// Status returns a party's status; a party that no longer exists is Idle.
func (s *Service) Status(ctx context.Context, id string) (Status, error) {
	var status Status
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		p, err := tx.LockParty(id)
		if errors.Is(err, ErrPartyNotFound) {
			status = Idle
			return nil
		}
		status = p.Status
		return err
	})
	return status, err
}

// changeByID applies change to each existing party, in ID order so two units
// of work never lock the same parties in opposite orders.
func (s *Service) changeByID(ctx context.Context, ids []string, change func(*Party) error) error {
	sorted := append([]string(nil), ids...)
	sort.Strings(sorted)
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		for _, id := range sorted {
			p, err := tx.LockParty(id)
			if errors.Is(err, ErrPartyNotFound) {
				continue
			}
			if err != nil {
				return err
			}
			if err := change(&p); err != nil {
				return fmt.Errorf("party %s: %w", id, err)
			}
			if err := tx.SaveParty(p); err != nil {
				return err
			}
		}
		return nil
	})
}

func (s *Service) CancelQueue(ctx context.Context, actor string) (Party, error) {
	return s.mutate(ctx, actor, func(p *Party) error { return p.CancelQueue(actor) })
}

// Leave takes the actor out of their party.
func (s *Service) Leave(ctx context.Context, actor string) error {
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		id, err := tx.PartyIDOf(actor)
		if err != nil {
			return err
		}
		p, err := tx.LockParty(id)
		if err != nil {
			return err
		}
		return s.removeAndSave(tx, &p, actor)
	})
}

// Invite lets any member invite a friend (§1). An actor without a party gets
// a new mode-less party they lead (UX-11). A queued party cannot invite (§2).
// Capacity is checked when the invite is accepted, not now, and an invite
// never reserves a slot.
func (s *Service) Invite(ctx context.Context, actor, invitee string) (Invite, error) {
	if actor == invitee {
		return Invite{}, ErrSelf
	}
	if err := s.checkSocial(ctx, actor, invitee); err != nil {
		return Invite{}, err
	}
	var inv Invite
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		p, err := s.ownOrNewParty(tx, actor)
		if err != nil {
			return err
		}
		if p.Status != Idle {
			return ErrPartyLocked
		}
		if p.IsMember(invitee) {
			return ErrAlreadyInParty
		}
		if err := s.save(tx, p); err != nil {
			return err
		}
		now := s.now()
		inv = Invite{
			ID:        newID(),
			PartyID:   p.ID,
			InviterID: actor,
			InviteeID: invitee,
			CreatedAt: now,
			ExpiresAt: now.Add(s.settings.InviteLifetime),
		}
		return tx.PutInvite(inv)
	})
	return inv, err
}

// DeclineInvite discards an invitation addressed to the actor.
func (s *Service) DeclineInvite(ctx context.Context, actor, inviteID string) error {
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		inv, err := tx.Invite(inviteID, s.now())
		if err != nil {
			return err
		}
		if inv.InviteeID != actor {
			return ErrInviteNotFound
		}
		return tx.DeleteInvite(inv.ID)
	})
}

// AcceptInvite moves the actor into the inviting party. Capacity, the queue
// lock and blocks against every member are checked now (§1–2). Accepting
// while in another party leaves it, unless that party is queue-locked (UX-100).
func (s *Service) AcceptInvite(ctx context.Context, actor, inviteID string) (Party, error) {
	var out Party
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		inv, err := tx.Invite(inviteID, s.now())
		if err != nil {
			return err
		}
		if inv.InviteeID != actor {
			return ErrInviteNotFound
		}
		p, err := s.moveInto(ctx, tx, actor, inv.PartyID, nil)
		if err != nil {
			return err
		}
		out = p
		return tx.DeleteInvite(inv.ID)
	})
	return out, err
}

// JoinPublic lets a friend join a Public party with an open slot, without an
// invitation (§1).
//
// PROVISIONAL: §1 says "friends may join directly" without saying whose
// friends. Until ruled on, being a friend of any current member qualifies.
func (s *Service) JoinPublic(ctx context.Context, actor, partyID string) (Party, error) {
	var out Party
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		p, err := s.moveInto(ctx, tx, actor, partyID, func(target Party) error {
			if target.Privacy != Public {
				return ErrPartyNotJoinable
			}
			friend, err := s.social.FriendOfAny(ctx, actor, memberIDs(target))
			if err != nil {
				return err
			}
			if !friend {
				return ErrPartyNotJoinable
			}
			return nil
		})
		out = p
		return err
	})
	return out, err
}

// JoinableParties returns, for each of friends whose party the actor could
// join directly, that party's ID: a Public party, idle so its membership is
// not locked, with an open slot, and not the actor's own (Parties & Social
// Bible §1-§2), none of whose members blocks or is blocked by the actor (§6).
// It is what the friends list offers, by the rules JoinPublic applies;
// JoinPublic still decides. The parties, and the blocks between the actor and
// their members, are read in two bulk lookups however many friends the actor
// has.
func (s *Service) JoinableParties(ctx context.Context, actor string, friends []string) (map[string]string, error) {
	parties, err := s.store.PartiesOf(ctx, append([]string{actor}, friends...))
	if err != nil {
		return nil, err
	}
	own := parties[actor]
	open := map[string]Party{}
	var members []string
	for _, friend := range friends {
		p, ok := parties[friend]
		if !ok || (own.ID != "" && p.ID == own.ID) || p.Privacy != Public || p.Status != Idle || len(p.Members) >= s.settings.Rules.MaxSize {
			continue
		}
		open[friend] = p
		members = append(members, memberIDs(p)...)
	}
	if len(open) == 0 {
		return map[string]string{}, nil
	}
	blocked, err := s.social.BlockedWith(ctx, actor, members)
	if err != nil {
		return nil, err
	}
	out := map[string]string{}
	for friend, p := range open {
		if !slices.ContainsFunc(memberIDs(p), func(id string) bool { return blocked[id] }) {
			out[friend] = p.ID
		}
	}
	return out, nil
}

// OnBlock applies a new block to party state: pending invites that would put
// the two accounts in one party are withdrawn, whoever sent them, and two
// accounts may never share a party (§6).
//
// PROVISIONAL: the bibles do not say who leaves when a member blocks another
// member of their own party. Until ruled on, the blocked account is removed,
// with no penalty, exactly as if the leader had removed them.
func (s *Service) OnBlock(ctx context.Context, blocker, blocked string) error {
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		if err := tx.DeleteInvitesBetween(blocker, blocked); err != nil {
			return err
		}
		for _, pair := range [][2]string{{blocker, blocked}, {blocked, blocker}} {
			id, err := tx.PartyIDOf(pair[0])
			if errors.Is(err, ErrNotInParty) {
				continue
			}
			if err != nil {
				return err
			}
			if err := tx.DeleteInvitesInto(id, pair[1]); err != nil {
				return err
			}
		}
		id, err := tx.PartyIDOf(blocker)
		if errors.Is(err, ErrNotInParty) {
			return nil
		}
		if err != nil {
			return err
		}
		p, err := tx.LockParty(id)
		if err != nil {
			return err
		}
		if !p.IsMember(blocked) {
			return nil
		}
		return s.removeAndSave(tx, &p, blocked)
	})
}

// moveInto joins actor to the target party, leaving their current party
// first when they have one. Both parties are locked in ID order. check, when
// given, runs against the locked target before any change.
func (s *Service) moveInto(ctx context.Context, tx Tx, actor, targetID string, check func(Party) error) (Party, error) {
	currentID, err := tx.PartyIDOf(actor)
	if err != nil && !errors.Is(err, ErrNotInParty) {
		return Party{}, err
	}
	if currentID == targetID {
		return Party{}, ErrAlreadyInParty
	}

	ids := []string{targetID}
	if currentID != "" {
		ids = append(ids, currentID)
	}
	sort.Strings(ids)
	locked := map[string]Party{}
	for _, id := range ids {
		p, err := tx.LockParty(id)
		if err != nil {
			return Party{}, err
		}
		locked[id] = p
	}

	target := locked[targetID]
	if check != nil {
		if err := check(target); err != nil {
			return Party{}, err
		}
	}
	blocked, err := s.social.BlockedWithAny(ctx, actor, memberIDs(target))
	if err != nil {
		return Party{}, err
	}
	if blocked {
		return Party{}, ErrBlocked
	}
	if err := target.Add(actor, s.settings.Rules, s.now()); err != nil {
		return Party{}, err
	}

	if currentID != "" {
		current := locked[currentID]
		if current.Status != Idle {
			return Party{}, ErrPartyLocked
		}
		if err := s.removeAndSave(tx, &current, actor); err != nil {
			return Party{}, err
		}
	}
	return target, tx.SaveParty(target)
}

func (s *Service) checkSocial(ctx context.Context, a, b string) error {
	blocked, err := s.social.BlockedWithAny(ctx, a, []string{b})
	if err != nil {
		return err
	}
	if blocked {
		return ErrBlocked
	}
	friends, err := s.social.AreFriends(ctx, a, b)
	if err != nil {
		return err
	}
	if !friends {
		return ErrNotFriends
	}
	return nil
}

func (s *Service) mutate(ctx context.Context, actor string, fn func(*Party) error) (Party, error) {
	return s.mutateIn(ctx, actor, func(_ context.Context, p *Party) error { return fn(p) })
}

// mutateIn is mutate for a change that also asks other domains, with the ctx
// that carries its transaction.
func (s *Service) mutateIn(ctx context.Context, actor string, fn func(context.Context, *Party) error) (Party, error) {
	var out Party
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		id, err := tx.PartyIDOf(actor)
		if err != nil {
			return err
		}
		p, err := tx.LockParty(id)
		if err != nil {
			return err
		}
		if err := fn(ctx, &p); err != nil {
			return err
		}
		out = p
		return tx.SaveParty(p)
	})
	return out, err
}

// ownOrNewParty returns the actor's locked party, or a new party they lead.
// A new party is not stored until save is called.
func (s *Service) ownOrNewParty(tx Tx, actor string) (Party, error) {
	id, err := tx.PartyIDOf(actor)
	if err == nil {
		return tx.LockParty(id)
	}
	if !errors.Is(err, ErrNotInParty) {
		return Party{}, err
	}
	return Party{
		ID:       newID(),
		LeaderID: actor,
		Privacy:  s.settings.DefaultPrivacy,
		Status:   Idle,
		Members:  []Member{{AccountID: actor, JoinedAt: s.now()}},
		isNew:    true,
	}, nil
}

func (s *Service) save(tx Tx, p Party) error {
	if p.isNew {
		p.isNew = false
		return tx.CreateParty(p)
	}
	return tx.SaveParty(p)
}

func (s *Service) removeAndSave(tx Tx, p *Party, account string) error {
	empty, err := p.Remove(account)
	if err != nil {
		return err
	}
	if empty {
		return tx.DeleteParty(p.ID)
	}
	return tx.SaveParty(*p)
}

func memberIDs(p Party) []string {
	ids := make([]string, len(p.Members))
	for i, m := range p.Members {
		ids[i] = m.AccountID
	}
	return ids
}

// newID returns a random RFC 4122 version-4 UUID string.
func newID() string {
	var b [16]byte
	if _, err := rand.Read(b[:]); err != nil {
		panic(fmt.Sprintf("crypto/rand failed: %v", err))
	}
	b[6] = (b[6] & 0x0f) | 0x40
	b[8] = (b[8] & 0x3f) | 0x80
	return fmt.Sprintf("%x-%x-%x-%x-%x", b[0:4], b[4:6], b[6:8], b[8:10], b[10:16])
}
