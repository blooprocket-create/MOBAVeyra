package config

import "time"

// Names configures display-name changes (ADR-049 §3, §8). The cooldown is
// canon (Profiles & Identity Bible §4); the claim threshold is the bible's
// working year (§5); the prices are Provisional.
type Names struct {
	// RenameCooldown is the wait between voluntary changes, paid or not.
	RenameCooldown time.Duration
	// ClaimAfter is how long since its last launcher login an account's
	// name may be claimed by another.
	ClaimAfter time.Duration
	// PriceFlux and PriceRefinedFlux pay for a voluntary change after the
	// free one.
	PriceFlux        int64
	PriceRefinedFlux int64
}

type fileNames struct {
	RenameCooldown *Duration `json:"renameCooldown"`
	ClaimAfter     *Duration `json:"claimAfter"`
	RenamePrice    *struct {
		Flux        *int64 `json:"flux"`
		RefinedFlux *int64 `json:"refinedFlux"`
	} `json:"renamePrice"`
}

// parseNames validates the names section, reporting through missing,
// problem and positive.
func parseNames(f *fileNames, missing, problem func(string), positive func(string, *Duration) time.Duration) Names {
	var n Names
	if f == nil {
		missing("names")
		return n
	}
	n.RenameCooldown = positive("names.renameCooldown", f.RenameCooldown)
	n.ClaimAfter = positive("names.claimAfter", f.ClaimAfter)
	if f.RenamePrice == nil {
		missing("names.renamePrice")
		return n
	}
	for _, price := range []struct {
		field string
		value *int64
		out   *int64
	}{{"names.renamePrice.flux", f.RenamePrice.Flux, &n.PriceFlux}, {"names.renamePrice.refinedFlux", f.RenamePrice.RefinedFlux, &n.PriceRefinedFlux}} {
		switch {
		case price.value == nil:
			missing(price.field)
		case *price.value <= 0:
			problem(price.field + " must be positive")
		default:
			*price.out = *price.value
		}
	}
	return n
}
