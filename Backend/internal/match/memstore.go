package match

import (
	"bytes"
	"context"
	"slices"
	"sort"
	"sync"
)

// MemStore is an in-memory Store for tests. It is not used in any deployed
// environment. Transactions run under one mutex and roll back on error.
type MemStore struct {
	mu      sync.Mutex
	matches map[string]Match
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore { return &MemStore{matches: map[string]Match{}} }

func copyMatch(m Match) Match {
	m.Participants = append([]Participant(nil), m.Participants...)
	m.Bots = append([]Bot(nil), m.Bots...)
	m.JoinKey = append([]byte(nil), m.JoinKey...)
	if len(m.JoinKey) == 0 {
		m.JoinKey = nil
	}
	m.ServerCredentialHash = append([]byte(nil), m.ServerCredentialHash...)
	if m.Result != nil {
		r := *m.Result
		r.Participants = append([]ParticipantResult(nil), r.Participants...)
		r.Players = copyPlayers(r.Players)
		r.Wells = slices.Clone(r.Wells)
		m.Result = &r
	}
	return m
}

func (s *MemStore) clone() map[string]Match {
	c := make(map[string]Match, len(s.matches))
	for k, v := range s.matches {
		c[k] = copyMatch(v)
	}
	return c
}

func (s *MemStore) InTx(ctx context.Context, fn func(context.Context, Tx) error) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	snapshot := s.clone()
	if err := fn(ctx, memTx{s}); err != nil {
		s.matches = snapshot
		return err
	}
	return nil
}

func (s *MemStore) activeMatchFor(accountID string) (Match, bool) {
	for _, m := range s.matches {
		if _, in := m.Participant(accountID); in && m.State.Active() {
			return m, true
		}
	}
	return Match{}, false
}

func (s *MemStore) ActiveMatchFor(_ context.Context, accountID string) (Match, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	m, ok := s.activeMatchFor(accountID)
	if !ok {
		return Match{}, ErrMatchNotFound
	}
	return copyMatch(m), nil
}

func (s *MemStore) MatchByID(_ context.Context, id string) (Match, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	m, ok := s.matches[id]
	if !ok {
		return Match{}, ErrMatchNotFound
	}
	return copyMatch(m), nil
}

func (s *MemStore) MatchByServerCredential(_ context.Context, hash []byte) (Match, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, m := range s.matches {
		if bytes.Equal(m.ServerCredentialHash, hash) {
			return copyMatch(m), nil
		}
	}
	return Match{}, ErrMatchNotFound
}

func (s *MemStore) MatchBySelectID(_ context.Context, selectID string) (Match, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, m := range s.matches {
		if selectID != "" && m.SelectID == selectID {
			return copyMatch(m), nil
		}
	}
	return Match{}, ErrMatchNotFound
}

func (s *MemStore) LastFluxSpells(_ context.Context, accountID, vanguardID string) ([2]string, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var last Match
	var spells [2]string
	for _, m := range s.matches {
		for _, p := range m.Participants {
			if p.AccountID == accountID && p.VanguardID == vanguardID && !m.ReadyAt.IsZero() && (last.ID == "" || m.CreatedAt.After(last.CreatedAt)) {
				last, spells = m, p.FluxSpells
			}
		}
	}
	return spells, nil
}

func (s *MemStore) MatchHistory(_ context.Context, accountID string, filter HistoryFilter, after *HistoryCursor, limit int) ([]HistoryEntry, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var out []HistoryEntry
	for _, m := range s.matches {
		if e, ok := historyEntryFor(m, accountID, filter); ok && (after == nil || after.before(e)) {
			out = append(out, e)
		}
	}
	sort.Slice(out, func(i, j int) bool {
		if !out[i].EndedAt.Equal(out[j].EndedAt) {
			return out[i].EndedAt.After(out[j].EndedAt)
		}
		return out[i].MatchID > out[j].MatchID
	})
	if len(out) > limit {
		out = out[:limit]
	}
	return out, nil
}

func (s *MemStore) HistoryModes(_ context.Context, accountID string) ([]string, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var out []string
	for _, m := range s.matches {
		if e, ok := historyEntryFor(m, accountID, HistoryFilter{}); ok && !slices.Contains(out, e.Mode) {
			out = append(out, e.Mode)
		}
	}
	slices.Sort(out)
	return out, nil
}

func (s *MemStore) MatchesNeedingAttention(_ context.Context) ([]Match, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var out []Match
	for _, m := range s.matches {
		if m.State.Active() || m.Server.RemovedAt.IsZero() {
			out = append(out, copyMatch(m))
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].CreatedAt.Before(out[j].CreatedAt) })
	return out, nil
}

type memTx struct{ s *MemStore }

func (t memTx) FreePort(lo, hi int) (int, error) {
	held := map[int]bool{}
	for _, m := range t.s.matches {
		if m.Server.RemovedAt.IsZero() {
			held[m.Server.HostPort] = true
		}
	}
	for port := lo; port <= hi; port++ {
		if !held[port] {
			return port, nil
		}
	}
	return 0, ErrNoServerCapacity
}

func (t memTx) CreateMatch(m Match) error {
	// Mirror the database's one-match-per-select and one-active-match-per-account constraints.
	for _, other := range t.s.matches {
		if m.SelectID != "" && other.SelectID == m.SelectID {
			return ErrSelectHasMatch
		}
	}
	for _, p := range m.Participants {
		if _, busy := t.s.activeMatchFor(p.AccountID); busy {
			return ErrAlreadyInMatch
		}
	}
	t.s.matches[m.ID] = copyMatch(m)
	return nil
}

func (t memTx) LockMatch(id string) (Match, error) {
	m, ok := t.s.matches[id]
	if !ok {
		return Match{}, ErrMatchNotFound
	}
	return copyMatch(m), nil
}

func (t memTx) SaveMatch(m Match) error {
	if _, ok := t.s.matches[m.ID]; !ok {
		return ErrMatchNotFound
	}
	t.s.matches[m.ID] = copyMatch(m)
	return nil
}
