package firebaseauth

import (
	"context"
	"crypto"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha256"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/base64"
	"encoding/json"
	"encoding/pem"
	"errors"
	"math/big"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync/atomic"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
)

const project = "veyra-test"

var epoch = time.Date(2026, 10, 1, 12, 0, 0, 0, time.UTC)

// googleKeys stands in for Google's securetoken key endpoint.
type googleKeys struct {
	keys    map[string]*rsa.PrivateKey
	fetches atomic.Int32
	status  int
	maxAge  string
}

func newGoogleKeys(t *testing.T, kids ...string) *googleKeys {
	t.Helper()
	g := &googleKeys{keys: map[string]*rsa.PrivateKey{}, status: http.StatusOK, maxAge: "public, max-age=3600, must-revalidate"}
	for _, kid := range kids {
		k, err := rsa.GenerateKey(rand.Reader, 2048)
		if err != nil {
			t.Fatal(err)
		}
		g.keys[kid] = k
	}
	return g
}

func (g *googleKeys) ServeHTTP(w http.ResponseWriter, _ *http.Request) {
	g.fetches.Add(1)
	if g.status != http.StatusOK {
		w.WriteHeader(g.status)
		return
	}
	doc := map[string]string{}
	for kid, k := range g.keys {
		tmpl := &x509.Certificate{SerialNumber: big.NewInt(1), Subject: pkix.Name{CommonName: kid},
			NotBefore: epoch.Add(-time.Hour), NotAfter: epoch.Add(48 * time.Hour)}
		der, err := x509.CreateCertificate(rand.Reader, tmpl, tmpl, &k.PublicKey, k)
		if err != nil {
			panic(err)
		}
		doc[kid] = string(pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: der}))
	}
	w.Header().Set("Cache-Control", g.maxAge)
	_ = json.NewEncoder(w).Encode(doc)
}

func sign(t *testing.T, key *rsa.PrivateKey, hdr map[string]any, claims map[string]any) string {
	t.Helper()
	enc := func(v any) string {
		b, err := json.Marshal(v)
		if err != nil {
			t.Fatal(err)
		}
		return base64.RawURLEncoding.EncodeToString(b)
	}
	signing := enc(hdr) + "." + enc(claims)
	digest := sha256.Sum256([]byte(signing))
	sig, err := rsa.SignPKCS1v15(rand.Reader, key, crypto.SHA256, digest[:])
	if err != nil {
		t.Fatal(err)
	}
	return signing + "." + base64.RawURLEncoding.EncodeToString(sig)
}

func goodClaims() map[string]any {
	return map[string]any{
		"iss":       "https://securetoken.google.com/" + project,
		"aud":       project,
		"sub":       "firebase-uid-1",
		"iat":       epoch.Add(-time.Minute).Unix(),
		"auth_time": epoch.Add(-time.Minute).Unix(),
		"exp":       epoch.Add(59 * time.Minute).Unix(),
		"email":     "player@example.com",
	}
}

type harness struct {
	t      *testing.T
	google *googleKeys
	now    time.Time
	v      *Verifier
}

func newHarness(t *testing.T) *harness {
	t.Helper()
	h := &harness{t: t, google: newGoogleKeys(t, "k1", "k2"), now: epoch}
	srv := httptest.NewServer(h.google)
	t.Cleanup(srv.Close)
	v, err := New(Config{ProjectID: project, KeysURL: srv.URL, ClockSkew: 30 * time.Second,
		Client: srv.Client(), Now: func() time.Time { return h.now }})
	if err != nil {
		t.Fatal(err)
	}
	h.v = v
	return h
}

func (h *harness) token(kid string, mutate func(map[string]any)) string {
	c := goodClaims()
	if mutate != nil {
		mutate(c)
	}
	return sign(h.t, h.google.keys[kid], map[string]any{"alg": "RS256", "kid": kid, "typ": "JWT"}, c)
}

func TestVerifiesAGenuineToken(t *testing.T) {
	h := newHarness(t)
	who, err := h.v.Verify(context.Background(), h.token("k1", nil))
	if err != nil {
		t.Fatal(err)
	}
	if who != (identity.ProviderIdentity{Provider: "firebase", Subject: "firebase-uid-1"}) {
		t.Fatalf("got %+v", who)
	}
	// The second key and a second token reuse the cached keys.
	if _, err := h.v.Verify(context.Background(), h.token("k2", nil)); err != nil {
		t.Fatal(err)
	}
	if n := h.google.fetches.Load(); n != 1 {
		t.Fatalf("keys fetched %d times, want 1", n)
	}
}

func TestRejectsBadTokens(t *testing.T) {
	h := newHarness(t)
	stranger, _ := rsa.GenerateKey(rand.Reader, 2048)
	cases := map[string]string{
		"empty":         "",
		"not a jwt":     "abc.def",
		"garbage":       "a.b.c",
		"wrong project": h.token("k1", func(c map[string]any) { c["aud"] = "someone-else" }),
		"wrong issuer":  h.token("k1", func(c map[string]any) { c["iss"] = "https://evil.example/" + project }),
		"no subject":    h.token("k1", func(c map[string]any) { c["sub"] = "" }),
		"long subject":  h.token("k1", func(c map[string]any) { c["sub"] = strings.Repeat("x", 129) }),
		"expired":       h.token("k1", func(c map[string]any) { c["exp"] = epoch.Add(-time.Minute).Unix() }),
		"issued later":  h.token("k1", func(c map[string]any) { c["iat"] = epoch.Add(5 * time.Minute).Unix() }),
		"auth later":    h.token("k1", func(c map[string]any) { c["auth_time"] = epoch.Add(5 * time.Minute).Unix() }),
		"no expiry":     h.token("k1", func(c map[string]any) { delete(c, "exp") }),
		"string expiry": h.token("k1", func(c map[string]any) { c["exp"] = "soon" }),
		"forged":        sign(t, stranger, map[string]any{"alg": "RS256", "kid": "k1"}, goodClaims()),
		"alg none":      sign(t, h.google.keys["k1"], map[string]any{"alg": "none", "kid": "k1"}, goodClaims()),
		"alg HS256":     sign(t, h.google.keys["k1"], map[string]any{"alg": "HS256", "kid": "k1"}, goodClaims()),
		"no kid":        sign(t, h.google.keys["k1"], map[string]any{"alg": "RS256"}, goodClaims()),
		"unknown kid":   sign(t, h.google.keys["k1"], map[string]any{"alg": "RS256", "kid": "k9"}, goodClaims()),
		"too long":      strings.Repeat("a", maxTokenBytes+1),
	}
	for name, tok := range cases {
		t.Run(name, func(t *testing.T) {
			if _, err := h.v.Verify(context.Background(), tok); !errors.Is(err, identity.ErrInvalidCredentials) {
				t.Fatalf("got %v, want ErrInvalidCredentials", err)
			}
		})
	}
}

func TestToleratesClockSkew(t *testing.T) {
	h := newHarness(t)
	tok := h.token("k1", func(c map[string]any) {
		c["iat"] = epoch.Add(20 * time.Second).Unix()
		c["exp"] = epoch.Add(-20 * time.Second).Unix()
	})
	if _, err := h.v.Verify(context.Background(), tok); err != nil {
		t.Fatalf("within skew: %v", err)
	}
}

func TestUnknownKeyIDsDoNotHammerGoogle(t *testing.T) {
	h := newHarness(t)
	unknown := sign(t, h.google.keys["k1"], map[string]any{"alg": "RS256", "kid": "k9"}, goodClaims())
	for range 5 {
		_, _ = h.v.Verify(context.Background(), unknown)
	}
	if n := h.google.fetches.Load(); n != 1 {
		t.Fatalf("keys fetched %d times, want 1", n)
	}
	// After the refetch interval an unknown key ID may refresh the keys: Google rotated them.
	h.now = h.now.Add(minKeyRefetch)
	_, _ = h.v.Verify(context.Background(), unknown)
	if n := h.google.fetches.Load(); n != 2 {
		t.Fatalf("keys fetched %d times, want 2", n)
	}
}

func TestKeysAreRefetchedWhenTheyExpire(t *testing.T) {
	h := newHarness(t)
	if _, err := h.v.Verify(context.Background(), h.token("k1", nil)); err != nil {
		t.Fatal(err)
	}
	h.now = h.now.Add(time.Hour)
	tok := h.token("k1", func(c map[string]any) {
		c["iat"] = h.now.Unix()
		c["auth_time"] = h.now.Unix()
		c["exp"] = h.now.Add(time.Hour).Unix()
	})
	if _, err := h.v.Verify(context.Background(), tok); err != nil {
		t.Fatal(err)
	}
	if n := h.google.fetches.Load(); n != 2 {
		t.Fatalf("keys fetched %d times, want 2", n)
	}
}

// Google asking for immediate revalidation, as in an emergency key
// revocation, leaves no key cached: the next token fetches them again.
func TestAZeroMaxAgeKeepsNoKey(t *testing.T) {
	h := newHarness(t)
	h.google.maxAge = "public, max-age=0, must-revalidate"
	for range 2 {
		if _, err := h.v.Verify(context.Background(), h.token("k1", nil)); err != nil {
			t.Fatal(err)
		}
	}
	if n := h.google.fetches.Load(); n != 2 {
		t.Fatalf("keys fetched %d times, want 2", n)
	}
}

func TestAKeyOutageIsNotABadPassword(t *testing.T) {
	h := newHarness(t)
	h.google.status = http.StatusServiceUnavailable
	_, err := h.v.Verify(context.Background(), h.token("k1", nil))
	if err == nil || errors.Is(err, identity.ErrInvalidCredentials) {
		t.Fatalf("got %v, want an outage error", err)
	}
}

func TestCacheLifetime(t *testing.T) {
	cases := map[string]time.Duration{
		"public, max-age=19800, must-revalidate, no-transform": 5*time.Hour + 30*time.Minute,
		"max-age=999999":                     maxKeyCache,
		"public, max-age=0, must-revalidate": 0,
		"no-cache":                           minKeyRefetch,
		"max-age=abc":                        minKeyRefetch,
		"max-age=-5":                         minKeyRefetch,
		"":                                   minKeyRefetch,
	}
	for header, want := range cases {
		if got := cacheLifetime(header); got != want {
			t.Errorf("%q: got %s, want %s", header, got, want)
		}
	}
}

func TestNewRequiresEveryValue(t *testing.T) {
	if _, err := New(Config{}); err == nil {
		t.Fatal("empty config accepted")
	}
}
