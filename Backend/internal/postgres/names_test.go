package postgres

import (
	"context"
	"errors"
	"os"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
)

// freeNames charges nothing: these tests exercise the names, not the payment.
type freeNames struct{}

func (freeNames) ChargeNameChange(context.Context, string, string, int64) error { return nil }

func newNamesService(t *testing.T, store *Store, now *time.Time) *identity.Service {
	t.Helper()
	svc := identity.NewService(store, identity.Settings{DevLoginEnabled: true, LauncherSessionLifetime: time.Hour}, func() time.Time { return *now })
	svc.SetNames(identity.NameSettings{Cooldown: 24 * time.Hour, ClaimAfter: 365 * 24 * time.Hour, PriceFlux: 6000, PriceRefinedFlux: 600},
		freeNames{}, store.Atomic)
	return svc
}

func TestNameChangesAndClaimsInPostgres(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	now := time.Now().UTC()
	svc := newNamesService(t, store, &now)
	holder, err := store.EnsureDevAccount(ctx, "PgHolder")
	if err != nil {
		t.Fatal(err)
	}
	claimant, err := store.EnsureDevAccount(ctx, "PgClaimant")
	if err != nil {
		t.Fatal(err)
	}
	// New and migrated accounts count as logged in at their creation.
	if _, err := svc.ChangeDisplayName(ctx, claimant.ID, "pgholder", ""); !errors.Is(err, identity.ErrDisplayNameTaken) {
		t.Fatalf("an active holder: %v", err)
	}
	if got, err := svc.ChangeDisplayName(ctx, claimant.ID, "PgRenamed", ""); err != nil || got.DisplayName != "PgRenamed" {
		t.Fatalf("free change: %+v %v", got, err)
	}
	if _, err := svc.ChangeDisplayName(ctx, claimant.ID, "PgAgain", identity.CurrencyFlux); !errors.Is(err, identity.ErrRenameCooldown) {
		t.Fatalf("cooldown: %v", err)
	}
	// A year later the holder's name is claimable, and they choose again for free.
	now = now.Add(366 * 24 * time.Hour)
	if got, err := svc.ChangeDisplayName(ctx, claimant.ID, "PGHOLDER", identity.CurrencyFlux); err != nil || got.DisplayName != "PGHOLDER" {
		t.Fatalf("claim: %+v %v", got, err)
	}
	if required, err := svc.RenameRequired(ctx, holder.ID); err != nil || !required {
		t.Fatalf("the holder must choose again: %v %v", required, err)
	}
	if got, err := svc.ChangeDisplayName(ctx, holder.ID, "PgReturned", ""); err != nil || got.DisplayName != "PgReturned" || got.ID != holder.ID {
		t.Fatalf("required rename: %+v %v", got, err)
	}
	if status, _ := svc.NameStatus(ctx, holder.ID); status.RenameRequired || !status.FreeChangeAvailable {
		t.Fatalf("after the required rename: %+v", status)
	}
}

// Two accounts racing for one name: exactly one gets it.
func TestConcurrentChangesToOneNameInPostgres(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	now := time.Now().UTC()
	svc := newNamesService(t, store, &now)
	var ids []string
	for _, name := range []string{"PgRacerA", "PgRacerB"} {
		a, err := store.EnsureDevAccount(ctx, name)
		if err != nil {
			t.Fatal(err)
		}
		ids = append(ids, a.ID)
	}
	var wg sync.WaitGroup
	errs := make([]error, len(ids))
	for i, id := range ids {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, errs[i] = svc.ChangeDisplayName(ctx, id, "PgWinner", "")
		}()
	}
	wg.Wait()
	won, lost := 0, 0
	for _, err := range errs {
		switch {
		case err == nil:
			won++
		case errors.Is(err, identity.ErrDisplayNameTaken):
			lost++
		default:
			t.Fatalf("unexpected: %v", err)
		}
	}
	if won != 1 || lost != 1 {
		t.Fatalf("one winner and one loser, got %d and %d", won, lost)
	}
}

// A rename reads and writes only through its own transaction: with a single
// pooled connection it still completes rather than waiting on itself.
func TestARenameNeedsOnlyItsOwnConnectionInPostgres(t *testing.T) {
	openTestStore(t)
	url := os.Getenv(testDatabaseURLEnv)
	separator := "?"
	if strings.Contains(url, "?") {
		separator = "&"
	}
	store, err := Open(context.Background(), url+separator+"pool_max_conns=1")
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	t.Cleanup(store.Close)
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	now := time.Now().UTC()
	svc := newNamesService(t, store, &now)
	a, err := store.EnsureDevAccount(ctx, "PgSolo")
	if err != nil {
		t.Fatal(err)
	}
	if got, err := svc.ChangeDisplayName(ctx, a.ID, "PgSoloRenamed", ""); err != nil || got.DisplayName != "PgSoloRenamed" {
		t.Fatalf("rename on one connection: %+v %v", got, err)
	}
	if _, _, err := svc.DevLogin(ctx, "PgSoloRenamed"); err != nil {
		t.Fatalf("a login, its session and its record in one transaction: %v", err)
	}
}
