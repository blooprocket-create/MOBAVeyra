package postgres

import (
	"context"
	"io"
	"log/slog"
	"sync"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/matchmaking"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
)

// openedSelects stands in for champion select, recording what it opened.
type openedSelects struct {
	mu     sync.Mutex
	opened int
}

func (s *openedSelects) OpenCasual(context.Context, string, []matchmaking.SelectSeat) (string, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.opened++
	return "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee", nil
}

type matchmakingFixture struct {
	partyFixture
	selects *openedSelects
	svc     *matchmaking.Service
}

func newMatchmakingFixture(t *testing.T, names ...string) matchmakingFixture {
	t.Helper()
	f := matchmakingFixture{partyFixture: newPartyFixture(t, names...), selects: &openedSelects{}}
	f.svc = matchmaking.NewService(f.store.Matchmaking(), f.parties, f.social, f.selects, matchmaking.Settings{
		Modes:          []matchmaking.Mode{{ID: "casual", TeamSize: 1}},
		AcceptDuration: time.Minute,
	}, time.Now, slog.New(slog.NewTextHandler(io.Discard, nil)))
	return f
}

func (f matchmakingFixture) queue(t *testing.T, name string) {
	t.Helper()
	ctx := context.Background()
	if _, err := f.parties.SelectMode(ctx, f.ids[name], "casual"); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.SetReady(ctx, f.ids[name], true); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.StartQueue(ctx, f.ids[name]); err != nil {
		t.Fatal(err)
	}
}

// A match found survives a round trip, and everyone accepting moves both
// parties into champion select and ends it.
func TestMatchFoundInPostgres(t *testing.T) {
	f := newMatchmakingFixture(t, "A", "B")
	ctx := context.Background()
	f.queue(t, "A")
	f.queue(t, "B")
	if err := f.svc.MatchOnce(ctx); err != nil {
		t.Fatalf("MatchOnce: %v", err)
	}
	found, ok, err := f.svc.Current(ctx, f.ids["A"])
	if err != nil || !ok || found.State != matchmaking.Pending || len(found.Parties) != 2 || len(found.Seats) != 2 {
		t.Fatalf("match found: %+v %v", found, err)
	}
	if p, _ := f.parties.Get(ctx, f.ids["B"]); p.Status != party.Found || p.QueuedAt.IsZero() {
		t.Fatalf("B's party: %+v", p)
	}
	if _, err := f.svc.Accept(ctx, f.ids["A"]); err != nil {
		t.Fatalf("Accept A: %v", err)
	}
	accepted, err := f.svc.Accept(ctx, f.ids["B"])
	if err != nil || accepted.State != matchmaking.Accepted || accepted.SelectID == "" || f.selects.opened != 1 {
		t.Fatalf("accepted: %+v %v", accepted, err)
	}
	stored, err := f.store.Matchmaking().FoundByID(ctx, found.ID)
	if err != nil || stored.State != matchmaking.Accepted || stored.Seats[0].Decision != matchmaking.Accept {
		t.Fatalf("stored: %+v %v", stored, err)
	}
	if _, ok, _ := f.svc.Current(ctx, f.ids["A"]); ok {
		t.Fatal("an accepted match found releases its players")
	}
	if p, _ := f.parties.Get(ctx, f.ids["A"]); p.Status != party.Selecting {
		t.Fatalf("A's party: %s", p.Status)
	}
}

// Matchmaker passes racing each other take each party at most once.
func TestConcurrentMatchmakerPassesMatchAPartyOnce(t *testing.T) {
	f := newMatchmakingFixture(t, "A", "B", "C", "D")
	ctx := context.Background()
	for _, name := range []string{"A", "B", "C", "D"} {
		f.queue(t, name)
	}
	var wg sync.WaitGroup
	errs := make(chan error, 4)
	for range 4 {
		wg.Add(1)
		go func() {
			defer wg.Done()
			errs <- f.svc.MatchOnce(ctx)
		}()
	}
	wg.Wait()
	close(errs)
	for err := range errs {
		if err != nil {
			t.Fatalf("MatchOnce: %v", err)
		}
	}
	pending, err := f.store.Matchmaking().Pending(ctx)
	if err != nil || len(pending) != 2 {
		t.Fatalf("four players make two matches, whoever formed them: %d %v", len(pending), err)
	}
	seen := map[string]bool{}
	for _, found := range pending {
		for _, seat := range found.Seats {
			if seen[seat.AccountID] {
				t.Fatalf("%s is in two matches", seat.AccountID)
			}
			seen[seat.AccountID] = true
		}
	}
}
