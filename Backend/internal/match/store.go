package match

import "context"

// Tx is one storage transaction. A match is locked when loaded and stays
// locked until the transaction ends.
type Tx interface {
	// FreePort returns the lowest port in [lo, hi] that no match holds (a
	// match holds its port until its server is removed), or
	// ErrNoServerCapacity. It serialises with other callers, so the port is
	// safe to use for a match created in the same transaction.
	FreePort(lo, hi int) (int, error)
	// CreateMatch stores a new match. It fails with ErrAlreadyInMatch if any
	// participant already has an active match, and with ErrSelectHasMatch if
	// its champion select already created one.
	CreateMatch(m Match) error
	// LockMatch loads and locks a match, or returns ErrMatchNotFound.
	LockMatch(id string) (Match, error)
	// SaveMatch stores a match's changes. A match that is no longer active
	// releases its participants.
	SaveMatch(m Match) error
}

// Store persists matches.
type Store interface {
	InTx(ctx context.Context, fn func(ctx context.Context, tx Tx) error) error
	// ActiveMatchFor returns the account's active match, or ErrMatchNotFound.
	ActiveMatchFor(ctx context.Context, accountID string) (Match, error)
	// MatchByID returns a match, or ErrMatchNotFound.
	MatchByID(ctx context.Context, id string) (Match, error)
	// MatchByServerCredential returns the match whose server credential has
	// this hash, or ErrMatchNotFound.
	MatchByServerCredential(ctx context.Context, hash []byte) (Match, error)
	// MatchBySelectID returns the match a champion select created, or
	// ErrMatchNotFound.
	MatchBySelectID(ctx context.Context, selectID string) (Match, error)
	// MatchesNeedingAttention returns the active matches, and the finished
	// matches whose server has not been removed.
	MatchesNeedingAttention(ctx context.Context) ([]Match, error)
	// LastFluxSpells returns the starting Flux Spells of the account's most
	// recently created match with the Vanguard whose server became ready, so
	// a match that failed to start saves nothing, or two empty slots if it has
	// none.
	LastFluxSpells(ctx context.Context, accountID, vanguardID string) ([2]string, error)
	// MatchHistory returns up to limit of the account's completed matches
	// that match filter, newest first (by end, then by ID), after the cursor
	// when one is given.
	MatchHistory(ctx context.Context, accountID string, filter HistoryFilter, after *HistoryCursor, limit int) ([]HistoryEntry, error)
}
