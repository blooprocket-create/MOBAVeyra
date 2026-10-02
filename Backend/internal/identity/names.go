package identity

import (
	"context"
	"crypto/rand"
	"errors"
	"time"
)

// Name changes (ADR-049; Profiles & Identity Bible §4–§5): one free voluntary
// change, later ones paid for; a cooldown between voluntary changes; a name
// claimed from an account inactive past a threshold, its holder then choosing
// a new name for free. Every check runs in one unit of work.
var (
	ErrRenameCooldown  = errors.New("the display name changed too recently")
	ErrSameDisplayName = errors.New("that is already the display name")
	ErrInvalidCurrency = errors.New("not a currency a name change is paid with")
	// ErrRenameRequired refuses what would show a claimed account's
	// placeholder to others until it chooses a new name.
	ErrRenameRequired = errors.New("the account must choose a new display name first")
	// ErrNamesDisabled: name changes are not configured.
	ErrNamesDisabled = errors.New("name changes are not configured")
)

// The currencies a name change is paid with, as the client names them.
const (
	CurrencyFlux        = "flux"
	CurrencyRefinedFlux = "refinedFlux"
)

// placeholderPrefix starts a claimed account's interim name; a random suffix
// makes it unique like any name (ADR-049 §8.6).
const (
	placeholderPrefix   = "Player_"
	placeholderSuffix   = 8
	placeholderAttempts = 5
	placeholderAlphabet = "abcdefghijklmnopqrstuvwxyz0123456789"
)

// NameSettings configures name changes; the config package validates them.
type NameSettings struct {
	// Cooldown is the wait between voluntary changes, paid or not.
	Cooldown time.Duration
	// ClaimAfter is how long since its last launcher login an account's name
	// may be claimed.
	ClaimAfter time.Duration
	// PriceFlux and PriceRefinedFlux pay for a voluntary change after the
	// free one.
	PriceFlux        int64
	PriceRefinedFlux int64
}

// NameState is what identity keeps about an account's name.
type NameState struct {
	FreeChangeUsed bool
	// LastChangeAt is the last voluntary change; zero before any.
	LastChangeAt time.Time
	// RenameRequired is set when another account claimed the name.
	RenameRequired bool
	// LastLauncherLogin is the last successful launcher login; zero when
	// unknown, which never counts as inactive.
	LastLauncherLogin time.Time
}

// NameStatus is what the player sees about changing their name.
type NameStatus struct {
	Account             Account
	FreeChangeAvailable bool
	// NextChangeAt is when a voluntary change is next allowed; zero for now.
	NextChangeAt     time.Time
	RenameRequired   bool
	PriceFlux        int64
	PriceRefinedFlux int64
}

// Payer charges for a voluntary name change after the free one, in the
// caller's unit of work, or refuses it.
type Payer interface {
	ChargeNameChange(ctx context.Context, accountID, currency string, amount int64) error
}

// SetNames enables name changes. atomic runs a unit of work whose ctx every
// store call joins, so the charge, a claim and the change commit together.
func (s *Service) SetNames(settings NameSettings, payer Payer, atomic func(ctx context.Context, fn func(context.Context) error) error) {
	s.names, s.payer, s.atomic = &settings, payer, atomic
}

// NameStatus returns the account's name, whether its free change is left,
// when it may next change it, and the price.
func (s *Service) NameStatus(ctx context.Context, accountID string) (NameStatus, error) {
	if s.names == nil {
		return NameStatus{}, ErrNamesDisabled
	}
	a, err := s.store.AccountByID(ctx, accountID)
	if err != nil {
		return NameStatus{}, err
	}
	state, err := s.store.NameState(ctx, accountID)
	if err != nil {
		return NameStatus{}, err
	}
	out := NameStatus{Account: a, FreeChangeAvailable: !state.FreeChangeUsed, RenameRequired: state.RenameRequired,
		PriceFlux: s.names.PriceFlux, PriceRefinedFlux: s.names.PriceRefinedFlux}
	if next := state.LastChangeAt.Add(s.names.Cooldown); !state.LastChangeAt.IsZero() && !state.RenameRequired && s.now().Before(next) {
		out.NextChangeAt = next
	}
	return out, nil
}

// RenameRequired reports whether the account must choose a new name before
// it is shown to others again.
func (s *Service) RenameRequired(ctx context.Context, accountID string) (bool, error) {
	state, err := s.store.NameState(ctx, accountID)
	return state.RenameRequired, err
}

// ChangeDisplayName gives the account name. A required rename is free and
// exempt from the cooldown; a voluntary one keeps the cooldown and, after
// the free one, is charged in currency. A name another account holds is
// claimed when that account has not logged into the launcher for
// ClaimAfter; otherwise it is ErrDisplayNameTaken. All of it, or none,
// commits.
func (s *Service) ChangeDisplayName(ctx context.Context, accountID, name, currency string) (Account, error) {
	if s.names == nil {
		return Account{}, ErrNamesDisabled
	}
	if err := ValidateDisplayName(name); err != nil {
		return Account{}, err
	}
	err := s.atomic(ctx, func(ctx context.Context) error {
		state, err := s.store.LockNameState(ctx, accountID)
		if err != nil {
			return err
		}
		current, err := s.store.AccountByID(ctx, accountID)
		if err != nil {
			return err
		}
		if current.DisplayName == name {
			return ErrSameDisplayName
		}
		now := s.now()
		voluntary := !state.RenameRequired
		if voluntary && !state.LastChangeAt.IsZero() && now.Before(state.LastChangeAt.Add(s.names.Cooldown)) {
			return ErrRenameCooldown
		}
		price := int64(0)
		if voluntary && state.FreeChangeUsed {
			switch currency {
			case CurrencyFlux:
				price = s.names.PriceFlux
			case CurrencyRefinedFlux:
				price = s.names.PriceRefinedFlux
			default:
				return ErrInvalidCurrency
			}
		}
		// Who holds the name, in any letter case: nobody, this account (a
		// change of case), or another, whose name is claimed only past the
		// inactivity threshold.
		holder, err := s.store.HolderOfName(ctx, name)
		claim := ""
		switch {
		case errors.Is(err, ErrNotFound) || (err == nil && holder.ID == accountID):
		case err != nil:
			return err
		default:
			held, err := s.store.NameState(ctx, holder.ID)
			if err != nil {
				return err
			}
			if held.LastLauncherLogin.IsZero() || now.Before(held.LastLauncherLogin.Add(s.names.ClaimAfter)) {
				return ErrDisplayNameTaken
			}
			claim = holder.ID
		}
		if price > 0 {
			if err := s.payer.ChargeNameChange(ctx, accountID, currency, price); err != nil {
				return err
			}
		}
		if claim != "" {
			placeholder, err := s.placeholder(ctx)
			if err != nil {
				return err
			}
			if err := s.store.RequireRename(ctx, claim, placeholder); err != nil {
				return err
			}
		}
		return s.store.SetDisplayName(ctx, accountID, name, voluntary, now)
	})
	if err != nil {
		return Account{}, err
	}
	return s.store.AccountByID(ctx, accountID)
}

// placeholder returns an unused interim name for a claimed account.
func (s *Service) placeholder(ctx context.Context) (string, error) {
	for range placeholderAttempts {
		suffix := make([]byte, placeholderSuffix)
		if _, err := rand.Read(suffix); err != nil {
			return "", err
		}
		for i := range suffix {
			suffix[i] = placeholderAlphabet[int(suffix[i])%len(placeholderAlphabet)]
		}
		name := placeholderPrefix + string(suffix)
		if _, err := s.store.HolderOfName(ctx, name); errors.Is(err, ErrNotFound) {
			return name, nil
		} else if err != nil {
			return "", err
		}
	}
	return "", ErrDisplayNameTaken
}

// DevResetName gives the development account now named current, or the one
// named original if none is, its original name back, with its free change,
// cooldown and required rename forgotten, so scripted runs can rename it
// again. Development only.
func (s *Service) DevResetName(ctx context.Context, current, original string) (Account, error) {
	a, err := s.store.AccountByDisplayName(ctx, current)
	if errors.Is(err, ErrNotFound) {
		a, err = s.store.DevAccountByDisplayName(ctx, original)
	}
	if err != nil {
		return Account{}, err
	}
	if err := s.store.DevResetName(ctx, a.ID, original); err != nil {
		return Account{}, err
	}
	return s.store.AccountByID(ctx, a.ID)
}

// touchLauncherLogin records a successful launcher login, which keeps the
// account's name from being claimed.
func (s *Service) touchLauncherLogin(ctx context.Context, accountID string) error {
	return s.store.TouchLauncherLogin(ctx, accountID, s.now())
}

// issueLauncherSession creates a launcher session and records the successful
// launcher login that keeps the account's name from being claimed (ADR-049
// §1). With name changes enabled the two commit together; either way, a
// session that could not be created records no login.
func (s *Service) issueLauncherSession(ctx context.Context, accountID string) (IssuedToken, error) {
	var tok IssuedToken
	issue := func(ctx context.Context) error {
		var err error
		if tok, err = s.createSession(ctx, accountID, SessionLauncher, "", s.settings.LauncherSessionLifetime, prefixLauncherSession); err != nil {
			return err
		}
		return s.touchLauncherLogin(ctx, accountID)
	}
	if s.atomic == nil {
		return tok, issue(ctx)
	}
	if err := s.atomic(ctx, issue); err != nil {
		return IssuedToken{}, err
	}
	return tok, nil
}
