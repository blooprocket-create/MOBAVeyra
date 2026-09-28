package matchmaking

import "context"

// Tx is one storage transaction. A proposed match is locked when loaded and
// stays locked until the transaction ends.
type Tx interface {
	// CreateFound stores a new proposed match. It fails with ErrAlreadyFound
	// if any of its players is already in a pending one.
	CreateFound(f Found) error
	// LockFound loads and locks a proposed match, or returns ErrFoundNotFound.
	LockFound(id string) (Found, error)
	// SaveFound stores a proposed match's changes. One no longer pending
	// releases its players.
	SaveFound(f Found) error
	// PendingFoundOf returns the ID of the account's pending proposed match,
	// or ErrFoundNotFound.
	PendingFoundOf(accountID string) (string, error)
}

// Store persists proposed matches.
type Store interface {
	// InTx runs fn in one transaction. The ctx passed to fn carries it, so the
	// party and select calls matchmaking makes with that ctx join it.
	InTx(ctx context.Context, fn func(ctx context.Context, tx Tx) error) error
	// PendingFor returns the account's pending proposed match, or
	// ErrFoundNotFound.
	PendingFor(ctx context.Context, accountID string) (Found, error)
	// Pending returns every pending proposed match.
	Pending(ctx context.Context) ([]Found, error)
}
