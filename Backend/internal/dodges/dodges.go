// Package dodges keeps queue-dodge restrictions (Match Flow Bible §2; Parties,
// Social & Matchmaking Bible §3; ADR-060): a player who leaves a matchmade
// champion select on purpose cannot queue for a while. A restriction is
// personal, holds back any party the player is in, and is no moderation
// sanction. Which selects count is selection's to say, and who may queue is
// party's; this domain only keeps when each restriction ends.
package dodges

import (
	"context"
	"time"
)

// Store persists restrictions.
type Store interface {
	// Restrict sets the account's restriction to end at until, replacing any.
	Restrict(ctx context.Context, accountID string, until time.Time) error
	// Ends returns, for those of accounts whose restriction outlasts now, when
	// each ends.
	Ends(ctx context.Context, accountIDs []string, now time.Time) (map[string]time.Time, error)
}

// Service applies the restriction rules.
type Service struct {
	store       Store
	restriction time.Duration
	now         func() time.Time
}

// NewService builds a Service; restriction is the configured length of one.
// now is injectable for tests.
func NewService(store Store, restriction time.Duration, now func() time.Time) *Service {
	return &Service{store: store, restriction: restriction, now: now}
}

// Dodged restricts an account that left a matchmade champion select on
// purpose, from now for the configured length; a dodge while restricted
// starts it again (ADR-060 §1).
func (s *Service) Dodged(ctx context.Context, accountID string) error {
	return s.store.Restrict(ctx, accountID, s.now().Add(s.restriction))
}

// Remaining returns how long each restricted one of accounts stays so; an
// account that is free is absent.
func (s *Service) Remaining(ctx context.Context, accountIDs []string) (map[string]time.Duration, error) {
	now := s.now()
	ends, err := s.store.Ends(ctx, accountIDs, now)
	if err != nil {
		return nil, err
	}
	out := make(map[string]time.Duration, len(ends))
	for id, end := range ends {
		if left := end.Sub(now); left > 0 {
			out[id] = left
		}
	}
	return out, nil
}
