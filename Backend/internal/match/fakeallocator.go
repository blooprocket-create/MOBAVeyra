package match

import (
	"context"
	"sync"
)

// FakeAllocator is an in-memory Allocator for tests. It is not used in any
// deployed environment. Servers it "starts" run until a test says otherwise.
type FakeAllocator struct {
	mu       sync.Mutex
	servers  map[string]*fakeServer
	startErr error
	removed  []string
}

type fakeServer struct {
	spec   ServerSpec
	status ServerStatus
}

// NewFakeAllocator returns an empty FakeAllocator.
func NewFakeAllocator() *FakeAllocator {
	return &FakeAllocator{servers: map[string]*fakeServer{}}
}

// FailStarts makes every later Start fail with err, or succeed again if nil.
func (f *FakeAllocator) FailStarts(err error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.startErr = err
}

// Spec returns what the match's server was started with.
func (f *FakeAllocator) Spec(matchID string) (ServerSpec, bool) {
	f.mu.Lock()
	defer f.mu.Unlock()
	s, ok := f.servers[matchID]
	if !ok {
		return ServerSpec{}, false
	}
	return s.spec, true
}

// Exit makes the match's server look as if it stopped with code.
func (f *FakeAllocator) Exit(matchID string, code int) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if s, ok := f.servers[matchID]; ok {
		s.status = ServerStatus{Exited: true, ExitCode: code}
	}
}

// Removed lists the match IDs whose servers were removed, in order.
func (f *FakeAllocator) Removed() []string {
	f.mu.Lock()
	defer f.mu.Unlock()
	return append([]string(nil), f.removed...)
}

func (f *FakeAllocator) Start(_ context.Context, spec ServerSpec) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.startErr != nil {
		return f.startErr
	}
	spec.Assignment = append([]byte(nil), spec.Assignment...)
	f.servers[spec.MatchID] = &fakeServer{spec: spec, status: ServerStatus{Running: true}}
	return nil
}

func (f *FakeAllocator) Status(_ context.Context, matchID string) (ServerStatus, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	s, ok := f.servers[matchID]
	if !ok {
		return ServerStatus{Missing: true}, nil
	}
	return s.status, nil
}

func (f *FakeAllocator) Remove(_ context.Context, matchID string) error {
	f.mu.Lock()
	defer f.mu.Unlock()
	delete(f.servers, matchID)
	f.removed = append(f.removed, matchID)
	return nil
}
