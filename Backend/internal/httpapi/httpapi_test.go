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
)

type okPinger struct{}

func (okPinger) Ping(context.Context) error { return nil }

const testBodyLimit = 1024

func newTestServer(t *testing.T, devLogin bool) *httptest.Server {
	t.Helper()
	store := identity.NewMemStore()
	if _, err := store.EnsureDevAccount(context.Background(), "DevOne"); err != nil {
		t.Fatal(err)
	}
	svc := identity.NewService(store, identity.Settings{
		LauncherSessionLifetime: time.Hour,
		GameSessionLifetime:     time.Hour,
		LaunchCodeLifetime:      20 * time.Second,
		DevLoginEnabled:         devLogin,
	}, time.Now)
	log := slog.New(slog.NewTextHandler(io.Discard, nil))
	srv := httptest.NewServer(New(svc, okPinger{}, testBodyLimit, devLogin, log))
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
