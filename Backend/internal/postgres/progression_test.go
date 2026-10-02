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
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/progression"
)

// progressionTuning is fixture tuning, independent of the committed config.
func progressionTuning() progression.Tuning {
	return progression.Tuning{
		AccountXP:     progression.AccountXP{PerMinute: 6, WinBonus: 30, CoopBelowLevel: 10},
		AccountLevels: progression.Curve{First: 100, Growth: 10, GrowthUntil: 50},
		FluxPerLevel:  400,
		RefinedFlux:   progression.RefinedFlux{Milestones: []progression.Milestone{{Level: 3, Amount: 250}}},
		Mastery: progression.MasteryTuning{PerMinute: 10, WinBonus: 100, PerformanceCap: 300, Levels: progression.Curve{First: 500, Growth: 100, GrowthUntil: 5},
			EmoteTierLevels: []int{1, 5}},
		Prices: map[string]progression.Price{"cairn": {Flux: 1500, RefinedFlux: 300}, "qazharr": {Flux: 1500, RefinedFlux: 300},
			"oriel": {Flux: 1500, RefinedFlux: 300}, "bryn": {Flux: 3000, RefinedFlux: 550}},
	}
}

func newProgressionFixture(t *testing.T) (*matchFixture, *progression.Service, *account.Service) {
	t.Helper()
	f := newMatchFixture(t, "DevOne", "DevTwo")
	c := catalog.New(catalog.Settings{Released: []string{"cairn", "qazharr", "oriel", "bryn"}, Starters: []string{"cairn", "qazharr", "oriel"},
		Rotation: catalog.RotationSettings{Slots: 1, Epoch: time.Date(2026, 9, 28, 0, 0, 0, 0, time.UTC), Week: 7 * 24 * time.Hour, Seed: "test"}}, time.Now)
	accounts := account.NewService(f.store.Account(), c, func() time.Time { return time.Now().UTC().Truncate(time.Microsecond) })
	progress := progression.NewService(f.store.Progression(), accounts, progressionTuning(), map[string]string{"casual": progression.CategoryCasual},
		func() time.Time { return time.Now().UTC().Truncate(time.Microsecond) })
	f.svc.SetRewards(progress)
	return f, progress, accounts
}

func TestAResultGrantsProgressionOnceInPostgres(t *testing.T) {
	f, progress, _ := newProgressionFixture(t)
	ctx := context.Background()
	m, err := f.svc.Create(ctx, f.casual(f.seats(map[string]match.Side{"DevOne": match.SideA, "DevTwo": match.SideB})))
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	stored, _ := f.store.Match().MatchByID(ctx, m.ID)
	cred := f.credential(t, m.ID)
	if err := f.svc.ServerReady(ctx, cred, m.ID); err != nil {
		t.Fatalf("ServerReady: %v", err)
	}
	if _, err := progress.MatchRewards(ctx, m.ID, f.ids["DevOne"]); !errors.Is(err, progression.ErrNoGrant) {
		t.Fatalf("before the result: %v", err)
	}
	r := match.Result{EndReason: match.EndPrimeWellDestroyed, Winner: match.SideA, DurationSeconds: 1800}
	for _, p := range stored.Participants {
		r.Participants = append(r.Participants, match.ParticipantResult{AccountID: p.AccountID, Joined: true, ConnectedAtEnd: true})
	}
	for range 2 {
		if err := f.svc.ServerResult(ctx, cred, m.ID, r); err != nil {
			t.Fatalf("ServerResult: %v", err)
		}
	}
	// 30 minutes × 6 + 30 = 210 XP: Levels 2 and 3, two level-ups' Flux and Level 3's Refined Flux, once.
	won, err := progress.Progression(ctx, f.ids["DevOne"])
	if err != nil || won.Level != 3 || won.LifetimeXP != 210 || won.Flux != 800 || won.RefinedFlux != 250 {
		t.Fatalf("the winner: %+v %v", won, err)
	}
	g, err := progress.MatchRewards(ctx, m.ID, f.ids["DevTwo"])
	if err != nil || g.Reason != progression.Earned || g.AccountXP != 180 || g.VanguardID != "cairn" || g.MasteryPoints != 300 || g.MasteryAfter != 1 {
		t.Fatalf("the loser's grant: %+v %v", g, err)
	}
	masteries, _ := f.store.Progression().Masteries(ctx, f.ids["DevTwo"])
	if len(masteries) != 1 || masteries[0] != (progression.Mastery{VanguardID: "cairn", Level: 1, LevelPoints: 300, LifetimePoints: 300}) {
		t.Fatalf("Mastery: %+v", masteries)
	}
}

func TestPurchasesInPostgres(t *testing.T) {
	f, progress, accounts := newProgressionFixture(t)
	ctx := context.Background()
	buyer := f.ids["DevOne"]
	if _, err := progress.DevGrant(ctx, buyer, 4000, 0); err != nil {
		t.Fatalf("DevGrant: %v", err)
	}
	if _, summary, err := progress.Buy(ctx, buyer, "11111111-1111-1111-1111-111111111111", "bryn", progression.CurrencyFlux); err != nil || summary.Flux != 1000 {
		t.Fatalf("Buy: %+v %v", summary, err)
	}
	if e, _ := accounts.Entitlements(ctx, buyer); len(e) != 1 || e[0].VanguardID != "bryn" || e[0].Source != account.SourcePurchase {
		t.Fatalf("the entitlement: %+v", e)
	}
	// The same ID from another account is someone else's purchase.
	if _, err := progress.DevGrant(ctx, f.ids["DevTwo"], 4000, 0); err != nil {
		t.Fatalf("DevGrant: %v", err)
	}
	if _, _, err := progress.Buy(ctx, f.ids["DevTwo"], "11111111-1111-1111-1111-111111111111", "bryn", progression.CurrencyFlux); !errors.Is(err, progression.ErrPurchaseConflict) {
		t.Fatalf("another account's purchase ID: %v", err)
	}
	if a, _ := progress.Progression(ctx, f.ids["DevTwo"]); a.Flux != 4000 {
		t.Fatalf("a refused purchase spent: %+v", a)
	}
	if _, err := f.store.pool.Exec(ctx, `UPDATE progression.accounts SET flux = -1 WHERE account_id = $1::uuid`, buyer); err == nil {
		t.Fatal("the schema must refuse a negative balance")
	}
}

// Racing purchases of one Vanguard spend once (Bible §6: retries must not
// double-charge).
func TestConcurrentPurchasesSpendOnce(t *testing.T) {
	f, progress, _ := newProgressionFixture(t)
	ctx := context.Background()
	buyer := f.ids["DevOne"]
	if _, err := progress.DevGrant(ctx, buyer, 9000, 0); err != nil {
		t.Fatalf("DevGrant: %v", err)
	}
	ids := []string{"22222222-2222-2222-2222-222222222222", "33333333-3333-3333-3333-333333333333", "44444444-4444-4444-4444-444444444444"}
	errs := make([]error, len(ids))
	var wg sync.WaitGroup
	for i, id := range ids {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, _, errs[i] = progress.Buy(ctx, buyer, id, "bryn", progression.CurrencyFlux)
		}()
	}
	wg.Wait()
	succeeded := slices.IndexFunc(errs, func(err error) bool { return err == nil })
	owned := 0
	for _, err := range errs {
		if err == nil {
			owned++
		} else if !errors.Is(err, progression.ErrAlreadyOwned) {
			t.Fatalf("a racing purchase: %v", err)
		}
	}
	if succeeded < 0 || owned != 1 {
		t.Fatalf("purchases: %v", errs)
	}
	if a, _ := progress.Progression(ctx, buyer); a.Flux != 6000 {
		t.Fatalf("spent more than once: %+v", a)
	}
}
