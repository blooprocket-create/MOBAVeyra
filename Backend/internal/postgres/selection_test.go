package postgres

import (
	"context"
	"errors"
	"io"
	"log/slog"
	"sync"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/catalog"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

// newSelectionFixture wires the account, match and selection services to
// Postgres, with DevOne onboarded as Oriel.
func newSelectionFixture(t *testing.T) (*selection.Service, *match.Service, string) {
	t.Helper()
	f := newMatchFixture(t, "DevOne")
	ctx := context.Background()
	c := catalog.New(catalog.Settings{Released: []string{"cairn", "qazharr", "oriel", "bryn"}, Starters: []string{"cairn", "qazharr", "oriel"},
		RotationSlots: 12, StandIn: catalog.StandInNone})
	accounts := account.NewService(f.store.Account(), c, func() time.Time { return f.now })
	if _, err := accounts.ChooseStarter(ctx, f.ids["DevOne"], "oriel"); err != nil {
		t.Fatal(err)
	}
	names := match.AccountsFunc(func(ctx context.Context, ids []string) (map[string]string, error) {
		list, err := f.store.AccountsByIDs(ctx, ids)
		if err != nil {
			return nil, err
		}
		out := map[string]string{}
		for _, a := range list {
			out[a.ID] = a.DisplayName
		}
		return out, nil
	})
	notQueued := selection.PartiesFunc(func(context.Context, string) (bool, error) { return false, nil })
	svc := selection.NewService(f.store.Selection(), accounts, names, f.svc, notQueued, social.NewService(f.store.Social()), selection.Settings{
		Practice:        selection.PracticeSettings{Enabled: true, Mode: "custom_practice", HostSide: match.SideA, PickDuration: time.Minute},
		StartingTimeout: time.Minute,
	}, func() time.Time { return f.now }, slog.New(slog.NewTextHandler(io.Discard, nil)))
	return svc, f.svc, f.ids["DevOne"]
}

func TestPracticeSelectInPostgres(t *testing.T) {
	svc, matches, id := newSelectionFixture(t)
	ctx := context.Background()
	s, err := svc.StartPractice(ctx, id)
	if err != nil {
		t.Fatalf("StartPractice: %v", err)
	}
	if current, ok, err := svc.Current(ctx, id); err != nil || !ok || current.ID != s.ID || current.Seats[0].DisplayName != "DevOne" {
		t.Fatalf("Current: %+v %v %v", current, ok, err)
	}
	if _, err := svc.Hover(ctx, id, "oriel"); err != nil {
		t.Fatalf("Hover: %v", err)
	}
	locked, err := svc.Lock(ctx, id, "oriel")
	if err != nil || locked.State != selection.Started || locked.MatchID == "" {
		t.Fatalf("Lock: %+v %v", locked, err)
	}
	m, found, err := matches.BySelect(ctx, s.ID)
	if err != nil || !found || m.ID != locked.MatchID || m.SelectID != s.ID || m.Participants[0].VanguardID != "oriel" {
		t.Fatalf("the select's match: %+v %v %v", m, found, err)
	}
	final, err := svc.ForParticipant(ctx, id, s.ID)
	if err != nil || final.State != selection.Started || final.Seats[0].Locked != "oriel" || final.Seats[0].LockedAt.IsZero() {
		t.Fatalf("the stored select: %+v %v", final, err)
	}
	if _, ok, _ := svc.Current(ctx, id); ok {
		t.Fatal("a started select releases its player")
	}
	// The database refuses a second match for the select.
	_, err = matches.Create(ctx, match.Spec{Mode: "custom_practice", Rules: match.RulesPractice, HostAccountID: id, SelectID: s.ID,
		Seats: []match.Seat{{AccountID: id, Side: match.SideA, VanguardID: "oriel"}}})
	if !errors.Is(err, match.ErrSelectHasMatch) && !errors.Is(err, match.ErrAlreadyInMatch) {
		t.Fatalf("a second match for the select: %v", err)
	}
}

// Racing practice starts for one account must open exactly one select.
func TestConcurrentPracticeStarts(t *testing.T) {
	svc, _, id := newSelectionFixture(t)
	ctx := context.Background()
	const attempts = 3
	var wg sync.WaitGroup
	errs := make(chan error, attempts)
	for range attempts {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, err := svc.StartPractice(ctx, id)
			errs <- err
		}()
	}
	wg.Wait()
	close(errs)
	opened := 0
	for err := range errs {
		switch {
		case err == nil:
			opened++
		case errors.Is(err, selection.ErrBusy):
		default:
			t.Fatalf("unexpected error: %v", err)
		}
	}
	if opened != 1 {
		t.Fatalf("want exactly one select, got %d", opened)
	}
}
