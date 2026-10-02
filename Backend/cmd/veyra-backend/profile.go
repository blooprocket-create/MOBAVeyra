package main

import (
	"context"
	"errors"
	"slices"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/config"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/profile"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/progression"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

// profileAccounts resolves a profile's name through identity; an unknown
// name is unavailable, as a blocked one is (ADR-048 §3).
type profileAccounts struct{ identity *identity.Service }

func (p profileAccounts) ByName(ctx context.Context, name string) (string, string, error) {
	a, err := p.identity.LookupAccount(ctx, name)
	if errors.Is(err, identity.ErrNotFound) {
		return "", "", profile.ErrUnavailable
	}
	if err != nil {
		return "", "", err
	}
	return a.ID, a.DisplayName, nil
}

// profileProgress reads a profile's level and Mastery from progression, and
// permanent ownership from the account's entitlements; the rotation never
// counts (Profiles Bible §2).
type profileProgress struct {
	progress *progression.Service
	accounts *account.Service
}

func (p profileProgress) Level(ctx context.Context, accountID string) (int, error) {
	s, err := p.progress.Progression(ctx, accountID)
	return s.Level, err
}

func (p profileProgress) Owns(ctx context.Context, accountID, vanguardID string) (bool, error) {
	entitlements, err := p.accounts.Entitlements(ctx, accountID)
	if err != nil {
		return false, err
	}
	return slices.ContainsFunc(entitlements, func(e account.Entitlement) bool { return e.VanguardID == vanguardID }), nil
}

func (p profileProgress) Owned(ctx context.Context, accountID string) ([]string, error) {
	entitlements, err := p.accounts.Entitlements(ctx, accountID)
	if err != nil {
		return nil, err
	}
	out := make([]string, 0, len(entitlements))
	for _, e := range entitlements {
		out = append(out, e.VanguardID)
	}
	return out, nil
}

func (p profileProgress) MasteryLevel(ctx context.Context, accountID, vanguardID string) (int, error) {
	m, err := p.progress.MasteryOf(ctx, accountID, vanguardID)
	return m.Level, err
}

// newProfileService builds player profiles over identity, progression,
// ownership and blocks.
func newProfileService(store profile.Store, cfg config.Profile, ident *identity.Service, progress *progression.Service, accounts *account.Service,
	soc *social.Service) *profile.Service {
	return profile.NewService(store, profileAccounts{identity: ident}, profileProgress{progress: progress, accounts: accounts}, soc,
		profile.Catalog{Icons: cfg.Icons, Backgrounds: cfg.Backgrounds, DefaultIcon: cfg.DefaultIcon, DefaultBackground: cfg.DefaultBackground})
}
