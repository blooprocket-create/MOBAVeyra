package progression

import (
	"slices"
	"testing"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// Fixture tuning, independent of the shipped configuration.
var testCurve = Curve{First: 100, Growth: 10, GrowthUntil: 4}

func TestACurveGrowsThenHoldsConstant(t *testing.T) {
	for level, want := range map[int]int64{1: 100, 2: 110, 3: 120, 4: 130, 5: 130, 500: 130, 0: 100} {
		if got := testCurve.Need(level); got != want {
			t.Errorf("Need(%d) = %d, want %d", level, got, want)
		}
	}
}

func TestAddingCrossesEveryLevelOnTheWay(t *testing.T) {
	// 100 + 110 + 120 = 330 takes level 1 to 4, and 25 more is into level 4.
	got, reached := testCurve.Add(Standing{Level: 1, Into: 0}, 355)
	if got != (Standing{Level: 4, Into: 25}) || !slices.Equal(reached, []int{2, 3, 4}) {
		t.Fatalf("Add = %+v reaching %v", got, reached)
	}
	// Progress carries over, and a gain short of the next level reaches none.
	got, reached = testCurve.Add(Standing{Level: 2, Into: 100}, 5)
	if got != (Standing{Level: 2, Into: 105}) || reached != nil {
		t.Fatalf("Add = %+v reaching %v", got, reached)
	}
	// Levels have no ceiling: a huge gain keeps climbing at the constant need.
	got, _ = testCurve.Add(Standing{Level: 4}, 130*1000)
	if got.Level != 1004 || got.Into != 0 {
		t.Fatalf("Add = %+v", got)
	}
}

func TestRefinedFluxComesAtMilestonesThenEveryFewLevels(t *testing.T) {
	r := RefinedFlux{Milestones: []Milestone{{30, 250}, {50, 400}, {75, 500}, {100, 750}}, EveryLevels: 25, Amount: 500}
	for level, want := range map[int]int64{29: 0, 30: 250, 50: 400, 75: 500, 100: 750, 101: 0, 125: 500, 150: 500, 140: 0} {
		if got := r.At(level); got != want {
			t.Errorf("At(%d) = %d, want %d", level, got, want)
		}
	}
	if (RefinedFlux{}).At(30) != 0 {
		t.Error("no milestones award nothing")
	}
}

func TestLevelUpsAwardFluxEachAndRefinedFluxAtMilestones(t *testing.T) {
	tuning := Tuning{FluxPerLevel: 400, RefinedFlux: RefinedFlux{Milestones: []Milestone{{30, 250}}, EveryLevels: 25, Amount: 500}}
	flux, refined := LevelUpRewards(tuning, []int{29, 30, 31})
	if flux != 1200 || refined != 250 {
		t.Fatalf("flux %d, refined %d", flux, refined)
	}
}

func standardMatch(reason match.EndReason) *match.Match {
	return &match.Match{Rules: match.RulesStandard, Result: &match.Result{EndReason: reason, DurationSeconds: 1800}}
}

func TestEligibilityFollowsTheBible(t *testing.T) {
	xpTuning := AccountXP{PerMinute: 6, WinBonus: 30, CoopBelowLevel: 10}
	joined := match.ParticipantResult{Joined: true, ConnectedAtEnd: true}
	cases := []struct {
		name     string
		m        *match.Match
		category string
		p        match.ParticipantResult
		level    int
		xp, mast bool
		reason   Reason
	}{
		{"a completed casual match", standardMatch(match.EndPrimeWellDestroyed), CategoryCasual, joined, 40, true, true, Earned},
		{"a surrender", standardMatch(match.EndSurrender), CategoryCasual, joined, 40, true, true, Earned},
		{"ranked", standardMatch(match.EndPrimeWellDestroyed), CategoryRanked, joined, 40, true, true, Earned},
		{"co-op below the gate", standardMatch(match.EndPrimeWellDestroyed), CategoryAI, joined, 9, true, true, Earned},
		{"co-op at the gate keeps Mastery", standardMatch(match.EndPrimeWellDestroyed), CategoryAI, joined, 10, false, true, ReasonCoopLevel},
		{"practice", &match.Match{Rules: match.RulesPractice, Result: &match.Result{EndReason: match.EndHostEnded}}, CategoryCasual, joined, 1, false, false, ReasonCustom},
		{"custom", &match.Match{Rules: match.RulesCustom, Result: &match.Result{EndReason: match.EndPrimeWellDestroyed}}, CategoryCasual, joined, 1, false, false, ReasonCustom},
		{"a development match", standardMatch(match.EndDeveloperRequest), "", joined, 1, false, false, ReasonNotMatchmade},
		{"a remake", standardMatch(match.EndRemake), CategoryCasual, joined, 1, false, false, ReasonNoContest},
		{"abandoned", standardMatch(match.EndAbandoned), CategoryCasual, joined, 1, false, false, ReasonNotCompleted},
		{"never joined", standardMatch(match.EndPrimeWellDestroyed), CategoryCasual, match.ParticipantResult{}, 1, false, false, ReasonNotJoined},
		{"a personal loss", standardMatch(match.EndPrimeWellDestroyed), CategoryCasual, match.ParticipantResult{Joined: true, PersonalLoss: true}, 1, false, false, ReasonPersonalLoss},
	}
	for _, c := range cases {
		xp, mastery, reason := Eligibility(xpTuning, c.m, c.category, c.p, c.level)
		if xp != c.xp || mastery != c.mast || reason != c.reason {
			t.Errorf("%s: got xp %v, mastery %v, %q", c.name, xp, mastery, reason)
		}
	}
}

func TestMatchXPIsPerMinutePlusTheWinBonus(t *testing.T) {
	tuning := AccountXP{PerMinute: 6, WinBonus: 30}
	r := &match.Result{DurationSeconds: 25*60 + 10}
	if got := MatchXP(tuning, r, false); got != 151 {
		t.Errorf("a loss: %d", got)
	}
	if got := MatchXP(tuning, r, true); got != 181 {
		t.Errorf("a win: %d", got)
	}
}

func TestMasteryCreditsMoreThanKillsAndCapsPerformance(t *testing.T) {
	tuning := Mastery{PerMinute: 10, WinBonus: 100, PerformanceCap: 300,
		Weights: Weights{Kills: 15, Assists: 10, VanguardDamage: 0.005, DamageShielded: 0.005, TeammateHealing: 0.005,
			CrowdControlSeconds: 2, TowerDamage: 0.005, WellsSecured: 20, WardsPlaced: 3, WardsDestroyed: 5}}
	r := &match.Result{DurationSeconds: 30 * 60}
	// A protector with no kills: 8 assists, 10,000 shielded, 6,000 healed, 30 s of crowd control, 12 wards placed.
	protector := match.PlayerStatistics{Assists: 8, Deaths: 9, DamageShielded: 10000, TeammateHealing: 6000,
		CrowdControl: match.CrowdControl{Total: 30}, WardsPlaced: 12}
	if got := Performance(tuning, protector); got != 80+50+30+60+36 {
		t.Errorf("protector performance %d", got)
	}
	if got := MatchMastery(tuning, r, true, protector); got != 300+100+256 {
		t.Errorf("protector mastery %d", got)
	}
	carry := match.PlayerStatistics{Kills: 30, Assists: 20, VanguardDamage: 90000}
	if got := Performance(tuning, carry); got != 300 {
		t.Errorf("performance is capped: %d", got)
	}
	if got := MatchMastery(tuning, r, false, match.PlayerStatistics{Deaths: 20}); got != 300 {
		t.Errorf("deaths never subtract: %d", got)
	}
}

func TestTheEmoteTierCountsTheMilestonesReached(t *testing.T) {
	tuning := Mastery{EmoteTierLevels: []int{1, 5, 10}}
	for level, want := range map[int]int{1: 1, 4: 1, 5: 2, 9: 2, 10: 3, 400: 3} {
		if got := EmoteTier(tuning, level); got != want {
			t.Errorf("EmoteTier(%d) = %d, want %d", level, got, want)
		}
	}
}
