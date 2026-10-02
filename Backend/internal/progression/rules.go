// Package progression owns what persists for an account between matches
// (ADR-045; Account, Collection & Mastery Bible §2–§6): its account level and
// account XP, its Flux and Refined Flux, the Vanguards it buys, and its
// Mastery of each Vanguard. Rewards come only from verified match results;
// the client never mints anything. Flux and Refined Flux here are persistent
// account currencies, never in-match Team Flux, Gold or a Flux Spell.
//
// The rules in this file are pure functions of a Tuning, unit-tested without
// a database.
package progression

import (
	"math"
	"slices"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// Curve is how much one level takes from the previous: First from level 1 to
// 2, Growth more for each level after, through GrowthUntil, and the same as
// the last thereafter (Bible §2: "grows gradually through Level 100, then
// becomes constant"). Levels have no ceiling.
type Curve struct {
	First       int64
	Growth      int64
	GrowthUntil int
}

// Need is what level takes to reach level+1. Levels start at 1.
func (c Curve) Need(level int) int64 {
	steps := min(max(level, 1), c.GrowthUntil) - 1
	return c.First + c.Growth*int64(max(steps, 0))
}

// Standing is a position on a curve: a level, and progress into it toward the
// next.
type Standing struct {
	Level int
	Into  int64
}

// Add moves s by gained along c. It returns the new standing and every level
// reached on the way, in order.
func (c Curve) Add(s Standing, gained int64) (Standing, []int) {
	var reached []int
	s.Level = max(s.Level, 1)
	s.Into += max(gained, 0)
	for need := c.Need(s.Level); s.Into >= need; need = c.Need(s.Level) {
		s.Into -= need
		s.Level++
		reached = append(reached, s.Level)
	}
	return s, reached
}

// Milestone is Refined Flux awarded on reaching a level.
type Milestone struct {
	Level  int
	Amount int64
}

// RefinedFlux is the milestone award: the listed milestones (Bible §3: Levels
// 30, 50, 75, 100, etc.), then Amount every EveryLevels levels past the last.
type RefinedFlux struct {
	Milestones  []Milestone
	EveryLevels int
	Amount      int64
}

// At is the Refined Flux reaching level awards; 0 for most levels.
func (r RefinedFlux) At(level int) int64 {
	for _, m := range r.Milestones {
		if m.Level == level {
			return m.Amount
		}
	}
	if len(r.Milestones) == 0 || r.EveryLevels <= 0 {
		return 0
	}
	last := r.Milestones[len(r.Milestones)-1].Level
	if level > last && (level-last)%r.EveryLevels == 0 {
		return r.Amount
	}
	return 0
}

// AccountXP tunes the account XP a match gives (Bible §2).
type AccountXP struct {
	PerMinute float64
	WinBonus  int64
	// CoopBelowLevel: Co-op vs AI gives account XP only to an account below
	// this level (Bible §2; Modes Bible §4).
	CoopBelowLevel int
}

// Weights are what each scoreboard statistic is worth toward a match's
// Mastery performance award (Bible §5.1): protection, utility, objectives and
// vision as well as kills. Deaths are never weighed.
type Weights struct {
	Kills               float64
	Assists             float64
	VanguardDamage      float64
	DamageShielded      float64
	TeammateHealing     float64
	CrowdControlSeconds float64
	TowerDamage         float64
	WellsSecured        float64
	WardsPlaced         float64
	WardsDestroyed      float64
}

// MasteryTuning tunes the Mastery points a match gives and their levels (Bible §5).
type MasteryTuning struct {
	PerMinute      float64
	WinBonus       int64
	PerformanceCap int64
	Weights        Weights
	Levels         Curve
	// EmoteTierLevels are the Mastery Levels at which the mastery emote's
	// appearance upgrades, ascending (Bible §5.2).
	EmoteTierLevels []int
}

// Price is a Vanguard's price in each account currency (Bible §3).
type Price struct {
	Flux        int64
	RefinedFlux int64
}

// Tuning is the progression section of the backend configuration (ADR-045
// §10). Every value is designer-editable data.
type Tuning struct {
	AccountXP     AccountXP
	AccountLevels Curve
	FluxPerLevel  int64
	RefinedFlux   RefinedFlux
	Mastery       MasteryTuning
	// Prices holds every released Vanguard's price.
	Prices map[string]Price
	// DevGrant mounts the development route that grants currency.
	DevGrant bool
}

// Reason says why a participant earned nothing, or that they earned.
type Reason string

const (
	// Earned: the participant earns account XP and Mastery.
	Earned Reason = ""
	// ReasonCustom: Practice and Custom matches give nothing (ADR-021).
	ReasonCustom Reason = "custom"
	// ReasonNotMatchmade: a Standard match outside the rewarded categories,
	// such as a development match.
	ReasonNotMatchmade Reason = "not_matchmade"
	// ReasonNoContest: a remake (Match Flow Bible §7).
	ReasonNoContest Reason = "no_contest"
	// ReasonNotCompleted: abandoned, ended by its host or by a developer.
	ReasonNotCompleted Reason = "not_completed"
	// ReasonNotJoined: the participant never joined the match.
	ReasonNotJoined Reason = "not_joined"
	// ReasonPersonalLoss: a personal loss stood at adjudication (Bible §2).
	ReasonPersonalLoss Reason = "personal_loss"
	// ReasonCoopLevel: Co-op vs AI at or above the level where its account
	// XP ends; Mastery is still earned (Bible §2, §5.1).
	ReasonCoopLevel Reason = "coop_level"
)

// The mode categories the configuration gives modes (ADR-039 §6).
const (
	CategoryCasual = "casual"
	CategoryRanked = "ranked"
	CategoryAI     = "ai"
)

// rewardedCategories are the mode categories whose completed matches reward
// (Bible §2, §5.1): matchmade PvP, future Ranked, and Co-op vs AI.
var rewardedCategories = []string{CategoryCasual, CategoryRanked, CategoryAI}

// Eligibility decides what one participant earns from a match. category is
// the match mode's category ("" for a mode with none), and level the
// account's level as the grant is made. It returns whether the participant
// earns account XP, whether they earn Mastery, and the reason when account XP
// is withheld.
func Eligibility(t AccountXP, m *match.Match, category string, p match.ParticipantResult, level int) (accountXP, mastery bool, reason Reason) {
	r := m.Result
	switch {
	case m.Rules != match.RulesStandard:
		return false, false, ReasonCustom
	case !slices.Contains(rewardedCategories, category):
		return false, false, ReasonNotMatchmade
	case r.EndReason == match.EndRemake:
		return false, false, ReasonNoContest
	case r.EndReason != match.EndPrimeWellDestroyed && r.EndReason != match.EndSurrender:
		return false, false, ReasonNotCompleted
	case !p.Joined:
		return false, false, ReasonNotJoined
	case p.PersonalLoss:
		// Provisionally no Mastery either (ADR-045 §10; Bible §8.3 is open).
		return false, false, ReasonPersonalLoss
	case category == CategoryAI && level >= t.CoopBelowLevel:
		return false, true, ReasonCoopLevel
	}
	return true, true, Earned
}

// minutes is the match clock in minutes, pauses excluded.
func minutes(r *match.Result) float64 { return math.Max(r.DurationSeconds, 0) / 60 }

// MatchXP is the account XP a match gives: per minute of the match clock,
// and the win bonus for the winning side (Bible §2).
func MatchXP(t AccountXP, r *match.Result, won bool) int64 {
	xp := int64(math.Round(t.PerMinute * minutes(r)))
	if won {
		xp += t.WinBonus
	}
	return xp
}

// Performance is a match's Mastery performance award from the player's
// verified statistics, capped (Bible §5.1).
func Performance(t MasteryTuning, s match.PlayerStatistics) int64 {
	w := t.Weights
	score := w.Kills*float64(s.Kills) + w.Assists*float64(s.Assists) + w.VanguardDamage*s.VanguardDamage +
		w.DamageShielded*s.DamageShielded + w.TeammateHealing*s.TeammateHealing + w.CrowdControlSeconds*s.CrowdControl.Total +
		w.TowerDamage*s.TowerDamage + w.WellsSecured*float64(s.WellsSecured) + w.WardsPlaced*float64(s.WardsPlaced) +
		w.WardsDestroyed*float64(s.WardsDestroyed)
	return min(int64(math.Round(math.Max(score, 0))), t.PerformanceCap)
}

// MatchMastery is the Mastery points a match gives the Vanguard played: per
// minute of the match clock, the win bonus, and the performance award.
func MatchMastery(t MasteryTuning, r *match.Result, won bool, s match.PlayerStatistics) int64 {
	points := int64(math.Round(t.PerMinute*minutes(r))) + Performance(t, s)
	if won {
		points += t.WinBonus
	}
	return points
}

// EmoteTier is how many of the emote's tiers a Mastery Level has reached.
func EmoteTier(t MasteryTuning, level int) int {
	tier := 0
	for _, at := range t.EmoteTierLevels {
		if level >= at {
			tier++
		}
	}
	return tier
}

// LevelUpRewards is what reaching each of levels awards: Flux at every
// level-up, Refined Flux at milestones (Bible §3).
func LevelUpRewards(t Tuning, levels []int) (flux, refinedFlux int64) {
	for _, level := range levels {
		flux += t.FluxPerLevel
		refinedFlux += t.RefinedFlux.At(level)
	}
	return flux, refinedFlux
}
