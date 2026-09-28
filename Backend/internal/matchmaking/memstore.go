package matchmaking

import (
	"context"
	"sort"
	"sync"
)

// MemStore is an in-memory Store for tests. It is not used in any deployed
// environment. Transactions run under one mutex and roll back on error; the
// party and select calls made inside one are not rolled back with it.
type MemStore struct {
	mu    sync.Mutex
	found map[string]Found
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore { return &MemStore{found: map[string]Found{}} }

func copyFound(f Found) Found {
	f.Parties = append([]FoundParty(nil), f.Parties...)
	f.Seats = append([]Seat(nil), f.Seats...)
	return f
}

func (m *MemStore) InTx(ctx context.Context, fn func(context.Context, Tx) error) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	snapshot := map[string]Found{}
	for k, v := range m.found {
		snapshot[k] = copyFound(v)
	}
	if err := fn(ctx, memTx{m}); err != nil {
		m.found = snapshot
		return err
	}
	return nil
}

func (m *MemStore) pendingFoundOf(accountID string) (string, error) {
	for id, f := range m.found {
		if f.State != Pending {
			continue
		}
		for _, seat := range f.Seats {
			if seat.AccountID == accountID {
				return id, nil
			}
		}
	}
	return "", ErrFoundNotFound
}

func (m *MemStore) PendingFor(_ context.Context, accountID string) (Found, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	id, err := m.pendingFoundOf(accountID)
	if err != nil {
		return Found{}, err
	}
	return copyFound(m.found[id]), nil
}

func (m *MemStore) Pending(context.Context) ([]Found, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	var out []Found
	for _, f := range m.found {
		if f.State == Pending {
			out = append(out, copyFound(f))
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].CreatedAt.Before(out[j].CreatedAt) })
	return out, nil
}

// ByID returns any proposed match, for tests.
func (m *MemStore) ByID(id string) (Found, bool) {
	m.mu.Lock()
	defer m.mu.Unlock()
	f, ok := m.found[id]
	return copyFound(f), ok
}

type memTx struct{ m *MemStore }

func (t memTx) CreateFound(f Found) error {
	for _, seat := range f.Seats {
		if _, err := t.m.pendingFoundOf(seat.AccountID); err == nil {
			return ErrAlreadyFound
		}
	}
	t.m.found[f.ID] = copyFound(f)
	return nil
}

func (t memTx) LockFound(id string) (Found, error) {
	f, ok := t.m.found[id]
	if !ok {
		return Found{}, ErrFoundNotFound
	}
	return copyFound(f), nil
}

func (t memTx) SaveFound(f Found) error {
	t.m.found[f.ID] = copyFound(f)
	return nil
}

func (t memTx) PendingFoundOf(accountID string) (string, error) { return t.m.pendingFoundOf(accountID) }
