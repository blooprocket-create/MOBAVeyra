package conduct

import (
	"context"
	"maps"
	"slices"
	"sync"
	"time"
)

// MemStore is an in-memory Store for tests. A transaction works on a copy
// that replaces the store's state only when its callback succeeds; one lock
// serializes transactions, which is what LockCase promises.
type MemStore struct {
	mu    sync.Mutex
	state memState
}

type memState struct {
	cases         map[string]time.Time
	reports       []Report
	commendations map[[2]string]Commendation
}

func (s memState) clone() memState {
	return memState{cases: maps.Clone(s.cases), reports: slices.Clone(s.reports), commendations: maps.Clone(s.commendations)}
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore {
	return &MemStore{state: memState{cases: map[string]time.Time{}, commendations: map[[2]string]Commendation{}}}
}

func (s *MemStore) InTx(ctx context.Context, fn func(context.Context, Tx) error) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	tx := &memTx{state: s.state.clone()}
	if err := fn(ctx, tx); err != nil {
		return err
	}
	s.state = tx.state
	return nil
}

func (s *MemStore) ReportsBy(_ context.Context, matchID, reporterID string) ([]Report, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var out []Report
	for _, r := range s.state.reports {
		if r.MatchID == matchID && r.ReporterID == reporterID {
			out = append(out, r)
		}
	}
	return out, nil
}

func (s *MemStore) CommendationBy(_ context.Context, matchID, commenderID string) (Commendation, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if c, ok := s.state.commendations[[2]string{matchID, commenderID}]; ok {
		return c, nil
	}
	return Commendation{}, ErrNoCommendation
}

func (s *MemStore) Case(_ context.Context, matchID string) (Case, bool, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	opened, ok := s.state.cases[matchID]
	if !ok {
		return Case{}, false, nil
	}
	c := Case{MatchID: matchID, OpenedAt: opened}
	for _, r := range s.state.reports {
		if r.MatchID == matchID {
			c.Reports = append(c.Reports, r)
		}
	}
	return c, true, nil
}

func (s *MemStore) Commendations(_ context.Context, matchID string) ([]Commendation, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var out []Commendation
	for key, c := range s.state.commendations {
		if key[0] == matchID {
			out = append(out, c)
		}
	}
	slices.SortFunc(out, func(a, b Commendation) int { return a.CreatedAt.Compare(b.CreatedAt) })
	return out, nil
}

type memTx struct{ state memState }

func (t *memTx) LockCase(string) error { return nil }

func (t *memTx) Report(matchID, reporterID, reportedID string) (Report, error) {
	for _, r := range t.state.reports {
		if r.MatchID == matchID && r.ReporterID == reporterID && r.ReportedID == reportedID {
			return r, nil
		}
	}
	return Report{}, ErrReportNotFound
}

func (t *memTx) ReportByClientID(reporterID, clientID string) (Report, error) {
	for _, r := range t.state.reports {
		if r.ReporterID == reporterID && r.ClientID == clientID {
			return r, nil
		}
	}
	return Report{}, ErrReportNotFound
}

func (t *memTx) OpenCase(matchID string, at time.Time) error {
	if _, ok := t.state.cases[matchID]; !ok {
		t.state.cases[matchID] = at
	}
	return nil
}

func (t *memTx) AddReport(r Report) error {
	t.state.reports = append(t.state.reports, r)
	return nil
}

func (t *memTx) Commendation(matchID, commenderID string) (Commendation, error) {
	if c, ok := t.state.commendations[[2]string{matchID, commenderID}]; ok {
		return c, nil
	}
	return Commendation{}, ErrNoCommendation
}

func (t *memTx) AddCommendation(c Commendation) error {
	t.state.commendations[[2]string{c.MatchID, c.CommenderID}] = c
	return nil
}
