package profile

import (
	"context"
	"sync"
)

// MemStore is an in-memory Store for tests. It is not used in any deployed
// environment.
type MemStore struct {
	mu          sync.Mutex
	appearances map[string]Appearance
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore { return &MemStore{appearances: map[string]Appearance{}} }

func (m *MemStore) Appearance(_ context.Context, accountID string) (Appearance, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	a, ok := m.appearances[accountID]
	if !ok {
		return Appearance{}, ErrNoAppearance
	}
	return a, nil
}

func (m *MemStore) DeleteAppearance(_ context.Context, accountID string) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	delete(m.appearances, accountID)
	return nil
}

func (m *MemStore) SaveAppearance(_ context.Context, accountID string, a Appearance) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.appearances[accountID] = a
	return nil
}
