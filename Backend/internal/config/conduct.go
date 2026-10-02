package config

import (
	"regexp"
	"slices"
	"time"
)

// Conduct configures reports and commendation (ADR-047 §6). Every value is
// Provisional tuning.
type Conduct struct {
	// Reasons a report may give, as the client shows them by code.
	Reasons []string
	// DetailsMaxCharacters is the longest a report's details may be.
	DetailsMaxCharacters int
	// ReportWindow is how long after a match ends it may be reported.
	ReportWindow time.Duration
	// CommendWindow is how long after a match ends a teammate may be
	// commended: the immediate results.
	CommendWindow time.Duration
}

type fileConduct struct {
	Reasons              []string  `json:"reasons"`
	DetailsMaxCharacters *int      `json:"detailsMaxCharacters"`
	ReportWindow         *Duration `json:"reportWindow"`
	CommendWindow        *Duration `json:"commendWindow"`
}

// reasonPattern is a report reason's code: a word the client names.
var reasonPattern = regexp.MustCompile(`^[a-z][a-z_]{0,31}$`)

// parseConduct validates the conduct section, reporting through missing,
// problem and positive.
func parseConduct(f *fileConduct, missing, problem func(string), positive func(string, *Duration) time.Duration) Conduct {
	var c Conduct
	if f == nil {
		missing("conduct")
		return c
	}
	if len(f.Reasons) == 0 {
		problem("conduct.reasons must name at least one reason")
	}
	for i, reason := range f.Reasons {
		if !reasonPattern.MatchString(reason) {
			problem("conduct.reasons must be lowercase codes such as \"afk\"")
		}
		if slices.Contains(f.Reasons[:i], reason) {
			problem("conduct.reasons must not repeat " + reason)
		}
	}
	c.Reasons = slices.Clone(f.Reasons)
	switch {
	case f.DetailsMaxCharacters == nil:
		missing("conduct.detailsMaxCharacters")
	case *f.DetailsMaxCharacters < 0:
		problem("conduct.detailsMaxCharacters must not be negative")
	default:
		c.DetailsMaxCharacters = *f.DetailsMaxCharacters
	}
	c.ReportWindow = positive("conduct.reportWindow", f.ReportWindow)
	c.CommendWindow = positive("conduct.commendWindow", f.CommendWindow)
	return c
}
