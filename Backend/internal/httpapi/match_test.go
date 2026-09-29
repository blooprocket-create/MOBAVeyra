package httpapi

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

func newMatchTestServer(t *testing.T, devMatches bool) (*httptest.Server, *match.FakeAllocator) {
	t.Helper()
	return newMatchTestServerWithLimit(t, devMatches, testBodyLimit)
}

// scoreboardBodyLimit is the production request limit (config/local.json),
// which a result with a scoreboard needs.
const scoreboardBodyLimit = 64 << 10

func newMatchTestServerWithLimit(t *testing.T, devMatches bool, bodyLimit int64) (*httptest.Server, *match.FakeAllocator) {
	t.Helper()
	d := newTestDeps(t, true)
	d.BodyLimitBytes = bodyLimit
	alloc := match.NewFakeAllocator()
	accounts := match.AccountsFunc(func(ctx context.Context, ids []string) (map[string]string, error) {
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
	d.Match = match.NewService(match.NewMemStore(), accounts, alloc, match.Settings{
		Modes:             map[string]match.Mode{"casual_select": {ID: "casual_select", Enabled: true, HumanPlayersPerTeam: 5}},
		Maps:              match.FakeMaps,
		Practice:          match.PracticeSettings{Enabled: true, Mode: "custom_practice", HostSide: match.SideA},
		ReadyTimeout:      time.Minute,
		MaxDuration:       time.Hour,
		RemoveServerAfter: time.Minute,
		HostPortMin:       7780,
		HostPortMax:       7789,
		PublicHost:        "127.0.0.1",
		BackendURL:        "http://backend:8080",
		HistoryPageSize:   2,
	}, time.Now)
	d.DevMatches = devMatches
	return serve(t, d), alloc
}

func serverCredential(t *testing.T, alloc *match.FakeAllocator, matchID string) string {
	t.Helper()
	spec, ok := alloc.Spec(matchID)
	if !ok {
		t.Fatalf("no server started for %s", matchID)
	}
	var a match.Assignment
	if err := json.Unmarshal(spec.Assignment, &a); err != nil {
		t.Fatal(err)
	}
	return a.ServerCredential
}

func TestMatchHandoffOverHTTP(t *testing.T) {
	srv, alloc := newMatchTestServer(t, true)
	one, oneID := gameSession(t, srv, "DevOne")
	_, twoID := gameSession(t, srv, "DevTwo")

	status, body := call(t, srv, "GET", "/v1/me/match", one, nil)
	if status != http.StatusOK || body["match"] != nil {
		t.Fatalf("no match yet: %d %v", status, body)
	}

	status, created := call(t, srv, "POST", "/v1/dev/matches", "", map[string]any{
		"mode":         "casual_select",
		"participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "cairn"}, {"accountId": twoID, "side": "B", "vanguardId": "cairn"}},
	})
	if status != http.StatusCreated {
		t.Fatalf("create: %d %v", status, created)
	}
	m := created["match"].(map[string]any)
	matchID := m["id"].(string)
	if m["state"] != "allocating" || m["hostPort"] != float64(7780) {
		t.Fatalf("created match: %v", m)
	}
	if spec, _ := alloc.Spec(matchID); spec.Map != match.FakeMaps[match.MapDevelopment] {
		t.Fatalf("a development match loads %q; want the development map", spec.Map)
	}

	_, mine := call(t, srv, "GET", "/v1/me/match", one, nil)
	allocating := mine["match"].(map[string]any)
	if allocating["state"] != "allocating" || allocating["ticket"] != nil || allocating["server"] != nil {
		t.Fatalf("before ready the ticket and server stay hidden: %v", allocating)
	}

	cred := serverCredential(t, alloc, matchID)
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/ready", "", map[string]any{}); status != http.StatusUnauthorized {
		t.Fatalf("ready without a credential: %d %v", status, body)
	}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/ready", cred, map[string]any{}); status != http.StatusOK {
		t.Fatalf("ready: %d %v", status, body)
	}

	_, mine = call(t, srv, "GET", "/v1/me/match", one, nil)
	ready := mine["match"].(map[string]any)
	server, _ := ready["server"].(map[string]any)
	ticket, _ := ready["ticket"].(string)
	if ready["state"] != "ready" || ready["side"] != "A" || server["host"] != "127.0.0.1" || server["port"] != float64(7780) || !strings.HasPrefix(ticket, "vjt_") {
		t.Fatalf("after ready: %v", ready)
	}

	result := map[string]any{
		"endReason":       "developer_request",
		"winner":          nil,
		"durationSeconds": 61.5,
		"participants": []map[string]any{
			{"accountId": oneID, "joined": true, "connectedAtEnd": true},
			{"accountId": twoID, "joined": true, "connectedAtEnd": false},
		},
	}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, result); status != http.StatusOK {
		t.Fatalf("result: %d %v", status, body)
	}
	if status, _ := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, result); status != http.StatusOK {
		t.Fatalf("a replayed result must succeed: %d", status)
	}
	result["durationSeconds"] = 62.0
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, result); status != http.StatusConflict || body["error"] != "result_conflict" {
		t.Fatalf("a different result: %d %v", status, body)
	}

	status, got := call(t, srv, "GET", "/v1/dev/matches/"+matchID, "", nil)
	ended := got["match"].(map[string]any)
	recorded, _ := ended["result"].(map[string]any)
	if status != http.StatusOK || ended["state"] != "ended" || recorded["endReason"] != "developer_request" || recorded["winner"] != nil {
		t.Fatalf("dev view of the ended match: %d %v", status, got)
	}

	_, mine = call(t, srv, "GET", "/v1/me/match", one, nil)
	if mine["match"] != nil {
		t.Fatalf("an ended match is no longer the player's: %v", mine)
	}
}

func TestAPracticeMatchAndItsResultOverHTTP(t *testing.T) {
	srv, alloc := newMatchTestServer(t, true)
	one, oneID := gameSession(t, srv, "DevOne")
	two, _ := gameSession(t, srv, "DevTwo")

	status, created := call(t, srv, "POST", "/v1/dev/matches", "", map[string]any{
		"mode": "custom_practice", "rules": "practice", "hostAccountId": oneID,
		"participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "oriel"}},
	})
	if status != http.StatusCreated {
		t.Fatalf("create practice: %d %v", status, created)
	}
	m := created["match"].(map[string]any)
	matchID := m["id"].(string)
	if m["rules"] != "practice" || m["hostAccountId"] != oneID || m["participants"].([]any)[0].(map[string]any)["vanguardId"] != "oriel" {
		t.Fatalf("dev view of a practice match: %v", m)
	}

	_, mine := call(t, srv, "GET", "/v1/me/match", one, nil)
	current := mine["match"].(map[string]any)
	if current["mode"] != "custom_practice" || current["rules"] != "practice" || current["vanguardId"] != "oriel" {
		t.Fatalf("the player's match: %v", current)
	}

	status, view := call(t, srv, "GET", "/v1/me/matches/"+matchID, one, nil)
	running := view["match"].(map[string]any)
	if status != http.StatusOK || running["state"] != "allocating" || running["result"] != nil || running["vanguardId"] != "oriel" {
		t.Fatalf("the result route before the end: %d %v", status, view)
	}
	if status, body := call(t, srv, "GET", "/v1/me/matches/"+matchID, two, nil); status != http.StatusNotFound || body["error"] != "match_not_found" {
		t.Fatalf("another player's match: %d %v", status, body)
	}
	if status, _ := call(t, srv, "GET", "/v1/me/matches/"+matchID, "", nil); status != http.StatusUnauthorized {
		t.Fatalf("without a session: want 401, got %d", status)
	}

	cred := serverCredential(t, alloc, matchID)
	call(t, srv, "POST", "/v1/server/matches/"+matchID+"/ready", cred, map[string]any{})
	result := map[string]any{"endReason": "host_ended", "winner": nil, "durationSeconds": 95.0,
		"participants": []map[string]any{{"accountId": oneID, "joined": true, "connectedAtEnd": true}}}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, result); status != http.StatusOK {
		t.Fatalf("host_ended result: %d %v", status, body)
	}
	_, view = call(t, srv, "GET", "/v1/me/matches/"+matchID, one, nil)
	ended := view["match"].(map[string]any)
	verified, _ := ended["result"].(map[string]any)
	if ended["state"] != "ended" || verified["endReason"] != "host_ended" || verified["winner"] != nil || verified["joined"] != true || verified["connectedAtEnd"] != true {
		t.Fatalf("the verified result: %v", ended)
	}
}

func TestDevMatchResponsesCarryNoSecrets(t *testing.T) {
	srv, alloc := newMatchTestServer(t, true)
	_, oneID := gameSession(t, srv, "DevOne")
	_, created := call(t, srv, "POST", "/v1/dev/matches", "", map[string]any{
		"mode": "casual_select", "participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "cairn"}},
	})
	matchID := created["match"].(map[string]any)["id"].(string)
	cred := serverCredential(t, alloc, matchID)
	call(t, srv, "POST", "/v1/server/matches/"+matchID+"/ready", cred, map[string]any{})
	_, got := call(t, srv, "GET", "/v1/dev/matches/"+matchID, "", nil)
	for _, body := range []map[string]any{created, got} {
		raw, _ := json.Marshal(body)
		if strings.Contains(string(raw), "vms_") || strings.Contains(string(raw), "vjt_") || strings.Contains(string(raw), cred) {
			t.Fatalf("a dev response leaked a secret: %s", raw)
		}
	}
}

func TestDevMatchRoutesAbsentWhenDisabled(t *testing.T) {
	srv, _ := newMatchTestServer(t, false)
	if status, _ := call(t, srv, "POST", "/v1/dev/matches", "", map[string]any{"mode": "casual_select"}); status != http.StatusNotFound {
		t.Fatalf("create: want 404, got %d", status)
	}
	if status, _ := call(t, srv, "GET", "/v1/dev/matches/00000000-0000-4000-8000-000000000000", "", nil); status != http.StatusNotFound {
		t.Fatalf("get: want 404, got %d", status)
	}
}

func TestMatchRoutesRejectBadRequests(t *testing.T) {
	srv, alloc := newMatchTestServer(t, true)
	_, oneID := gameSession(t, srv, "DevOne")

	if status, _ := call(t, srv, "GET", "/v1/me/match", "", nil); status != http.StatusUnauthorized {
		t.Fatalf("my match without a session: want 401, got %d", status)
	}
	cases := []struct {
		name   string
		body   map[string]any
		status int
		code   string
	}{
		{"unknown mode", map[string]any{"mode": "nope", "participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "cairn"}}}, http.StatusBadRequest, "unknown_mode"},
		{"bad side", map[string]any{"mode": "casual_select", "participants": []map[string]string{{"accountId": oneID, "side": "C", "vanguardId": "cairn"}}}, http.StatusBadRequest, "invalid_roster"},
		{"unknown account", map[string]any{"mode": "casual_select", "participants": []map[string]string{{"accountId": "ghost", "side": "A", "vanguardId": "cairn"}}}, http.StatusNotFound, "account_not_found"},
		{"unknown field", map[string]any{"mode": "casual_select", "extra": true}, http.StatusBadRequest, "malformed_request"},
		{"no Vanguard", map[string]any{"mode": "casual_select", "participants": []map[string]string{{"accountId": oneID, "side": "A"}}}, http.StatusBadRequest, "invalid_vanguard"},
		{"unknown rules", map[string]any{"mode": "casual_select", "rules": "draft", "participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "cairn"}}}, http.StatusBadRequest, "invalid_rules"},
		{"practice without host", map[string]any{"mode": "custom_practice", "rules": "practice", "participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "cairn"}}}, http.StatusBadRequest, "invalid_roster"},
		{"unknown map", map[string]any{"mode": "casual_select", "map": "arena", "participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "cairn"}}}, http.StatusBadRequest, "invalid_map"},
	}
	for _, tc := range cases {
		if status, body := call(t, srv, "POST", "/v1/dev/matches", "", tc.body); status != tc.status || body["error"] != tc.code {
			t.Fatalf("%s: %d %v", tc.name, status, body)
		}
	}

	_, created := call(t, srv, "POST", "/v1/dev/matches", "", map[string]any{
		"mode": "casual_select", "participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "cairn"}},
	})
	matchID := created["match"].(map[string]any)["id"].(string)
	cred := serverCredential(t, alloc, matchID)
	if status, body := call(t, srv, "POST", "/v1/dev/matches", "", map[string]any{
		"mode": "casual_select", "participants": []map[string]string{{"accountId": oneID, "side": "B", "vanguardId": "cairn"}},
	}); status != http.StatusConflict || body["error"] != "already_in_match" {
		t.Fatalf("a second match for one account: %d %v", status, body)
	}

	early := map[string]any{"endReason": "abandoned", "winner": nil, "durationSeconds": 0,
		"participants": []map[string]any{{"accountId": oneID, "joined": false, "connectedAtEnd": false}}}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, early); status != http.StatusConflict || body["error"] != "invalid_state" {
		t.Fatalf("a result before ready: %d %v", status, body)
	}
	call(t, srv, "POST", "/v1/server/matches/"+matchID+"/ready", cred, map[string]any{})
	emptyWinner := map[string]any{"endReason": "abandoned", "winner": "", "durationSeconds": 0,
		"participants": []map[string]any{{"accountId": oneID, "joined": false, "connectedAtEnd": false}}}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, emptyWinner); status != http.StatusBadRequest || body["error"] != "invalid_result" {
		t.Fatalf("an empty winner: %d %v", status, body)
	}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", "vms_wrong", early); status != http.StatusUnauthorized {
		t.Fatalf("a wrong credential: %d %v", status, body)
	}
}

// scoreboardLine is one line of a reported scoreboard, as a match server sends it.
func scoreboardLine(side, name string, accountID any, vanguardID string) map[string]any {
	return map[string]any{
		"side": side, "name": name, "accountId": accountID, "vanguardId": vanguardID,
		"statistics": map[string]any{
			"kills": 3, "deaths": 1, "assists": 2, "level": 11, "vanguardDamage": 5120.5,
			"damageDealt":    map[string]any{"physical": 14000.25, "magic": 300, "true": 45},
			"damageTaken":    map[string]any{"physical": 6000, "magic": 2100, "true": 0},
			"damageShielded": 150, "selfHealing": 420, "teammateHealing": 0,
			"crowdControl": map[string]any{"stun": 2.5, "slow": 4},
			"goldEarned":   7650,
			"goldBySource": map[string]any{"starting": 500, "kills": 900, "assists": 250, "minions": 3900, "jungle": 0, "objectives": 400, "wards": 30, "passive": 1670},
			"minionKills":  160, "jungleKills": 0, "towerDamage": 2400, "wellsSecured": 1, "wellDamage": 800, "wellFinalHits": 0, "wardsPlaced": 4, "wardsDestroyed": 1,
		},
		"items":      []string{"timing_coil", "basic_boots", "", "", "", ""},
		"fluxSpells": []string{"blink", "mend"},
	}
}

func TestAResultsScoreboardOverHTTP(t *testing.T) {
	srv, alloc := newMatchTestServerWithLimit(t, true, scoreboardBodyLimit)
	one, oneID := gameSession(t, srv, "DevOne")
	two, twoID := gameSession(t, srv, "DevTwo")
	_, created := call(t, srv, "POST", "/v1/dev/matches", "", map[string]any{
		"mode":         "casual_select",
		"participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "cairn"}, {"accountId": twoID, "side": "B", "vanguardId": "oriel"}},
	})
	matchID := created["match"].(map[string]any)["id"].(string)
	cred := serverCredential(t, alloc, matchID)
	call(t, srv, "POST", "/v1/server/matches/"+matchID+"/ready", cred, map[string]any{})

	result := map[string]any{
		"endReason": "developer_request", "winner": nil, "durationSeconds": 1510.5,
		"participants": []map[string]any{
			{"accountId": oneID, "joined": true, "connectedAtEnd": true},
			{"accountId": twoID, "joined": true, "connectedAtEnd": true},
		},
	}
	// A line the domain cannot hold is refused before anything is recorded.
	oneSpell := scoreboardLine("A", "DevOne", oneID, "cairn")
	oneSpell["fluxSpells"] = []string{"blink"}
	result["players"] = []map[string]any{oneSpell}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, result); status != http.StatusBadRequest || body["error"] != "invalid_result" {
		t.Fatalf("one spell slot: %d %v", status, body)
	}
	stranger := scoreboardLine("A", "DevOne", "00000000-0000-4000-8000-000000000000", "cairn")
	result["players"] = []map[string]any{stranger}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, result); status != http.StatusBadRequest || body["error"] != "invalid_result" {
		t.Fatalf("a stranger's line: %d %v", status, body)
	}
	extra := scoreboardLine("A", "DevOne", oneID, "cairn")
	extra["statistics"].(map[string]any)["visionScore"] = 12
	result["players"] = []map[string]any{extra}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, result); status != http.StatusBadRequest {
		t.Fatalf("an unknown statistic: %d %v", status, body)
	}

	result["players"] = []map[string]any{scoreboardLine("A", "DevOne", oneID, "cairn"), scoreboardLine("B", "DevTwo", twoID, "oriel")}
	result["wells"] = []map[string]any{{"site": 0, "side": "B", "atSeconds": 700.5}}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, result); status != http.StatusOK {
		t.Fatalf("result: %d %v", status, body)
	}

	for _, viewer := range []struct {
		token string
		you   int
	}{{one, 0}, {two, 1}} {
		status, view := call(t, srv, "GET", "/v1/me/matches/"+matchID, viewer.token, nil)
		verified, _ := view["match"].(map[string]any)["result"].(map[string]any)
		players, _ := verified["players"].([]any)
		if status != http.StatusOK || len(players) != 2 {
			t.Fatalf("the verified scoreboard: %d %v", status, view)
		}
		for i, line := range players {
			p := line.(map[string]any)
			if p["you"] != (i == viewer.you) {
				t.Fatalf("line %d for viewer %d: %v", i, viewer.you, p)
			}
			if _, ok := p["accountId"]; ok {
				t.Fatalf("a participant sees no account IDs: %v", p)
			}
		}
		first := players[0].(map[string]any)
		stats := first["statistics"].(map[string]any)
		if first["name"] != "DevOne" || first["vanguardId"] != "cairn" || stats["kills"] != float64(3) || stats["goldBySource"].(map[string]any)["passive"] != float64(1670) ||
			first["items"].([]any)[1] != "basic_boots" || first["fluxSpells"].([]any)[1] != "mend" {
			t.Fatalf("the scoreboard as recorded: %v", first)
		}
		wells, _ := verified["wells"].([]any)
		if len(wells) != 1 || wells[0].(map[string]any)["side"] != "B" || wells[0].(map[string]any)["atSeconds"] != 700.5 {
			t.Fatalf("the Flux Wells secured: %v", verified["wells"])
		}
	}
}

func TestAResultWithoutAScoreboardShowsNone(t *testing.T) {
	srv, alloc := newMatchTestServer(t, true)
	one, oneID := gameSession(t, srv, "DevOne")
	_, created := call(t, srv, "POST", "/v1/dev/matches", "", map[string]any{
		"mode": "casual_select", "participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "cairn"}},
	})
	matchID := created["match"].(map[string]any)["id"].(string)
	cred := serverCredential(t, alloc, matchID)
	call(t, srv, "POST", "/v1/server/matches/"+matchID+"/ready", cred, map[string]any{})
	result := map[string]any{"endReason": "abandoned", "winner": nil, "durationSeconds": 0.0,
		"participants": []map[string]any{{"accountId": oneID, "joined": false, "connectedAtEnd": false}}}
	if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, result); status != http.StatusOK {
		t.Fatalf("result: %d %v", status, body)
	}
	_, view := call(t, srv, "GET", "/v1/me/matches/"+matchID, one, nil)
	verified := view["match"].(map[string]any)["result"].(map[string]any)
	if players, ok := verified["players"]; !ok || players != nil {
		t.Fatalf("no scoreboard is null: %v", verified)
	}
	if wells, ok := verified["wells"]; !ok || wells != nil {
		t.Fatalf("no captures sent is null: %v", verified)
	}
}

func TestMatchHistoryOverHTTP(t *testing.T) {
	srv, alloc := newMatchTestServer(t, true)
	one, oneID := gameSession(t, srv, "DevOne")
	_, twoID := gameSession(t, srv, "DevTwo")
	play := func(winner any) string {
		t.Helper()
		_, created := call(t, srv, "POST", "/v1/dev/matches", "", map[string]any{
			"mode":         "casual_select",
			"participants": []map[string]string{{"accountId": oneID, "side": "A", "vanguardId": "cairn"}, {"accountId": twoID, "side": "B", "vanguardId": "oriel"}},
		})
		matchID := created["match"].(map[string]any)["id"].(string)
		cred := serverCredential(t, alloc, matchID)
		call(t, srv, "POST", "/v1/server/matches/"+matchID+"/ready", cred, map[string]any{})
		reason := "developer_request"
		if winner != nil {
			reason = "prime_well_destroyed"
		}
		result := map[string]any{"endReason": reason, "winner": winner, "durationSeconds": 600.5,
			"participants": []map[string]any{{"accountId": oneID, "joined": true, "connectedAtEnd": true}, {"accountId": twoID, "joined": true, "connectedAtEnd": true}}}
		if status, body := call(t, srv, "POST", "/v1/server/matches/"+matchID+"/result", cred, result); status != http.StatusOK {
			t.Fatalf("result: %d %v", status, body)
		}
		return matchID
	}
	first := play(nil)
	time.Sleep(2 * time.Millisecond)
	second := play("B")
	time.Sleep(2 * time.Millisecond)
	third := play("A")

	status, body := call(t, srv, "GET", "/v1/me/matches", one, nil)
	matches, _ := body["matches"].([]any)
	next, _ := body["next"].(string)
	if status != http.StatusOK || len(matches) != 2 || next == "" {
		t.Fatalf("the first page: %d %v", status, body)
	}
	newest := matches[0].(map[string]any)
	if newest["id"] != third || newest["outcome"] != "win" || newest["mode"] != "casual_select" || newest["vanguardId"] != "cairn" || newest["side"] != "A" ||
		newest["durationSeconds"] != 600.5 || newest["endedAt"] == nil || matches[1].(map[string]any)["outcome"] != "loss" {
		t.Fatalf("newest first, with the player's own outcome: %v", matches)
	}
	status, body = call(t, srv, "GET", "/v1/me/matches?cursor="+next, one, nil)
	matches, _ = body["matches"].([]any)
	if status != http.StatusOK || len(matches) != 1 || matches[0].(map[string]any)["id"] != first || matches[0].(map[string]any)["outcome"] != "no_contest" || body["next"] != nil {
		t.Fatalf("Load More: %d %v", status, body)
	}
	status, body = call(t, srv, "GET", "/v1/me/matches?outcome=loss&vanguard=cairn&mode=casual_select", one, nil)
	matches, _ = body["matches"].([]any)
	if status != http.StatusOK || len(matches) != 1 || matches[0].(map[string]any)["id"] != second {
		t.Fatalf("filtered: %d %v", status, body)
	}
	for query, code := range map[string]string{"?outcome=draw": "invalid_filter", "?vanguard=Cairn": "invalid_filter", "?cursor=nope": "invalid_cursor"} {
		if status, body := call(t, srv, "GET", "/v1/me/matches"+query, one, nil); status != http.StatusBadRequest || body["error"] != code {
			t.Fatalf("%s: %d %v", query, status, body)
		}
	}
	if status, _ := call(t, srv, "GET", "/v1/me/matches", "", nil); status != http.StatusUnauthorized {
		t.Fatalf("without a session: want 401, got %d", status)
	}
}
