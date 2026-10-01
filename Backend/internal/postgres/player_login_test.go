package postgres

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"testing"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
)

func TestProviderAccounts(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	if _, err := store.EnsureDevAccount(ctx, "DevOne"); err != nil {
		t.Fatal(err)
	}
	who := identity.ProviderIdentity{Provider: "firebase", Subject: "uid-1"}

	if _, err := store.AccountByProvider(ctx, who); !errors.Is(err, identity.ErrNotFound) {
		t.Fatalf("before: got %v, want ErrNotFound", err)
	}
	created, err := store.CreateProviderAccount(ctx, who, "Ember")
	if err != nil {
		t.Fatal(err)
	}
	found, err := store.AccountByProvider(ctx, who)
	if err != nil || found != created {
		t.Fatalf("AccountByProvider: %+v, %v; want %+v", found, err, created)
	}

	other := identity.ProviderIdentity{Provider: "firebase", Subject: "uid-2"}
	for name, c := range map[string]struct {
		who  identity.ProviderIdentity
		name string
		want error
	}{
		"same identity":     {who, "Another", identity.ErrAlreadyRegistered},
		"same name":         {other, "Ember", identity.ErrDisplayNameTaken},
		"name in new case":  {other, "EMBER", identity.ErrDisplayNameTaken},
		"a dev name folded": {other, "devone", identity.ErrDisplayNameTaken},
	} {
		if _, err := store.CreateProviderAccount(ctx, c.who, c.name); !errors.Is(err, c.want) {
			t.Errorf("%s: got %v, want %v", name, err, c.want)
		}
	}
	// The refused attempts left no half-made account behind.
	if _, err := store.AccountByDisplayName(ctx, "Another"); !errors.Is(err, identity.ErrNotFound) {
		t.Fatalf("refused registration left an account: %v", err)
	}
}

// Racing registrations for one name: exactly one wins.
func TestConcurrentRegistrationsForOneName(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	const racers = 8
	var wg sync.WaitGroup
	results := make(chan error, racers)
	for i := range racers {
		wg.Add(1)
		go func() {
			defer wg.Done()
			who := identity.ProviderIdentity{Provider: "firebase", Subject: fmt.Sprintf("uid-%d", i)}
			_, err := store.CreateProviderAccount(ctx, who, "Contested")
			results <- err
		}()
	}
	wg.Wait()
	close(results)
	wins := 0
	for err := range results {
		switch {
		case err == nil:
			wins++
		case errors.Is(err, identity.ErrDisplayNameTaken):
		default:
			t.Fatalf("unexpected error: %v", err)
		}
	}
	if wins != 1 {
		t.Fatalf("want exactly 1 registration, got %d", wins)
	}
}
