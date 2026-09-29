package match

import (
	"errors"
	"math"
	"testing"
)

// scoredMatch is a ready match with a human on each side and a bot on side B.
func scoredMatch() Match {
	m := readyMatch()
	m.Bots = []Bot{{Side: SideB, VanguardID: "oriel", Difficulty: BotBeginner}}
	return m
}

// scoreboard is a valid scoreboard for scoredMatch: its two players and its bot.
func scoreboard() []PlayerResult {
	line := func(side Side, name, account, vanguard string) PlayerResult {
		return PlayerResult{Side: side, Name: name, AccountID: account, VanguardID: vanguard,
			Statistics: PlayerStatistics{Kills: 2, Deaths: 1, Assists: 3, Level: 9, VanguardDamage: 4210.5,
				DamageDealt:  DamageByType{Physical: 9000, Magic: 120.25, True: 40},
				CrowdControl: CrowdControl{Stun: 1.5, Slow: 3.25}, GoldEarned: 5321.5,
				GoldBySource: GoldBySource{Starting: 500, Kills: 600, Minions: 3000, Passive: 1221.5}, MinionKills: 120, TowerDamage: 1800},
			Items: []string{"timing_coil", "", "", "", "", ""}, FluxSpells: [2]string{"blink", ""}}
	}
	return []PlayerResult{line(SideA, "P", "a", "cairn"), line(SideB, "P", "b", "cairn"), line(SideB, "Bot 1", "", "oriel")}
}

func TestAScoreboardIsKeptWithTheResult(t *testing.T) {
	m := scoredMatch()
	r := resultFor(m)
	r.Players = scoreboard()
	if err := m.End(r, t0); err != nil {
		t.Fatalf("End: %v", err)
	}
	if !samePlayers(m.Result.Players, scoreboard()) {
		t.Fatalf("the stored scoreboard differs: %+v", m.Result.Players)
	}
	// The stored scoreboard is a copy: changing the reported one changes nothing.
	r.Players[0].Items[0] = "other"
	if m.Result.Players[0].Items[0] != "timing_coil" {
		t.Fatal("the stored scoreboard shares the report's slices")
	}
}

func TestAResultMayComeWithoutAScoreboard(t *testing.T) {
	m := scoredMatch()
	if err := m.End(resultFor(m), t0); err != nil || m.Result.Players != nil {
		t.Fatalf("an older server's result: %v %+v", err, m.Result)
	}
}

func TestAReplayedScoreboardIsTheSameResult(t *testing.T) {
	m := scoredMatch()
	r := resultFor(m)
	r.Players = scoreboard()
	if err := m.End(r, t0); err != nil {
		t.Fatalf("End: %v", err)
	}
	again := resultFor(m)
	again.Players = scoreboard()
	if err := m.End(again, t0); err != nil {
		t.Fatalf("the same scoreboard again: %v", err)
	}
	different := resultFor(m)
	different.Players = scoreboard()
	different.Players[2].Statistics.Kills++
	if err := m.End(different, t0); !errors.Is(err, ErrResultConflict) {
		t.Fatalf("a different scoreboard: want a conflict, got %v", err)
	}
	if err := m.End(resultFor(m), t0); !errors.Is(err, ErrResultConflict) {
		t.Fatalf("the same result without its scoreboard: want a conflict, got %v", err)
	}
}

func TestTheScoreboardMustFitTheMatch(t *testing.T) {
	cases := map[string]func([]PlayerResult) []PlayerResult{
		"no such side":          func(p []PlayerResult) []PlayerResult { p[0].Side = "C"; return p },
		"no name":               func(p []PlayerResult) []PlayerResult { p[0].Name = " "; return p },
		"not a Vanguard ID":     func(p []PlayerResult) []PlayerResult { p[0].VanguardID = "Cairn!"; return p },
		"a stranger":            func(p []PlayerResult) []PlayerResult { p[0].AccountID = "z"; return p },
		"an account twice":      func(p []PlayerResult) []PlayerResult { p[1].AccountID = "a"; p[1].Side = SideA; return p },
		"the wrong side":        func(p []PlayerResult) []PlayerResult { p[0].Side = SideB; return p },
		"another Vanguard":      func(p []PlayerResult) []PlayerResult { p[0].VanguardID = "bryn"; return p },
		"a bot the match lacks": func(p []PlayerResult) []PlayerResult { return append(p, p[2]) },
		"a bot on another side": func(p []PlayerResult) []PlayerResult { p[2].Side = SideA; return p },
		"a negative count":      func(p []PlayerResult) []PlayerResult { p[0].Statistics.Deaths = -1; return p },
		"a negative amount":     func(p []PlayerResult) []PlayerResult { p[0].Statistics.GoldBySource.Wards = -1; return p },
		"an infinite amount":    func(p []PlayerResult) []PlayerResult { p[0].Statistics.CrowdControl.Stun = math.Inf(1); return p },
		"a NaN amount":          func(p []PlayerResult) []PlayerResult { p[0].Statistics.DamageTaken.Magic = math.NaN(); return p },
		"not an item ID":        func(p []PlayerResult) []PlayerResult { p[0].Items[1] = "a b"; return p },
		"a spell in both slots": func(p []PlayerResult) []PlayerResult { p[0].FluxSpells = [2]string{"blink", "blink"}; return p },
		"not a spell ID":        func(p []PlayerResult) []PlayerResult { p[0].FluxSpells[1] = "Mend"; return p },
		"no Vanguard":           func(p []PlayerResult) []PlayerResult { p[1].VanguardID = ""; return p },
	}
	for name, change := range cases {
		t.Run(name, func(t *testing.T) {
			m := scoredMatch()
			r := resultFor(m)
			r.Players = change(scoreboard())
			if err := m.End(r, t0); !errors.Is(err, ErrInvalidResult) {
				t.Fatalf("want ErrInvalidResult, got %v", err)
			}
			if m.State != Ready {
				t.Fatal("a rejected result must change nothing")
			}
		})
	}
}

func TestAScoreboardMayLackThoseWhoNeverPlayed(t *testing.T) {
	// A rostered player who never joined, and a bot that could not be seated, have no line.
	m := scoredMatch()
	r := resultFor(m)
	r.Players = scoreboard()[:1]
	if err := m.End(r, t0); err != nil {
		t.Fatalf("End: %v", err)
	}
}
