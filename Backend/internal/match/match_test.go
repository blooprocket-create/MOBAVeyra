package match

import (
	"errors"
	"math"
	"testing"
	"time"
)

var (
	t0      = time.Date(2026, 9, 26, 12, 0, 0, 0, time.UTC)
	fiveAll = Mode{ID: "casual_select", Enabled: true, HumanPlayersPerTeam: 5}
)

func roster(sides ...Side) []Participant {
	out := make([]Participant, len(sides))
	for i, s := range sides {
		out[i] = Participant{AccountID: string(rune('a' + i)), DisplayName: "P", Side: s, VanguardID: "cairn"}
	}
	return out
}

func TestValidateRoster(t *testing.T) {
	cases := map[string]struct {
		mode Mode
		ps   []Participant
		want error
	}{
		"one a side":        {fiveAll, roster(SideA, SideB), nil},
		"solo":              {fiveAll, roster(SideA), nil},
		"uneven":            {fiveAll, roster(SideA, SideA, SideB), nil},
		"empty":             {fiveAll, nil, ErrInvalidRoster},
		"disabled mode":     {Mode{ID: "ranked", HumanPlayersPerTeam: 5}, roster(SideA), ErrUnknownMode},
		"unknown side":      {fiveAll, roster(SideA, Side("C")), ErrInvalidRoster},
		"side too large":    {Mode{ID: "m", Enabled: true, HumanPlayersPerTeam: 1}, roster(SideA, SideA), ErrInvalidRoster},
		"duplicate account": {fiveAll, append(roster(SideA), Participant{AccountID: "a", Side: SideB}), ErrInvalidRoster},
		"blank account":     {fiveAll, []Participant{{Side: SideA}}, ErrInvalidRoster},
		"no Vanguard":       {fiveAll, []Participant{{AccountID: "a", Side: SideA}}, ErrInvalidVanguard},
		"bad Vanguard":      {fiveAll, []Participant{{AccountID: "a", Side: SideA, VanguardID: "not a content id"}}, ErrInvalidVanguard},
	}
	for name, tc := range cases {
		t.Run(name, func(t *testing.T) {
			if err := ValidateRoster(tc.mode, tc.ps); !errors.Is(err, tc.want) {
				t.Fatalf("want %v, got %v", tc.want, err)
			}
		})
	}
}

func TestValidatePractice(t *testing.T) {
	practice := PracticeSettings{Enabled: true, Mode: "custom_practice", HostSide: SideA}
	host := roster(SideA)
	cases := map[string]struct {
		settings PracticeSettings
		mode     string
		host     string
		ps       []Participant
		want     error
	}{
		"its host alone":   {practice, "custom_practice", "a", host, nil},
		"disabled":         {PracticeSettings{Mode: "custom_practice", HostSide: SideA}, "custom_practice", "a", host, ErrUnknownMode},
		"another mode":     {practice, "casual_select", "a", host, ErrUnknownMode},
		"no host":          {practice, "custom_practice", "", host, ErrInvalidRoster},
		"not the host":     {practice, "custom_practice", "b", host, ErrInvalidRoster},
		"two players":      {practice, "custom_practice", "a", roster(SideA, SideB), ErrInvalidRoster},
		"the other side":   {practice, "custom_practice", "a", roster(SideB), ErrInvalidRoster},
		"without Vanguard": {practice, "custom_practice", "a", []Participant{{AccountID: "a", Side: SideA}}, ErrInvalidVanguard},
	}
	for name, tc := range cases {
		t.Run(name, func(t *testing.T) {
			if err := ValidatePractice(tc.settings, tc.mode, tc.host, tc.ps); !errors.Is(err, tc.want) {
				t.Fatalf("want %v, got %v", tc.want, err)
			}
		})
	}
}

func readyMatch() Match {
	return Match{ID: "m", Rules: RulesStandard, State: Ready, Participants: roster(SideA, SideB), ReadyAt: t0, JoinKey: []byte("key")}
}

func TestHostEndedNeedsPracticeRules(t *testing.T) {
	m := readyMatch()
	r := resultFor(m)
	r.EndReason = EndHostEnded
	if err := m.End(r, t0); !errors.Is(err, ErrInvalidResult) {
		t.Fatalf("standard rules: want ErrInvalidResult, got %v", err)
	}
	m.Rules = RulesPractice
	if err := m.End(r, t0); err != nil || m.Result.EndReason != EndHostEnded {
		t.Fatalf("practice rules: %v %+v", err, m.Result)
	}
}

func TestSurrenderAndRemakeEndOnlyAStandardMatch(t *testing.T) {
	for _, c := range []struct {
		reason EndReason
		winner Side
	}{{EndSurrender, SideB}, {EndRemake, ""}} {
		practice := readyMatch()
		practice.Rules = RulesPractice
		r := resultFor(practice)
		r.EndReason, r.Winner = c.reason, c.winner
		if err := practice.End(r, t0); !errors.Is(err, ErrInvalidResult) {
			t.Fatalf("%s: practice takes no votes, got %v", c.reason, err)
		}
		m := readyMatch()
		r.Participants[0].PersonalLoss, r.Participants[0].AbsentSeconds = true, 30
		if err := m.End(r, t0); err != nil || m.Result.EndReason != c.reason || m.Result.Winner != c.winner || !m.Result.Participants[0].PersonalLoss {
			t.Fatalf("%s: %v %+v", c.reason, err, m.Result)
		}
	}
}

func TestPrimeWellDestroyedNeedsAWinnerAndStandardRules(t *testing.T) {
	m := readyMatch()
	r := resultFor(m)
	r.EndReason = EndPrimeWellDestroyed
	if err := m.End(r, t0); !errors.Is(err, ErrInvalidResult) {
		t.Fatalf("no winner: want ErrInvalidResult, got %v", err)
	}
	r.Winner = SideB
	practice := readyMatch()
	practice.Rules = RulesPractice
	if err := practice.End(r, t0); !errors.Is(err, ErrInvalidResult) {
		t.Fatalf("practice has no victory: want ErrInvalidResult, got %v", err)
	}
	if err := m.End(r, t0); err != nil || m.Result.EndReason != EndPrimeWellDestroyed || m.Result.Winner != SideB {
		t.Fatalf("standard rules: %v %+v", err, m.Result)
	}
}

func resultFor(m Match) Result {
	r := Result{EndReason: EndDeveloperRequest, DurationSeconds: 42}
	for _, p := range m.Participants {
		r.Participants = append(r.Participants, ParticipantResult{AccountID: p.AccountID, Joined: true, ConnectedAtEnd: true})
	}
	return r
}

func TestMarkReady(t *testing.T) {
	m := Match{State: Allocating}
	if err := m.MarkReady(t0); err != nil || m.State != Ready || !m.ReadyAt.Equal(t0) {
		t.Fatalf("MarkReady: %v %+v", err, m)
	}
	if err := m.MarkReady(t0.Add(time.Minute)); err != nil || !m.ReadyAt.Equal(t0) {
		t.Fatalf("a repeated ready must change nothing: %v %+v", err, m)
	}
	m.State = Ended
	if err := m.MarkReady(t0); !errors.Is(err, ErrInvalidState) {
		t.Fatalf("an ended match cannot become ready: %v", err)
	}
}

func TestEndRecordsTheResultAndErasesTheKey(t *testing.T) {
	m := readyMatch()
	if err := m.End(resultFor(m), t0.Add(time.Minute)); err != nil {
		t.Fatalf("End: %v", err)
	}
	if m.State != Ended || m.JoinKey != nil || m.Result == nil || !m.EndedAt.Equal(t0.Add(time.Minute)) {
		t.Fatalf("not ended properly: %+v", m)
	}
}

func TestEndIsIdempotentAndRejectsAConflict(t *testing.T) {
	m := readyMatch()
	r := resultFor(m)
	if err := m.End(r, t0); err != nil {
		t.Fatalf("End: %v", err)
	}
	reordered := r
	reordered.Participants = []ParticipantResult{r.Participants[1], r.Participants[0]}
	if err := m.End(reordered, t0.Add(time.Second)); err != nil {
		t.Fatalf("the same result again must be accepted: %v", err)
	}
	if !m.EndedAt.Equal(t0) {
		t.Fatal("a replayed result must change nothing")
	}
	different := resultFor(m)
	different.DurationSeconds = 43
	if err := m.End(different, t0); !errors.Is(err, ErrResultConflict) {
		t.Fatalf("want conflict, got %v", err)
	}
}

func TestEndRejectsInvalidResults(t *testing.T) {
	base := readyMatch()
	cases := map[string]func(*Result){
		"unknown reason":       func(r *Result) { r.EndReason = "forfeit" },
		"surrender, no winner": func(r *Result) { r.EndReason = EndSurrender },
		"remake with winner":   func(r *Result) { r.EndReason, r.Winner = EndRemake, SideA },
		"negative absence":     func(r *Result) { r.Participants[0].AbsentSeconds = -1 },
		"absence past the end": func(r *Result) { r.Participants[0].AbsentSeconds = r.DurationSeconds + 1 },
		"NaN absence":          func(r *Result) { r.Participants[0].AbsentSeconds = math.NaN() },
		"loss, never joined": func(r *Result) {
			r.Participants[1].Joined, r.Participants[1].ConnectedAtEnd, r.Participants[1].PersonalLoss = false, false, true
		},
		"unknown winner":     func(r *Result) { r.Winner = "C" },
		"winner, no victory": func(r *Result) { r.Winner = SideA },
		"negative duration":  func(r *Result) { r.DurationSeconds = -1 },
		"NaN duration":       func(r *Result) { r.DurationSeconds = math.NaN() },
		"missing player":     func(r *Result) { r.Participants = r.Participants[:1] },
		"stranger":           func(r *Result) { r.Participants[1].AccountID = "z" },
		"duplicate":          func(r *Result) { r.Participants[1].AccountID = r.Participants[0].AccountID },
		"connected unjoined": func(r *Result) { r.Participants[0].Joined = false },
	}
	for name, change := range cases {
		t.Run(name, func(t *testing.T) {
			m := readyMatch()
			r := resultFor(base)
			change(&r)
			if err := m.End(r, t0); !errors.Is(err, ErrInvalidResult) {
				t.Fatalf("want ErrInvalidResult, got %v", err)
			}
			if m.State != Ready {
				t.Fatal("a rejected result must change nothing")
			}
		})
	}
}

func TestEndNeedsAReadyMatch(t *testing.T) {
	m := readyMatch()
	m.State = Allocating
	if err := m.End(resultFor(m), t0); !errors.Is(err, ErrInvalidState) {
		t.Fatalf("want ErrInvalidState, got %v", err)
	}
}

func TestFail(t *testing.T) {
	m := readyMatch()
	m.Fail(FailServerExited, t0)
	if m.State != Failed || m.FailureReason != FailServerExited || m.JoinKey != nil {
		t.Fatalf("not failed properly: %+v", m)
	}
	m.Fail(FailMaxDuration, t0.Add(time.Hour))
	if m.FailureReason != FailServerExited || !m.EndedAt.Equal(t0) {
		t.Fatal("failing a finished match must change nothing")
	}
}

func TestOnlyACoopModesStandardMatchesCarryTheirEnemyTeam(t *testing.T) {
	coop := Mode{ID: "coop_beginner", Enabled: true, HumanPlayersPerTeam: 1, AIPerTeam: 2, AIDifficulty: BotBeginner}
	human := []Participant{{AccountID: "acc-1", Side: SideA, VanguardID: "cairn"}}
	team := func(side Side, difficulty BotDifficulty, ids ...string) []Bot {
		out := make([]Bot, len(ids))
		for i, id := range ids {
			out[i] = Bot{Side: side, VanguardID: id, Difficulty: difficulty}
		}
		return out
	}
	if err := ValidateOpponents(coop, human, team(SideB, BotBeginner, "cairn", "oriel")); err != nil {
		t.Fatalf("a co-op match's enemy team, mirroring its human: %v", err)
	}
	for name, bots := range map[string][]Bot{
		"too few":             team(SideB, BotBeginner, "cairn"),
		"on the human's side": team(SideA, BotBeginner, "cairn", "oriel"),
		"another difficulty":  team(SideB, BotIntermediate, "cairn", "oriel"),
		"a Vanguard twice":    team(SideB, BotBeginner, "oriel", "oriel"),
		"split across sides":  {{Side: SideA, VanguardID: "cairn", Difficulty: BotBeginner}, {Side: SideB, VanguardID: "oriel", Difficulty: BotBeginner}},
	} {
		if err := ValidateOpponents(coop, human, bots); !errors.Is(err, ErrInvalidRoster) {
			t.Fatalf("%s: %v", name, err)
		}
	}
	casual := Mode{ID: "casual_select", Enabled: true, HumanPlayersPerTeam: 1}
	if err := ValidateOpponents(casual, human, team(SideB, BotBeginner, "cairn")); !errors.Is(err, ErrInvalidRoster) {
		t.Fatalf("a PvP match never carries bots: %v", err)
	}
	if err := ValidateOpponents(casual, human, nil); err != nil {
		t.Fatalf("a PvP match without bots: %v", err)
	}
}
