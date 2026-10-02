package selection

import (
	"context"
	"sort"
	"sync"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// MemStore is an in-memory Store for tests. It is not used in any deployed
// environment. Transactions run under one mutex and roll back on error.
type MemStore struct {
	mu       sync.Mutex
	sessions map[string]Session
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore { return &MemStore{sessions: map[string]Session{}} }

func copySession(s Session) Session {
	s.Seats = append([]Seat(nil), s.Seats...)
	s.Bots = append([]match.Bot(nil), s.Bots...)
	s.Bans = append([]Ban(nil), s.Bans...)
	s.Trades = append([]Trade(nil), s.Trades...)
	s.Timing.Turns = append([]Turn(nil), s.Timing.Turns...)
	if s.Custom != nil {
		custom := *s.Custom
		s.Custom = &custom
	}
	return s
}

func (m *MemStore) InTx(ctx context.Context, fn func(context.Context, Tx) error) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	snapshot := make(map[string]Session, len(m.sessions))
	for k, v := range m.sessions {
		snapshot[k] = copySession(v)
	}
	if err := fn(ctx, memTx{m}); err != nil {
		m.sessions = snapshot
		return err
	}
	return nil
}

func (m *MemStore) activeFor(accountID string) (Session, bool) {
	for _, s := range m.sessions {
		if s.State.Active() && s.Has(accountID) {
			return s, true
		}
	}
	return Session{}, false
}

func (m *MemStore) ActiveFor(_ context.Context, accountID string) (Session, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	s, ok := m.activeFor(accountID)
	if !ok {
		return Session{}, ErrSelectNotFound
	}
	return copySession(s), nil
}

func (m *MemStore) ByID(_ context.Context, id string) (Session, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	s, ok := m.sessions[id]
	if !ok {
		return Session{}, ErrSelectNotFound
	}
	return copySession(s), nil
}

func (m *MemStore) Active(_ context.Context) ([]Session, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	var out []Session
	for _, s := range m.sessions {
		if s.State.Active() {
			out = append(out, copySession(s))
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].CreatedAt.Before(out[j].CreatedAt) })
	return out, nil
}

type memTx struct{ m *MemStore }

func (t memTx) CreateSession(s Session) error {
	for _, seat := range s.Seats {
		if _, busy := t.m.activeFor(seat.AccountID); busy {
			return ErrAlreadySelecting
		}
	}
	t.m.sessions[s.ID] = copySession(s)
	return nil
}

func (t memTx) LockSession(id string) (Session, error) {
	s, ok := t.m.sessions[id]
	if !ok {
		return Session{}, ErrSelectNotFound
	}
	return copySession(s), nil
}

func (t memTx) SaveSession(s Session) error {
	if _, ok := t.m.sessions[s.ID]; !ok {
		return ErrSelectNotFound
	}
	t.m.sessions[s.ID] = copySession(s)
	return nil
}
