package httpapi

import (
	"fmt"
	"net/http"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/lobby"
)

func newLobbyTestDeps(t *testing.T) Deps {
	t.Helper()
	d := newTestDeps(t, true)
	d.Lobby = lobby.NewService(lobby.NewMemStore(), d.Social, nil, lobby.Limits{
		PlayersPerSide:  5,
		Released:        map[string]bool{"cairn": true, "oriel": true},
		StartingGoldMin: 0,
		StartingGoldMax: 20000,
	}, time.Minute, time.Now)
	return d
}

func seatsOf(t *testing.T, body map[string]any) []map[string]any {
	t.Helper()
	l, ok := body["lobby"].(map[string]any)
	if !ok {
		t.Fatalf("no lobby in %v", body)
	}
	raw := l["seats"].([]any)
	out := make([]map[string]any, len(raw))
	for i, s := range raw {
		out[i] = s.(map[string]any)
	}
	return out
}

func TestACustomLobbyOverHTTP(t *testing.T) {
	srv := serve(t, newLobbyTestDeps(t))
	one, oneID := gameSession(t, srv, "DevOne")
	two, twoID := gameSession(t, srv, "DevTwo")

	if status, body := call(t, srv, "GET", "/v1/lobby", one, nil); status != http.StatusOK || body["lobby"] != nil {
		t.Fatalf("no lobby yet: %d %v", status, body)
	}
	status, body := call(t, srv, "POST", "/v1/lobby", one, nil)
	if status != http.StatusOK {
		t.Fatalf("create: %d %v", status, body)
	}
	seats := seatsOf(t, body)
	if len(seats) != 10 || seats[0]["kind"] != "human" || seats[0]["accountId"] != oneID || seats[0]["host"] != true || seats[5]["side"] != "B" {
		t.Fatalf("seats: %v", seats)
	}
	if l := body["lobby"].(map[string]any); l["settings"].(map[string]any)["startingGold"] != nil || l["startingGoldRange"].(map[string]any)["max"] != 20000.0 {
		t.Fatalf("settings: %v", l)
	}
	// What a bot may be: every released Vanguard, sorted, at either difficulty.
	if l := body["lobby"].(map[string]any); fmt.Sprint(l["botVanguards"]) != "[cairn oriel]" || fmt.Sprint(l["botDifficulties"]) != "[beginner intermediate]" {
		t.Fatalf("bot choices: %v %v", l["botVanguards"], l["botDifficulties"])
	}

	if status, body := call(t, srv, "POST", "/v1/lobby/invites", one, map[string]string{"accountId": twoID}); status != http.StatusForbidden || body["error"] != "not_friends" {
		t.Fatalf("inviting a stranger: %d %v", status, body)
	}
	call(t, srv, "POST", "/v1/friends/requests", one, map[string]string{"accountId": twoID})
	call(t, srv, "POST", "/v1/friends/requests/"+oneID+"/accept", two, nil)
	if status, body := call(t, srv, "POST", "/v1/lobby/invites", one, map[string]string{"accountId": twoID}); status != http.StatusOK {
		t.Fatalf("invite: %d %v", status, body)
	}
	_, invites := call(t, srv, "GET", "/v1/lobby/invites", two, nil)
	list := invites["invites"].([]any)
	if len(list) != 1 {
		t.Fatalf("invites: %v", invites)
	}
	inviteID := list[0].(map[string]any)["id"].(string)
	status, body = call(t, srv, "POST", "/v1/lobby/invites/"+inviteID+"/accept", two, nil)
	if status != http.StatusOK || seatsOf(t, body)[5]["accountId"] != twoID {
		t.Fatalf("accept: %d %v", status, body)
	}

	if status, body := call(t, srv, "PUT", "/v1/lobby/seats/B/1/bot", two, map[string]string{"vanguardId": "oriel", "difficulty": "beginner"}); status != http.StatusForbidden || body["error"] != "not_host" {
		t.Fatalf("a guest adding a bot: %d %v", status, body)
	}
	status, body = call(t, srv, "PUT", "/v1/lobby/seats/B/1/bot", one, map[string]string{"vanguardId": "oriel", "difficulty": "intermediate"})
	if status != http.StatusOK {
		t.Fatalf("add bot: %d %v", status, body)
	}
	if bot := seatsOf(t, body)[6]; bot["kind"] != "bot" || bot["vanguardId"] != "oriel" || bot["difficulty"] != "intermediate" {
		t.Fatalf("bot seat: %v", bot)
	}
	if status, body := call(t, srv, "PUT", "/v1/lobby/seats/B/9/bot", one, map[string]string{"vanguardId": "oriel", "difficulty": "beginner"}); status != http.StatusBadRequest || body["error"] != "no_such_seat" {
		t.Fatalf("a seat off the side: %d %v", status, body)
	}
	if status, body := call(t, srv, "PUT", "/v1/lobby/settings", one, map[string]any{"victoryEnabled": true, "startingGold": 99999}); status != http.StatusBadRequest || body["error"] != "starting_gold_out_of_range" {
		t.Fatalf("gold out of range: %d %v", status, body)
	}
	status, body = call(t, srv, "PUT", "/v1/lobby/settings", one, map[string]any{"victoryEnabled": false, "startingGold": 5000})
	if status != http.StatusOK {
		t.Fatalf("settings: %d %v", status, body)
	}
	if s := body["lobby"].(map[string]any)["settings"].(map[string]any); s["victoryEnabled"] != false || s["startingGold"] != 5000.0 {
		t.Fatalf("settings: %v", s)
	}
	if status, _ := call(t, srv, "PUT", "/v1/lobby/members/"+twoID+"/seat", one, map[string]any{"side": "A", "index": 2}); status != http.StatusOK {
		t.Fatalf("move: %d", status)
	}
	if status, _ := call(t, srv, "POST", "/v1/lobby/leave", one, nil); status != http.StatusNoContent {
		t.Fatalf("leave: %d", status)
	}
	status, body = call(t, srv, "GET", "/v1/lobby", two, nil)
	if status != http.StatusOK || body["lobby"].(map[string]any)["hostAccountId"] != twoID {
		t.Fatalf("the host passes on: %d %v", status, body)
	}
}
