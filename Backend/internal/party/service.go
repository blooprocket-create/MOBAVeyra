package party

import (
	"context"
	"crypto/rand"
	"errors"
	"fmt"
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
	settings Settings
	now      func() time.Time
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, social SocialGraph, settings Settings, now func() time.Time) *Service {
	return &Service{store: store, social: social, settings: settings, now: now}
}

// Get returns the actor's party.
func (s *Service) Get(ctx context.Context, actor string) (Party, error) {
	return s.store.PartyOf(ctx, actor)
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

func (s *Service) StartQueue(ctx context.Context, actor string) (Party, error) {
	return s.mutate(ctx, actor, func(p *Party) error { return p.StartQueue(actor, s.settings.Rules) })
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
		if err := fn(&p); err != nil {
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
