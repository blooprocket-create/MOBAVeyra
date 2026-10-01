// Package account owns how far an account is through onboarding and which
// Vanguards it has unlocked (ADR-010 §6; Account, Collection & Mastery Bible
// §1). The first-time tutorial is stubbed as a one-time starter choice until
// the tutorial exists: choosing a starter unlocks it permanently and completes
// the requirement. Other domains ask this one which Vanguards a player may
// pick; only this package writes onboarding and entitlements.
package account

import (
	"context"
	"errors"
	"slices"
	"time"
)

// Errors describing rule violations.
var (
	ErrNotAStarter   = errors.New("not a starter Vanguard")
	ErrAlreadyChosen = errors.New("the starter was already chosen")
)

// Source says how an account came to own a Vanguard.
type Source string

// SourceStarter is the Vanguard chosen at the end of onboarding.
const SourceStarter Source = "starter"

// Profile is an account's onboarding state.
type Profile struct {
	AccountID string
	// TutorialCompleted is set once the account has chosen its starter.
	TutorialCompleted bool
	StarterVanguardID string
	CompletedAt       time.Time
}

// Entitlement is one Vanguard an account owns.
type Entitlement struct {
	VanguardID string
	Source     Source
	GrantedAt  time.Time
}

// Availability is which Vanguards an account may pick (Modes & Access Bible
// §3): what it owns and what the rotation offers, limited to released ones.
type Availability struct {
	Owned    []string
	Rotation []string
	// Available is owned and rotation together, each once, in the catalog's
	// order.
	Available []string
	// Starters are what a new account may choose from.
	Starters []string
	// Released is every released Vanguard, in the catalog's order: what a
	// draft's bans may name (ADR-041 §1).
	Released []string
}

// Catalog answers questions about released Vanguards. The catalog package
// implements it.
type Catalog interface {
	IsReleased(id string) bool
	IsStarter(id string) bool
	Released() []string
	Starters() []string
	Rotation() []string
}

// Tx is one storage transaction.
type Tx interface {
	// CompleteOnboarding stores a finished onboarding, or returns
	// ErrAlreadyChosen if the account already has one.
	CompleteOnboarding(p Profile) error
	// Grant gives the account a Vanguard. Owning it already changes nothing.
	Grant(accountID string, e Entitlement) error
}

// Store persists onboarding and entitlements.
type Store interface {
	InTx(ctx context.Context, fn func(Tx) error) error
	// Profile returns the account's onboarding; an account that has not
	// finished it has a zero Profile with its ID.
	Profile(ctx context.Context, accountID string) (Profile, error)
	Entitlements(ctx context.Context, accountID string) ([]Entitlement, error)
	// ResetOnboarding removes the account's onboarding and starter.
	ResetOnboarding(ctx context.Context, accountID string) error
}

// Service applies onboarding and entitlement rules.
type Service struct {
	store   Store
	catalog Catalog
	now     func() time.Time
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, catalog Catalog, now func() time.Time) *Service {
	return &Service{store: store, catalog: catalog, now: now}
}

// Profile returns the account's onboarding state.
func (s *Service) Profile(ctx context.Context, accountID string) (Profile, error) {
	return s.store.Profile(ctx, accountID)
}

// ChooseStarter completes onboarding with a starter, which the account then
// owns permanently. It can be done once, and only with a starter.
func (s *Service) ChooseStarter(ctx context.Context, accountID, vanguardID string) (Profile, error) {
	if !s.catalog.IsStarter(vanguardID) || !s.catalog.IsReleased(vanguardID) {
		return Profile{}, ErrNotAStarter
	}
	now := s.now()
	p := Profile{AccountID: accountID, TutorialCompleted: true, StarterVanguardID: vanguardID, CompletedAt: now}
	err := s.store.InTx(ctx, func(tx Tx) error {
		if err := tx.CompleteOnboarding(p); err != nil {
			return err
		}
		return tx.Grant(accountID, Entitlement{VanguardID: vanguardID, Source: SourceStarter, GrantedAt: now})
	})
	if err != nil {
		return Profile{}, err
	}
	return p, nil
}

// Vanguards returns which Vanguards the account owns, which the rotation
// offers, and which it may therefore pick.
func (s *Service) Vanguards(ctx context.Context, accountID string) (Availability, error) {
	entitlements, err := s.store.Entitlements(ctx, accountID)
	if err != nil {
		return Availability{}, err
	}
	owned := map[string]bool{}
	for _, e := range entitlements {
		owned[e.VanguardID] = true
	}
	rotation := s.catalog.Rotation()
	out := Availability{Owned: []string{}, Rotation: append([]string{}, rotation...), Available: []string{}, Starters: s.catalog.Starters(),
		Released: append([]string{}, s.catalog.Released()...)}
	// In the catalog's order, so every client lists them the same way. A
	// Vanguard no longer released is owned but not pickable.
	for _, id := range s.catalog.Released() {
		if owned[id] {
			out.Owned = append(out.Owned, id)
		}
		if owned[id] || slices.Contains(rotation, id) {
			out.Available = append(out.Available, id)
		}
	}
	return out, nil
}

// IsReleased reports whether a Vanguard is released: one a draft may ban
// (ADR-041 §1).
func (s *Service) IsReleased(vanguardID string) bool { return s.catalog.IsReleased(vanguardID) }

// MayPick reports whether the account may pick a Vanguard now: it has
// finished onboarding, and owns the Vanguard or the rotation offers it.
func (s *Service) MayPick(ctx context.Context, accountID, vanguardID string) (bool, error) {
	available, err := s.Vanguards(ctx, accountID)
	if err != nil {
		return false, err
	}
	return slices.Contains(available.Available, vanguardID), nil
}

// ResetOnboarding returns an account to before its starter choice. Callers
// expose it only in development, for repeatable test runs.
func (s *Service) ResetOnboarding(ctx context.Context, accountID string) error {
	return s.store.ResetOnboarding(ctx, accountID)
}
