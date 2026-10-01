package catalog

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"slices"
	"sort"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/config"
)

// Fixture values: twenty Vanguards, a week from a Monday epoch, and a seed.
var (
	epoch    = time.Date(2026, 9, 28, 0, 0, 0, 0, time.UTC)
	week     = 7 * 24 * time.Hour
	released = func() []string {
		ids := make([]string, 20)
		for i := range ids {
			ids[i] = fmt.Sprintf("v%02d", i)
		}
		return ids
	}()
)

func rotating(slots int, releases map[string]time.Time) *Catalog {
	return New(Settings{Released: released, Rotation: RotationSettings{Slots: slots, Epoch: epoch, Week: week, Seed: "test", Releases: releases}},
		func() time.Time { return epoch })
}

func weekStart(w int) time.Time { return epoch.Add(time.Duration(w) * week) }

func TestEachWeekOffersItsSlotsDistinctAndReleasedInDisplayOrder(t *testing.T) {
	c := rotating(12, nil)
	for w := 0; w < 10; w++ {
		got := c.RotationAt(weekStart(w).Add(time.Hour))
		if len(got) != 12 {
			t.Fatalf("week %d: %d Vanguards %v", w, len(got), got)
		}
		seen := map[string]bool{}
		last := -1
		for _, id := range got {
			at := slices.Index(released, id)
			if seen[id] || at < 0 || at <= last {
				t.Fatalf("week %d: not distinct, released and in display order: %v", w, got)
			}
			seen[id] = true
			last = at
		}
	}
}

func TestTheSameSeedAndWeekGiveTheSameRotationAndTheWeekHoldsAllWeek(t *testing.T) {
	a, b := rotating(12, nil), rotating(12, nil)
	for w := 0; w < 6; w++ {
		early, late := a.RotationAt(weekStart(w)), b.RotationAt(weekStart(w+1).Add(-time.Second))
		if !slices.Equal(early, late) {
			t.Fatalf("week %d differs within itself or across services: %v %v", w, early, late)
		}
	}
	other := New(Settings{Released: released, Rotation: RotationSettings{Slots: 12, Epoch: epoch, Week: week, Seed: "another"}}, time.Now)
	same := 0
	for w := 0; w < 6; w++ {
		if slices.Equal(other.RotationAt(weekStart(w)), a.RotationAt(weekStart(w))) {
			same++
		}
	}
	if same == 6 {
		t.Fatal("a different seed should draw differently")
	}
}

func TestLastWeeksVanguardsSitOutWhileEnoughOthersRemain(t *testing.T) {
	// Twenty released and eight slots: twelve others always remain.
	c := rotating(8, nil)
	for w := 1; w < 10; w++ {
		previous, current := c.RotationAt(weekStart(w-1)), c.RotationAt(weekStart(w))
		for _, id := range current {
			if slices.Contains(previous, id) {
				t.Fatalf("week %d repeats %s from the week before: %v after %v", w, id, current, previous)
			}
		}
	}
}

func TestTooFewOthersRelaxesTheNoRepeatRuleFirst(t *testing.T) {
	// Twenty released and twelve slots: eight others remain, so four of last week's return.
	c := rotating(12, nil)
	for w := 1; w < 6; w++ {
		previous, current := c.RotationAt(weekStart(w-1)), c.RotationAt(weekStart(w))
		repeats := 0
		for _, id := range current {
			if slices.Contains(previous, id) {
				repeats++
			}
		}
		if len(current) != 12 || repeats != 4 {
			t.Fatalf("week %d: %d Vanguards, %d repeats", w, len(current), repeats)
		}
	}
}

func TestANewVanguardWaitsAWeekAfterItsRelease(t *testing.T) {
	// v00 is released mid-week 2: it may first appear in week 4, the first that starts a week after.
	release := weekStart(2).Add(3 * 24 * time.Hour)
	c := rotating(19, map[string]time.Time{"v00": release})
	for w := 0; w < 4; w++ {
		if slices.Contains(c.RotationAt(weekStart(w)), "v00") {
			t.Fatalf("week %d offers v00 before a week has passed since its release", w)
		}
	}
	appeared := false
	for w := 4; w < 8; w++ {
		appeared = appeared || slices.Contains(c.RotationAt(weekStart(w)), "v00")
	}
	if !appeared {
		t.Fatal("v00 never joined the rotation once eligible")
	}
}

func TestFewerEligibleThanSlotsOffersNothingAndNothingBeforeTheEpoch(t *testing.T) {
	if got := rotating(21, nil).RotationAt(weekStart(0)); got != nil {
		t.Fatalf("twenty eligible for twenty-one slots: %v", got)
	}
	// Two waiting for their release week leave eighteen for nineteen slots.
	waiting := map[string]time.Time{"v00": weekStart(1), "v01": weekStart(1)}
	if got := rotating(19, waiting).RotationAt(weekStart(1)); got != nil {
		t.Fatalf("too few while new Vanguards wait: %v", got)
	}
	if got := rotating(12, nil).RotationAt(epoch.Add(-time.Second)); got != nil {
		t.Fatalf("before the epoch: %v", got)
	}
}

func TestReleasedAndStarters(t *testing.T) {
	c := New(Settings{Released: []string{"cairn", "oriel"}, Starters: []string{"oriel"}, Rotation: RotationSettings{Slots: 12, Epoch: epoch, Week: week}}, time.Now)
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
