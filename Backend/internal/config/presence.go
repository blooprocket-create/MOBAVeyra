package config

import "time"

// Presence configures who counts as online (ADR-061). Canon leaves the
// thresholds open (Parties, Social & Matchmaking Bible §4), so every value is
// Provisional.
type Presence struct {
	// OfflineAfter: an account not seen for this long is offline, and leaves
	// an Idle or Queued party.
	OfflineAfter time.Duration
	// TouchEvery: how often at most an account's last-seen time is written.
	TouchEvery time.Duration
	// SweepInterval: how often the backend looks for offline party members.
	SweepInterval time.Duration
	// PostMatchGrace: how long after a match ends an offline member keeps
	// their party place.
	PostMatchGrace time.Duration
}

type filePresence struct {
	OfflineAfter   *Duration `json:"offlineAfter"`
	TouchEvery     *Duration `json:"touchEvery"`
	SweepInterval  *Duration `json:"sweepInterval"`
	PostMatchGrace *Duration `json:"postMatchGrace"`
}

// parsePresence validates the presence section, reporting through missing,
// problem and positive.
func parsePresence(f *filePresence, missing, problem func(string), positive func(string, *Duration) time.Duration) Presence {
	if f == nil {
		missing("presence")
		return Presence{}
	}
	p := Presence{
		OfflineAfter:   positive("presence.offlineAfter", f.OfflineAfter),
		TouchEvery:     positive("presence.touchEvery", f.TouchEvery),
		SweepInterval:  positive("presence.sweepInterval", f.SweepInterval),
		PostMatchGrace: positive("presence.postMatchGrace", f.PostMatchGrace),
	}
	// A player whose writes come further apart than the threshold would look
	// offline between them.
	if p.OfflineAfter > 0 && p.TouchEvery >= p.OfflineAfter {
		problem("presence.touchEvery must be shorter than presence.offlineAfter")
	}
	return p
}
