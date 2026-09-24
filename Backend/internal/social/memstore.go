package social

import (
	"context"
	"sort"
	"sync"
)

// MemStore is an in-memory Store for tests. It is not used in any deployed
// environment. Transactions run under one mutex and roll back on error.
type MemStore struct {
	mu       sync.Mutex
	accounts map[string]bool
	state    memState
}

type pair struct{ from, to string }

type memState struct {
	requests map[pair]bool
	friends  map[pair]bool // stored with from < to
	blocks   map[pair]bool // from blocks to
}

func (s memState) clone() memState {
	c := memState{requests: map[pair]bool{}, friends: map[pair]bool{}, blocks: map[pair]bool{}}
	for k := range s.requests {
		c.requests[k] = true
	}
	for k := range s.friends {
		c.friends[k] = true
	}
	for k := range s.blocks {
		c.blocks[k] = true
	}
	return c
}

// NewMemStore returns a store that knows the given account IDs.
func NewMemStore(accounts ...string) *MemStore {
	m := &MemStore{accounts: map[string]bool{}, state: memState{}.clone()}
	for _, a := range accounts {
		m.accounts[a] = true
	}
	return m
}

func ordered(a, b string) pair {
	if a < b {
		return pair{a, b}
	}
	return pair{b, a}
}

type memTx struct{ m *MemStore }

func (m *MemStore) InTx(_ context.Context, fn func(Tx) error) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	snapshot := m.state.clone()
	if err := fn(memTx{m}); err != nil {
		m.state = snapshot
		return err
	}
	return nil
}

func (t memTx) LockPair(_, _ string) error            { return nil }
func (t memTx) AccountExists(id string) (bool, error) { return t.m.accounts[id], nil }
func (t memTx) BlockedEither(a, b string) (bool, error) {
	return t.m.state.blocks[pair{a, b}] || t.m.state.blocks[pair{b, a}], nil
}
func (t memTx) AreFriends(a, b string) (bool, error) { return t.m.state.friends[ordered(a, b)], nil }
func (t memTx) RequestExists(from, to string) (bool, error) {
	return t.m.state.requests[pair{from, to}], nil
}
func (t memTx) PutRequest(from, to string) error {
	t.m.state.requests[pair{from, to}] = true
	return nil
}
func (t memTx) DeleteRequest(from, to string) error {
	delete(t.m.state.requests, pair{from, to})
	return nil
}
func (t memTx) PutFriendship(a, b string) error {
	t.m.state.friends[ordered(a, b)] = true
	return nil
}
func (t memTx) DeleteFriendship(a, b string) error {
	delete(t.m.state.friends, ordered(a, b))
	return nil
}
func (t memTx) PutBlock(blocker, blocked string) error {
	t.m.state.blocks[pair{blocker, blocked}] = true
	return nil
}
func (t memTx) DeleteBlock(blocker, blocked string) error {
	delete(t.m.state.blocks, pair{blocker, blocked})
	return nil
}

func (m *MemStore) collect(match func(pair) (string, bool), set map[pair]bool) []string {
	var out []string
	for k := range set {
		if id, ok := match(k); ok {
			out = append(out, id)
		}
	}
	sort.Strings(out)
	return out
}

func (m *MemStore) Friends(_ context.Context, account string) ([]string, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.collect(func(p pair) (string, bool) {
		switch account {
		case p.from:
			return p.to, true
		case p.to:
			return p.from, true
		}
		return "", false
	}, m.state.friends), nil
}

func (m *MemStore) IncomingRequests(_ context.Context, account string) ([]string, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.collect(func(p pair) (string, bool) { return p.from, p.to == account }, m.state.requests), nil
}

func (m *MemStore) OutgoingRequests(_ context.Context, account string) ([]string, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.collect(func(p pair) (string, bool) { return p.to, p.from == account }, m.state.requests), nil
}

func (m *MemStore) BlocksBy(_ context.Context, account string) ([]string, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.collect(func(p pair) (string, bool) { return p.to, p.from == account }, m.state.blocks), nil
}

func (m *MemStore) AreFriends(_ context.Context, a, b string) (bool, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.state.friends[ordered(a, b)], nil
}

func (m *MemStore) FriendOfAny(_ context.Context, account string, others []string) (bool, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	for _, o := range others {
		if m.state.friends[ordered(account, o)] {
			return true, nil
		}
	}
	return false, nil
}

func (m *MemStore) BlockedWithAny(_ context.Context, account string, others []string) (bool, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	for _, o := range others {
		if m.state.blocks[pair{account, o}] || m.state.blocks[pair{o, account}] {
			return true, nil
		}
	}
	return false, nil
}
