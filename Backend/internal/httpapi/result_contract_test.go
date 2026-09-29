package httpapi

import (
	"bytes"
	"encoding/json"
	"os"
	"path/filepath"
	"reflect"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// The game's result body (ADR-007 §7, ADR-017 §5), as its match server builds
// it for a fixed result: its test writes and checks this file, and this one
// reads it the way the result route does, so the two sides agree on every
// field. After an intended change to the body, rewrite the file from the game:
// run its test Veyra.Services.ResultContract with VEYRA_UPDATE_CONTRACT=1.
var resultContractPath = filepath.Join("..", "..", "..", "Game", "Source", "VeyraDeveloper", "TestData", "MatchResult.example.json")

// contractMatch is the match the game's fixed result ends: two rostered
// players and a bot, as the game's test builds them.
func contractMatch() match.Match {
	return match.Match{ID: "c", Rules: match.RulesStandard, State: match.Ready,
		Participants: []match.Participant{
			{AccountID: "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee", DisplayName: "DevOne", Side: match.SideA, VanguardID: "cairn"},
			{AccountID: "bbbbbbbb-cccc-4ddd-8eee-ffffffffffff", DisplayName: "DevTwo", Side: match.SideB, VanguardID: "oriel"},
		},
		Bots: []match.Bot{{Side: match.SideB, VanguardID: "bryn", Difficulty: match.BotBeginner}},
	}
}

func TestTheGamesResultBodyIsAccepted(t *testing.T) {
	body, err := os.ReadFile(resultContractPath)
	if err != nil {
		t.Fatalf("read the game's result body: %v", err)
	}
	// Strictly, as the result route decodes: every field the game sends is known here.
	dec := json.NewDecoder(bytes.NewReader(body))
	dec.DisallowUnknownFields()
	var req resultJSON
	if err := dec.Decode(&req); err != nil {
		t.Fatalf("the game's result body does not decode: %v", err)
	}
	players, ok := playersFrom(req.Players)
	if !ok || len(players) != 3 {
		t.Fatalf("the game's scoreboard: %v %+v", ok, req.Players)
	}
	if len(req.Wells) != 2 || req.Wells[1] != (match.WellCapture{Site: 1, Side: match.SideB, AtSeconds: 905.25}) {
		t.Fatalf("the game's Flux Well captures: %+v", req.Wells)
	}
	r := match.Result{EndReason: match.EndReason(req.EndReason), DurationSeconds: req.DurationSeconds, Players: players, Wells: req.Wells}
	if req.Winner != nil {
		r.Winner = match.Side(*req.Winner)
	}
	for _, p := range req.Participants {
		r.Participants = append(r.Participants, match.ParticipantResult(p))
	}
	m := contractMatch()
	if err := m.End(r, time.Now()); err != nil {
		t.Fatalf("the game's result does not fit its match: %v", err)
	}
	// The game's fixture sets every statistic, so one left at zero was sent
	// under a name this side does not read.
	for _, p := range players {
		if zero := zeroFields(reflect.ValueOf(p.Statistics), "statistics"); len(zero) > 0 {
			t.Fatalf("%s: statistics the game sent were not read: %v", p.Name, zero)
		}
	}
	if players[2].AccountID != "" || players[0].Items[0] == "" || players[0].FluxSpells[1] == "" {
		t.Fatalf("the game's lines: %+v", players)
	}
}

// zeroFields names every number in v, a struct of numbers and structs, that is zero.
func zeroFields(v reflect.Value, path string) []string {
	var zero []string
	for i := 0; i < v.NumField(); i++ {
		f, name := v.Field(i), path+"."+v.Type().Field(i).Name
		switch f.Kind() {
		case reflect.Struct:
			zero = append(zero, zeroFields(f, name)...)
		default:
			if f.IsZero() {
				zero = append(zero, name)
			}
		}
	}
	return zero
}

// TestAnEmptyScoreboardIsNone: a server that recorded nobody sends no
// scoreboard, and an empty one reads the same (ADR-017 §5).
func TestAnEmptyScoreboardIsNone(t *testing.T) {
	if players, ok := playersFrom([]scoreboardLineJSON{}); !ok || players != nil {
		t.Fatalf("an empty scoreboard: want none, got %v %v", players, ok)
	}
}
