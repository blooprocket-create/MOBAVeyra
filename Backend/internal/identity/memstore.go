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
	devSeeded   map[string]bool    // by ID
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
		devSeeded:   map[string]bool{},
		sessions:    map[string]Session{},
		launchCodes: map[string]*memLaunchCode{},
	}
}

func (m *MemStore) EnsureDevAccount(_ context.Context, displayName string) (Account, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	if a, ok := m.byName(displayName); ok {
		if !m.devSeeded[a.ID] {
			return Account{}, ErrNotDevAccount
		}
		return a, nil
	}
	return m.create(displayName, true)
}

// CreateAccount adds an ordinary (non-dev) account, for tests.
func (m *MemStore) CreateAccount(displayName string) (Account, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.create(displayName, false)
}

func (m *MemStore) DevAccountByDisplayName(_ context.Context, displayName string) (Account, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	a, ok := m.byName(displayName)
	if !ok || !m.devSeeded[a.ID] {
		return Account{}, ErrNotFound
	}
	return a, nil
}

func (m *MemStore) AccountByDisplayName(_ context.Context, displayName string) (Account, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	a, ok := m.byName(displayName)
	if !ok {
		return Account{}, ErrNotFound
	}
	return a, nil
}

func (m *MemStore) AccountsByIDs(_ context.Context, ids []string) ([]Account, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	var out []Account
	for _, id := range ids {
		if a, ok := m.accounts[id]; ok {
			out = append(out, a)
		}
	}
	return out, nil
}

func (m *MemStore) byName(displayName string) (Account, bool) {
	for _, a := range m.accounts {
		if a.DisplayName == displayName {
			return a, true
		}
	}
	return Account{}, false
}

func (m *MemStore) create(displayName string, dev bool) (Account, error) {
	id := make([]byte, 16)
	if _, err := rand.Read(id); err != nil {
		return Account{}, err
	}
	a := Account{ID: hex.EncodeToString(id), DisplayName: displayName}
	m.accounts[a.ID] = a
	m.devSeeded[a.ID] = dev
	return a, nil
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

func (m *MemStore) RedeemLaunchCode(_ context.Context, codeHash []byte, buildVersion string, now time.Time, newSession Session) (Account, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	lc, ok := m.launchCodes[hex.EncodeToString(codeHash)]
	if !ok || lc.consumed || !now.Before(lc.code.ExpiresAt) {
		return Account{}, ErrNotFound
	}
	lc.consumed = true
	if lc.code.BuildVersion != buildVersion {
		return Account{}, ErrBuildMismatch
	}
	a, ok := m.accounts[lc.code.AccountID]
	if !ok {
		lc.consumed = false
		return Account{}, ErrNotFound
	}
	newSession.AccountID = a.ID
	m.sessions[hex.EncodeToString(newSession.TokenHash)] = newSession
	return a, nil
}
