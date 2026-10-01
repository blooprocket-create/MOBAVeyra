package postgres

import (
	"context"
	"errors"
	"slices"
	"sync"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/catalog"
)

func newAccountFixture(t *testing.T) (*account.Service, string) {
	t.Helper()
	store := openTestStore(t)
	a, err := store.EnsureDevAccount(context.Background(), "DevOne")
	if err != nil {
		t.Fatal(err)
	}
	c := catalog.New(catalog.Settings{Released: []string{"cairn", "qazharr", "oriel", "bryn"}, Starters: []string{"cairn", "qazharr", "oriel"},
		Rotation: catalog.RotationSettings{Slots: 12, Epoch: time.Date(2026, 9, 28, 0, 0, 0, 0, time.UTC), Week: 7 * 24 * time.Hour, Seed: "test"}}, time.Now)
	return account.NewService(store.Account(), c, func() time.Time { return time.Now().UTC().Truncate(time.Microsecond) }), a.ID
}

func TestOnboardingInPostgres(t *testing.T) {
	svc, id := newAccountFixture(t)
	ctx := context.Background()
	if p, err := svc.Profile(ctx, id); err != nil || p.TutorialCompleted {
		t.Fatalf("a new account: %+v %v", p, err)
	}
	if _, err := svc.ChooseStarter(ctx, id, "oriel"); err != nil {
		t.Fatalf("ChooseStarter: %v", err)
	}
	p, err := svc.Profile(ctx, id)
	if err != nil || !p.TutorialCompleted || p.StarterVanguardID != "oriel" || p.CompletedAt.IsZero() {
		t.Fatalf("stored profile: %+v %v", p, err)
	}
	if a, _ := svc.Vanguards(ctx, id); !slices.Equal(a.Owned, []string{"oriel"}) {
		t.Fatalf("owned: %+v", a)
	}
	if err := svc.ResetOnboarding(ctx, id); err != nil {
		t.Fatalf("ResetOnboarding: %v", err)
	}
	if p, _ := svc.Profile(ctx, id); p.TutorialCompleted {
		t.Fatalf("after a reset: %+v", p)
	}
	if a, _ := svc.Vanguards(ctx, id); len(a.Owned) != 0 {
		t.Fatalf("after a reset: %+v", a)
	}
}

// Racing starter choices for one account must complete onboarding exactly once.
func TestConcurrentStarterChoices(t *testing.T) {
	svc, id := newAccountFixture(t)
	ctx := context.Background()
	choices := []string{"cairn", "qazharr", "oriel"}
	var wg sync.WaitGroup
	errs := make(chan error, len(choices))
	for _, c := range choices {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, err := svc.ChooseStarter(ctx, id, c)
			errs <- err
		}()
	}
	wg.Wait()
	close(errs)
	chosen := 0
	for err := range errs {
		switch {
		case err == nil:
			chosen++
		case errors.Is(err, account.ErrAlreadyChosen):
		default:
			t.Fatalf("unexpected error: %v", err)
		}
	}
	if a, _ := svc.Vanguards(ctx, id); chosen != 1 || len(a.Owned) != 1 {
		t.Fatalf("want exactly one starter, got %d choices and %+v", chosen, a)
	}
}
