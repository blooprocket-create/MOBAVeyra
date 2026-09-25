package httpapi

import (
	"bytes"
	"context"
	"encoding/json"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

type okPinger struct{}

func (okPinger) Ping(context.Context) error { return nil }

const testBodyLimit = 1024

var testAccounts = []string{"DevOne", "DevTwo", "DevThree"}

func newTestServer(t *testing.T, devLogin bool) *httptest.Server {
	t.Helper()
	store := identity.NewMemStore()
	var ids []string
	for _, name := range testAccounts {
		a, err := store.EnsureDevAccount(context.Background(), name)
		if err != nil {
			t.Fatal(err)
		}
		ids = append(ids, a.ID)
	}
	svc := identity.NewService(store, identity.Settings{
		LauncherSessionLifetime: time.Hour,
		GameSessionLifetime:     time.Hour,
		LaunchCodeLifetime:      20 * time.Second,
		DevLoginEnabled:         devLogin,
	}, time.Now)
	soc := social.NewService(social.NewMemStore(ids...))
	parties := party.NewService(party.NewMemStore(), soc, party.Settings{
		Rules: party.Rules{
			MaxSize: 5,
			Modes:   map[string]party.Mode{"casual_select": {ID: "casual_select", Enabled: true, HumanPlayersPerTeam: 5}},
		},
		InviteLifetime: time.Minute,
		DefaultPrivacy: party.Private,
	}, time.Now)
	log := slog.New(slog.NewTextHandler(io.Discard, nil))
	srv := httptest.NewServer(New(Deps{
		Identity: svc,
		Social:   soc,
		Party:    parties,
		Modes:    []ModeInfo{{ID: "casual_select", Enabled: true, HumanPlayersPerTeam: 5}},
		Ready:    okPinger{},
		// The in-memory stores are separate, so tests run steps in sequence.
		Atomic:         func(ctx context.Context, fn func(context.Context) error) error { return fn(ctx) },
		BodyLimitBytes: testBodyLimit,
		DevLogin:       devLogin,
		Log:            log,
	}))
	t.Cleanup(srv.Close)
	return srv
}

func call(t *testing.T, srv *httptest.Server, method, path, token string, body any) (int, map[string]any) {
	t.Helper()
	var reader io.Reader
	if body != nil {
		b, err := json.Marshal(body)
		if err != nil {
			t.Fatal(err)
		}
		reader = bytes.NewReader(b)
	}
	req, err := http.NewRequest(method, srv.URL+path, reader)
	if err != nil {
		t.Fatal(err)
	}
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}
	resp, err := srv.Client().Do(req)
	if err != nil {
		t.Fatal(err)
	}
	defer resp.Body.Close()
	out := map[string]any{}
	_ = json.NewDecoder(resp.Body).Decode(&out)
	return resp.StatusCode, out
}

func TestHandoffOverHTTP(t *testing.T) {
	srv := newTestServer(t, true)

	status, login := call(t, srv, "POST", "/v1/dev/login", "", map[string]string{"accountName": "DevOne"})
	if status != http.StatusOK {
		t.Fatalf("login: %d %v", status, login)
	}
	status, code := call(t, srv, "POST", "/v1/launch-codes", login["token"].(string), map[string]string{"buildVersion": "dev-1"})
	if status != http.StatusOK {
		t.Fatalf("launch code: %d %v", status, code)
	}
	status, game := call(t, srv, "POST", "/v1/game-sessions", "", map[string]string{"launchCode": code["token"].(string), "buildVersion": "dev-1"})
	if status != http.StatusOK {
		t.Fatalf("redeem: %d %v", status, game)
	}
	status, me := call(t, srv, "GET", "/v1/me", game["token"].(string), nil)
	if status != http.StatusOK || me["displayName"] != "DevOne" {
		t.Fatalf("me: %d %v", status, me)
	}

	// The code is spent.
	status, _ = call(t, srv, "POST", "/v1/game-sessions", "", map[string]string{"launchCode": code["token"].(string), "buildVersion": "dev-1"})
	if status != http.StatusUnauthorized {
		t.Fatalf("second redeem: want 401, got %d", status)
	}
}

func TestDevLoginRouteAbsentWhenDisabled(t *testing.T) {
	srv := newTestServer(t, false)
	status, _ := call(t, srv, "POST", "/v1/dev/login", "", map[string]string{"accountName": "DevOne"})
	if status != http.StatusNotFound {
		t.Fatalf("want 404, got %d", status)
	}
}

func TestRejectsBadRequests(t *testing.T) {
	srv := newTestServer(t, true)

	if status, _ := call(t, srv, "GET", "/v1/me", "", nil); status != http.StatusUnauthorized {
		t.Fatalf("missing bearer: want 401, got %d", status)
	}
	if status, _ := call(t, srv, "POST", "/v1/dev/login", "", map[string]string{"accountName": "DevOne", "password": "x"}); status != http.StatusBadRequest {
		t.Fatalf("unknown field: want 400, got %d", status)
	}

	big := strings.Repeat("a", testBodyLimit)
	if status, _ := call(t, srv, "POST", "/v1/dev/login", "", map[string]string{"accountName": big}); status != http.StatusRequestEntityTooLarge {
		t.Fatalf("oversized body: want 413, got %d", status)
	}

	_, login := call(t, srv, "POST", "/v1/dev/login", "", map[string]string{"accountName": "DevOne"})
	if status, body := call(t, srv, "POST", "/v1/launch-codes", login["token"].(string), map[string]string{"buildVersion": "bad version"}); status != http.StatusBadRequest {
		t.Fatalf("bad build version: want 400, got %d %v", status, body)
	}
}

// gameSession logs in as a dev account and returns a game session token.
func gameSession(t *testing.T, srv *httptest.Server, name string) (token, id string) {
	t.Helper()
	_, login := call(t, srv, "POST", "/v1/dev/login", "", map[string]string{"accountName": name})
	_, code := call(t, srv, "POST", "/v1/launch-codes", login["token"].(string), map[string]string{"buildVersion": "dev"})
	status, game := call(t, srv, "POST", "/v1/game-sessions", "", map[string]string{"launchCode": code["token"].(string), "buildVersion": "dev"})
	if status != http.StatusOK {
		t.Fatalf("game session for %s: %d %v", name, status, game)
	}
	return game["token"].(string), game["account"].(map[string]any)["id"].(string)
}

func TestSocialAndPartyOverHTTP(t *testing.T) {
	srv := newTestServer(t, true)
	one, _ := gameSession(t, srv, "DevOne")
	two, twoID := gameSession(t, srv, "DevTwo")

	if status, _ := call(t, srv, "GET", "/v1/party", "", nil); status != http.StatusUnauthorized {
		t.Fatalf("party without a session: want 401, got %d", status)
	}

	status, found := call(t, srv, "GET", "/v1/accounts?displayName=DevTwo", one, nil)
	if status != http.StatusOK || found["id"] != twoID {
		t.Fatalf("lookup: %d %v", status, found)
	}

	// Not friends yet: no invite.
	if status, body := call(t, srv, "POST", "/v1/party/invites", one, map[string]string{"accountId": twoID}); status != http.StatusForbidden || body["error"] != "not_friends" {
		t.Fatalf("invite stranger: %d %v", status, body)
	}

	_, oneMe := call(t, srv, "GET", "/v1/me", one, nil)
	oneID := oneMe["id"].(string)
	if status, body := call(t, srv, "POST", "/v1/friends/requests", one, map[string]string{"accountId": twoID}); status != http.StatusOK || body["outcome"] != "requested" {
		t.Fatalf("friend request: %d %v", status, body)
	}
	if status, _ := call(t, srv, "POST", "/v1/friends/requests/"+oneID+"/accept", two, nil); status != http.StatusNoContent {
		t.Fatalf("accept friend: %d", status)
	}

	if status, body := call(t, srv, "POST", "/v1/party/invites", one, map[string]string{"accountId": twoID}); status != http.StatusOK {
		t.Fatalf("invite: %d %v", status, body)
	}
	_, invites := call(t, srv, "GET", "/v1/party/invites", two, nil)
	list := invites["invites"].([]any)
	if len(list) != 1 {
		t.Fatalf("invites: %v", invites)
	}
	inviteID := list[0].(map[string]any)["id"].(string)
	status, joined := call(t, srv, "POST", "/v1/party/invites/"+inviteID+"/accept", two, nil)
	if status != http.StatusOK || len(joined["party"].(map[string]any)["members"].([]any)) != 2 {
		t.Fatalf("accept invite: %d %v", status, joined)
	}

	if status, body := call(t, srv, "PUT", "/v1/party/mode", two, map[string]string{"mode": "casual_select"}); status != http.StatusForbidden || body["error"] != "not_leader" {
		t.Fatalf("non-leader mode change: %d %v", status, body)
	}
	call(t, srv, "PUT", "/v1/party/mode", one, map[string]string{"mode": "casual_select"})
	if status, body := call(t, srv, "POST", "/v1/party/queue", one, nil); status != http.StatusConflict || body["error"] != "not_all_ready" {
		t.Fatalf("queue before ready: %d %v", status, body)
	}
	for _, tok := range []string{one, two} {
		if status, body := call(t, srv, "PUT", "/v1/party/ready", tok, map[string]bool{"ready": true}); status != http.StatusOK {
			t.Fatalf("ready: %d %v", status, body)
		}
	}
	status, queued := call(t, srv, "POST", "/v1/party/queue", one, nil)
	if status != http.StatusOK || queued["party"].(map[string]any)["status"] != "queued" {
		t.Fatalf("queue: %d %v", status, queued)
	}

	// Blocking a party-mate removes them (provisional rule) and ends the friendship.
	if status, _ := call(t, srv, "PUT", "/v1/blocks/"+twoID, one, nil); status != http.StatusNoContent {
		t.Fatalf("block: %d", status)
	}
	_, twoParty := call(t, srv, "GET", "/v1/party", two, nil)
	if twoParty["party"] != nil {
		t.Fatalf("blocked member should have been removed: %v", twoParty)
	}
	_, friends := call(t, srv, "GET", "/v1/friends", one, nil)
	if len(friends["friends"].([]any)) != 0 {
		t.Fatalf("block should end the friendship: %v", friends)
	}
}
