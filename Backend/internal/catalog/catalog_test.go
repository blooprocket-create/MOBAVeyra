package catalog

import (
	"encoding/json"
	"os"
	"path/filepath"
	"slices"
	"sort"
	"testing"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/config"
)

func TestTheStandInOffersEveryReleasedVanguardUntilAFullRotation(t *testing.T) {
	few := New(Settings{Released: []string{"cairn", "oriel"}, Starters: []string{"cairn"}, RotationSlots: 3, StandIn: StandInAllReleased})
	if got := few.Rotation(); !slices.Equal(got, []string{"cairn", "oriel"}) {
		t.Fatalf("fewer than a rotation: %v", got)
	}
	full := New(Settings{Released: []string{"a", "b", "c"}, RotationSlots: 3, StandIn: StandInAllReleased})
	if got := full.Rotation(); len(got) != 0 {
		t.Fatalf("a full rotation's worth must not be stood in for: %v", got)
	}
	none := New(Settings{Released: []string{"cairn"}, RotationSlots: 3, StandIn: StandInNone})
	if got := none.Rotation(); len(got) != 0 {
		t.Fatalf("no stand-in: %v", got)
	}
}

func TestReleasedAndStarters(t *testing.T) {
	c := New(Settings{Released: []string{"cairn", "oriel"}, Starters: []string{"oriel"}, RotationSlots: 12, StandIn: StandInNone})
	if !c.IsReleased("cairn") || c.IsReleased("test_vanguard") || !c.IsStarter("oriel") || c.IsStarter("cairn") {
		t.Fatal("wrong answers")
	}
	starters := c.Starters()
	starters[0] = "changed"
	if c.Starters()[0] != "oriel" {
		t.Fatal("callers must not change the catalog")
	}
}

// The committed configuration's released Vanguards are exactly the Playable
// ones Game/Tuning/Vanguards.json defines (ADR-010 §6), so the backend never
// offers a Vanguard the game cannot host or hides one it can.
func TestCatalogMatchesVanguardsJSON(t *testing.T) {
	cfg, err := config.Load(filepath.Join("..", "..", "config", "local.json"))
	if err != nil {
		t.Fatalf("load config: %v", err)
	}
	raw, err := os.ReadFile(filepath.Join("..", "..", "..", "Game", "Tuning", "Vanguards.json"))
	if err != nil {
		t.Fatalf("read the game's Vanguards.json: %v", err)
	}
	var tuning struct {
		Vanguards map[string]struct {
			Availability string `json:"availability"`
		} `json:"vanguards"`
	}
	if err := json.Unmarshal(raw, &tuning); err != nil {
		t.Fatalf("parse Vanguards.json: %v", err)
	}
	var playable []string
	for id, v := range tuning.Vanguards {
		if v.Availability == "Playable" {
			playable = append(playable, id)
		}
	}
	released := append([]string(nil), cfg.Vanguards.Released...)
	sort.Strings(playable)
	sort.Strings(released)
	if !slices.Equal(released, playable) {
		t.Fatalf("config vanguards.released %v, but Vanguards.json's Playable Vanguards are %v", released, playable)
	}
}
