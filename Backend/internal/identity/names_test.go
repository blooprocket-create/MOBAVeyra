package identity

import (
	"context"
	"errors"
	"testing"
	"time"
)

// Fixture name settings, independent of the committed config.
var testNames = NameSettings{Cooldown: 24 * time.Hour, ClaimAfter: 365 * 24 * time.Hour, PriceFlux: 6000, PriceRefinedFlux: 600}

// fakePayer records charges, and refuses them while it is broke.
type fakePayer struct {
	broke   bool
	charges []string
}

var errBroke = errors.New("the balance does not cover the price")

func (p *fakePayer) ChargeNameChange(_ context.Context, accountID, currency string, amount int64) error {
	if p.broke {
		return errBroke
	}
	p.charges = append(p.charges, accountID+":"+currency)
	return nil
}

type namesFixture struct {
	svc   *Service
	store *MemStore
	payer *fakePayer
	now   time.Time
}

func newNamesFixture(t *testing.T) *namesFixture {
	t.Helper()
	f := &namesFixture{store: NewMemStore(), payer: &fakePayer{}, now: time.Date(2026, 10, 2, 12, 0, 0, 0, time.UTC)}
	f.svc = NewService(f.store, Settings{DevLoginEnabled: true, LauncherSessionLifetime: time.Hour}, func() time.Time { return f.now })
	f.svc.SetNames(testNames, f.payer, func(ctx context.Context, fn func(context.Context) error) error { return fn(ctx) })
	return f
}

func (f *namesFixture) account(t *testing.T, name string) Account {
	t.Helper()
	a, err := f.store.CreateAccount(name)
	if err != nil {
		t.Fatal(err)
	}
	if err := f.store.TouchLauncherLogin(context.Background(), a.ID, f.now); err != nil {
		t.Fatal(err)
	}
	return a
}

func TestTheFirstChangeIsFreeAndLaterOnesArePaidAfterTheCooldown(t *testing.T) {
	f := newNamesFixture(t)
	ctx := context.Background()
	a := f.account(t, "Alpha")
	if got, err := f.svc.ChangeDisplayName(ctx, a.ID, "Bravo", ""); err != nil || got.DisplayName != "Bravo" || got.ID != a.ID {
		t.Fatalf("free change: %+v %v", got, err)
	}
	if len(f.payer.charges) != 0 {
		t.Fatalf("the first change is free: %v", f.payer.charges)
	}
	// Paying never skips the cooldown.
	if _, err := f.svc.ChangeDisplayName(ctx, a.ID, "Charlie", CurrencyFlux); !errors.Is(err, ErrRenameCooldown) {
		t.Fatalf("within the cooldown: %v", err)
	}
	status, _ := f.svc.NameStatus(ctx, a.ID)
	if status.FreeChangeAvailable || !status.NextChangeAt.Equal(f.now.Add(testNames.Cooldown)) || status.PriceFlux != 6000 {
		t.Fatalf("status: %+v", status)
	}
	f.now = f.now.Add(testNames.Cooldown)
	if _, err := f.svc.ChangeDisplayName(ctx, a.ID, "Charlie", "gold"); !errors.Is(err, ErrInvalidCurrency) {
		t.Fatalf("a paid change names its currency: %v", err)
	}
	if got, err := f.svc.ChangeDisplayName(ctx, a.ID, "Charlie", CurrencyRefinedFlux); err != nil || got.DisplayName != "Charlie" {
		t.Fatalf("paid change: %+v %v", got, err)
	}
	if len(f.payer.charges) != 1 || f.payer.charges[0] != a.ID+":"+CurrencyRefinedFlux {
		t.Fatalf("charged: %v", f.payer.charges)
	}
}

func TestAnUnpaidChangeChangesNothing(t *testing.T) {
	f := newNamesFixture(t)
	ctx := context.Background()
	a := f.account(t, "Alpha")
	if _, err := f.svc.ChangeDisplayName(ctx, a.ID, "Bravo", ""); err != nil {
		t.Fatal(err)
	}
	f.now = f.now.Add(testNames.Cooldown)
	f.payer.broke = true
	if _, err := f.svc.ChangeDisplayName(ctx, a.ID, "Charlie", CurrencyFlux); !errors.Is(err, errBroke) {
		t.Fatalf("refused payment: %v", err)
	}
	if got, _ := f.store.AccountByID(ctx, a.ID); got.DisplayName != "Bravo" {
		t.Fatalf("the name must not change: %+v", got)
	}
}

func TestNamesAreUniqueIgnoringCaseAndARelinquishedNameIsFreeAtOnce(t *testing.T) {
	f := newNamesFixture(t)
	ctx := context.Background()
	a := f.account(t, "Alpha")
	b := f.account(t, "Bravo")
	if _, err := f.svc.ChangeDisplayName(ctx, a.ID, "BRAVO", ""); !errors.Is(err, ErrDisplayNameTaken) {
		t.Fatalf("taken in another case: %v", err)
	}
	if _, err := f.svc.ChangeDisplayName(ctx, a.ID, "Alpha", ""); !errors.Is(err, ErrSameDisplayName) {
		t.Fatalf("the same name: %v", err)
	}
	if _, err := f.svc.ChangeDisplayName(ctx, a.ID, "x", ""); !errors.Is(err, ErrInvalidDisplayName) {
		t.Fatalf("an invalid name: %v", err)
	}
	// A change of case alone is a change.
	if got, err := f.svc.ChangeDisplayName(ctx, b.ID, "BRAVO", ""); err != nil || got.DisplayName != "BRAVO" {
		t.Fatalf("case change: %+v %v", got, err)
	}
	// Alpha's old name is anyone's at once.
	if _, err := f.svc.ChangeDisplayName(ctx, a.ID, "Delta", ""); err != nil {
		t.Fatal(err)
	}
	c := f.account(t, "Charlie")
	if got, err := f.svc.ChangeDisplayName(ctx, c.ID, "alpha", ""); err != nil || got.DisplayName != "alpha" {
		t.Fatalf("a relinquished name: %+v %v", got, err)
	}
}

func TestAnInactiveHoldersNameIsClaimedAndTheyChooseAgainForFree(t *testing.T) {
	f := newNamesFixture(t)
	ctx := context.Background()
	holder := f.account(t, "Alpha")
	claimant := f.account(t, "Bravo")
	// Logged in recently: the name stays theirs.
	if _, err := f.svc.ChangeDisplayName(ctx, claimant.ID, "Alpha", ""); !errors.Is(err, ErrDisplayNameTaken) {
		t.Fatalf("an active holder: %v", err)
	}
	f.now = f.now.Add(testNames.ClaimAfter)
	if err := f.store.TouchLauncherLogin(ctx, claimant.ID, f.now); err != nil {
		t.Fatal(err)
	}
	if got, err := f.svc.ChangeDisplayName(ctx, claimant.ID, "alpha", ""); err != nil || got.DisplayName != "alpha" {
		t.Fatalf("claim: %+v %v", got, err)
	}
	old, _ := f.store.AccountByID(ctx, holder.ID)
	if old.ID != holder.ID || old.DisplayName == "Alpha" || ValidateDisplayName(old.DisplayName) != nil {
		t.Fatalf("the holder keeps its account under a valid placeholder: %+v", old)
	}
	if required, _ := f.svc.RenameRequired(ctx, holder.ID); !required {
		t.Fatal("the holder must choose a new name")
	}
	// Free, past any cooldown, and its free voluntary change still unused.
	f.payer.broke = true
	if got, err := f.svc.ChangeDisplayName(ctx, holder.ID, "Echo", ""); err != nil || got.DisplayName != "Echo" {
		t.Fatalf("required rename: %+v %v", got, err)
	}
	status, _ := f.svc.NameStatus(ctx, holder.ID)
	if status.RenameRequired || !status.FreeChangeAvailable || !status.NextChangeAt.IsZero() {
		t.Fatalf("after the required rename: %+v", status)
	}
}

func TestALauncherLoginKeepsTheNameFromBeingClaimed(t *testing.T) {
	f := newNamesFixture(t)
	ctx := context.Background()
	holder, err := f.store.EnsureDevAccount(ctx, "DevOne")
	if err != nil {
		t.Fatal(err)
	}
	claimant := f.account(t, "Bravo")
	f.now = f.now.Add(testNames.ClaimAfter * 2)
	if _, _, err := f.svc.DevLogin(ctx, "DevOne"); err != nil {
		t.Fatal(err)
	}
	if _, err := f.svc.ChangeDisplayName(ctx, claimant.ID, "DevOne", ""); !errors.Is(err, ErrDisplayNameTaken) {
		t.Fatalf("logged in just now: %v", err)
	}
	if got, _ := f.store.AccountByID(ctx, holder.ID); got.DisplayName != "DevOne" {
		t.Fatalf("the holder keeps the name: %+v", got)
	}
}
