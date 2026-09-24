package identity

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"sync"
	"time"
)

// MemStore is an in-memory Store for tests. It is not used in any deployed
// environment.
type MemStore struct {
	mu          sync.Mutex
	accounts    map[string]Account // by ID
	sessions    map[string]Session // by hex token hash
	launchCodes map[string]*memLaunchCode
}

type memLaunchCode struct {
	code     LaunchCode
	consumed bool
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore {
	return &MemStore{
		accounts:    map[string]Account{},
		sessions:    map[string]Session{},
		launchCodes: map[string]*memLaunchCode{},
	}
}

func (m *MemStore) EnsureDevAccount(_ context.Context, displayName string) (Account, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	for _, a := range m.accounts {
		if a.DisplayName == displayName {
			return a, nil
		}
	}
	id := make([]byte, 16)
	if _, err := rand.Read(id); err != nil {
		return Account{}, err
	}
	a := Account{ID: hex.EncodeToString(id), DisplayName: displayName}
	m.accounts[a.ID] = a
	return a, nil
}

func (m *MemStore) AccountByDisplayName(_ context.Context, displayName string) (Account, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	for _, a := range m.accounts {
		if a.DisplayName == displayName {
			return a, nil
		}
	}
	return Account{}, ErrNotFound
}

func (m *MemStore) AccountByID(_ context.Context, id string) (Account, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	a, ok := m.accounts[id]
	if !ok {
		return Account{}, ErrNotFound
	}
	return a, nil
}

func (m *MemStore) CreateSession(_ context.Context, s Session) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.sessions[hex.EncodeToString(s.TokenHash)] = s
	return nil
}

func (m *MemStore) ActiveSession(_ context.Context, tokenHash []byte, now time.Time) (Session, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	s, ok := m.sessions[hex.EncodeToString(tokenHash)]
	if !ok || !now.Before(s.ExpiresAt) {
		return Session{}, ErrNotFound
	}
	return s, nil
}

func (m *MemStore) CreateLaunchCode(_ context.Context, c LaunchCode) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.launchCodes[hex.EncodeToString(c.CodeHash)] = &memLaunchCode{code: c}
	return nil
}

func (m *MemStore) ConsumeLaunchCode(_ context.Context, codeHash []byte, now time.Time) (LaunchCode, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	lc, ok := m.launchCodes[hex.EncodeToString(codeHash)]
	if !ok || lc.consumed || !now.Before(lc.code.ExpiresAt) {
		return LaunchCode{}, ErrNotFound
	}
	lc.consumed = true
	return lc.code, nil
}
