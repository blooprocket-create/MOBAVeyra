package httpapi

import (
	"context"
	"net/http"
	"testing"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/favorites"
)

type favoriteCatalog []string

func (c favoriteCatalog) Released() []string { return c }

type favoritePlaying map[string]bool

func (s favoritePlaying) Playing(_ context.Context, accountID string) (bool, error) {
	return s[accountID], nil
}

func TestFavoritesOverHTTP(t *testing.T) {
	d := newTestDeps(t, true)
	busy := favoritePlaying{}
	d.Favorites = favorites.NewService(favorites.NewMemStore(), favoriteCatalog{"cairn", "oriel"}, busy, 2)
	srv := serve(t, d)
	one, oneID := gameSession(t, srv, testAccounts[0])

	if status, got := call(t, srv, "GET", "/v1/me/favorites", one, nil); status != http.StatusOK || len(got["favorites"].([]any)) != 0 {
		t.Fatalf("none yet: %d %v", status, got)
	}
	if status, got := call(t, srv, "PUT", "/v1/me/favorites/oriel", one, nil); status != http.StatusOK || len(got["favorites"].([]any)) != 1 ||
		got["favorites"].([]any)[0] != "oriel" {
		t.Fatalf("marked: %d %v", status, got)
	}
	if status, got := call(t, srv, "PUT", "/v1/me/favorites/no_such_vanguard", one, nil); status != http.StatusBadRequest || got["error"] != "unknown_vanguard" {
		t.Fatalf("unknown: %d %v", status, got)
	}
	// Never from champion select (UX-30) or a match.
	busy[oneID] = true
	if status, got := call(t, srv, "DELETE", "/v1/me/favorites/oriel", one, nil); status != http.StatusConflict || got["error"] != "playing" {
		t.Fatalf("in select: %d %v", status, got)
	}
	busy[oneID] = false
	if status, got := call(t, srv, "DELETE", "/v1/me/favorites/oriel", one, nil); status != http.StatusOK || len(got["favorites"].([]any)) != 0 {
		t.Fatalf("unmarked: %d %v", status, got)
	}
	if status, _ := call(t, srv, "GET", "/v1/me/favorites", "", nil); status != http.StatusUnauthorized {
		t.Fatalf("needs a session: %d", status)
	}
}
