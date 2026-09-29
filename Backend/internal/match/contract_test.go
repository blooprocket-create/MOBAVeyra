package match

import (
	"bytes"
	"flag"
	"os"
	"path/filepath"
	"testing"
)

// The game's copy of the assignment contract (ADR-007 §5). The game's tests
// validate this file against the schema the match server uses, so a change to
// the assignment's shape fails on both sides until both agree. After an
// intended change, rewrite it with:
//
//	go test ./internal/match -run TestAssignmentMatchesTheGamesContract -args -update-contract
var updateContract = flag.Bool("update-contract", false, "rewrite the game's example assignment")

var contractPath = filepath.Join("..", "..", "..", "Game", "Source", "VeyraDeveloper", "TestData", "MatchAssignment.example.json")

// contractCredential stands in for a match-server credential. It has the
// credential's format and is not one.
const contractCredential = "vms_EXAMPLE-ONLY-not-a-real-server-credential-0"

func TestAssignmentMatchesTheGamesContract(t *testing.T) {
	if len(contractCredential) != len("vms_")+43 {
		t.Fatalf("the example credential has %d characters, want the format's %d", len(contractCredential), len("vms_")+43)
	}
	// The ticket vector's match and account, so the game can check the
	// first participant's ticket hash against the vector's ticket.
	m := Match{
		ID:      vectorMatchID,
		Mode:    "casual_select",
		Rules:   RulesStandard,
		JoinKey: vectorKey(),
		Participants: []Participant{
			{AccountID: vectorAccountID, DisplayName: "DevOne", Side: SideA, VanguardID: "cairn", FluxSpells: [2]string{"blink", "scorch"}},
			{AccountID: "bbbbbbbb-cccc-4ddd-8eee-ffffffffffff", DisplayName: "DevTwo", Side: SideB, VanguardID: "oriel", FluxSpells: [2]string{"mend", ""}},
		},
	}
	line, err := BuildAssignment(m, contractCredential, "http://backend:8080")
	if err != nil {
		t.Fatal(err)
	}
	if *updateContract {
		if err := os.WriteFile(contractPath, line, 0o644); err != nil {
			t.Fatal(err)
		}
		t.Logf("rewrote %s", contractPath)
		return
	}
	want, err := os.ReadFile(contractPath)
	if err != nil {
		t.Fatalf("read the game's contract: %v", err)
	}
	if !bytes.Equal(line, want) {
		t.Fatalf("the assignment no longer matches %s:\n got %s\nwant %s\nIf the change is intended, update the game's schema and rewrite the file (see the comment above).",
			contractPath, line, want)
	}
	if !bytes.Contains(want, []byte(vectorTicketHash)) {
		t.Fatal("the contract should carry the ticket vector's hash")
	}
}
