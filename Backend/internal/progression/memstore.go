package progression

import (
	"context"
	"maps"
	"sync"
)

// MemStore is an in-memory Store for tests. A transaction works on a copy
// that replaces the store's state only when its callback succeeds.
type MemStore struct {
	mu    sync.Mutex
	state memState
}

type memState struct {
	accounts    map[string]Account
	masteries   map[[2]string]Mastery
	grants      map[[2]string]Grant
	purchases   map[string]Purchase
	adjustments []DevAdjustment
}

func (s memState) clone() memState {
	return memState{accounts: maps.Clone(s.accounts), masteries: maps.Clone(s.masteries), grants: maps.Clone(s.grants),
		purchases: maps.Clone(s.purchases), adjustments: append([]DevAdjustment(nil), s.adjustments...)}
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore {
	return &MemStore{state: memState{accounts: map[string]Account{}, masteries: map[[2]string]Mastery{}, grants: map[[2]string]Grant{},
		purchases: map[string]Purchase{}}}
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

func newAccount(accountID string) Account { return Account{AccountID: accountID, Level: 1} }

func (s *MemStore) Account(_ context.Context, accountID string) (Account, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if a, ok := s.state.accounts[accountID]; ok {
		return a, nil
	}
	return newAccount(accountID), nil
}

func (s *MemStore) Masteries(_ context.Context, accountID string) ([]Mastery, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var out []Mastery
	for key, m := range s.state.masteries {
		if key[0] == accountID {
			out = append(out, m)
		}
	}
	return out, nil
}

func (s *MemStore) Grant(_ context.Context, matchID, accountID string) (Grant, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if g, ok := s.state.grants[[2]string{matchID, accountID}]; ok {
		return g, nil
	}
	return Grant{}, ErrNoGrant
}

// Adjustments returns the development adjustments recorded, for tests.
func (s *MemStore) Adjustments() []DevAdjustment {
	s.mu.Lock()
	defer s.mu.Unlock()
	return append([]DevAdjustment(nil), s.state.adjustments...)
}

type memTx struct{ state memState }

func (t *memTx) LockAccount(accountID string) (Account, error) {
	if a, ok := t.state.accounts[accountID]; ok {
		return a, nil
	}
	return newAccount(accountID), nil
}

func (t *memTx) SaveAccount(a Account) error {
	t.state.accounts[a.AccountID] = a
	return nil
}

func (t *memTx) LockMastery(accountID, vanguardID string) (Mastery, error) {
	if m, ok := t.state.masteries[[2]string{accountID, vanguardID}]; ok {
		return m, nil
	}
	return Mastery{VanguardID: vanguardID, Level: 1}, nil
}

func (t *memTx) SaveMastery(accountID string, m Mastery) error {
	t.state.masteries[[2]string{accountID, m.VanguardID}] = m
	return nil
}

func (t *memTx) AddGrant(g Grant) error {
	key := [2]string{g.MatchID, g.AccountID}
	if _, ok := t.state.grants[key]; ok {
		return ErrAlreadyGranted
	}
	t.state.grants[key] = g
	return nil
}

func (t *memTx) Purchase(purchaseID string) (Purchase, error) {
	if p, ok := t.state.purchases[purchaseID]; ok {
		return p, nil
	}
	return Purchase{}, ErrPurchaseNotFound
}

func (t *memTx) AddPurchase(p Purchase) error {
	t.state.purchases[p.PurchaseID] = p
	return nil
}

func (t *memTx) AddDevAdjustment(a DevAdjustment) error {
	t.state.adjustments = append(t.state.adjustments, a)
	return nil
}
