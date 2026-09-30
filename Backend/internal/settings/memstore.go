package settings

import (
	"context"
	"maps"
	"sync"
)

// MemStore is an in-memory Store for tests and local runs without Postgres.
type MemStore struct {
	mu   sync.Mutex
	docs map[string]Document
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore { return &MemStore{docs: map[string]Document{}} }

func (m *MemStore) Get(_ context.Context, accountID string) (Document, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	d, ok := m.docs[accountID]
	if !ok {
		return Document{Values: map[string]string{}}, nil
	}
	return Document{Revision: d.Revision, Values: maps.Clone(d.Values)}, nil
}

func (m *MemStore) Put(_ context.Context, accountID string, base int64, values map[string]string) (Document, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.docs[accountID].Revision != base {
		return Document{}, ErrConflict
	}
	d := Document{Revision: base + 1, Values: maps.Clone(values)}
	m.docs[accountID] = d
	return Document{Revision: d.Revision, Values: maps.Clone(values)}, nil
}
