package dodges

import (
	"context"
	"sync"
	"time"
)

// MemStore keeps restrictions in memory, for tests.
type MemStore struct {
	mu   sync.Mutex
	ends map[string]time.Time
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore { return &MemStore{ends: map[string]time.Time{}} }

func (m *MemStore) Restrict(_ context.Context, accountID string, until time.Time) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.ends[accountID] = until
	return nil
}

func (m *MemStore) Ends(_ context.Context, accountIDs []string, now time.Time) (map[string]time.Time, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	out := map[string]time.Time{}
	for _, id := range accountIDs {
		if end, ok := m.ends[id]; ok && end.After(now) {
			out[id] = end
		}
	}
	return out, nil
}
