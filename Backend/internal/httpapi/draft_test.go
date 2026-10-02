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

// draftTestTurns is a short draft: a ban a side, two picks for A, one for B.
var draftTestTurns = []selection.Turn{{Ban: true, Side: match.SideA, Count: 1}, {Ban: true, Side: match.SideB, Count: 1},
	{Side: match.SideA, Count: 2}, {Side: match.SideB, Count: 1}}

// newDraftTestServer serves champion select with Draft Pick settings, and
// returns its select service so a test can open a draft as matchmaking does.
// Every released Vanguard is in the rotation.
func newDraftTestServer(t *testing.T) (*httptest.Server, *selection.Service) {
	t.Helper()
	d := newTestDeps(t, true)
	names := match.AccountsFunc(func(ctx context.Context, ids []string) (map[string]string, error) {
		found, err := d.Identity.Accounts(ctx, ids)
		out := map[string]string{}
		for id, a := range found {
			out[id] = a.DisplayName
		}
		return out, err
	})
	d.Match = match.NewService(match.NewMemStore(), names, match.NewFakeAllocator(), match.Settings{
		Modes:             map[string]match.Mode{"draft_pick": {ID: "draft_pick", Enabled: true, HumanPlayersPerTeam: 2}},
		Maps:              match.FakeMaps,
		ReadyTimeout:      time.Minute,
		MaxDuration:       time.Hour,
		RemoveServerAfter: time.Minute,
		HostPortMin:       7780,
		HostPortMax:       7789,
		PublicHost:        "127.0.0.1",
		BackendURL:        "http://backend:8080",
	}, time.Now)
	c := catalog.New(catalog.Settings{Released: []string{"cairn", "qazharr", "oriel", "bryn", "silt"}, Starters: []string{"cairn", "qazharr", "oriel"},
		Rotation: catalog.RotationSettings{Slots: 5, Epoch: time.Date(2026, 9, 28, 0, 0, 0, 0, time.UTC), Week: 7 * 24 * time.Hour, Seed: "test"}}, time.Now)
	d.Account = account.NewService(account.NewMemStore(), c, time.Now)
	notQueued := selection.PartiesFunc(func(context.Context, string) (bool, error) { return false, nil })
	d.Selection = selection.NewService(selection.NewMemStore(), d.Account, names, d.Match, notQueued, d.Social, selection.Settings{
		Draft: selection.DraftSettings{
			Timing:          selection.Timing{Turns: draftTestTurns, Ban: 20 * time.Second, Pick: 30 * time.Second, Final: 40 * time.Second},
			PresenceTimeout: time.Minute,
		},
		StartingTimeout: time.Minute,
	}, time.Now, slog.New(slog.NewTextHandler(io.Discard, nil)))
	return serve(t, d), d.Selection
}

// draftSeatsOf returns the select's seats as a player sees them.
func draftSeatsOf(t *testing.T, srv *httptest.Server, token string) []map[string]any {
	t.Helper()
	_, body := call(t, srv, "GET", "/v1/me/select", token, nil)
	sel, _ := body["select"].(map[string]any)
	if sel == nil {
		t.Fatalf("no select: %v", body)
	}
	var out []map[string]any
	for _, seat := range sel["seats"].([]any) {
		out = append(out, seat.(map[string]any))
	}
	return out
}

func TestDraftPickOverHTTP(t *testing.T) {
	srv, selects := newDraftTestServer(t)
	one, oneID := gameSession(t, srv, "DevOne")
	three, threeID := gameSession(t, srv, "DevThree")
	two, twoID := gameSession(t, srv, "DevTwo")
	// Seats 0 and 1 on side A, seat 2 on side B.
	if _, err := selects.OpenDraft(context.Background(), "draft_pick", []selection.CasualSeat{
		{AccountID: oneID, Side: match.SideA}, {AccountID: threeID, Side: match.SideA}, {AccountID: twoID, Side: match.SideB}}); err != nil {
		t.Fatalf("OpenDraft: %v", err)
	}

	_, opened := call(t, srv, "GET", "/v1/me/select", one, nil)
	sel := opened["select"].(map[string]any)
	turn, _ := sel["turn"].(map[string]any)
	if sel["kind"] != "draft" || sel["phase"] != "banning" || sel["pickSeconds"].(float64) != 20 || turn == nil || turn["ban"] != true ||
		turn["side"] != "A" || turn["count"].(float64) != 1 || turn["done"].(float64) != 0 || len(sel["bans"].([]any)) != 0 {
		t.Fatalf("the draft opens on side A's ban: %v", sel)
	}
	if seats := draftSeatsOf(t, srv, two); seats[0]["acting"] != true || seats[1]["acting"] != false || seats[2]["acting"] != false {
		t.Fatalf("everyone sees who acts: %v", seats)
	}

	// A ban hover is the team's; a locked ban is everyone's.
	if status, body := call(t, srv, "PUT", "/v1/me/select/ban/hover", one, map[string]string{"vanguardId": "bryn"}); status != http.StatusOK {
		t.Fatalf("ban hover: %d %v", status, body)
	}
	if seats := draftSeatsOf(t, srv, three); seats[0]["banHover"] != "bryn" {
		t.Fatalf("a teammate sees the ban hover: %v", seats)
	}
	if seats := draftSeatsOf(t, srv, two); seats[0]["banHover"] != nil {
		t.Fatalf("an enemy does not: %v", seats)
	}
	if status, body := call(t, srv, "POST", "/v1/me/select/ban", two, map[string]string{"vanguardId": "cairn"}); status != http.StatusConflict ||
		body["error"] != "not_your_turn" {
		t.Fatalf("a ban out of turn: %d %v", status, body)
	}
	call(t, srv, "POST", "/v1/me/select/ban", one, map[string]string{"vanguardId": "bryn"})
	_, seen := call(t, srv, "GET", "/v1/me/select", two, nil)
	bans := seen["select"].(map[string]any)["bans"].([]any)
	if len(bans) != 1 || bans[0].(map[string]any)["side"] != "A" || bans[0].(map[string]any)["vanguardId"] != "bryn" {
		t.Fatalf("the enemy sees the ban: %v", seen)
	}
	_, picking := call(t, srv, "POST", "/v1/me/select/ban", two, map[string]string{"vanguardId": "cairn"})
	if p := picking["select"].(map[string]any); p["phase"] != "picking" || p["pickSeconds"].(float64) != 30 || p["turn"].(map[string]any)["count"].(float64) != 2 {
		t.Fatalf("side A's picks: %v", picking)
	}

	// Locked teammates trade; an offer is seen by its two players only.
	call(t, srv, "POST", "/v1/me/select/lock", one, map[string]string{"vanguardId": "oriel"})
	call(t, srv, "POST", "/v1/me/select/lock", three, map[string]string{"vanguardId": "qazharr"})
	if status, body := call(t, srv, "POST", "/v1/me/select/trade", one, map[string]int{"seat": 2}); status != http.StatusConflict || body["error"] != "cannot_trade" {
		t.Fatalf("a trade with an enemy: %d %v", status, body)
	}
	if status, body := call(t, srv, "POST", "/v1/me/select/trade", one, map[string]any{}); status != http.StatusConflict || body["error"] != "cannot_trade" {
		t.Fatalf("a trade with nobody: %d %v", status, body)
	}
	call(t, srv, "POST", "/v1/me/select/trade", one, map[string]int{"seat": 1})
	if seats := draftSeatsOf(t, srv, one); seats[1]["offeredByYou"] != true || seats[1]["offersYou"] != false {
		t.Fatalf("the offer, as its maker sees it: %v", seats)
	}
	if seats := draftSeatsOf(t, srv, three); seats[0]["offersYou"] != true {
		t.Fatalf("the offer, as its teammate sees it: %v", seats)
	}
	if seats := draftSeatsOf(t, srv, two); seats[0]["offersYou"] != false || seats[0]["offeredByYou"] != false || seats[1]["offersYou"] != false {
		t.Fatalf("the enemy sees no offer: %v", seats)
	}
	if status, body := call(t, srv, "POST", "/v1/me/select/trade/accept", three, map[string]int{"seat": 0}); status != http.StatusOK {
		t.Fatalf("accept: %d %v", status, body)
	}
	if seats := draftSeatsOf(t, srv, one); seats[0]["locked"] != "qazharr" || seats[1]["locked"] != "oriel" || seats[1]["offeredByYou"] != false {
		t.Fatalf("the trade: %v", seats)
	}
	if status, body := call(t, srv, "POST", "/v1/me/select/trade/decline", three, map[string]int{"seat": 0}); status != http.StatusConflict ||
		body["error"] != "cannot_trade" {
		t.Fatalf("nothing to decline: %d %v", status, body)
	}

	// The last pick opens the final window.
	if status, body := call(t, srv, "POST", "/v1/me/select/lock", two, map[string]string{"vanguardId": "cairn"}); status != http.StatusConflict || body["error"] != "taken" {
		t.Fatalf("a banned Vanguard: %d %v", status, body)
	}
	_, final := call(t, srv, "POST", "/v1/me/select/lock", two, map[string]string{"vanguardId": "silt"})
	if f := final["select"].(map[string]any); f["phase"] != "final" || f["turn"] != nil || f["pickSeconds"].(float64) != 40 || f["state"] != "picking" {
		t.Fatalf("the final window: %v", final)
	}
}
