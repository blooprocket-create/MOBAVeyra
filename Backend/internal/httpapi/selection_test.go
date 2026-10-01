package httpapi

import (
	"context"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/catalog"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
)

// newPracticeTestServer serves onboarding, practice, champion select and
// matches together, with a fixture catalog and practice settings.
func newPracticeTestServer(t *testing.T) *httptest.Server {
	t.Helper()
	d := newTestDeps(t, true)
	names := match.AccountsFunc(func(ctx context.Context, ids []string) (map[string]string, error) {
		found, err := d.Identity.Accounts(ctx, ids)
		if err != nil {
			return nil, err
		}
		out := map[string]string{}
		for id, a := range found {
			out[id] = a.DisplayName
		}
		return out, nil
	})
	d.Match = match.NewService(match.NewMemStore(), names, match.NewFakeAllocator(), match.Settings{
		Modes:             map[string]match.Mode{},
		Maps:              match.FakeMaps,
		Practice:          match.PracticeSettings{Enabled: true, Mode: "custom_practice", HostSide: match.SideA},
		ReadyTimeout:      time.Minute,
		MaxDuration:       time.Hour,
		RemoveServerAfter: time.Minute,
		HostPortMin:       7780,
		HostPortMax:       7789,
		PublicHost:        "127.0.0.1",
		BackendURL:        "http://backend:8080",
	}, time.Now)
	c := catalog.New(catalog.Settings{Released: []string{"cairn", "qazharr", "oriel", "bryn"}, Starters: []string{"cairn", "qazharr", "oriel"},
		Rotation: catalog.RotationSettings{Slots: 4, Epoch: time.Date(2026, 9, 28, 0, 0, 0, 0, time.UTC), Week: 7 * 24 * time.Hour, Seed: "test"}}, time.Now)
	d.Account = account.NewService(account.NewMemStore(), c, time.Now)
	notQueued := selection.PartiesFunc(func(context.Context, string) (bool, error) { return false, nil })
	d.Selection = selection.NewService(selection.NewMemStore(), d.Account, names, d.Match, notQueued, d.Social, selection.Settings{
		Practice:        selection.PracticeSettings{Enabled: true, Mode: "custom_practice", HostSide: match.SideA, PickDuration: time.Minute},
		StartingTimeout: time.Minute,
		FluxSpells:      []string{"blink", "scorch"},
	}, time.Now, slog.New(slog.NewTextHandler(io.Discard, nil)))
	return serve(t, d)
}

func TestPracticeSelectOverHTTP(t *testing.T) {
	srv := newPracticeTestServer(t)
	one, _ := gameSession(t, srv, "DevOne")
	two, _ := gameSession(t, srv, "DevTwo")

	if status, body := call(t, srv, "POST", "/v1/practice", one, nil); status != http.StatusConflict || body["error"] != "tutorial_required" {
		t.Fatalf("before the tutorial: %d %v", status, body)
	}
	call(t, srv, "POST", "/v1/me/starter", one, map[string]string{"vanguardId": "cairn"})

	status, opened := call(t, srv, "POST", "/v1/practice", one, nil)
	sel, _ := opened["select"].(map[string]any)
	if status != http.StatusCreated || sel["state"] != "picking" || sel["kind"] != "practice" || sel["remainingSeconds"].(float64) <= 0 {
		t.Fatalf("practice: %d %v", status, opened)
	}
	// The timer's full length, for a client's countdown bar: never less than what is left of it.
	if pick, ok := sel["pickSeconds"].(float64); !ok || pick <= 0 || pick < sel["remainingSeconds"].(float64) {
		t.Fatalf("pickSeconds: %v", sel)
	}
	seats := sel["seats"].([]any)
	if len(seats) != 1 || seats[0].(map[string]any)["you"] != true || seats[0].(map[string]any)["displayName"] != "DevOne" {
		t.Fatalf("seats: %v", seats)
	}
	selectID := sel["id"].(string)

	_, current := call(t, srv, "GET", "/v1/me/select", one, nil)
	if current["select"].(map[string]any)["id"] != selectID {
		t.Fatalf("the player's select: %v", current)
	}
	if _, none := call(t, srv, "GET", "/v1/me/select", two, nil); none["select"] != nil {
		t.Fatalf("another player has none: %v", none)
	}
	if status, body := call(t, srv, "PUT", "/v1/me/select/hover", one, map[string]string{"vanguardId": "test_vanguard"}); status != http.StatusBadRequest || body["error"] != "not_available" {
		t.Fatalf("an unavailable Vanguard: %d %v", status, body)
	}
	_, hovered := call(t, srv, "PUT", "/v1/me/select/hover", one, map[string]string{"vanguardId": "bryn"})
	if hovered["select"].(map[string]any)["seats"].([]any)[0].(map[string]any)["hover"] != "bryn" {
		t.Fatalf("the rotation's Vanguard is pickable: %v", hovered)
	}
	if status, body := call(t, srv, "PUT", "/v1/me/select/spells", one, map[string][]string{"fluxSpells": {"blink"}}); status != http.StatusBadRequest ||
		body["error"] != "invalid_flux_spells" {
		t.Fatalf("one slot of two: %d %v", status, body)
	}
	_, chosen := call(t, srv, "PUT", "/v1/me/select/spells", one, map[string][]string{"fluxSpells": {"scorch", ""}})
	if spells, _ := chosen["select"].(map[string]any)["seats"].([]any)[0].(map[string]any)["fluxSpells"].([]any); len(spells) != 2 || spells[0] != "scorch" ||
		spells[1] != "" {
		t.Fatalf("the player's spells: %v", chosen)
	}
	status, locked := call(t, srv, "POST", "/v1/me/select/lock", one, map[string]string{"vanguardId": "bryn"})
	started, _ := locked["select"].(map[string]any)
	if status != http.StatusOK || started["state"] != "started" || started["matchId"] == nil || started["remainingSeconds"].(float64) != 0 {
		t.Fatalf("lock: %d %v", status, locked)
	}

	_, mine := call(t, srv, "GET", "/v1/me/match", one, nil)
	m, _ := mine["match"].(map[string]any)
	if m["id"] != started["matchId"] || m["rules"] != "practice" || m["mode"] != "custom_practice" || m["vanguardId"] != "bryn" {
		t.Fatalf("the practice match: %v", mine)
	}
	if _, over := call(t, srv, "GET", "/v1/me/selects/"+selectID, one, nil); over["select"].(map[string]any)["state"] != "started" {
		t.Fatalf("the select's outcome: %v", over)
	}
	if status, body := call(t, srv, "GET", "/v1/me/selects/"+selectID, two, nil); status != http.StatusNotFound || body["error"] != "select_not_found" {
		t.Fatalf("another player's select: %d %v", status, body)
	}
	if status, body := call(t, srv, "POST", "/v1/practice", one, nil); status != http.StatusConflict || body["error"] != "busy" {
		t.Fatalf("practice while in a match: %d %v", status, body)
	}
	if status, _ := call(t, srv, "POST", "/v1/practice", "", nil); status != http.StatusUnauthorized {
		t.Fatalf("without a session: want 401, got %d", status)
	}
}
