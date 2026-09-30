package lobby

import (
	"context"
	"crypto/rand"
	"errors"
	"fmt"
	"time"
)

// Invite is a pending invitation into a lobby.
type Invite struct {
	ID        string
	LobbyID   string
	InviterID string
	InviteeID string
	CreatedAt time.Time
	ExpiresAt time.Time
}

// Tx is one storage transaction. Lobbies are locked when loaded and stay
// locked until the transaction ends, so a rule check and its write never
// interleave with another change to the same lobby.
type Tx interface {
	// LobbyIDOf returns the lobby the account is in, or ErrNotInLobby.
	LobbyIDOf(accountID string) (string, error)
	// LockLobby loads and locks a lobby, or returns ErrLobbyNotFound.
	LockLobby(id string) (Lobby, error)
	CreateLobby(l Lobby) error
	SaveLobby(l Lobby) error
	DeleteLobby(id string) error
	// PutInvite stores an invite, replacing any pending invite from the same
	// lobby to the same invitee.
	PutInvite(inv Invite) error
	// Invite returns an unexpired invite, or ErrInviteNotFound.
	Invite(id string, now time.Time) (Invite, error)
	DeleteInvite(id string) error
	// DeleteInvitesBetween removes pending invites where either account
	// invited the other.
	DeleteInvitesBetween(a, b string) error
	// DeleteInvitesInto removes pending invites into a lobby for an invitee.
	DeleteInvitesInto(lobbyID, invitee string) error
}

// Store persists lobbies and invites.
type Store interface {
	// InTx runs fn in one transaction; the ctx passed to fn carries it, as
	// party.Store's does.
	InTx(ctx context.Context, fn func(ctx context.Context, tx Tx) error) error
	// LobbyOf returns the account's current lobby, or ErrNotInLobby.
	LobbyOf(ctx context.Context, accountID string) (Lobby, error)
	// InvitesFor lists unexpired invites addressed to the account.
	InvitesFor(ctx context.Context, accountID string, now time.Time) ([]Invite, error)
}

// SocialGraph answers the friendship and block questions lobby rules need,
// as it does for parties. Lobby never writes social state.
type SocialGraph interface {
	AreFriends(ctx context.Context, a, b string) (bool, error)
	// BlockedWithAny reports whether account and any of others block each
	// other in either direction.
	BlockedWithAny(ctx context.Context, account string, others []string) (bool, error)
}

// Activity answers whether any of accounts is in a match, a champion select or
// a matchmaking queue, which keeps them from a lobby's changes and launch.
type Activity interface {
	Busy(ctx context.Context, accounts []string) (bool, error)
}

// Launch is what a lobby hands champion select when its host launches: its
// humans and bots in each side's seat order, and its rules (ADR-021 §2).
type Launch struct {
	LobbyID  string
	HostID   string
	Members  []Member
	Bots     []Bot
	Settings Settings
}

// Launcher opens a launched lobby's champion select, inside the lobby's
// transaction. The selection package implements it, through cmd/veyra-backend.
type Launcher interface {
	OpenCustom(ctx context.Context, launch Launch) error
}

// Service applies lobby rules for an acting account.
type Service struct {
	store          Store
	social         SocialGraph
	activity       Activity
	launcher       Launcher
	limits         Limits
	inviteLifetime time.Duration
	now            func() time.Time
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, social SocialGraph, activity Activity, limits Limits, inviteLifetime time.Duration, now func() time.Time) *Service {
	return &Service{store: store, social: social, activity: activity, limits: limits, inviteLifetime: inviteLifetime, now: now}
}

// Limits returns the configuration the rules apply.
func (s *Service) Limits() Limits { return s.limits }

// SetLauncher connects champion select, which in turn tells the lobby how its
// select ended; each needs the other, so this is set after both exist.
func (s *Service) SetLauncher(l Launcher) { s.launcher = l }

// Launch is the host starting the match: every human must be free, and the
// lobby is fixed while the champion select it opens runs (ADR-021 §2).
func (s *Service) Launch(ctx context.Context, actor string) (Lobby, error) {
	var out Lobby
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		l, err := s.lockOwn(tx, actor)
		if err != nil {
			return err
		}
		if err := l.CheckLaunch(actor); err != nil {
			return err
		}
		if err := s.requireFree(ctx, l.MemberIDs()...); err != nil {
			return err
		}
		if s.launcher == nil {
			return ErrLaunchUnavailable
		}
		if err := s.launcher.OpenCustom(ctx, l.launch()); err != nil {
			return err
		}
		l.BeginSelecting()
		out = l
		return tx.SaveLobby(l)
	})
	return out, err
}

// SelectEnded settles a lobby after its champion select: a match that started
// closes it, and anything else returns everyone to it. A lobby already gone is
// nothing to settle.
func (s *Service) SelectEnded(ctx context.Context, lobbyID string, started bool) error {
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		l, err := tx.LockLobby(lobbyID)
		if errors.Is(err, ErrLobbyNotFound) {
			return nil
		}
		if err != nil {
			return err
		}
		if started {
			return tx.DeleteLobby(l.ID)
		}
		l.Reopen()
		return tx.SaveLobby(l)
	})
}

// Get returns the actor's lobby.
func (s *Service) Get(ctx context.Context, actor string) (Lobby, error) {
	return s.store.LobbyOf(ctx, actor)
}

// Invites lists the actor's pending invitations.
func (s *Service) Invites(ctx context.Context, actor string) ([]Invite, error) {
	return s.store.InvitesFor(ctx, actor, s.now())
}

// Create opens a lobby the actor hosts. Someone in a match, select or queue
// cannot, nor can someone already in a lobby.
func (s *Service) Create(ctx context.Context, actor string) (Lobby, error) {
	if err := s.requireFree(ctx, actor); err != nil {
		return Lobby{}, err
	}
	var out Lobby
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		if _, err := tx.LobbyIDOf(actor); err == nil {
			return ErrAlreadyInLobby
		} else if !errors.Is(err, ErrNotInLobby) {
			return err
		}
		out = New(newID(), actor, s.now())
		return tx.CreateLobby(out)
	})
	return out, err
}

// Leave takes the actor out of their lobby; the last human out closes it.
func (s *Service) Leave(ctx context.Context, actor string) error {
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		l, err := s.lockOwn(tx, actor)
		if err != nil {
			return err
		}
		if err := l.requireOpen(); err != nil {
			return err
		}
		return s.removeAndSave(tx, &l, actor)
	})
}

// Kick, Move, SetBot, RemoveBot and SetSettings are the host's.

func (s *Service) Kick(ctx context.Context, actor, target string) (Lobby, error) {
	return s.mutate(ctx, actor, func(l *Lobby) error { return l.Kick(actor, target) })
}

func (s *Service) Move(ctx context.Context, actor, target string, seat Seat) (Lobby, error) {
	return s.mutate(ctx, actor, func(l *Lobby) error { return l.Move(actor, target, seat, s.limits) })
}

func (s *Service) SetBot(ctx context.Context, actor string, bot Bot) (Lobby, error) {
	return s.mutate(ctx, actor, func(l *Lobby) error { return l.SetBot(actor, bot, s.limits) })
}

func (s *Service) RemoveBot(ctx context.Context, actor string, seat Seat) (Lobby, error) {
	return s.mutate(ctx, actor, func(l *Lobby) error { return l.RemoveBot(actor, seat) })
}

func (s *Service) SetSettings(ctx context.Context, actor string, victory bool, startingGold *float64) (Lobby, error) {
	return s.mutate(ctx, actor, func(l *Lobby) error { return l.SetSettings(actor, victory, startingGold, s.limits) })
}

// Invite lets the host invite a friend no member blocks or is blocked by
// (ADR-021 §1). An invite reserves no seat; capacity is checked on accepting.
func (s *Service) Invite(ctx context.Context, actor, invitee string) (Invite, error) {
	if actor == invitee {
		return Invite{}, ErrSelf
	}
	friends, err := s.social.AreFriends(ctx, actor, invitee)
	if err != nil {
		return Invite{}, err
	}
	if !friends {
		return Invite{}, ErrNotFriends
	}
	var inv Invite
	err = s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		l, err := s.lockOwn(tx, actor)
		if err != nil {
			return err
		}
		if err := l.requireHost(actor); err != nil {
			return err
		}
		if err := l.requireOpen(); err != nil {
			return err
		}
		if l.IsMember(invitee) {
			return ErrAlreadyInLobby
		}
		blocked, err := s.social.BlockedWithAny(ctx, invitee, l.MemberIDs())
		if err != nil {
			return err
		}
		if blocked {
			return ErrBlocked
		}
		now := s.now()
		inv = Invite{ID: newID(), LobbyID: l.ID, InviterID: actor, InviteeID: invitee, CreatedAt: now, ExpiresAt: now.Add(s.inviteLifetime)}
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

// AcceptInvite seats the actor in the inviting lobby. They must be free and in
// no other lobby; blocks against every member and capacity are checked now.
func (s *Service) AcceptInvite(ctx context.Context, actor, inviteID string) (Lobby, error) {
	if err := s.requireFree(ctx, actor); err != nil {
		return Lobby{}, err
	}
	var out Lobby
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		inv, err := tx.Invite(inviteID, s.now())
		if err != nil {
			return err
		}
		if inv.InviteeID != actor {
			return ErrInviteNotFound
		}
		if _, err := tx.LobbyIDOf(actor); err == nil {
			return ErrAlreadyInLobby
		} else if !errors.Is(err, ErrNotInLobby) {
			return err
		}
		l, err := tx.LockLobby(inv.LobbyID)
		if errors.Is(err, ErrLobbyNotFound) {
			return ErrInviteNotFound
		}
		if err != nil {
			return err
		}
		blocked, err := s.social.BlockedWithAny(ctx, actor, l.MemberIDs())
		if err != nil {
			return err
		}
		if blocked {
			return ErrBlocked
		}
		if err := l.Join(actor, s.limits, s.now()); err != nil {
			return err
		}
		out = l
		if err := tx.DeleteInvite(inv.ID); err != nil {
			return err
		}
		return tx.SaveLobby(l)
	})
	return out, err
}

// OnBlock applies a new block to lobby state, as party.Service.OnBlock does to
// parties: pending invites between the two are withdrawn, and two accounts
// never share a lobby; the blocked one leaves it.
func (s *Service) OnBlock(ctx context.Context, blocker, blocked string) error {
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		if err := tx.DeleteInvitesBetween(blocker, blocked); err != nil {
			return err
		}
		for _, pair := range [][2]string{{blocker, blocked}, {blocked, blocker}} {
			id, err := tx.LobbyIDOf(pair[0])
			if errors.Is(err, ErrNotInLobby) {
				continue
			}
			if err != nil {
				return err
			}
			if err := tx.DeleteInvitesInto(id, pair[1]); err != nil {
				return err
			}
		}
		id, err := tx.LobbyIDOf(blocker)
		if errors.Is(err, ErrNotInLobby) {
			return nil
		}
		if err != nil {
			return err
		}
		l, err := tx.LockLobby(id)
		if err != nil {
			return err
		}
		if !l.IsMember(blocked) {
			return nil
		}
		return s.removeAndSave(tx, &l, blocked)
	})
}

func (s *Service) requireFree(ctx context.Context, accounts ...string) error {
	if s.activity == nil {
		return nil
	}
	busy, err := s.activity.Busy(ctx, accounts)
	if err != nil {
		return err
	}
	if busy {
		return ErrBusy
	}
	return nil
}

func (s *Service) lockOwn(tx Tx, actor string) (Lobby, error) {
	id, err := tx.LobbyIDOf(actor)
	if err != nil {
		return Lobby{}, err
	}
	return tx.LockLobby(id)
}

func (s *Service) mutate(ctx context.Context, actor string, fn func(*Lobby) error) (Lobby, error) {
	var out Lobby
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		l, err := s.lockOwn(tx, actor)
		if err != nil {
			return err
		}
		if err := fn(&l); err != nil {
			return err
		}
		out = l
		return tx.SaveLobby(l)
	})
	return out, err
}

func (s *Service) removeAndSave(tx Tx, l *Lobby, account string) error {
	empty, err := l.Remove(account)
	if err != nil {
		return err
	}
	if empty {
		return tx.DeleteLobby(l.ID)
	}
	return tx.SaveLobby(*l)
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
