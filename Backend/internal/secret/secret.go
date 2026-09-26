// Package secret makes the backend's bearer secrets: session tokens, launch
// codes and match-server credentials (ADR-005, ADR-007). Each is a prefix
// that makes a leaked secret recognisable to secret scanners, followed by 256
// random bits in unpadded base64url. The backend stores only a secret's
// SHA-256 hash, never the secret itself.
package secret

import (
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
)

// Bytes is the entropy of every secret (256 bits).
const Bytes = 32

// New returns a fresh secret with the given prefix and its hash.
func New(prefix string) (secret string, hash []byte, err error) {
	buf := make([]byte, Bytes)
	if _, err := rand.Read(buf); err != nil {
		return "", nil, err
	}
	secret = prefix + base64.RawURLEncoding.EncodeToString(buf)
	return secret, Hash(secret), nil
}

// Hash returns the SHA-256 of a secret, the only form the backend stores.
func Hash(secret string) []byte {
	sum := sha256.Sum256([]byte(secret))
	return sum[:]
}
