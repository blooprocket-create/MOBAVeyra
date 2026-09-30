package match

import (
	"errors"
	"testing"
)

// fixtureCustom configures custom matches for the tests, independent of the
// committed config.
var fixtureCustom = CustomModeSettings{Enabled: true, Mode: "custom_game", PlayersPerSide: 2, StartingGoldMin: 0, StartingGoldMax: 20000}

func gold(g float64) *float64 { return &g }

// customSpec is a host on side A against a bot, with a friend beside the host.
func customSpec() Spec {
	return Spec{
		Mode:          fixtureCustom.Mode,
		Rules:         RulesCustom,
		HostAccountID: "acc-1",
		Seats: []Seat{
			{AccountID: "acc-1", Side: SideA, VanguardID: "cairn"},
			{AccountID: "acc-2", Side: SideA, VanguardID: "oriel"},
		},
		Bots:   []Bot{{Side: SideB, VanguardID: "cairn", Difficulty: BotIntermediate}, {Side: SideB, VanguardID: "bryn", Difficulty: BotBeginner}},
		Custom: &CustomSettings{VictoryEnabled: true, StartingGold: gold(3000)},
	}
}

func TestACustomMatchIsItsHumansItsBotsAndItsSettings(t *testing.T) {
	f := newFixture(t)
	m, err := f.svc.Create(ctx, customSpec())
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if m.Rules != RulesCustom || m.HostAccountID != "acc-1" || len(m.Bots) != 2 || m.Custom == nil || !m.Custom.VictoryEnabled || *m.Custom.StartingGold != 3000 {
		t.Fatalf("custom match: %+v", m)
	}
	a := f.assignment(t, m.ID)
	if a.Rules != "Custom" || len(a.HostAccountID) != 1 || len(a.Participants) != 2 {
		t.Fatalf("custom assignment: %+v", a)
	}
	want := []AssignedBot{{Side: SideB, VanguardID: "cairn", Difficulty: "Intermediate"}, {Side: SideB, VanguardID: "bryn", Difficulty: "Beginner"}}
	if len(a.Bots) != 2 || a.Bots[0] != want[0] || a.Bots[1] != want[1] {
		t.Fatalf("the host's bots, in seat order: %+v", a.Bots)
	}
	if len(a.Settings) != 1 || !a.Settings[0].VictoryEnabled || len(a.Settings[0].StartingGold) != 1 || a.Settings[0].StartingGold[0] != 3000 {
		t.Fatalf("custom settings: %+v", a.Settings)
	}
}

func TestOnlyACustomMatchCarriesSettings(t *testing.T) {
	f := newFixture(t)
	spec := customSpec()
	spec.Custom.StartingGold = nil
	m, err := f.svc.Create(ctx, spec)
	if err != nil {
		t.Fatal(err)
	}
	if a := f.assignment(t, m.ID); len(a.Settings) != 1 || a.Settings[0].StartingGold == nil || len(a.Settings[0].StartingGold) != 0 {
		t.Fatalf("no starting Gold is an empty list, the game's own: %+v", a.Settings)
	}
	standard := f.create(t, Seat{AccountID: "acc-3", Side: SideA, VanguardID: "bryn"})
	if a := f.assignment(t, standard.ID); a.Settings == nil || len(a.Settings) != 0 {
		t.Fatalf("a standard match has no settings, written as an empty list: %+v", a.Settings)
	}
}

func TestValidateCustom(t *testing.T) {
	cases := map[string]struct {
		change func(*Spec)
		want   error
	}{
		"as the host arranged it":  {func(*Spec) {}, nil},
		"disabled":                 {func(s *Spec) { s.Mode = "custom_practice" }, ErrUnknownMode},
		"no host":                  {func(s *Spec) { s.HostAccountID = "" }, ErrInvalidRoster},
		"a host who does not play": {func(s *Spec) { s.HostAccountID = "acc-3" }, ErrInvalidRoster},
		"no settings":              {func(s *Spec) { s.Custom = nil }, ErrInvalidRoster},
		"a side too full": {func(s *Spec) {
			s.Bots = append(s.Bots, Bot{Side: SideB, VanguardID: "qazharr", Difficulty: BotBeginner})
		}, ErrInvalidRoster},
		"a Vanguard twice on a side": {func(s *Spec) { s.Seats[1].VanguardID = "cairn" }, ErrInvalidRoster},
		"a bot twice on a side":      {func(s *Spec) { s.Bots[1].VanguardID = "cairn" }, ErrInvalidRoster},
		"an unknown difficulty":      {func(s *Spec) { s.Bots[0].Difficulty = "expert" }, ErrInvalidRoster},
		"a bot on no side":           {func(s *Spec) { s.Bots[0].Side = "C" }, ErrInvalidRoster},
		"Gold out of range":          {func(s *Spec) { s.Custom.StartingGold = gold(fixtureCustom.StartingGoldMax + 1) }, ErrInvalidRoster},
		"victory with no opponent":   {func(s *Spec) { s.Bots = nil }, ErrInvalidRoster},
		"open-ended with no opponent": {func(s *Spec) {
			s.Bots = nil
			s.Custom.VictoryEnabled = false
		}, nil},
		"a human without a Vanguard": {func(s *Spec) { s.Seats[1].VanguardID = "" }, ErrInvalidVanguard},
	}
	for name, tc := range cases {
		t.Run(name, func(t *testing.T) {
			spec := customSpec()
			tc.change(&spec)
			participants := make([]Participant, len(spec.Seats))
			for i, s := range spec.Seats {
				participants[i] = Participant{AccountID: s.AccountID, Side: s.Side, VanguardID: s.VanguardID}
			}
			if err := ValidateCustom(fixtureCustom, spec.Mode, spec.HostAccountID, participants, spec.Bots, spec.Custom); !errors.Is(err, tc.want) {
				t.Fatalf("want %v, got %v", tc.want, err)
			}
		})
	}
}

func TestACustomMatchIsWonOnlyWithVictoryOn(t *testing.T) {
	custom := readyMatch()
	custom.Rules, custom.HostAccountID = RulesCustom, custom.Participants[0].AccountID
	custom.Custom = &CustomSettings{VictoryEnabled: false}
	r := resultFor(custom)
	r.EndReason, r.Winner = EndPrimeWellDestroyed, SideA
	if err := custom.End(r, t0); !errors.Is(err, ErrInvalidResult) {
		t.Fatalf("victory off: want ErrInvalidResult, got %v", err)
	}
	remake := r
	remake.EndReason, remake.Winner = EndRemake, ""
	custom.Custom.VictoryEnabled = true
	if err := custom.End(remake, t0); !errors.Is(err, ErrInvalidResult) {
		t.Fatalf("remakes are for matchmade matches: got %v", err)
	}
	if err := custom.End(r, t0); err != nil || custom.Result.Winner != SideA {
		t.Fatalf("victory on: %v %+v", err, custom.Result)
	}

	hosted := readyMatch()
	hosted.Rules, hosted.HostAccountID = RulesCustom, hosted.Participants[0].AccountID
	hosted.Custom = &CustomSettings{VictoryEnabled: true}
	ended := resultFor(hosted)
	ended.EndReason = EndHostEnded
	if err := hosted.End(ended, t0); err != nil {
		t.Fatalf("the custom host may end any custom match: %v", err)
	}
}
