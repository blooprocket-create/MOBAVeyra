package match

import (
	"encoding/json"
	"errors"
	"fmt"
)

// AssignmentSchemaVersion is the version of the assignment document this
// backend writes. The match server reads exactly one version (ADR-007 §5).
// Version 2 adds the mode, the rules, the practice host and each
// participant's Vanguard (ADR-010 §9).
const AssignmentSchemaVersion = 2

// Assignment is what a match server receives on its standard input when it
// starts: its match, where to report, its credential, the rules it plays by
// and its roster with a hash of each participant's join ticket.
type Assignment struct {
	SchemaVersion    int    `json:"schemaVersion"`
	MatchID          string `json:"matchId"`
	BackendURL       string `json:"backendUrl"`
	ServerCredential string `json:"serverCredential"`
	Mode             string `json:"mode"`
	// Rules is the game's name for the match rules: "Standard" or "Practice".
	Rules string `json:"rules"`
	// HostAccountID holds the practice host, or nothing: the assignment's
	// dialect writes an optional value as an array of at most one.
	HostAccountID []string              `json:"hostAccountId"`
	Participants  []AssignedParticipant `json:"participants"`
}

// AssignedParticipant is one roster entry in an Assignment.
type AssignedParticipant struct {
	AccountID   string `json:"accountId"`
	DisplayName string `json:"displayName"`
	Side        Side   `json:"side"`
	TicketHash  string `json:"ticketHash"`
	VanguardID  string `json:"vanguardId"`
}

// assignedRules maps rules to the names the game's schema uses.
var assignedRules = map[Rules]string{RulesStandard: "Standard", RulesPractice: "Practice"}

// BuildAssignment returns the assignment for a match as one line of JSON,
// ending in a newline. The match must still hold its join key.
func BuildAssignment(m Match, serverCredential, backendURL string) ([]byte, error) {
	if len(m.JoinKey) == 0 {
		return nil, errors.New("match has no join key")
	}
	rules, ok := assignedRules[m.Rules]
	if !ok {
		return nil, fmt.Errorf("match has unknown rules %q", m.Rules)
	}
	a := Assignment{
		SchemaVersion:    AssignmentSchemaVersion,
		MatchID:          m.ID,
		BackendURL:       backendURL,
		ServerCredential: serverCredential,
		Mode:             m.Mode,
		Rules:            rules,
		HostAccountID:    []string{},
	}
	if m.HostAccountID != "" {
		a.HostAccountID = append(a.HostAccountID, m.HostAccountID)
	}
	for _, p := range m.Participants {
		a.Participants = append(a.Participants, AssignedParticipant{
			AccountID:   p.AccountID,
			DisplayName: p.DisplayName,
			Side:        p.Side,
			TicketHash:  TicketHash(DeriveTicket(m.JoinKey, m.ID, p.AccountID)),
			VanguardID:  p.VanguardID,
		})
	}
	line, err := json.Marshal(a)
	if err != nil {
		return nil, err
	}
	return append(line, '\n'), nil
}
