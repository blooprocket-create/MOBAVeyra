// Package firebaseauth verifies Firebase Authentication ID tokens, the
// credential the launcher gets when a player registers or signs in with
// Firebase (ADR-038). It implements identity.Verifier.
//
// It follows Firebase's documented checks for third-party verification of ID
// tokens: an RS256 JWT signed by one of Google's published securetoken keys,
// whose audience is the Firebase project ID, whose issuer is
// https://securetoken.google.com/<projectId>, which has not expired, was not
// issued in the future, and names a non-empty subject (the Firebase user ID).
// It uses only the standard library, so the backend takes on no SDK.
package firebaseauth

import (
	"bytes"
	"context"
	"crypto"
	"crypto/rsa"
	"crypto/sha256"
	"crypto/x509"
	"encoding/base64"
	"encoding/json"
	"encoding/pem"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
)

// ProviderName is the provider recorded with every account Firebase vouches for.
const ProviderName = "firebase"

// Protocol limits, not tuning.
const (
	// maxTokenBytes bounds the ID token accepted; real ones are about 1 KB.
	maxTokenBytes = 8 << 10
	// maxKeysBytes bounds the key document read from Google.
	maxKeysBytes = 1 << 20
	// maxSubjectLength is Firebase's limit on user IDs.
	maxSubjectLength = 128
	// minKeyRefetch stops a stream of tokens naming an unknown key ID from
	// making the backend fetch Google's keys on every request.
	minKeyRefetch = 30 * time.Second
	// maxKeyCache bounds how long keys are trusted when Google's
	// Cache-Control says longer or says nothing.
	maxKeyCache = 6 * time.Hour
)

// Config configures a Verifier. Every value is required.
type Config struct {
	// ProjectID is the Firebase project ID, the tokens' audience.
	ProjectID string
	// KeysURL serves the X.509 certificates that sign ID tokens, keyed by
	// key ID.
	KeysURL string
	// ClockSkew is the leeway allowed on expiry and issue times.
	ClockSkew time.Duration
	// Client fetches the keys; its Timeout bounds each fetch.
	Client *http.Client
	// Now is the clock; injectable for tests.
	Now func() time.Time
}

// Verifier verifies Firebase ID tokens. It is safe for concurrent use.
type Verifier struct {
	cfg    Config
	issuer string

	mu        sync.Mutex
	keys      map[string]*rsa.PublicKey
	expires   time.Time
	lastFetch time.Time
}

// New builds a Verifier. Keys are fetched on first use, so starting the
// backend never depends on reaching Google.
func New(cfg Config) (*Verifier, error) {
	if cfg.ProjectID == "" || cfg.KeysURL == "" || cfg.Client == nil || cfg.Now == nil || cfg.ClockSkew < 0 {
		return nil, errors.New("firebaseauth: incomplete config")
	}
	return &Verifier{cfg: cfg, issuer: "https://securetoken.google.com/" + cfg.ProjectID}, nil
}

type header struct {
	Alg string `json:"alg"`
	Kid string `json:"kid"`
}

type claims struct {
	Iss      string      `json:"iss"`
	Aud      string      `json:"aud"`
	Sub      string      `json:"sub"`
	Iat      json.Number `json:"iat"`
	Exp      json.Number `json:"exp"`
	AuthTime json.Number `json:"auth_time"`
}

// Verify checks an ID token and returns the Firebase user it identifies.
// Every rejection of the token is identity.ErrInvalidCredentials; failing to
// fetch Google's keys is a different error, so it is logged as an outage
// rather than shown as a bad password.
func (v *Verifier) Verify(ctx context.Context, token string) (identity.ProviderIdentity, error) {
	invalid := identity.ProviderIdentity{}
	if len(token) > maxTokenBytes {
		return invalid, identity.ErrInvalidCredentials
	}
	parts := strings.Split(token, ".")
	if len(parts) != 3 {
		return invalid, identity.ErrInvalidCredentials
	}
	var h header
	if !decodeSegment(parts[0], &h) || h.Alg != "RS256" || h.Kid == "" {
		return invalid, identity.ErrInvalidCredentials
	}
	sig, err := base64.RawURLEncoding.DecodeString(parts[2])
	if err != nil {
		return invalid, identity.ErrInvalidCredentials
	}
	key, err := v.key(ctx, h.Kid)
	if err != nil {
		return invalid, err
	}
	digest := sha256.Sum256([]byte(parts[0] + "." + parts[1]))
	if rsa.VerifyPKCS1v15(key, crypto.SHA256, digest[:], sig) != nil {
		return invalid, identity.ErrInvalidCredentials
	}
	var c claims
	if !decodeSegment(parts[1], &c) {
		return invalid, identity.ErrInvalidCredentials
	}
	if err := v.checkClaims(c); err != nil {
		return invalid, err
	}
	return identity.ProviderIdentity{Provider: ProviderName, Subject: c.Sub}, nil
}

func (v *Verifier) checkClaims(c claims) error {
	now := v.cfg.Now()
	skew := v.cfg.ClockSkew
	exp, okExp := unixTime(c.Exp)
	iat, okIat := unixTime(c.Iat)
	authTime, okAuth := unixTime(c.AuthTime)
	switch {
	case c.Aud != v.cfg.ProjectID, c.Iss != v.issuer:
		return identity.ErrInvalidCredentials
	case c.Sub == "", len(c.Sub) > maxSubjectLength:
		return identity.ErrInvalidCredentials
	case !okExp || !okIat || !okAuth:
		return identity.ErrInvalidCredentials
	case !now.Before(exp.Add(skew)):
		return identity.ErrInvalidCredentials
	case iat.After(now.Add(skew)), authTime.After(now.Add(skew)):
		return identity.ErrInvalidCredentials
	}
	return nil
}

// key returns the public key with this ID, fetching Google's keys when the
// cached ones have expired or do not include it.
func (v *Verifier) key(ctx context.Context, kid string) (*rsa.PublicKey, error) {
	v.mu.Lock()
	defer v.mu.Unlock()
	now := v.cfg.Now()
	if k, ok := v.keys[kid]; ok && now.Before(v.expires) {
		return k, nil
	}
	stale := !now.Before(v.expires)
	if !stale && now.Sub(v.lastFetch) < minKeyRefetch {
		// Fresh keys that lack this ID: the token names a key Google does not use.
		return nil, identity.ErrInvalidCredentials
	}
	if err := v.fetch(ctx, now); err != nil {
		return nil, err
	}
	if k, ok := v.keys[kid]; ok {
		return k, nil
	}
	return nil, identity.ErrInvalidCredentials
}

func (v *Verifier) fetch(ctx context.Context, now time.Time) error {
	v.lastFetch = now
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, v.cfg.KeysURL, nil)
	if err != nil {
		return fmt.Errorf("firebaseauth: keys request: %w", err)
	}
	resp, err := v.cfg.Client.Do(req)
	if err != nil {
		return fmt.Errorf("firebaseauth: fetch keys: %w", err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("firebaseauth: fetch keys: HTTP %d", resp.StatusCode)
	}
	body, err := io.ReadAll(io.LimitReader(resp.Body, maxKeysBytes+1))
	if err != nil {
		return fmt.Errorf("firebaseauth: read keys: %w", err)
	}
	if len(body) > maxKeysBytes {
		return errors.New("firebaseauth: keys document too large")
	}
	keys, err := parseKeys(body)
	if err != nil {
		return err
	}
	v.keys = keys
	v.expires = now.Add(cacheLifetime(resp.Header.Get("Cache-Control")))
	return nil
}

// parseKeys reads Google's {"<kid>": "<PEM certificate>"} document.
func parseKeys(body []byte) (map[string]*rsa.PublicKey, error) {
	var certs map[string]string
	if err := json.Unmarshal(body, &certs); err != nil {
		return nil, fmt.Errorf("firebaseauth: keys document: %w", err)
	}
	keys := make(map[string]*rsa.PublicKey, len(certs))
	for kid, text := range certs {
		block, _ := pem.Decode([]byte(text))
		if block == nil {
			return nil, fmt.Errorf("firebaseauth: key %q is not PEM", kid)
		}
		cert, err := x509.ParseCertificate(block.Bytes)
		if err != nil {
			return nil, fmt.Errorf("firebaseauth: key %q: %w", kid, err)
		}
		pub, ok := cert.PublicKey.(*rsa.PublicKey)
		if !ok {
			return nil, fmt.Errorf("firebaseauth: key %q is not RSA", kid)
		}
		keys[kid] = pub
	}
	if len(keys) == 0 {
		return nil, errors.New("firebaseauth: keys document is empty")
	}
	return keys, nil
}

// cacheLifetime reads max-age from a Cache-Control header, bounded by
// maxKeyCache. max-age=0 keeps nothing: Google asks for every key to be
// revalidated, as it may when it revokes one. Without a valid max-age, keys
// are refetched after minKeyRefetch.
func cacheLifetime(cacheControl string) time.Duration {
	for _, directive := range strings.Split(cacheControl, ",") {
		name, value, ok := strings.Cut(strings.TrimSpace(directive), "=")
		if !ok || !strings.EqualFold(name, "max-age") {
			continue
		}
		seconds, err := strconv.Atoi(value)
		if err != nil || seconds < 0 {
			break
		}
		return min(time.Duration(seconds)*time.Second, maxKeyCache)
	}
	return minKeyRefetch
}

func decodeSegment(segment string, dst any) bool {
	raw, err := base64.RawURLEncoding.DecodeString(segment)
	if err != nil {
		return false
	}
	dec := json.NewDecoder(bytes.NewReader(raw))
	dec.UseNumber()
	return dec.Decode(dst) == nil
}

func unixTime(n json.Number) (time.Time, bool) {
	if n == "" {
		return time.Time{}, false
	}
	s, err := n.Int64()
	if err != nil || s <= 0 {
		return time.Time{}, false
	}
	return time.Unix(s, 0), true
}
