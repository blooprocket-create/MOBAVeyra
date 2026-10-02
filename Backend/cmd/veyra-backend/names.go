package main

import (
	"context"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/config"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/progression"
)

// nameChangeReason records what a name change's charge paid for.
const nameChangeReason = "display_name_change"

// namePayer charges a voluntary name change after the free one to the
// account's progression, in the change's unit of work (ADR-049 §3).
type namePayer struct{ progress *progression.Service }

func (p namePayer) ChargeNameChange(ctx context.Context, accountID, currency string, amount int64) error {
	return p.progress.Spend(ctx, accountID, nameChangeReason, progression.Currency(currency), amount)
}

// enableNameChanges lets accounts change their names, charged through
// progression, every change in one unit of work.
func enableNameChanges(svc *identity.Service, cfg config.Names, progress *progression.Service,
	atomic func(ctx context.Context, fn func(context.Context) error) error) {
	svc.SetNames(identity.NameSettings{Cooldown: cfg.RenameCooldown, ClaimAfter: cfg.ClaimAfter, PriceFlux: cfg.PriceFlux, PriceRefinedFlux: cfg.PriceRefinedFlux},
		namePayer{progress: progress}, atomic)
}
