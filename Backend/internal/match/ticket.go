package match

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/secret"
)

// Credential prefixes (ADR-007 §2).
const (
	ticketPrefix           = "vjt_"
	serverCredentialPrefix = "vms_"
)

// ticketContext versions the ticket derivation, so a later scheme can never
// produce a ticket equal to one of this scheme's.
const ticketContext = "veyra-join-ticket-v1"

// NewJoinKey returns a fresh random key for one match.
func NewJoinKey() ([]byte, error) {
	key := make([]byte, secret.Bytes)
	if _, err := rand.Read(key); err != nil {
		return nil, err
	}
	return key, nil
}

// DeriveTicket returns an account's join ticket for a match. The same key,
// match and account always give the same ticket, so the backend can hand it
// out again after a crash without storing it (ADR-007 §3).
func DeriveTicket(key []byte, matchID, accountID string) string {
	mac := hmac.New(sha256.New, key)
	mac.Write([]byte(ticketContext))
	mac.Write([]byte{0})
	mac.Write([]byte(matchID))
	mac.Write([]byte{0})
	mac.Write([]byte(accountID))
	return ticketPrefix + base64.RawURLEncoding.EncodeToString(mac.Sum(nil))
}

// TicketHash is the form of a ticket a match server receives: the lowercase
// hex SHA-256 of the ticket string.
func TicketHash(ticket string) string {
	return hex.EncodeToString(secret.Hash(ticket))
}
