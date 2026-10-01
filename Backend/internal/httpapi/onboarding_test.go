package httpapi

import (
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/catalog"
)

// newOnboardingTestServer serves the onboarding routes with a fixture catalog:
// four released, three starters, and a stand-in rotation of everything.
func newOnboardingTestServer(t *testing.T, devLogin bool) *httptest.Server {
	t.Helper()
	d := newTestDeps(t, devLogin)
	c := catalog.New(catalog.Settings{Released: []string{"cairn", "qazharr", "oriel", "bryn"}, Starters: []string{"cairn", "qazharr", "oriel"},
		Rotation: catalog.RotationSettings{Slots: 4, Epoch: time.Date(2026, 9, 28, 0, 0, 0, 0, time.UTC), Week: 7 * 24 * time.Hour, Seed: "test"}}, time.Now)
	d.Account = account.NewService(account.NewMemStore(), c, time.Now)
	d.DevAccounts = testAccounts
	return serve(t, d)
}

func TestOnboardingOverHTTP(t *testing.T) {
	srv := newOnboardingTestServer(t, true)
	one, oneID := gameSession(t, srv, "DevOne")

	status, profile := call(t, srv, "GET", "/v1/me/profile", one, nil)
	tutorial, _ := profile["tutorial"].(map[string]any)
	acct, _ := profile["account"].(map[string]any)
	if status != http.StatusOK || tutorial["completed"] != false || tutorial["starterVanguardId"] != nil || acct["id"] != oneID || acct["displayName"] != "DevOne" {
		t.Fatalf("a new profile: %d %v", status, profile)
	}

	status, vanguards := call(t, srv, "GET", "/v1/me/vanguards", one, nil)
	if status != http.StatusOK || len(vanguards["owned"].([]any)) != 0 || len(vanguards["starters"].([]any)) != 3 || len(vanguards["available"].([]any)) != 4 {
		t.Fatalf("a new account's Vanguards: %d %v", status, vanguards)
	}

	if status, body := call(t, srv, "POST", "/v1/me/starter", one, map[string]string{"vanguardId": "bryn"}); status != http.StatusBadRequest || body["error"] != "not_a_starter" {
		t.Fatalf("a Vanguard that is not a starter: %d %v", status, body)
	}
	status, chosen := call(t, srv, "POST", "/v1/me/starter", one, map[string]string{"vanguardId": "oriel"})
	tutorial, _ = chosen["tutorial"].(map[string]any)
	if status != http.StatusOK || tutorial["completed"] != true || tutorial["starterVanguardId"] != "oriel" {
		t.Fatalf("choosing a starter: %d %v", status, chosen)
	}
	if status, body := call(t, srv, "POST", "/v1/me/starter", one, map[string]string{"vanguardId": "cairn"}); status != http.StatusConflict || body["error"] != "already_completed" {
		t.Fatalf("choosing again: %d %v", status, body)
	}
	_, vanguards = call(t, srv, "GET", "/v1/me/vanguards", one, nil)
	if owned := vanguards["owned"].([]any); len(owned) != 1 || owned[0] != "oriel" {
		t.Fatalf("owned after the choice: %v", vanguards)
	}
	for _, path := range []string{"/v1/me/profile", "/v1/me/vanguards"} {
		if status, _ := call(t, srv, "GET", path, "", nil); status != http.StatusUnauthorized {
			t.Fatalf("%s without a session: want 401, got %d", path, status)
		}
	}
}

func TestDevAccountRoutes(t *testing.T) {
	srv := newOnboardingTestServer(t, true)
	status, listed := call(t, srv, "GET", "/v1/dev/accounts", "", nil)
	accounts, _ := listed["accounts"].([]any)
	if status != http.StatusOK || len(accounts) != len(testAccounts) || accounts[0].(map[string]any)["displayName"] != "DevOne" {
		t.Fatalf("dev accounts: %d %v", status, listed)
	}

	one, _ := gameSession(t, srv, "DevOne")
	call(t, srv, "POST", "/v1/me/starter", one, map[string]string{"vanguardId": "cairn"})
	if status, body := call(t, srv, "POST", "/v1/dev/accounts/DevOne/reset-onboarding", "", nil); status != http.StatusNoContent {
		t.Fatalf("reset: %d %v", status, body)
	}
	_, profile := call(t, srv, "GET", "/v1/me/profile", one, nil)
	if profile["tutorial"].(map[string]any)["completed"] != false {
		t.Fatalf("after a reset: %v", profile)
	}
	if status, _ := call(t, srv, "POST", "/v1/dev/accounts/Stranger/reset-onboarding", "", nil); status != http.StatusNotFound {
		t.Fatalf("an account that is not a dev account: want 404, got %d", status)
	}
}

func TestDevAccountRoutesAbsentWithoutDevLogin(t *testing.T) {
	srv := newOnboardingTestServer(t, false)
	if status, _ := call(t, srv, "GET", "/v1/dev/accounts", "", nil); status != http.StatusNotFound {
		t.Fatalf("list: want 404, got %d", status)
	}
	if status, _ := call(t, srv, "POST", "/v1/dev/accounts/DevOne/reset-onboarding", "", nil); status != http.StatusNotFound {
		t.Fatalf("reset: want 404, got %d", status)
	}
}
