package party

import (
	"context"
	"sort"
	"sync"
	"time"
)

// MemStore is an in-memory Store for tests. It is not used in any deployed
// environment. Transactions run under one mutex and roll back on error.
type MemStore struct {
	mu    sync.Mutex
	state memState
}

type memState struct {
	parties map[string]Party
	invites map[string]Invite
}

func copyParty(p Party) Party {
	p.Members = append([]Member(nil), p.Members...)
	return p
}

func (s memState) clone() memState {
	c := memState{parties: map[string]Party{}, invites: map[string]Invite{}}
	for k, v := range s.parties {
		c.parties[k] = copyParty(v)
	}
	for k, v := range s.invites {
		c.invites[k] = v
	}
	return c
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore { return &MemStore{state: memState{}.clone()} }

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

func (m *MemStore) partyIDOf(accountID string) (string, error) {
	for id, p := range m.state.parties {
		if p.IsMember(accountID) {
			return id, nil
		}
	}
	return "", ErrNotInParty
}

func (m *MemStore) PartyOf(_ context.Context, accountID string) (Party, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	id, err := m.partyIDOf(accountID)
	if err != nil {
		return Party{}, err
	}
	return copyParty(m.state.parties[id]), nil
}

func (m *MemStore) InvitesFor(_ context.Context, accountID string, now time.Time) ([]Invite, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	var out []Invite
	for _, inv := range m.state.invites {
		if inv.InviteeID == accountID && now.Before(inv.ExpiresAt) {
			out = append(out, inv)
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].CreatedAt.Before(out[j].CreatedAt) })
	return out, nil
}

type memTx struct{ m *MemStore }

func (t memTx) PartyIDOf(accountID string) (string, error) { return t.m.partyIDOf(accountID) }

func (t memTx) LockParty(id string) (Party, error) {
	p, ok := t.m.state.parties[id]
	if !ok {
		return Party{}, ErrPartyNotFound
	}
	return copyParty(p), nil
}

func (t memTx) CreateParty(p Party) error { return t.SaveParty(p) }

func (t memTx) SaveParty(p Party) error {
	// Mirror the database's one-party-per-account constraint.
	for _, mem := range p.Members {
		if id, err := t.m.partyIDOf(mem.AccountID); err == nil && id != p.ID {
			return ErrAlreadyInParty
		}
	}
	t.m.state.parties[p.ID] = copyParty(p)
	return nil
}

func (t memTx) DeleteParty(id string) error {
	delete(t.m.state.parties, id)
	for k, inv := range t.m.state.invites {
		if inv.PartyID == id {
			delete(t.m.state.invites, k)
		}
	}
	return nil
}

func (t memTx) PutInvite(inv Invite) error {
	for k, old := range t.m.state.invites {
		if old.PartyID == inv.PartyID && old.InviteeID == inv.InviteeID {
			delete(t.m.state.invites, k)
		}
	}
	t.m.state.invites[inv.ID] = inv
	return nil
}

func (t memTx) Invite(id string, now time.Time) (Invite, error) {
	inv, ok := t.m.state.invites[id]
	if !ok || !now.Before(inv.ExpiresAt) {
		return Invite{}, ErrInviteNotFound
	}
	return inv, nil
}

func (t memTx) DeleteInvite(id string) error {
	delete(t.m.state.invites, id)
	return nil
}

func (t memTx) DeleteInvitesBetween(a, b string) error {
	for k, inv := range t.m.state.invites {
		if (inv.InviterID == a && inv.InviteeID == b) || (inv.InviterID == b && inv.InviteeID == a) {
			delete(t.m.state.invites, k)
		}
	}
	return nil
}
