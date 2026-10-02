package match

import (
	"encoding/json"
	"errors"
	"fmt"
)

// AssignmentSchemaVersion is the version of the assignment document this
// backend writes. The match server reads exactly one version (ADR-007 §5).
// Version 2 adds the mode, the rules, the practice host, each participant's
// Vanguard and the bots (ADR-010 §7, §9); version 3 adds each bot's
// difficulty (ADR-013 §6); version 4 adds each participant's starting Flux
// Spells (ADR-015 §5); version 5 adds custom rules, bots for every hosted
// match and the custom session's settings (ADR-021 §3); version 6 adds each
// participant's Mastery Level and emote tier for the Vanguard they play
// (ADR-045 §9).
const AssignmentSchemaVersion = 6

// Assignment is what a match server receives on its standard input when it
// starts: its match, where to report, its credential, the rules it plays by
// and its roster with a hash of each participant's join ticket.
type Assignment struct {
	SchemaVersion    int    `json:"schemaVersion"`
	MatchID          string `json:"matchId"`
	BackendURL       string `json:"backendUrl"`
	ServerCredential string `json:"serverCredential"`
	Mode             string `json:"mode"`
	// Rules is the game's name for the match rules: "Standard", "Practice" or
	// "Custom".
	Rules string `json:"rules"`
	// HostAccountID holds the practice host, or nothing: the assignment's
	// dialect writes an optional value as an array of at most one.
	HostAccountID []string              `json:"hostAccountId"`
	Participants  []AssignedParticipant `json:"participants"`
	// Bots are the AI participants the server adds when the match starts;
	// always a list, empty for a standard match.
	Bots []AssignedBot `json:"bots"`
	// Settings holds a custom match's session rules, or nothing: at most one.
	Settings []AssignedSettings `json:"settings"`
}

// AssignedSettings are a custom match's session rules in an Assignment.
type AssignedSettings struct {
	// Victory is "Enabled" or "Disabled": the assignment's dialect has no
	// booleans.
	Victory string `json:"victory"`
	// StartingGold holds the session's starting Gold, or nothing for the
	// game's own: at most one.
	StartingGold []float64 `json:"startingGold"`
}

// AssignedBot is one AI participant in an Assignment.
type AssignedBot struct {
	Side       Side   `json:"side"`
	VanguardID string `json:"vanguardId"`
	// Difficulty is the game's name for the bot's difficulty: "Beginner" or
	// "Intermediate".
	Difficulty string `json:"difficulty"`
}

// AssignedParticipant is one roster entry in an Assignment.
type AssignedParticipant struct {
	AccountID   string `json:"accountId"`
	DisplayName string `json:"displayName"`
	Side        Side   `json:"side"`
	TicketHash  string `json:"ticketHash"`
	VanguardID  string `json:"vanguardId"`
	// FluxSpells are the participant's starting Flux Spells, two in slot
	// order, "" for an empty slot.
	FluxSpells []string `json:"fluxSpells"`
	// MasteryLevel and EmoteTier are the account's Mastery of the Vanguard it
	// plays, which its mastery emote shows (ADR-045 §9); 0 when unknown.
	MasteryLevel int `json:"masteryLevel"`
	EmoteTier    int `json:"emoteTier"`
}

// ParticipantMastery is an account's Mastery of the Vanguard it plays, as the
// progression domain reports it for the assignment.
type ParticipantMastery struct {
	Level     int
	EmoteTier int
}

// assignedRules maps rules to the names the game's schema uses.
var assignedRules = map[Rules]string{RulesStandard: "Standard", RulesPractice: "Practice", RulesCustom: "Custom"}

// assignedDifficulties maps bot difficulties to the names the game's schema uses.
var assignedDifficulties = map[BotDifficulty]string{BotBeginner: "Beginner", BotIntermediate: "Intermediate"}

// BuildAssignment returns the assignment for a match as one line of JSON,
// ending in a newline. The match must still hold its join key. masteries
// holds each participant's Mastery by account ID; one absent is 0.
func BuildAssignment(m Match, serverCredential, backendURL string, masteries map[string]ParticipantMastery) ([]byte, error) {
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
		Bots:             []AssignedBot{},
		Settings:         []AssignedSettings{},
	}
	if (m.Rules == RulesCustom) != (m.Custom != nil) {
		return nil, fmt.Errorf("match %s has rules %q and custom settings %v", m.ID, m.Rules, m.Custom != nil)
	}
	if m.Custom != nil {
		settings := AssignedSettings{Victory: "Disabled", StartingGold: []float64{}}
		if m.Custom.VictoryEnabled {
			settings.Victory = "Enabled"
		}
		if m.Custom.StartingGold != nil {
			settings.StartingGold = append(settings.StartingGold, *m.Custom.StartingGold)
		}
		a.Settings = append(a.Settings, settings)
	}
	if m.HostAccountID != "" {
		a.HostAccountID = append(a.HostAccountID, m.HostAccountID)
	}
	for _, p := range m.Participants {
		a.Participants = append(a.Participants, AssignedParticipant{
			AccountID:    p.AccountID,
			DisplayName:  p.DisplayName,
			Side:         p.Side,
			TicketHash:   TicketHash(DeriveTicket(m.JoinKey, m.ID, p.AccountID)),
			VanguardID:   p.VanguardID,
			FluxSpells:   append([]string(nil), p.FluxSpells[:]...),
			MasteryLevel: masteries[p.AccountID].Level,
			EmoteTier:    masteries[p.AccountID].EmoteTier,
		})
	}
	for _, b := range m.Bots {
		difficulty, ok := assignedDifficulties[b.Difficulty]
		if !ok {
			return nil, fmt.Errorf("match has a bot of unknown difficulty %q", b.Difficulty)
		}
		a.Bots = append(a.Bots, AssignedBot{Side: b.Side, VanguardID: b.VanguardID, Difficulty: difficulty})
	}
	line, err := json.Marshal(a)
	if err != nil {
		return nil, err
	}
	return append(line, '\n'), nil
}
