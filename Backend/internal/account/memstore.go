package account

import (
	"context"
	"sync"
)

// MemStore is an in-memory Store for tests. It is not used in any deployed
// environment. Transactions run under one mutex and roll back on error.
type MemStore struct {
	mu           sync.Mutex
	onboarding   map[string]Profile
	entitlements map[string][]Entitlement
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore {
	return &MemStore{onboarding: map[string]Profile{}, entitlements: map[string][]Entitlement{}}
}

func (s *MemStore) InTx(_ context.Context, fn func(Tx) error) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	onboarding := make(map[string]Profile, len(s.onboarding))
	for k, v := range s.onboarding {
		onboarding[k] = v
	}
	entitlements := make(map[string][]Entitlement, len(s.entitlements))
	for k, v := range s.entitlements {
		entitlements[k] = append([]Entitlement(nil), v...)
	}
	if err := fn(memTx{s}); err != nil {
		s.onboarding, s.entitlements = onboarding, entitlements
		return err
	}
	return nil
}

func (s *MemStore) Profile(_ context.Context, accountID string) (Profile, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if p, ok := s.onboarding[accountID]; ok {
		return p, nil
	}
	return Profile{AccountID: accountID}, nil
}

func (s *MemStore) Entitlements(_ context.Context, accountID string) ([]Entitlement, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	return append([]Entitlement(nil), s.entitlements[accountID]...), nil
}

func (s *MemStore) ResetOnboarding(_ context.Context, accountID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.onboarding, accountID)
	var kept []Entitlement
	for _, e := range s.entitlements[accountID] {
		if e.Source != SourceStarter {
			kept = append(kept, e)
		}
	}
	s.entitlements[accountID] = kept
	return nil
}

func (s *MemStore) ResetPurchases(_ context.Context, accountID string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	var kept []Entitlement
	for _, e := range s.entitlements[accountID] {
		if e.Source != SourcePurchase {
			kept = append(kept, e)
		}
	}
	s.entitlements[accountID] = kept
	return nil
}

type memTx struct{ s *MemStore }

func (t memTx) CompleteOnboarding(p Profile) error {
	if _, done := t.s.onboarding[p.AccountID]; done {
		return ErrAlreadyChosen
	}
	t.s.onboarding[p.AccountID] = p
	return nil
}

func (t memTx) Grant(accountID string, e Entitlement) error {
	for _, owned := range t.s.entitlements[accountID] {
		if owned.VanguardID == e.VanguardID {
			return nil
		}
	}
	t.s.entitlements[accountID] = append(t.s.entitlements[accountID], e)
	return nil
}
