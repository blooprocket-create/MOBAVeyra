// Package settings keeps each account's settings document (ADR-024 §1): the
// account-level player settings the client syncs across machines, such as
// bindings, camera and interface choices. The backend keeps the latest
// revision only, and never reads what a setting means: the client's registry
// does. A write names the revision it was based on, so two machines never
// overwrite each other silently (Settings & Accessibility Bible §7).
package settings

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"regexp"
)

// Errors describing a refused write.
var (
	// ErrConflict means the stored revision is not the one the write was based on.
	ErrConflict = errors.New("the settings changed since the revision given")
	// ErrTooLarge means the document is over the configured limit.
	ErrTooLarge = errors.New("the settings document is too large")
	// ErrInvalid means a setting ID or value is malformed.
	ErrInvalid = errors.New("the settings document is invalid")
)

// settingID is the client's setting ID format, the content ID format.
var settingID = regexp.MustCompile(`^[a-z][a-z0-9]*(_[a-z0-9]+)*$`)

// Document is an account's settings: its revision, 0 before any write, and
// each changed setting's value as text by setting ID.
type Document struct {
	Revision int64
	Values   map[string]string
}

// Store persists documents.
type Store interface {
	// Get returns the account's document, or a zero Document with empty values
	// when it has none.
	Get(ctx context.Context, accountID string) (Document, error)
	// Put stores values as revision base+1 when the stored revision is base (0
	// when the account has none yet), and returns ErrConflict, changing
	// nothing, otherwise.
	Put(ctx context.Context, accountID string, base int64, values map[string]string) (Document, error)
}

// Service reads and writes documents within a size limit.
type Service struct {
	store     Store
	limitSize int
}

// NewService returns a Service that refuses documents over maxDocumentBytes
// of JSON.
func NewService(store Store, maxDocumentBytes int) *Service {
	return &Service{store: store, limitSize: maxDocumentBytes}
}

// Get returns the account's document.
func (s *Service) Get(ctx context.Context, accountID string) (Document, error) {
	return s.store.Get(ctx, accountID)
}

// Put replaces the account's document, based on revision base.
func (s *Service) Put(ctx context.Context, accountID string, base int64, values map[string]string) (Document, error) {
	if base < 0 {
		return Document{}, fmt.Errorf("%w: revision must be at least 0", ErrInvalid)
	}
	for id := range values {
		if !settingID.MatchString(id) {
			return Document{}, fmt.Errorf("%w: %q is not a setting ID", ErrInvalid, id)
		}
	}
	encoded, err := json.Marshal(values)
	if err != nil {
		return Document{}, err
	}
	if len(encoded) > s.limitSize {
		return Document{}, ErrTooLarge
	}
	if values == nil {
		values = map[string]string{}
	}
	return s.store.Put(ctx, accountID, base, values)
}
