// Package profile keeps each account's public profile appearance: its
// official icon and background, the permanently owned Vanguard it features,
// and whether its Match History is shared (ADR-048). Levels, ownership and
// Mastery belong to progression and blocks to social; this domain reads them
// through narrow interfaces and owns none of them.
package profile

import (
	"context"
	"errors"
	"slices"
)

// Errors describing rule violations. Callers map them to response codes.
var (
	// ErrUnavailable answers for an unknown name and for a block in either
	// direction alike, so a block is never revealed (ADR-048 §3).
	ErrUnavailable = errors.New("no profile can be shown")
	// ErrHistoryPrivate answers while the owner does not share Match History.
	ErrHistoryPrivate    = errors.New("the owner does not share Match History")
	ErrInvalidIcon       = errors.New("not an icon in the catalog")
	ErrInvalidBackground = errors.New("not a background in the catalog")
	ErrNotOwned          = errors.New("the featured Vanguard is not permanently owned")
	// ErrNoAppearance is a store's answer for an account that never chose.
	ErrNoAppearance = errors.New("no appearance saved")
)

// Catalog is the official icons and backgrounds every account may choose,
// validated by the config package (ADR-048 §2).
type Catalog struct {
	Icons             []string
	Backgrounds       []string
	DefaultIcon       string
	DefaultBackground string
}

// Appearance is what an account chose for its profile.
type Appearance struct {
	Icon       string
	Background string
	// FeaturedVanguard is a permanently owned Vanguard; empty for none.
	FeaturedVanguard string
	// ShowMatchHistory shares the account's Match History on its profile;
	// false until the owner turns it on (UX-72).
	ShowMatchHistory bool
}

// Featured is a profile's featured Vanguard with its owner's Mastery Level.
type Featured struct {
	VanguardID   string
	MasteryLevel int
}

// Profile is what another player sees (Profiles Bible §1). It never carries
// the account's ID, email, security settings or anything private.
type Profile struct {
	Name       string
	Icon       string
	Background string
	Level      int
	// Featured is nil when the owner features none.
	Featured           *Featured
	SharesMatchHistory bool
}

// Store persists appearances.
type Store interface {
	// Appearance returns the account's choices, or ErrNoAppearance.
	Appearance(ctx context.Context, accountID string) (Appearance, error)
	SaveAppearance(ctx context.Context, accountID string, a Appearance) error
}

// Accounts resolves a display name to its account.
type Accounts interface {
	// ByName returns the account's ID and display name, or ErrUnavailable.
	ByName(ctx context.Context, name string) (id, displayName string, err error)
}

// Progress is what a profile reads of account progression.
type Progress interface {
	Level(ctx context.Context, accountID string) (int, error)
	// Owns reports permanent ownership; the weekly rotation never counts.
	Owns(ctx context.Context, accountID, vanguardID string) (bool, error)
	// Owned lists the Vanguards the account permanently owns.
	Owned(ctx context.Context, accountID string) ([]string, error)
	MasteryLevel(ctx context.Context, accountID, vanguardID string) (int, error)
}

// Blocks answers whether two accounts block each other.
type Blocks interface {
	// BlockedWithAny reports a block in either direction.
	BlockedWithAny(ctx context.Context, account string, others []string) (bool, error)
}

// Service applies the profile rules for an acting account.
type Service struct {
	store    Store
	accounts Accounts
	progress Progress
	blocks   Blocks
	catalog  Catalog
}

// NewService builds a Service.
func NewService(store Store, accounts Accounts, progress Progress, blocks Blocks, catalog Catalog) *Service {
	return &Service{store: store, accounts: accounts, progress: progress, blocks: blocks, catalog: catalog}
}

// Catalog returns the choices every account may make.
func (s *Service) Catalog() Catalog { return s.catalog }

// FeaturedChoices lists the Vanguards the account may feature: those it
// permanently owns (Profiles Bible §2).
func (s *Service) FeaturedChoices(ctx context.Context, accountID string) ([]string, error) {
	return s.progress.Owned(ctx, accountID)
}

// Settings returns the account's appearance as it shows now: the catalog's
// defaults where it never chose, or chose what the catalog no longer lists,
// and no featured Vanguard it no longer owns.
func (s *Service) Settings(ctx context.Context, accountID string) (Appearance, error) {
	a, err := s.store.Appearance(ctx, accountID)
	if errors.Is(err, ErrNoAppearance) {
		a, err = Appearance{}, nil
	}
	if err != nil {
		return Appearance{}, err
	}
	if !slices.Contains(s.catalog.Icons, a.Icon) {
		a.Icon = s.catalog.DefaultIcon
	}
	if !slices.Contains(s.catalog.Backgrounds, a.Background) {
		a.Background = s.catalog.DefaultBackground
	}
	if a.FeaturedVanguard != "" {
		owned, err := s.progress.Owns(ctx, accountID, a.FeaturedVanguard)
		if err != nil {
			return Appearance{}, err
		}
		if !owned {
			a.FeaturedVanguard = ""
		}
	}
	return a, nil
}

// SaveSettings stores the account's choices: catalog entries, and a
// featured Vanguard it permanently owns or none (ADR-048 §4).
func (s *Service) SaveSettings(ctx context.Context, accountID string, a Appearance) (Appearance, error) {
	if !slices.Contains(s.catalog.Icons, a.Icon) {
		return Appearance{}, ErrInvalidIcon
	}
	if !slices.Contains(s.catalog.Backgrounds, a.Background) {
		return Appearance{}, ErrInvalidBackground
	}
	if a.FeaturedVanguard != "" {
		owned, err := s.progress.Owns(ctx, accountID, a.FeaturedVanguard)
		if err != nil {
			return Appearance{}, err
		}
		if !owned {
			return Appearance{}, ErrNotOwned
		}
	}
	if err := s.store.SaveAppearance(ctx, accountID, a); err != nil {
		return Appearance{}, err
	}
	return a, nil
}

// View returns the profile named name as viewer sees it, or ErrUnavailable
// for an unknown name or a block between them (Profiles Bible §1).
func (s *Service) View(ctx context.Context, viewer, name string) (Profile, error) {
	ownerID, displayName, err := s.owner(ctx, viewer, name)
	if err != nil {
		return Profile{}, err
	}
	a, err := s.Settings(ctx, ownerID)
	if err != nil {
		return Profile{}, err
	}
	level, err := s.progress.Level(ctx, ownerID)
	if err != nil {
		return Profile{}, err
	}
	p := Profile{Name: displayName, Icon: a.Icon, Background: a.Background, Level: level, SharesMatchHistory: a.ShowMatchHistory}
	if a.FeaturedVanguard != "" {
		mastery, err := s.progress.MasteryLevel(ctx, ownerID, a.FeaturedVanguard)
		if err != nil {
			return Profile{}, err
		}
		p.Featured = &Featured{VanguardID: a.FeaturedVanguard, MasteryLevel: mastery}
	}
	return p, nil
}

// HistoryOwner returns the account whose shared Match History viewer may
// read through name's profile: ErrUnavailable as View, and
// ErrHistoryPrivate while the owner does not share it (UX-72). A player's
// own history is always theirs to read.
func (s *Service) HistoryOwner(ctx context.Context, viewer, name string) (string, error) {
	ownerID, _, err := s.owner(ctx, viewer, name)
	if err != nil {
		return "", err
	}
	if ownerID == viewer {
		return ownerID, nil
	}
	a, err := s.Settings(ctx, ownerID)
	if err != nil {
		return "", err
	}
	if !a.ShowMatchHistory {
		return "", ErrHistoryPrivate
	}
	return ownerID, nil
}

// owner resolves name, refusing it across a block in either direction.
func (s *Service) owner(ctx context.Context, viewer, name string) (id, displayName string, err error) {
	id, displayName, err = s.accounts.ByName(ctx, name)
	if err != nil {
		return "", "", err
	}
	if id == viewer {
		return id, displayName, nil
	}
	blocked, err := s.blocks.BlockedWithAny(ctx, viewer, []string{id})
	if err != nil {
		return "", "", err
	}
	if blocked {
		return "", "", ErrUnavailable
	}
	return id, displayName, nil
}
