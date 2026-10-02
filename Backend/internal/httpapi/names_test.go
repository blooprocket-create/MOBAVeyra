package httpapi

import (
	"context"
	"net/http"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
)

// namesFree charges nothing: the route tests exercise the names.
type namesFree struct{}

func (namesFree) ChargeNameChange(context.Context, string, string, int64) error { return nil }

func TestDisplayNamesOverHTTP(t *testing.T) {
	d := newTestDeps(t, true)
	// Identity on a clock the test moves, with sessions that outlive it.
	now := time.Now()
	store := identity.NewMemStore()
	for _, name := range testAccounts {
		if _, err := store.EnsureDevAccount(context.Background(), name); err != nil {
			t.Fatal(err)
		}
	}
	const longLived = 10 * 365 * 24 * time.Hour
	d.Identity = identity.NewService(store, identity.Settings{LauncherSessionLifetime: longLived, GameSessionLifetime: longLived,
		LaunchCodeLifetime: longLived, DevLoginEnabled: true}, func() time.Time { return now })
	// Fixture settings, independent of the committed config.
	d.Identity.SetNames(identity.NameSettings{Cooldown: 24 * time.Hour, ClaimAfter: 365 * 24 * time.Hour, PriceFlux: 6000, PriceRefinedFlux: 600},
		namesFree{}, func(ctx context.Context, fn func(context.Context) error) error { return fn(ctx) })
	srv := serve(t, d)
	one, _ := gameSession(t, srv, testAccounts[0])
	two, _ := gameSession(t, srv, testAccounts[1])

	status, got := call(t, srv, "GET", "/v1/me/display-name", one, nil)
	name := got["displayName"].(map[string]any)
	if status != http.StatusOK || name["name"] != testAccounts[0] || name["freeChangeAvailable"] != true || name["nextChangeAt"] != nil ||
		name["price"].(map[string]any)["flux"] != float64(6000) {
		t.Fatalf("status: %d %v", status, got)
	}
	for _, c := range []struct {
		body map[string]string
		code string
	}{
		{map[string]string{"name": "x"}, "invalid_display_name"},
		{map[string]string{"name": testAccounts[0]}, "same_display_name"},
		{map[string]string{"name": testAccounts[1]}, "display_name_taken"},
	} {
		if status, got := call(t, srv, "PUT", "/v1/me/display-name", one, c.body); got["error"] != c.code {
			t.Errorf("%v: %d %v, want %s", c.body, status, got, c.code)
		}
	}
	status, got = call(t, srv, "PUT", "/v1/me/display-name", one, map[string]string{"name": "OneRenamed"})
	name = got["displayName"].(map[string]any)
	if status != http.StatusOK || name["name"] != "OneRenamed" || name["freeChangeAvailable"] != false || name["nextChangeAt"] == nil {
		t.Fatalf("free change: %d %v", status, got)
	}
	if status, got = call(t, srv, "PUT", "/v1/me/display-name", one, map[string]string{"name": "OneAgain", "currency": "flux"}); got["error"] != "rename_cooldown" {
		t.Fatalf("cooldown: %d %v", status, got)
	}

	// Two years on, the first player's name is claimable; until they choose again, nothing shows their placeholder.
	now = now.Add(2 * 365 * 24 * time.Hour)
	if status, got = call(t, srv, "PUT", "/v1/me/display-name", two, map[string]string{"name": "onerenamed"}); status != http.StatusOK {
		t.Fatalf("claim: %d %v", status, got)
	}
	if status, got = call(t, srv, "GET", "/v1/me/display-name", one, nil); got["displayName"].(map[string]any)["renameRequired"] != true {
		t.Fatalf("claimed: %d %v", status, got)
	}
	if status, got = call(t, srv, "POST", "/v1/party/queue", one, nil); got["error"] != "rename_required" {
		t.Fatalf("queueing before choosing: %d %v", status, got)
	}
	if status, got = call(t, srv, "PUT", "/v1/me/display-name", one, map[string]string{"name": "OneReturned"}); status != http.StatusOK ||
		got["displayName"].(map[string]any)["renameRequired"] != false {
		t.Fatalf("required rename: %d %v", status, got)
	}
	if _, got = call(t, srv, "POST", "/v1/party/queue", one, nil); got["error"] == "rename_required" {
		t.Fatalf("after choosing, the name no longer holds anything up: %v", got)
	}
}
