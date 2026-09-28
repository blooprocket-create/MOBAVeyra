package account

import (
	"context"
	"errors"
	"slices"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/catalog"
)

var (
	ctx = context.Background()
	t0  = time.Date(2026, 9, 27, 12, 0, 0, 0, time.UTC)
)

// A fixture catalog, independent of the committed config: four released,
// three starters, and a stand-in rotation of everything released.
func newService(standIn catalog.StandIn) *Service {
	c := catalog.New(catalog.Settings{
		Released:      []string{"cairn", "qazharr", "oriel", "bryn"},
		Starters:      []string{"cairn", "qazharr", "oriel"},
		RotationSlots: 12,
		StandIn:       standIn,
	})
	return NewService(NewMemStore(), c, func() time.Time { return t0 })
}

func TestANewAccountHasNotFinishedOnboarding(t *testing.T) {
	s := newService(catalog.StandInAllReleased)
	p, err := s.Profile(ctx, "acc-1")
	if err != nil || p.TutorialCompleted || p.StarterVanguardID != "" || p.AccountID != "acc-1" {
		t.Fatalf("new profile: %+v %v", p, err)
	}
}

func TestChoosingAStarterOwnsItAndFinishesOnboardingOnce(t *testing.T) {
	s := newService(catalog.StandInNone)
	p, err := s.ChooseStarter(ctx, "acc-1", "oriel")
	if err != nil || !p.TutorialCompleted || p.StarterVanguardID != "oriel" || !p.CompletedAt.Equal(t0) {
		t.Fatalf("ChooseStarter: %+v %v", p, err)
	}
	if got, _ := s.Profile(ctx, "acc-1"); !got.TutorialCompleted || got.StarterVanguardID != "oriel" {
		t.Fatalf("stored profile: %+v", got)
	}
	a, _ := s.Vanguards(ctx, "acc-1")
	if !slices.Equal(a.Owned, []string{"oriel"}) || !slices.Equal(a.Available, []string{"oriel"}) {
		t.Fatalf("availability without a rotation: %+v", a)
	}
	if _, err := s.ChooseStarter(ctx, "acc-1", "cairn"); !errors.Is(err, ErrAlreadyChosen) {
		t.Fatalf("a second choice: want ErrAlreadyChosen, got %v", err)
	}
	if a, _ := s.Vanguards(ctx, "acc-1"); !slices.Equal(a.Owned, []string{"oriel"}) {
		t.Fatalf("a refused second choice must grant nothing: %+v", a)
	}
}

func TestOnlyAStarterCanBeChosen(t *testing.T) {
	s := newService(catalog.StandInNone)
	for _, id := range []string{"bryn", "test_vanguard", ""} {
		if _, err := s.ChooseStarter(ctx, "acc-1", id); !errors.Is(err, ErrNotAStarter) {
			t.Fatalf("%q: want ErrNotAStarter, got %v", id, err)
		}
	}
	if p, _ := s.Profile(ctx, "acc-1"); p.TutorialCompleted {
		t.Fatal("a refused choice must not finish onboarding")
	}
}

func TestAvailableIsOwnedAndTheStandInRotation(t *testing.T) {
	s := newService(catalog.StandInAllReleased)
	if _, err := s.ChooseStarter(ctx, "acc-1", "qazharr"); err != nil {
		t.Fatal(err)
	}
	a, err := s.Vanguards(ctx, "acc-1")
	if err != nil {
		t.Fatal(err)
	}
	all := []string{"cairn", "qazharr", "oriel", "bryn"}
	if !slices.Equal(a.Owned, []string{"qazharr"}) || !slices.Equal(a.Rotation, all) || !slices.Equal(a.Available, all) ||
		!slices.Equal(a.Starters, []string{"cairn", "qazharr", "oriel"}) {
		t.Fatalf("availability: %+v", a)
	}
	for id, want := range map[string]bool{"bryn": true, "qazharr": true, "test_vanguard": false} {
		if got, _ := s.MayPick(ctx, "acc-1", id); got != want {
			t.Fatalf("MayPick(%s) = %v, want %v", id, got, want)
		}
	}
}

func TestResettingOnboardingForgetsTheStarter(t *testing.T) {
	s := newService(catalog.StandInNone)
	if _, err := s.ChooseStarter(ctx, "acc-1", "cairn"); err != nil {
		t.Fatal(err)
	}
	if err := s.ResetOnboarding(ctx, "acc-1"); err != nil {
		t.Fatal(err)
	}
	if p, _ := s.Profile(ctx, "acc-1"); p.TutorialCompleted {
		t.Fatalf("after a reset: %+v", p)
	}
	if a, _ := s.Vanguards(ctx, "acc-1"); len(a.Owned) != 0 {
		t.Fatalf("after a reset the starter is no longer owned: %+v", a)
	}
	if _, err := s.ChooseStarter(ctx, "acc-1", "oriel"); err != nil {
		t.Fatalf("choosing again after a reset: %v", err)
	}
}
