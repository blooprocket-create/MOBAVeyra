package config

import "time"

// Dodges configures queue-dodge restrictions (ADR-060 §1). Canon sets no
// schedule (Match Flow Bible §2), so the one length is Provisional.
type Dodges struct {
	// Restriction is how long leaving a matchmade champion select on purpose
	// keeps the player from queueing.
	Restriction time.Duration
}

type fileDodges struct {
	Restriction *Duration `json:"restriction"`
}

// parseDodges validates the dodges section, reporting through missing and
// positive.
func parseDodges(f *fileDodges, missing func(string), positive func(string, *Duration) time.Duration) Dodges {
	if f == nil {
		missing("dodges")
		return Dodges{}
	}
	return Dodges{Restriction: positive("dodges.restriction", f.Restriction)}
}
