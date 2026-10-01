package httpapi

import (
	"context"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
)

// stubVerifier accepts "valid:<subject>" as an identity-provider token.
type stubVerifier struct{}

func (stubVerifier) Verify(_ context.Context, token string) (identity.ProviderIdentity, error) {
	subject, ok := strings.CutPrefix(token, "valid:")
	if !ok || subject == "" {
		return identity.ProviderIdentity{}, identity.ErrInvalidCredentials
	}
	return identity.ProviderIdentity{Provider: "test", Subject: subject}, nil
}

func newPlayerLoginServer(t *testing.T, verifier identity.Verifier) *httptest.Server {
	t.Helper()
	d := newTestDeps(t, false)
	store := identity.NewMemStore()
	if _, err := store.EnsureDevAccount(context.Background(), "DevOne"); err != nil {
		t.Fatal(err)
	}
	d.Identity = identity.NewService(store, identity.Settings{
		LauncherSessionLifetime: time.Hour,
		GameSessionLifetime:     time.Hour,
		LaunchCodeLifetime:      20 * time.Second,
		PlayerLogin:             verifier,
	}, time.Now)
	return serve(t, d)
}

func TestRegisterAndLoginOverHTTP(t *testing.T) {
	srv := newPlayerLoginServer(t, stubVerifier{})

	status, body := call(t, srv, "POST", "/v1/login", "", map[string]string{"providerToken": "valid:uid-1"})
	if status != http.StatusNotFound || body["error"] != "not_registered" {
		t.Fatalf("login before registering: %d %v", status, body)
	}
	status, body = call(t, srv, "POST", "/v1/register", "", map[string]string{"providerToken": "valid:uid-1", "displayName": "Ember"})
	if status != http.StatusCreated {
		t.Fatalf("register: %d %v", status, body)
	}
	acct, _ := body["account"].(map[string]any)
	if acct["displayName"] != "Ember" || !strings.HasPrefix(body["token"].(string), "vls_") {
		t.Fatalf("register answer: %v", body)
	}
	status, body = call(t, srv, "POST", "/v1/login", "", map[string]string{"providerToken": "valid:uid-1"})
	if status != http.StatusOK {
		t.Fatalf("login: %d %v", status, body)
	}
	// The launcher session works for a launch code.
	status, body = call(t, srv, "POST", "/v1/launch-codes", body["token"].(string), map[string]string{"buildVersion": "dev-1"})
	if status != http.StatusOK || !strings.HasPrefix(body["token"].(string), "vlc_") {
		t.Fatalf("launch code: %d %v", status, body)
	}
}

func TestRegisterErrorsOverHTTP(t *testing.T) {
	srv := newPlayerLoginServer(t, stubVerifier{})
	if status, body := call(t, srv, "POST", "/v1/register", "", map[string]string{"providerToken": "valid:uid-1", "displayName": "Ember"}); status != http.StatusCreated {
		t.Fatalf("register: %d %v", status, body)
	}
	cases := map[string]struct {
		body   any
		status int
		code   string
	}{
		"name taken":       {map[string]string{"providerToken": "valid:uid-2", "displayName": "ember"}, http.StatusConflict, "display_name_taken"},
		"registered":       {map[string]string{"providerToken": "valid:uid-1", "displayName": "Other"}, http.StatusConflict, "already_registered"},
		"bad name":         {map[string]string{"providerToken": "valid:uid-2", "displayName": "x"}, http.StatusBadRequest, "invalid_display_name"},
		"bad token":        {map[string]string{"providerToken": "forged", "displayName": "Fresh"}, http.StatusUnauthorized, "invalid_credentials"},
		"unknown field":    {map[string]string{"providerToken": "valid:uid-2", "displayName": "Fresh", "email": "a@b.c"}, http.StatusBadRequest, "malformed_request"},
		"no display name":  {map[string]string{"providerToken": "valid:uid-2"}, http.StatusBadRequest, "invalid_display_name"},
		"dev name in case": {map[string]string{"providerToken": "valid:uid-2", "displayName": "DEVONE"}, http.StatusConflict, "display_name_taken"},
	}
	for name, c := range cases {
		t.Run(name, func(t *testing.T) {
			status, body := call(t, srv, "POST", "/v1/register", "", c.body)
			if status != c.status || body["error"] != c.code {
				t.Fatalf("got %d %v, want %d %s", status, body, c.status, c.code)
			}
		})
	}
}

func TestPlayerLoginWithoutAProviderIsNotFound(t *testing.T) {
	srv := newPlayerLoginServer(t, nil)
	for path, req := range map[string]map[string]string{
		"/v1/login":    {"providerToken": "valid:uid-1"},
		"/v1/register": {"providerToken": "valid:uid-1", "displayName": "Ember"},
	} {
		status, body := call(t, srv, "POST", path, "", req)
		if status != http.StatusNotFound || body["error"] != "not_found" {
			t.Fatalf("%s: %d %v", path, status, body)
		}
	}
}
