package httpapi

import (
	"context"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/catalog"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/progression"
)

// newProgressionTestServer serves matches, accounts and progression on
// in-memory stores, with fixture tuning independent of the committed config.
func newProgressionTestServer(t *testing.T) (*httptest.Server, *match.FakeAllocator) {
	t.Helper()
	d := newTestDeps(t, true)
	alloc := match.NewFakeAllocator()
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
	matches := match.NewService(match.NewMemStore(), names, alloc, match.Settings{
		Modes:             map[string]match.Mode{"casual_select": {ID: "casual_select", Enabled: true, HumanPlayersPerTeam: 5}},
		Maps:              match.FakeMaps,
		ReadyTimeout:      time.Minute,
		MaxDuration:       time.Hour,
		RemoveServerAfter: time.Minute,
		HostPortMin:       7780,
		HostPortMax:       7789,
		PublicHost:        "127.0.0.1",
		BackendURL:        "http://backend:8080",
		HistoryPageSize:   2,
	}, time.Now)
	vanguards := catalog.New(catalog.Settings{Released: []string{"cairn", "qazharr", "oriel", "bryn"}, Starters: []string{"cairn", "qazharr", "oriel"},
		Rotation: catalog.RotationSettings{Slots: 1, Epoch: time.Date(2026, 9, 28, 0, 0, 0, 0, time.UTC), Week: 7 * 24 * time.Hour, Seed: "test"}}, time.Now)
	accounts := account.NewService(account.NewMemStore(), vanguards, time.Now)
	progress := progression.NewService(progression.NewMemStore(), accounts, progression.Tuning{
		AccountXP:     progression.AccountXP{PerMinute: 6, WinBonus: 30, CoopBelowLevel: 10},
		AccountLevels: progression.Curve{First: 100, Growth: 10, GrowthUntil: 50},
		FluxPerLevel:  400,
		Mastery:       progression.MasteryTuning{PerMinute: 10, WinBonus: 100, PerformanceCap: 300, Levels: progression.Curve{First: 500, Growth: 100, GrowthUntil: 5}, EmoteTierLevels: []int{1, 5}},
		Prices: map[string]progression.Price{"cairn": {Flux: 1500, RefinedFlux: 300}, "qazharr": {Flux: 1500, RefinedFlux: 300},
			"oriel": {Flux: 1500, RefinedFlux: 300}, "bryn": {Flux: 3000, RefinedFlux: 550}},
		DevGrant: true,
	}, map[string]string{"casual_select": progression.CategoryCasual}, time.Now)
	matches.SetRewards(progress)
	d.Match, d.Account, d.Progression = matches, accounts, progress
	d.DevMatches, d.DevAccounts = true, testAccounts
	return serve(t, d), alloc
}

func TestAMatchResultCarriesItsRewardsOverHTTP(t *testing.T) {
	srv, alloc := newProgressionTestServer(t)
	one, oneID := gameSession(t, srv, "DevOne")
	_, twoID := gameSession(t, srv, "DevTwo")
	_, created := call(t, srv, "POST", "/v1/dev/matches", "", map[string]any{
		"mode":         "casual_select",
		"participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "cairn"}, {"accountId": twoID, "side": "B", "vanguardId": "bryn"}},
	})
	matchID := created["match"].(map[string]any)["id"].(string)
	cred := serverCredential(t, alloc, matchID)
	call(t, srv, "POST", "/v1/server/matches/"+matchID+"/ready", cred, map[string]any{})
	if _, view := call(t, srv, "GET", "/v1/me/matches/"+matchID, one, nil); view["match"].(map[string]any)["rewards"] != nil {
		t.Fatalf("rewards before the result: %v", view)
	}
	result := map[string]any{"endReason": "prime_well_destroyed", "winner": "A", "durationSeconds": 1800.0,
		"participants": []map[string]any{{"accountId": oneID, "joined": true, "connectedAtEnd": true}, {"accountId": twoID, "joined": true, "connectedAtEnd": true}}}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, result); status != http.StatusOK {
		t.Fatalf("result: %d %v", status, body)
	}
	_, view := call(t, srv, "GET", "/v1/me/matches/"+matchID, one, nil)
	rewards, _ := view["match"].(map[string]any)["rewards"].(map[string]any)
	if rewards == nil || rewards["reason"] != nil || rewards["accountXp"] != float64(210) || rewards["levelBefore"] != float64(1) || rewards["levelAfter"] != float64(3) ||
		rewards["flux"] != float64(800) || rewards["vanguardId"] != "cairn" || rewards["masteryPoints"] != float64(400) {
		t.Fatalf("the winner's rewards: %v", view)
	}
	status, body := call(t, srv, "GET", "/v1/me/progression", one, nil)
	p, _ := body["progression"].(map[string]any)
	if status != http.StatusOK || p["level"] != float64(3) || p["levelXp"] != float64(0) || p["levelNeed"] != float64(120) || p["lifetimeXp"] != float64(210) ||
		p["flux"] != float64(800) || p["refinedFlux"] != float64(0) {
		t.Fatalf("progression: %d %v", status, body)
	}
	if status, _ := call(t, srv, "GET", "/v1/me/progression", "", nil); status != http.StatusUnauthorized {
		t.Fatalf("without a session: %d", status)
	}
}

func TestTheCollectionAndPurchasesOverHTTP(t *testing.T) {
	srv, _ := newProgressionTestServer(t)
	one, _ := gameSession(t, srv, "DevOne")
	if status, body := call(t, srv, "POST", "/v1/me/starter", one, map[string]string{"vanguardId": "cairn"}); status != http.StatusOK {
		t.Fatalf("starter: %d %v", status, body)
	}
	status, body := call(t, srv, "GET", "/v1/me/collection", one, nil)
	list, _ := body["vanguards"].([]any)
	if status != http.StatusOK || len(list) != 4 {
		t.Fatalf("collection: %d %v", status, body)
	}
	cairn, bryn := list[0].(map[string]any), list[3].(map[string]any)
	if cairn["vanguardId"] != "cairn" || cairn["owned"] != true || cairn["source"] != "starter" || cairn["purchasable"] != false {
		t.Fatalf("the starter: %v", cairn)
	}
	mastery, _ := bryn["mastery"].(map[string]any)
	if bryn["owned"] != false || bryn["source"] != nil || bryn["purchasable"] != true || bryn["price"].(map[string]any)["flux"] != float64(3000) ||
		mastery["level"] != float64(1) || mastery["levelNeed"] != float64(500) || mastery["emoteTier"] != float64(1) {
		t.Fatalf("an unowned Vanguard: %v", bryn)
	}

	buy := map[string]string{"purchaseId": "11111111-1111-1111-1111-111111111111", "vanguardId": "bryn", "currency": "flux"}
	if status, body := call(t, srv, "POST", "/v1/me/purchases", one, buy); status != http.StatusConflict || body["error"] != "insufficient_balance" {
		t.Fatalf("with nothing to spend: %d %v", status, body)
	}
	if status, body := call(t, srv, "POST", "/v1/dev/accounts/DevOne/progression-grant", "", map[string]int64{"flux": 4000}); status != http.StatusOK {
		t.Fatalf("dev grant: %d %v", status, body)
	}
	for range 2 {
		status, body := call(t, srv, "POST", "/v1/me/purchases", one, buy)
		purchase, _ := body["purchase"].(map[string]any)
		after, _ := body["progression"].(map[string]any)
		if status != http.StatusOK || purchase["vanguardId"] != "bryn" || purchase["price"] != float64(3000) || after["flux"] != float64(1000) {
			t.Fatalf("purchase: %d %v", status, body)
		}
	}
	for _, c := range []struct {
		body   map[string]string
		status int
		code   string
	}{
		{map[string]string{"purchaseId": "22222222-2222-2222-2222-222222222222", "vanguardId": "bryn", "currency": "flux"}, http.StatusConflict, "already_owned"},
		{map[string]string{"purchaseId": "11111111-1111-1111-1111-111111111111", "vanguardId": "oriel", "currency": "flux"}, http.StatusConflict, "purchase_conflict"},
		{map[string]string{"purchaseId": "33333333-3333-3333-3333-333333333333", "vanguardId": "nobody", "currency": "flux"}, http.StatusBadRequest, "not_for_sale"},
		{map[string]string{"purchaseId": "44444444-4444-4444-4444-444444444444", "vanguardId": "oriel", "currency": "gold"}, http.StatusBadRequest, "invalid_purchase"},
	} {
		if status, body := call(t, srv, "POST", "/v1/me/purchases", one, c.body); status != c.status || body["error"] != c.code {
			t.Errorf("%v: %d %v", c.body, status, body)
		}
	}
	if status, body := call(t, srv, "POST", "/v1/dev/accounts/Nobody/progression-grant", "", map[string]int64{"flux": 1}); status != http.StatusNotFound {
		t.Fatalf("an unknown development account: %d %v", status, body)
	}

	// The development reset takes the bought Vanguard back, so a scripted run can buy it again.
	if status, body := call(t, srv, "POST", "/v1/dev/accounts/DevOne/progression-reset", "", nil); status != http.StatusNoContent {
		t.Fatalf("dev reset: %d %v", status, body)
	}
	_, body = call(t, srv, "GET", "/v1/me/collection", one, nil)
	if bryn := body["vanguards"].([]any)[3].(map[string]any); bryn["owned"] != false || bryn["purchasable"] != true {
		t.Fatalf("bryn after the reset: %v", bryn)
	}
	call(t, srv, "POST", "/v1/dev/accounts/DevOne/progression-grant", "", map[string]int64{"flux": 2000})
	if status, body := call(t, srv, "POST", "/v1/me/purchases", one, buy); status != http.StatusOK || body["progression"].(map[string]any)["flux"] != float64(0) {
		t.Fatalf("buying again after the reset: %d %v", status, body)
	}
}
