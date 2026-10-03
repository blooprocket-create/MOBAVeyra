// Package favorites keeps each account's favorite Vanguards (Pre-Game Client UX
// Bible 30; ADR-058 §5): marked while browsing the Collection, and offered as
// champion select's Favorites filter. A favorite never hovers, picks or grants
// anything. Which Vanguards are released belongs to the catalog, and whether an
// account is in a champion select or a match to selection and match; this
// domain reads them through narrow interfaces and owns neither.
package favorites

import (
	"context"
	"errors"
	"slices"
)

// Errors describing rule violations. Callers map them to response codes.
var (
	ErrUnknownVanguard = errors.New("not a released Vanguard")
	// ErrPlaying refuses a change from champion select, where favorites only
	// show (UX-30: no leaving select to edit them), or from a match.
	ErrPlaying = errors.New("favorites change outside champion select and matches")
	// ErrFull refuses a favorite past the configured most.
	ErrFull = errors.New("the account keeps its most favorites")
)

// Store persists favorites.
type Store interface {
	// Favorites returns the account's favorites in the order they were marked.
	Favorites(ctx context.Context, accountID string) ([]string, error)
	// Add marks vanguardID, once; ErrFull when the account already keeps most.
	Add(ctx context.Context, accountID, vanguardID string, most int) error
	// Remove unmarks vanguardID; unmarking one never marked is no error.
	Remove(ctx context.Context, accountID, vanguardID string) error
}

// Catalog lists the released Vanguards.
type Catalog interface {
	Released() []string
}

// Activity answers whether an account is in a champion select or a match.
type Activity interface {
	Playing(ctx context.Context, accountID string) (bool, error)
}

// Service applies the favorites' rules for an acting account.
type Service struct {
	store    Store
	catalog  Catalog
	activity Activity
	most     int
}

// NewService builds a Service; most is the configured greatest number of
// favorites an account keeps.
func NewService(store Store, catalog Catalog, activity Activity, most int) *Service {
	return &Service{store: store, catalog: catalog, activity: activity, most: most}
}

// Favorites returns the account's favorites that are still released, in the
// order they were marked.
func (s *Service) Favorites(ctx context.Context, accountID string) ([]string, error) {
	marked, err := s.store.Favorites(ctx, accountID)
	if err != nil {
		return nil, err
	}
	released := s.catalog.Released()
	out := make([]string, 0, len(marked))
	for _, id := range marked {
		if slices.Contains(released, id) {
			out = append(out, id)
		}
	}
	return out, nil
}

// Set marks or unmarks a released Vanguard as a favorite, outside champion
// select and matches, and returns the favorites as they now stand.
func (s *Service) Set(ctx context.Context, accountID, vanguardID string, favorite bool) ([]string, error) {
	if !slices.Contains(s.catalog.Released(), vanguardID) {
		return nil, ErrUnknownVanguard
	}
	if s.activity != nil {
		playing, err := s.activity.Playing(ctx, accountID)
		if err != nil {
			return nil, err
		}
		if playing {
			return nil, ErrPlaying
		}
	}
	var err error
	if favorite {
		err = s.store.Add(ctx, accountID, vanguardID, s.most)
	} else {
		err = s.store.Remove(ctx, accountID, vanguardID)
	}
	if err != nil {
		return nil, err
	}
	return s.Favorites(ctx, accountID)
}
