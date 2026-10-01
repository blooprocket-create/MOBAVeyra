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
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/matchmaking"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
)

// casualSeats adapts the select service for the matchmaker, as the backend does.
type casualSeats struct{ selects *selection.Service }

func (c casualSeats) OpenCasual(ctx context.Context, mode string, seats []matchmaking.SelectSeat) (string, error) {
	casual := make([]selection.CasualSeat, len(seats))
	for i, seat := range seats {
		casual[i] = selection.CasualSeat{AccountID: seat.AccountID, Side: seat.Side}
	}
	return c.selects.OpenCasual(ctx, mode, casual)
}

// playing says whether players are in a match or champion select, as the
// backend's own adapter does.
type playing struct {
	matches *match.Service
	selects *selection.Service
}

func (p playing) Busy(ctx context.Context, accounts []string) (bool, error) {
	for _, id := range accounts {
		if _, in, err := p.matches.Current(ctx, id); err != nil || in {
			return in, err
		}
		if _, in, err := p.selects.Current(ctx, id); err != nil || in {
			return in, err
		}
	}
	return false, nil
}

// newMatchmakingTestServer serves parties, Match Found, champion select and
// matches together, with Casual Select one a side.
func newMatchmakingTestServer(t *testing.T) (*httptest.Server, *matchmaking.Service) {
	t.Helper()
	d := newTestDeps(t, true)
	rules := party.Rules{MaxSize: 5, Modes: map[string]party.Mode{"casual_select": {ID: "casual_select", Enabled: true, HumanPlayersPerTeam: 1, Matchmade: true}}}
	d.Party = party.NewService(party.NewMemStore(), d.Social, party.Settings{Rules: rules, InviteLifetime: time.Minute, DefaultPrivacy: party.Private}, time.Now)
	names := match.AccountsFunc(func(ctx context.Context, ids []string) (map[string]string, error) {
		found, err := d.Identity.Accounts(ctx, ids)
		out := map[string]string{}
		for id, a := range found {
			out[id] = a.DisplayName
		}
		return out, err
	})
	d.Match = match.NewService(match.NewMemStore(), names, match.NewFakeAllocator(), match.Settings{
		Modes:             map[string]match.Mode{"casual_select": {ID: "casual_select", Enabled: true, HumanPlayersPerTeam: 1}},
		Maps:              match.FakeMaps,
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
	log := slog.New(slog.NewTextHandler(io.Discard, nil))
	queued := selection.PartiesFunc(func(ctx context.Context, id string) (bool, error) {
		p, err := d.Party.Get(ctx, id)
		return err == nil && p.Status.InMatchmaking(), nil
	})
	d.Selection = selection.NewService(selection.NewMemStore(), d.Account, names, d.Match, queued, d.Social, selection.Settings{
		Casual:          selection.CasualSettings{PickDuration: time.Minute, PresenceTimeout: time.Minute},
		StartingTimeout: time.Minute,
	}, time.Now, log)
	busy := playing{matches: d.Match, selects: d.Selection}
	d.Party.SetActivity(busy)
	d.Matchmaking = matchmaking.NewService(matchmaking.NewMemStore(), d.Party, d.Social, busy, casualSeats{d.Selection}, matchmaking.Settings{
		Modes:          []matchmaking.Mode{{ID: "casual_select", TeamSize: 1}},
		AcceptDuration: time.Minute,
		SearchLimit:    1000,
	}, time.Now, log)
	d.Selection.SetMatchmaking(d.Matchmaking)
	return serve(t, d), d.Matchmaking
}

func TestQueueMatchFoundAndCasualSelectOverHTTP(t *testing.T) {
	srv, matchmaker := newMatchmakingTestServer(t)
	one, _ := gameSession(t, srv, "DevOne")
	two, _ := gameSession(t, srv, "DevTwo")
	for _, token := range []string{one, two} {
		call(t, srv, "PUT", "/v1/party/mode", token, map[string]string{"mode": "casual_select"})
		call(t, srv, "PUT", "/v1/party/ready", token, map[string]bool{"ready": true})
		if status, body := call(t, srv, "POST", "/v1/party/queue", token, nil); status != http.StatusOK {
			t.Fatalf("queue: %d %v", status, body)
		}
	}
	if _, none := call(t, srv, "GET", "/v1/me/match-found", one, nil); none["matchFound"] != nil {
		t.Fatalf("no match before the matchmaker runs: %v", none)
	}
	if err := matchmaker.MatchOnce(context.Background()); err != nil {
		t.Fatalf("MatchOnce: %v", err)
	}

	_, found := call(t, srv, "GET", "/v1/me/match-found", one, nil)
	mf, _ := found["matchFound"].(map[string]any)
	if mf == nil || mf["state"] != "pending" || mf["total"].(float64) != 2 || mf["you"] != "pending" || mf["remainingSeconds"].(float64) <= 0 {
		t.Fatalf("match found: %v", found)
	}
	if _, p := call(t, srv, "GET", "/v1/party", one, nil); p["party"].(map[string]any)["status"] != "found" || p["party"].(map[string]any)["queuedSeconds"].(float64) < 0 {
		t.Fatalf("the party: %v", p)
	}
	_, accepted := call(t, srv, "POST", "/v1/me/match-found/accept", one, nil)
	if a := accepted["matchFound"].(map[string]any); a["you"] != "accepted" || a["accepted"].(float64) != 1 {
		t.Fatalf("accepted: %v", accepted)
	}
	status, last := call(t, srv, "POST", "/v1/me/match-found/accept", two, nil)
	if l := last["matchFound"].(map[string]any); status != http.StatusOK || l["state"] != "accepted" || l["selectId"] == nil {
		t.Fatalf("the last acceptance opens champion select: %d %v", status, last)
	}

	_, sel := call(t, srv, "GET", "/v1/me/select", two, nil)
	s, _ := sel["select"].(map[string]any)
	if s == nil || s["kind"] != "casual" || s["id"] != last["matchFound"].(map[string]any)["selectId"] || len(s["seats"].([]any)) != 2 {
		t.Fatalf("the casual select: %v", sel)
	}
	call(t, srv, "PUT", "/v1/me/select/hover", one, map[string]string{"vanguardId": "bryn"})
	_, seenByTwo := call(t, srv, "GET", "/v1/me/select", two, nil)
	for _, seat := range seenByTwo["select"].(map[string]any)["seats"].([]any) {
		if seat := seat.(map[string]any); seat["you"] == false && seat["hover"] != nil {
			t.Fatalf("an enemy's hover is hidden: %v", seat)
		}
	}
	call(t, srv, "POST", "/v1/me/select/lock", one, map[string]string{"vanguardId": "bryn"})
	if status, body := call(t, srv, "POST", "/v1/me/select/lock", two, map[string]string{"vanguardId": "bryn"}); status != http.StatusConflict || body["error"] != "taken" {
		t.Fatalf("a Vanguard locked across the table: %d %v", status, body)
	}
	if status, body := call(t, srv, "POST", "/v1/me/select/leave", two, nil); status != http.StatusOK ||
		body["select"].(map[string]any)["cancelReason"] != "left" {
		t.Fatalf("leave: %d %v", status, body)
	}
	_, requeued := call(t, srv, "GET", "/v1/party", one, nil)
	_, home := call(t, srv, "GET", "/v1/party", two, nil)
	if requeued["party"].(map[string]any)["status"] != "queued" || home["party"].(map[string]any)["status"] != "idle" {
		t.Fatalf("after the dodge: %v %v", requeued, home)
	}
	if status, body := call(t, srv, "POST", "/v1/me/match-found/decline", one, nil); status != http.StatusNotFound || body["error"] != "match_found_not_found" {
		t.Fatalf("nothing to decline: %d %v", status, body)
	}
}

func TestQueueRefusesAPartyWhoseMemberIsInAMatch(t *testing.T) {
	srv, matchmaker := newMatchmakingTestServer(t)
	one, _ := gameSession(t, srv, "DevOne")
	two, _ := gameSession(t, srv, "DevTwo")
	for _, token := range []string{one, two} {
		call(t, srv, "PUT", "/v1/party/mode", token, map[string]string{"mode": "casual_select"})
		call(t, srv, "PUT", "/v1/party/ready", token, map[string]bool{"ready": true})
		call(t, srv, "POST", "/v1/party/queue", token, nil)
	}
	if err := matchmaker.MatchOnce(context.Background()); err != nil {
		t.Fatalf("MatchOnce: %v", err)
	}
	call(t, srv, "POST", "/v1/me/match-found/accept", one, nil)
	call(t, srv, "POST", "/v1/me/match-found/accept", two, nil)
	call(t, srv, "POST", "/v1/me/select/lock", one, map[string]string{"vanguardId": "bryn"})
	if _, started := call(t, srv, "POST", "/v1/me/select/lock", two, map[string]string{"vanguardId": "cairn"}); started["select"].(map[string]any)["state"] != "started" {
		t.Fatalf("the match did not start: %v", started)
	}

	// The match lets the party go, Not Ready; while it runs, it cannot queue again.
	call(t, srv, "PUT", "/v1/party/ready", one, map[string]bool{"ready": true})
	if status, body := call(t, srv, "POST", "/v1/party/queue", one, nil); status != http.StatusConflict || body["error"] != "member_busy" {
		t.Fatalf("queueing from inside a match: %d %v", status, body)
	}
}
