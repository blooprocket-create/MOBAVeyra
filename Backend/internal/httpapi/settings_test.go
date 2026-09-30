package httpapi

import (
	"net/http"
	"strings"
	"testing"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/settings"
)

func TestAccountSettingsOverHTTP(t *testing.T) {
	d := newTestDeps(t, true)
	// A test limit, under the test body limit.
	d.Settings = settings.NewService(settings.NewMemStore(), testBodyLimit/2)
	srv := serve(t, d)
	token, _ := gameSession(t, srv, testAccounts[0])
	other, _ := gameSession(t, srv, testAccounts[1])

	status, got := call(t, srv, "GET", "/v1/account/settings", token, nil)
	if status != http.StatusOK || got["revision"].(float64) != 0 || len(got["values"].(map[string]any)) != 0 {
		t.Fatalf("before any write: %d %v", status, got)
	}
	status, got = call(t, srv, "PUT", "/v1/account/settings", token, map[string]any{"schemaVersion": 1, "revision": 0, "values": map[string]string{"camera_move_speed": "70"}})
	if status != http.StatusOK || got["revision"].(float64) != 1 {
		t.Fatalf("the first write: %d %v", status, got)
	}

	// Another machine's write from the stale base is refused with the current document.
	status, got = call(t, srv, "PUT", "/v1/account/settings", token, map[string]any{"schemaVersion": 1, "revision": 0, "values": map[string]string{"camera_move_speed": "10"}})
	current, _ := got["current"].(map[string]any)
	if status != http.StatusConflict || got["error"] != "settings_conflict" || current == nil || current["revision"].(float64) != 1 {
		t.Fatalf("a stale write: %d %v", status, got)
	}
	if current["values"].(map[string]any)["camera_move_speed"] != "70" {
		t.Fatalf("the conflict answers the stored values: %v", current)
	}

	// Each account keeps its own.
	if status, got = call(t, srv, "GET", "/v1/account/settings", other, nil); status != http.StatusOK || got["revision"].(float64) != 0 {
		t.Fatalf("another account's settings: %d %v", status, got)
	}

	status, got = call(t, srv, "PUT", "/v1/account/settings", token, map[string]any{"schemaVersion": 2, "revision": 1, "values": map[string]string{}})
	if status != http.StatusBadRequest || got["error"] != "bad_settings" {
		t.Fatalf("another document version: %d %v", status, got)
	}
	status, got = call(t, srv, "PUT", "/v1/account/settings", token, map[string]any{"schemaVersion": 1, "revision": 1, "values": map[string]string{"camera_move_speed": strings.Repeat("9", testBodyLimit/2)}})
	if status != http.StatusRequestEntityTooLarge || got["error"] != "settings_too_large" {
		t.Fatalf("an oversized document: %d %v", status, got)
	}
	if status, _ = call(t, srv, "GET", "/v1/account/settings", "", nil); status != http.StatusUnauthorized {
		t.Fatalf("without a session: %d", status)
	}
}
