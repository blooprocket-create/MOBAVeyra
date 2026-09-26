package match

import (
	"bytes"
	"encoding/json"
	"strings"
	"testing"
)

// A fixed vector computed independently (Python's hmac and hashlib), so the
// derivation cannot drift unnoticed. The game checks ticket hashes the same
// way (ADR-007 §3).
const (
	vectorMatchID    = "11111111-2222-4333-8444-555555555555"
	vectorAccountID  = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
	vectorTicket     = "vjt_xdMWyGQJg9xC_-yn9b-5ZYoh8_KKDRs9Bfjlh7WgwKQ"
	vectorTicketHash = "f9de9702e8e9c925cd96f865fcd36d6bdf45a76ca195068cd024ff2e28c9e737"
)

func vectorKey() []byte {
	key := make([]byte, 32)
	for i := range key {
		key[i] = byte(i + 1)
	}
	return key
}

func TestTicketMatchesTheFixedVector(t *testing.T) {
	ticket := DeriveTicket(vectorKey(), vectorMatchID, vectorAccountID)
	if ticket != vectorTicket {
		t.Fatalf("ticket %s, want %s", ticket, vectorTicket)
	}
	if h := TicketHash(ticket); h != vectorTicketHash {
		t.Fatalf("hash %s, want %s", h, vectorTicketHash)
	}
}

func TestTicketsAreStableAndDistinct(t *testing.T) {
	key := vectorKey()
	a := DeriveTicket(key, vectorMatchID, "account-a")
	if a != DeriveTicket(key, vectorMatchID, "account-a") {
		t.Fatal("the same inputs gave different tickets")
	}
	others := []string{
		DeriveTicket(key, vectorMatchID, "account-b"),
		DeriveTicket(key, "another-match", "account-a"),
		DeriveTicket(append([]byte{0}, key[1:]...), vectorMatchID, "account-a"),
	}
	for _, o := range others {
		if o == a {
			t.Fatal("a different input gave the same ticket")
		}
	}
	// The separator keeps "ab"+"c" and "a"+"bc" apart.
	if DeriveTicket(key, "ab", "c") == DeriveTicket(key, "a", "bc") {
		t.Fatal("match and account boundaries are ambiguous")
	}
}

func TestAssignmentIsOneLineWithTicketHashesOnly(t *testing.T) {
	m := Match{
		ID:      vectorMatchID,
		JoinKey: vectorKey(),
		Participants: []Participant{
			{AccountID: vectorAccountID, DisplayName: "DevOne", Side: SideA},
			{AccountID: "bbbbbbbb-cccc-4ddd-8eee-ffffffffffff", DisplayName: "DevTwo", Side: SideB},
		},
	}
	line, err := BuildAssignment(m, "vms_credential", "http://backend:8080")
	if err != nil {
		t.Fatalf("BuildAssignment: %v", err)
	}
	if !bytes.HasSuffix(line, []byte("\n")) || bytes.Count(line, []byte("\n")) != 1 {
		t.Fatalf("assignment is not exactly one line: %q", line)
	}
	if bytes.Contains(line, []byte("vjt_")) || bytes.Contains(line, m.JoinKey) {
		t.Fatal("the assignment must carry ticket hashes, never tickets or the key")
	}
	var a Assignment
	if err := json.Unmarshal(line, &a); err != nil {
		t.Fatalf("assignment is not JSON: %v", err)
	}
	if a.SchemaVersion != AssignmentSchemaVersion || a.MatchID != m.ID || a.ServerCredential != "vms_credential" || a.BackendURL != "http://backend:8080" {
		t.Fatalf("wrong header: %+v", a)
	}
	if len(a.Participants) != 2 || a.Participants[0].TicketHash != vectorTicketHash || a.Participants[1].Side != SideB {
		t.Fatalf("wrong roster: %+v", a.Participants)
	}
	if strings.ToLower(a.Participants[1].TicketHash) != a.Participants[1].TicketHash {
		t.Fatal("ticket hashes must be lowercase hex")
	}
}

func TestAssignmentNeedsAJoinKey(t *testing.T) {
	if _, err := BuildAssignment(Match{ID: "m"}, "vms_x", "http://b"); err == nil {
		t.Fatal("a match without a key must not produce an assignment")
	}
}
