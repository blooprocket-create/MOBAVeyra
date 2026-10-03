package favorites

import (
	"context"
	"slices"
	"sync"
)

// MemStore is an in-memory Store for tests. It is not used in any deployed
// environment.
type MemStore struct {
	mu        sync.Mutex
	favorites map[string][]string
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore { return &MemStore{favorites: map[string][]string{}} }

func (m *MemStore) Favorites(_ context.Context, accountID string) ([]string, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	return slices.Clone(m.favorites[accountID]), nil
}

func (m *MemStore) Add(_ context.Context, accountID, vanguardID string, most int) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	marked := m.favorites[accountID]
	if slices.Contains(marked, vanguardID) {
		return nil
	}
	if len(marked) >= most {
		return ErrFull
	}
	m.favorites[accountID] = append(marked, vanguardID)
	return nil
}

func (m *MemStore) Remove(_ context.Context, accountID, vanguardID string) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.favorites[accountID] = slices.DeleteFunc(m.favorites[accountID], func(id string) bool { return id == vanguardID })
	return nil
}
