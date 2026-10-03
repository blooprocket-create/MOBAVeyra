package presence

import (
	"context"
	"sync"
	"time"
)

// MemStore is an in-memory Store for tests. It is not used in any deployed
// environment.
type MemStore struct {
	mu      sync.Mutex
	seen    map[string]time.Time
	hidden  map[string]bool
	touches int
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore {
	return &MemStore{seen: map[string]time.Time{}, hidden: map[string]bool{}}
}

// Touches reports how many times Touch wrote, for tests of the throttle.
func (m *MemStore) Touches() int {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.touches
}

func (m *MemStore) Touch(_ context.Context, accountID string, at time.Time) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.touches++
	if at.After(m.seen[accountID]) {
		m.seen[accountID] = at
	}
	return nil
}

func (m *MemStore) Seen(_ context.Context, accountIDs []string) (map[string]time.Time, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	out := map[string]time.Time{}
	for _, id := range accountIDs {
		if at, ok := m.seen[id]; ok {
			out[id] = at
		}
	}
	return out, nil
}

func (m *MemStore) AppearingOffline(_ context.Context, accountIDs []string) (map[string]bool, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	out := map[string]bool{}
	for _, id := range accountIDs {
		if m.hidden[id] {
			out[id] = true
		}
	}
	return out, nil
}

func (m *MemStore) SetAppearOffline(_ context.Context, accountID string, on bool) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	if on {
		m.hidden[accountID] = true
	} else {
		delete(m.hidden, accountID)
	}
	return nil
}
