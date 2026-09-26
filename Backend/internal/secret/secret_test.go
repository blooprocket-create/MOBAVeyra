package secret

import (
	"bytes"
	"crypto/sha256"
	"encoding/base64"
	"strings"
	"testing"
)

func TestNewHasPrefixAndFullEntropy(t *testing.T) {
	s, hash, err := New("vxx_")
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	if !strings.HasPrefix(s, "vxx_") {
		t.Fatalf("secret %q lacks its prefix", s)
	}
	raw, err := base64.RawURLEncoding.DecodeString(strings.TrimPrefix(s, "vxx_"))
	if err != nil || len(raw) != Bytes {
		t.Fatalf("secret body is not %d base64url bytes: %v", Bytes, err)
	}
	if !bytes.Equal(hash, Hash(s)) {
		t.Fatal("New's hash differs from Hash")
	}
}

func TestSecretsDiffer(t *testing.T) {
	a, _, _ := New("vxx_")
	b, _, _ := New("vxx_")
	if a == b {
		t.Fatal("two secrets are equal")
	}
}

func TestHashIsSHA256(t *testing.T) {
	want := sha256.Sum256([]byte("vxx_example"))
	if !bytes.Equal(Hash("vxx_example"), want[:]) {
		t.Fatal("Hash is not the SHA-256 of the secret")
	}
}
