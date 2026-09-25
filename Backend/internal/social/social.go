// Package social owns friendships, friend requests and blocks (Parties &
// Social Bible §5–6). Blocks are symmetric in effect: if either account
// blocks the other, they cannot befriend, invite, share a party or share a
// match. Other domains read the graph through Service; only this package
// writes it.
package social

import (
	"context"
	"errors"
)

// Errors describing rule violations.
var (
	ErrSelf            = errors.New("cannot target yourself")
	ErrAccountNotFound = errors.New("account not found")
	ErrBlocked         = errors.New("blocked")
	ErrAlreadyFriends  = errors.New("already friends")
	ErrRequestNotFound = errors.New("friend request not found")
	ErrNotFriends      = errors.New("not friends")
)

// Tx is one storage transaction.
type Tx interface {
	// LockPair serialises concurrent changes between the same two accounts.
	LockPair(a, b string) error
	AccountExists(id string) (bool, error)
	// BlockedEither reports a block in either direction.
	BlockedEither(a, b string) (bool, error)
	AreFriends(a, b string) (bool, error)
	RequestExists(from, to string) (bool, error)
	PutRequest(from, to string) error
	DeleteRequest(from, to string) error
	PutFriendship(a, b string) error
	DeleteFriendship(a, b string) error
	PutBlock(blocker, blocked string) error
	DeleteBlock(blocker, blocked string) error
}

// Store persists the social graph.
type Store interface {
	InTx(ctx context.Context, fn func(Tx) error) error
	Friends(ctx context.Context, account string) ([]string, error)
	IncomingRequests(ctx context.Context, account string) ([]string, error)
	OutgoingRequests(ctx context.Context, account string) ([]string, error)
	// BlocksBy lists the accounts this account has blocked.
	BlocksBy(ctx context.Context, account string) ([]string, error)
	AreFriends(ctx context.Context, a, b string) (bool, error)
	FriendOfAny(ctx context.Context, account string, others []string) (bool, error)
	BlockedWithAny(ctx context.Context, account string, others []string) (bool, error)
}

// Service applies social rules for an acting account.
type Service struct {
	store Store
}

// NewService builds a Service.
func NewService(store Store) *Service { return &Service{store: store} }

// RequestOutcome says what sending a friend request did.
type RequestOutcome string

const (
	// Requested means a pending request now waits for the other account.
	Requested RequestOutcome = "requested"
	// BecameFriends means the other account had already asked, so sending
	// the request completed the friendship.
	BecameFriends RequestOutcome = "friends"
)

// SendFriendRequest asks target to be friends. Requests wait until answered,
// including while the target is in a match (§5).
func (s *Service) SendFriendRequest(ctx context.Context, actor, target string) (RequestOutcome, error) {
	if actor == target {
		return "", ErrSelf
	}
	var outcome RequestOutcome
	err := s.store.InTx(ctx, func(tx Tx) error {
		if err := s.checkPair(tx, actor, target); err != nil {
			return err
		}
		friends, err := tx.AreFriends(actor, target)
		if err != nil {
			return err
		}
		if friends {
			return ErrAlreadyFriends
		}
		reverse, err := tx.RequestExists(target, actor)
		if err != nil {
			return err
		}
		if reverse {
			outcome = BecameFriends
			if err := tx.DeleteRequest(target, actor); err != nil {
				return err
			}
			return tx.PutFriendship(actor, target)
		}
		outcome = Requested
		return tx.PutRequest(actor, target)
	})
	return outcome, err
}

// AcceptFriendRequest accepts a pending request from requester.
func (s *Service) AcceptFriendRequest(ctx context.Context, actor, requester string) error {
	return s.store.InTx(ctx, func(tx Tx) error {
		if err := tx.LockPair(actor, requester); err != nil {
			return err
		}
		exists, err := tx.RequestExists(requester, actor)
		if err != nil {
			return err
		}
		if !exists {
			return ErrRequestNotFound
		}
		if err := tx.DeleteRequest(requester, actor); err != nil {
			return err
		}
		return tx.PutFriendship(actor, requester)
	})
}

// DeclineFriendRequest discards a pending request from requester.
func (s *Service) DeclineFriendRequest(ctx context.Context, actor, requester string) error {
	return s.deleteRequest(ctx, requester, actor)
}

// CancelFriendRequest withdraws the actor's own pending request.
func (s *Service) CancelFriendRequest(ctx context.Context, actor, target string) error {
	return s.deleteRequest(ctx, actor, target)
}

// RemoveFriend ends a friendship.
func (s *Service) RemoveFriend(ctx context.Context, actor, other string) error {
	return s.store.InTx(ctx, func(tx Tx) error {
		if err := tx.LockPair(actor, other); err != nil {
			return err
		}
		friends, err := tx.AreFriends(actor, other)
		if err != nil {
			return err
		}
		if !friends {
			return ErrNotFriends
		}
		return tx.DeleteFriendship(actor, other)
	})
}

// Block blocks target. The friendship and any pending requests between the
// two accounts end immediately (§6). Party consequences are applied by the
// party domain, which the caller notifies after this succeeds.
func (s *Service) Block(ctx context.Context, actor, target string) error {
	if actor == target {
		return ErrSelf
	}
	return s.store.InTx(ctx, func(tx Tx) error {
		if err := tx.LockPair(actor, target); err != nil {
			return err
		}
		exists, err := tx.AccountExists(target)
		if err != nil {
			return err
		}
		if !exists {
			return ErrAccountNotFound
		}
		if err := tx.PutBlock(actor, target); err != nil {
			return err
		}
		if err := tx.DeleteFriendship(actor, target); err != nil {
			return err
		}
		if err := tx.DeleteRequest(actor, target); err != nil {
			return err
		}
		return tx.DeleteRequest(target, actor)
	})
}

// Unblock removes the actor's own block. A block the other account placed
// is unaffected.
func (s *Service) Unblock(ctx context.Context, actor, target string) error {
	return s.store.InTx(ctx, func(tx Tx) error {
		if err := tx.LockPair(actor, target); err != nil {
			return err
		}
		return tx.DeleteBlock(actor, target)
	})
}

// Lists for the acting account.

func (s *Service) Friends(ctx context.Context, actor string) ([]string, error) {
	return s.store.Friends(ctx, actor)
}

func (s *Service) IncomingRequests(ctx context.Context, actor string) ([]string, error) {
	return s.store.IncomingRequests(ctx, actor)
}

func (s *Service) OutgoingRequests(ctx context.Context, actor string) ([]string, error) {
	return s.store.OutgoingRequests(ctx, actor)
}

func (s *Service) Blocks(ctx context.Context, actor string) ([]string, error) {
	return s.store.BlocksBy(ctx, actor)
}

// Graph queries used by other domains (party.SocialGraph, matchmaking).

func (s *Service) AreFriends(ctx context.Context, a, b string) (bool, error) {
	return s.store.AreFriends(ctx, a, b)
}

func (s *Service) FriendOfAny(ctx context.Context, account string, others []string) (bool, error) {
	return s.store.FriendOfAny(ctx, account, others)
}

func (s *Service) BlockedWithAny(ctx context.Context, account string, others []string) (bool, error) {
	return s.store.BlockedWithAny(ctx, account, others)
}

func (s *Service) checkPair(tx Tx, actor, target string) error {
	if err := tx.LockPair(actor, target); err != nil {
		return err
	}
	exists, err := tx.AccountExists(target)
	if err != nil {
		return err
	}
	if !exists {
		return ErrAccountNotFound
	}
	blocked, err := tx.BlockedEither(actor, target)
	if err != nil {
		return err
	}
	if blocked {
		return ErrBlocked
	}
	return nil
}

func (s *Service) deleteRequest(ctx context.Context, from, to string) error {
	return s.store.InTx(ctx, func(tx Tx) error {
		if err := tx.LockPair(from, to); err != nil {
			return err
		}
		exists, err := tx.RequestExists(from, to)
		if err != nil {
			return err
		}
		if !exists {
			return ErrRequestNotFound
		}
		return tx.DeleteRequest(from, to)
	})
}
