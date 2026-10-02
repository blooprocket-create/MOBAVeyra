package httpapi

import (
	"context"
	"errors"
	"net/http"
	"net/url"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/profile"
)

// profileNames resolves a profile's name through the test identity service.
type profileNames struct{ identity *identity.Service }

func (p profileNames) ByName(ctx context.Context, name string) (string, string, error) {
	a, err := p.identity.LookupAccount(ctx, name)
	if errors.Is(err, identity.ErrNotFound) {
		return "", "", profile.ErrUnavailable
	}
	return a.ID, a.DisplayName, err
}

// profileProgress stands in for progression: everyone is level 7 and owns
// cairn, at Mastery Level 3.
type profileProgress struct{}

func (profileProgress) Level(context.Context, string) (int, error) { return 7, nil }
func (profileProgress) Owns(_ context.Context, _, vanguardID string) (bool, error) {
	return vanguardID == "cairn", nil
}
func (profileProgress) Owned(context.Context, string) ([]string, error) {
	return []string{"cairn"}, nil
}
func (profileProgress) MasteryLevel(context.Context, string, string) (int, error) { return 3, nil }

func TestProfilesOverHTTP(t *testing.T) {
	d := newTestDeps(t, true)
	names := match.AccountsFunc(func(ctx context.Context, ids []string) (map[string]string, error) { return map[string]string{}, nil })
	d.Match = match.NewService(match.NewMemStore(), names, match.NewFakeAllocator(), match.Settings{
		Modes: map[string]match.Mode{"casual_select": {ID: "casual_select", Enabled: true, HumanPlayersPerTeam: 5}}, Maps: match.FakeMaps,
		ReadyTimeout: time.Minute, MaxDuration: time.Hour, RemoveServerAfter: time.Minute, HostPortMin: 7780, HostPortMax: 7789,
		PublicHost: "127.0.0.1", BackendURL: "http://backend:8080", HistoryPageSize: 2,
	}, time.Now)
	// Fixture catalog, independent of the committed config.
	d.Profile = profile.NewService(profile.NewMemStore(), profileNames{identity: d.Identity}, profileProgress{}, d.Social, profile.Catalog{
		Icons: []string{"default", "vanguard_cairn"}, Backgrounds: []string{"default"}, DefaultIcon: "default", DefaultBackground: "default"})
	srv := serve(t, d)
	one, _ := gameSession(t, srv, testAccounts[0])
	two, twoID := gameSession(t, srv, testAccounts[1])
	oneProfile := "/v1/profiles/" + url.PathEscape(testAccounts[0])

	// Never chosen: the defaults, nothing featured, history private.
	status, got := call(t, srv, "GET", "/v1/me/profile-settings", one, nil)
	settings := got["settings"].(map[string]any)
	if status != http.StatusOK || settings["icon"] != "default" || settings["featuredVanguardId"] != nil || settings["showMatchHistory"] != false ||
		len(got["catalog"].(map[string]any)["icons"].([]any)) != 2 || len(got["catalog"].(map[string]any)["featuredChoices"].([]any)) != 1 {
		t.Fatalf("settings: %d %v", status, got)
	}
	if status, got = call(t, srv, "GET", oneProfile+"/matches", two, nil); got["error"] != "history_private" {
		t.Fatalf("private by default: %d %v", status, got)
	}

	// Only catalog entries and owned Vanguards save.
	for _, c := range []struct {
		body map[string]any
		code string
	}{
		{map[string]any{"icon": "uploaded", "background": "default"}, "invalid_icon"},
		{map[string]any{"icon": "default", "background": "vanguard_cairn"}, "invalid_background"},
		{map[string]any{"icon": "default", "background": "default", "featuredVanguardId": "oriel"}, "not_owned"},
	} {
		if status, got := call(t, srv, "PUT", "/v1/me/profile-settings", one, c.body); got["error"] != c.code {
			t.Errorf("%v: %d %v, want %s", c.body, status, got, c.code)
		}
	}
	status, got = call(t, srv, "PUT", "/v1/me/profile-settings", one,
		map[string]any{"icon": "vanguard_cairn", "background": "default", "featuredVanguardId": "cairn", "showMatchHistory": true})
	if status != http.StatusOK || got["settings"].(map[string]any)["featuredVanguardId"] != "cairn" {
		t.Fatalf("save: %d %v", status, got)
	}

	// Another player sees the public profile, and the shared history pages.
	status, got = call(t, srv, "GET", oneProfile, two, nil)
	p := got["profile"].(map[string]any)
	featured, _ := p["featured"].(map[string]any)
	if status != http.StatusOK || p["name"] != testAccounts[0] || p["icon"] != "vanguard_cairn" || p["level"] != float64(7) || featured["vanguardId"] != "cairn" ||
		featured["masteryLevel"] != float64(3) || p["sharesMatchHistory"] != true || p["id"] != nil || p["accountId"] != nil {
		t.Fatalf("profile: %d %v", status, got)
	}
	if status, got = call(t, srv, "GET", oneProfile+"/matches", two, nil); status != http.StatusOK || got["matches"] == nil {
		t.Fatalf("shared history: %d %v", status, got)
	}
	if status, got = call(t, srv, "GET", oneProfile+"/matches/00000000-0000-4000-8000-000000000000", two, nil); got["error"] != "match_not_found" {
		t.Fatalf("a record the owner never played: %d %v", status, got)
	}

	// A block either way reads as an unknown name.
	if status, got = call(t, srv, "PUT", "/v1/blocks/"+twoID, one, nil); status != http.StatusOK && status != http.StatusNoContent {
		t.Fatalf("block: %d %v", status, got)
	}
	for _, path := range []string{oneProfile, oneProfile + "/matches", "/v1/profiles/Nobody"} {
		if status, got := call(t, srv, "GET", path, two, nil); got["error"] != "profile_unavailable" {
			t.Errorf("%s: %d %v", path, status, got)
		}
	}
}
