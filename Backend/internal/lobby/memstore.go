package lobby

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
	lobbies map[string]Lobby
	invites map[string]Invite
}

func copyLobby(l Lobby) Lobby {
	l.Members = append([]Member(nil), l.Members...)
	l.Bots = append([]Bot(nil), l.Bots...)
	if l.Settings.StartingGold != nil {
		g := *l.Settings.StartingGold
		l.Settings.StartingGold = &g
	}
	return l
}

func (s memState) clone() memState {
	c := memState{lobbies: map[string]Lobby{}, invites: map[string]Invite{}}
	for k, v := range s.lobbies {
		c.lobbies[k] = copyLobby(v)
	}
	for k, v := range s.invites {
		c.invites[k] = v
	}
	return c
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore { return &MemStore{state: memState{}.clone()} }

func (m *MemStore) InTx(ctx context.Context, fn func(context.Context, Tx) error) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	snapshot := m.state.clone()
	if err := fn(ctx, memTx{m}); err != nil {
		m.state = snapshot
		return err
	}
	return nil
}

func (m *MemStore) lobbyIDOf(accountID string) (string, error) {
	for id, l := range m.state.lobbies {
		if l.IsMember(accountID) {
			return id, nil
		}
	}
	return "", ErrNotInLobby
}

func (m *MemStore) LobbyOf(_ context.Context, accountID string) (Lobby, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	id, err := m.lobbyIDOf(accountID)
	if err != nil {
		return Lobby{}, err
	}
	return copyLobby(m.state.lobbies[id]), nil
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

func (t memTx) LobbyIDOf(accountID string) (string, error) { return t.m.lobbyIDOf(accountID) }

func (t memTx) LockLobby(id string) (Lobby, error) {
	l, ok := t.m.state.lobbies[id]
	if !ok {
		return Lobby{}, ErrLobbyNotFound
	}
	return copyLobby(l), nil
}

func (t memTx) CreateLobby(l Lobby) error {
	t.m.state.lobbies[l.ID] = copyLobby(l)
	return nil
}

func (t memTx) SaveLobby(l Lobby) error {
	if _, ok := t.m.state.lobbies[l.ID]; !ok {
		return ErrLobbyNotFound
	}
	t.m.state.lobbies[l.ID] = copyLobby(l)
	return nil
}

func (t memTx) DeleteLobby(id string) error {
	delete(t.m.state.lobbies, id)
	for k, inv := range t.m.state.invites {
		if inv.LobbyID == id {
			delete(t.m.state.invites, k)
		}
	}
	return nil
}

func (t memTx) PutInvite(inv Invite) error {
	for k, old := range t.m.state.invites {
		if old.LobbyID == inv.LobbyID && old.InviteeID == inv.InviteeID {
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

func (t memTx) DeleteInvitesInto(lobbyID, invitee string) error {
	for k, inv := range t.m.state.invites {
		if inv.LobbyID == lobbyID && inv.InviteeID == invitee {
			delete(t.m.state.invites, k)
		}
	}
	return nil
}
