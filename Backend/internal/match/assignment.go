package match

import (
	"encoding/json"
	"errors"
)

// AssignmentSchemaVersion is the version of the assignment document this
// backend writes. The match server reads exactly one version (ADR-007 §5).
const AssignmentSchemaVersion = 1

// Assignment is what a match server receives on its standard input when it
// starts: its match, where to report, its credential and its roster with a
// hash of each participant's join ticket.
type Assignment struct {
	SchemaVersion    int                   `json:"schemaVersion"`
	MatchID          string                `json:"matchId"`
	BackendURL       string                `json:"backendUrl"`
	ServerCredential string                `json:"serverCredential"`
	Participants     []AssignedParticipant `json:"participants"`
}

// AssignedParticipant is one roster entry in an Assignment.
type AssignedParticipant struct {
	AccountID   string `json:"accountId"`
	DisplayName string `json:"displayName"`
	Side        Side   `json:"side"`
	TicketHash  string `json:"ticketHash"`
}

// BuildAssignment returns the assignment for a match as one line of JSON,
// ending in a newline. The match must still hold its join key.
func BuildAssignment(m Match, serverCredential, backendURL string) ([]byte, error) {
	if len(m.JoinKey) == 0 {
		return nil, errors.New("match has no join key")
	}
	a := Assignment{
		SchemaVersion:    AssignmentSchemaVersion,
		MatchID:          m.ID,
		BackendURL:       backendURL,
		ServerCredential: serverCredential,
	}
	for _, p := range m.Participants {
		a.Participants = append(a.Participants, AssignedParticipant{
			AccountID:   p.AccountID,
			DisplayName: p.DisplayName,
			Side:        p.Side,
			TicketHash:  TicketHash(DeriveTicket(m.JoinKey, m.ID, p.AccountID)),
		})
	}
	line, err := json.Marshal(a)
	if err != nil {
		return nil, err
	}
	return append(line, '\n'), nil
}
