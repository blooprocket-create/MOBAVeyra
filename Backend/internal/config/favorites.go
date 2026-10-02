package config

// Favorites configures favorite Vanguards (ADR-058 §5). The most is
// Provisional: canon sets none, so it allows the whole released roster.
type Favorites struct {
	// MaxPerAccount is the most favorites an account keeps.
	MaxPerAccount int
}

type fileFavorites struct {
	MaxPerAccount *int `json:"maxPerAccount"`
}

// parseFavorites validates the favorites section, reporting through missing
// and problem.
func parseFavorites(f *fileFavorites, missing, problem func(string)) Favorites {
	var out Favorites
	if f == nil {
		missing("favorites")
		return out
	}
	switch {
	case f.MaxPerAccount == nil:
		missing("favorites.maxPerAccount")
	case *f.MaxPerAccount < 1:
		problem("favorites.maxPerAccount must be at least 1")
	default:
		out.MaxPerAccount = *f.MaxPerAccount
	}
	return out
}
