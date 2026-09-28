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
	d := newTestDeps(t, true)
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
