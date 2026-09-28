package selection

import "context"

// Tx is one storage transaction. A session is locked when loaded and stays
// locked until the transaction ends.
type Tx interface {
	// CreateSession stores a new session. It fails with ErrAlreadySelecting if
	// any seated account is already in an active select.
	CreateSession(s Session) error
	// LockSession loads and locks a session, or returns ErrSelectNotFound.
	LockSession(id string) (Session, error)
	// SaveSession stores a session's changes. A session that is no longer
	// active releases its players.
	SaveSession(s Session) error
}

// Store persists champion selects.
type Store interface {
	// InTx runs fn in one transaction. The ctx passed to fn carries it, so the
	// matchmaking calls a select makes with that ctx join it.
	InTx(ctx context.Context, fn func(ctx context.Context, tx Tx) error) error
	// ActiveFor returns the account's active select, or ErrSelectNotFound.
	ActiveFor(ctx context.Context, accountID string) (Session, error)
	// ByID returns a session, or ErrSelectNotFound.
	ByID(ctx context.Context, id string) (Session, error)
	// Active returns every active session.
	Active(ctx context.Context) ([]Session, error)
}
