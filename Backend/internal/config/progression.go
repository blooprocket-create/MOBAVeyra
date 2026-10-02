package config

import (
	"fmt"
	"slices"
	"strconv"
)

// Progression configures account progression, the account currencies, the
// storefront and Vanguard Mastery (ADR-045 §10; Account, Collection & Mastery
// Bible §2–§5). Every value is Provisional tuning.
type Progression struct {
	AccountXP     ProgressionAccountXP
	AccountLevels ProgressionCurve
	// FluxPerLevel is the earned account currency each level-up awards.
	FluxPerLevel int64
	RefinedFlux  ProgressionRefinedFlux
	Mastery      ProgressionMastery
	// Prices holds a price for exactly the released Vanguards.
	Prices map[string]ProgressionPrice
	// DevGrant mounts the development route that grants currency; only in
	// the local environment.
	DevGrant bool
}

// ProgressionAccountXP is the account XP a match gives.
type ProgressionAccountXP struct {
	PerMinute float64
	WinBonus  int64
	// CoopBelowLevel ends Co-op vs AI account XP at this account level.
	CoopBelowLevel int
}

// ProgressionCurve is what each level takes: First from 1 to 2, Growth more
// per level through GrowthUntil, then constant.
type ProgressionCurve struct {
	First       int64
	Growth      int64
	GrowthUntil int
}

// ProgressionMilestone is Refined Flux awarded on reaching Level.
type ProgressionMilestone struct {
	Level  int
	Amount int64
}

// ProgressionRefinedFlux is the milestone award: the listed milestones, then
// Amount every EveryLevels levels past the last; EveryLevels 0 means none.
type ProgressionRefinedFlux struct {
	Milestones  []ProgressionMilestone
	EveryLevels int
	Amount      int64
}

// ProgressionWeights are the Mastery performance weights per statistic.
type ProgressionWeights struct {
	Kills, Assists, VanguardDamage, DamageShielded, TeammateHealing float64
	CrowdControlSeconds, TowerDamage, WellsSecured, WardsPlaced     float64
	WardsDestroyed                                                  float64
}

// ProgressionMastery is the Mastery a match gives and its levels.
type ProgressionMastery struct {
	PerMinute       float64
	WinBonus        int64
	PerformanceCap  int64
	Weights         ProgressionWeights
	Levels          ProgressionCurve
	EmoteTierLevels []int
}

// ProgressionPrice is one Vanguard's price in each account currency.
type ProgressionPrice struct {
	Flux        int64
	RefinedFlux int64
}

type fileProgressionCurve struct {
	FirstLevel       *int64 `json:"firstLevel"`
	GrowthPerLevel   *int64 `json:"growthPerLevel"`
	GrowthUntilLevel *int   `json:"growthUntilLevel"`
}

type fileProgression struct {
	AccountXP *struct {
		PerMinute      *float64 `json:"perMinute"`
		WinBonus       *int64   `json:"winBonus"`
		CoopBelowLevel *int     `json:"coopBelowLevel"`
	} `json:"accountXp"`
	AccountLevels *fileProgressionCurve `json:"accountLevels"`
	Flux          *struct {
		PerLevelUp *int64 `json:"perLevelUp"`
	} `json:"flux"`
	RefinedFlux *struct {
		Milestones []struct {
			Level  *int   `json:"level"`
			Amount *int64 `json:"amount"`
		} `json:"milestones"`
		EveryLevels           *int   `json:"everyLevels"`
		AmountAfterMilestones *int64 `json:"amountAfterMilestones"`
	} `json:"refinedFlux"`
	Mastery *struct {
		PerMinute      *float64 `json:"perMinute"`
		WinBonus       *int64   `json:"winBonus"`
		PerformanceCap *int64   `json:"performanceCap"`
		Weights        *struct {
			Kills               *float64 `json:"kills"`
			Assists             *float64 `json:"assists"`
			VanguardDamage      *float64 `json:"vanguardDamage"`
			DamageShielded      *float64 `json:"damageShielded"`
			TeammateHealing     *float64 `json:"teammateHealing"`
			CrowdControlSeconds *float64 `json:"crowdControlSeconds"`
			TowerDamage         *float64 `json:"towerDamage"`
			WellsSecured        *float64 `json:"wellsSecured"`
			WardsPlaced         *float64 `json:"wardsPlaced"`
			WardsDestroyed      *float64 `json:"wardsDestroyed"`
		} `json:"weights"`
		Levels          *fileProgressionCurve `json:"levels"`
		EmoteTierLevels []int                 `json:"emoteTierLevels"`
	} `json:"mastery"`
	Prices map[string]struct {
		Flux        *int64 `json:"flux"`
		RefinedFlux *int64 `json:"refinedFlux"`
	} `json:"prices"`
	DevGrant *struct {
		Enabled *bool `json:"enabled"`
	} `json:"devGrant"`
}

// parseProgression validates the progression section against the released
// Vanguards and the environment, reporting through missing and problem.
func parseProgression(f *fileProgression, released []string, environment string, missing, problem func(string)) Progression {
	var p Progression
	if f == nil {
		missing("progression")
		return p
	}
	count := func(field string, v *int64) int64 {
		switch {
		case v == nil:
			missing(field)
		case *v < 0:
			problem(field + " must not be negative")
		default:
			return *v
		}
		return 0
	}
	rate := func(field string, v *float64) float64 {
		switch {
		case v == nil:
			missing(field)
		case !(*v >= 0):
			problem(field + " must not be negative")
		default:
			return *v
		}
		return 0
	}
	level := func(field string, v *int) int {
		switch {
		case v == nil:
			missing(field)
		case *v < 1:
			problem(field + " must be at least 1")
		default:
			return *v
		}
		return 0
	}
	curve := func(field string, c *fileProgressionCurve) ProgressionCurve {
		if c == nil {
			missing(field)
			return ProgressionCurve{}
		}
		out := ProgressionCurve{First: count(field+".firstLevel", c.FirstLevel), Growth: count(field+".growthPerLevel", c.GrowthPerLevel),
			GrowthUntil: level(field+".growthUntilLevel", c.GrowthUntilLevel)}
		if c.FirstLevel != nil && *c.FirstLevel == 0 {
			problem(field + ".firstLevel must be positive, or a level would take nothing")
		}
		return out
	}

	if f.AccountXP == nil {
		missing("progression.accountXp")
	} else {
		p.AccountXP = ProgressionAccountXP{PerMinute: rate("progression.accountXp.perMinute", f.AccountXP.PerMinute),
			WinBonus: count("progression.accountXp.winBonus", f.AccountXP.WinBonus), CoopBelowLevel: level("progression.accountXp.coopBelowLevel", f.AccountXP.CoopBelowLevel)}
	}
	p.AccountLevels = curve("progression.accountLevels", f.AccountLevels)
	if f.Flux == nil {
		missing("progression.flux")
	} else {
		p.FluxPerLevel = count("progression.flux.perLevelUp", f.Flux.PerLevelUp)
	}

	if f.RefinedFlux == nil {
		missing("progression.refinedFlux")
	} else {
		previous := 1
		for i, m := range f.RefinedFlux.Milestones {
			field := fmt.Sprintf("progression.refinedFlux.milestones[%d]", i)
			at, amount := level(field+".level", m.Level), count(field+".amount", m.Amount)
			if m.Level != nil && *m.Level <= previous {
				problem(field + ".level must be above 1 and above the milestone before it")
			}
			if m.Amount != nil && *m.Amount == 0 {
				problem(field + ".amount must be positive")
			}
			previous = max(previous, at)
			p.RefinedFlux.Milestones = append(p.RefinedFlux.Milestones, ProgressionMilestone{Level: at, Amount: amount})
		}
		switch every := f.RefinedFlux.EveryLevels; {
		case every == nil:
			missing("progression.refinedFlux.everyLevels")
		case *every < 0:
			problem("progression.refinedFlux.everyLevels must not be negative")
		default:
			p.RefinedFlux.EveryLevels = *every
		}
		p.RefinedFlux.Amount = count("progression.refinedFlux.amountAfterMilestones", f.RefinedFlux.AmountAfterMilestones)
		if p.RefinedFlux.EveryLevels > 0 && (p.RefinedFlux.Amount == 0 || len(p.RefinedFlux.Milestones) == 0) {
			problem("progression.refinedFlux.everyLevels needs at least one milestone and a positive amountAfterMilestones")
		}
	}

	if f.Mastery == nil {
		missing("progression.mastery")
	} else {
		m := f.Mastery
		p.Mastery = ProgressionMastery{PerMinute: rate("progression.mastery.perMinute", m.PerMinute), WinBonus: count("progression.mastery.winBonus", m.WinBonus),
			PerformanceCap: count("progression.mastery.performanceCap", m.PerformanceCap), Levels: curve("progression.mastery.levels", m.Levels)}
		if w := m.Weights; w == nil {
			missing("progression.mastery.weights")
		} else {
			weight := func(name string, v *float64) float64 { return rate("progression.mastery.weights."+name, v) }
			p.Mastery.Weights = ProgressionWeights{Kills: weight("kills", w.Kills), Assists: weight("assists", w.Assists),
				VanguardDamage: weight("vanguardDamage", w.VanguardDamage), DamageShielded: weight("damageShielded", w.DamageShielded),
				TeammateHealing: weight("teammateHealing", w.TeammateHealing), CrowdControlSeconds: weight("crowdControlSeconds", w.CrowdControlSeconds),
				TowerDamage: weight("towerDamage", w.TowerDamage), WellsSecured: weight("wellsSecured", w.WellsSecured),
				WardsPlaced: weight("wardsPlaced", w.WardsPlaced), WardsDestroyed: weight("wardsDestroyed", w.WardsDestroyed)}
		}
		if len(m.EmoteTierLevels) == 0 {
			missing("progression.mastery.emoteTierLevels")
		}
		for i, at := range m.EmoteTierLevels {
			if at < 1 || (i > 0 && at <= m.EmoteTierLevels[i-1]) {
				problem("progression.mastery.emoteTierLevels must be levels of at least 1, ascending")
				break
			}
		}
		p.Mastery.EmoteTierLevels = slices.Clone(m.EmoteTierLevels)
	}

	// A price for exactly the released Vanguards, each positive in both
	// currencies (Bible §3: every Vanguard either currency buys from release).
	if f.Prices == nil {
		missing("progression.prices")
	}
	p.Prices = map[string]ProgressionPrice{}
	for id, price := range f.Prices {
		field := "progression.prices." + id
		if !slices.Contains(released, id) {
			problem(field + " prices " + strconv.Quote(id) + ", which is not in vanguards.released")
			continue
		}
		flux, refined := count(field+".flux", price.Flux), count(field+".refinedFlux", price.RefinedFlux)
		if (price.Flux != nil && *price.Flux == 0) || (price.RefinedFlux != nil && *price.RefinedFlux == 0) {
			problem(field + " must be positive in both currencies")
		}
		p.Prices[id] = ProgressionPrice{Flux: flux, RefinedFlux: refined}
	}
	if f.Prices != nil {
		for _, id := range released {
			if _, ok := f.Prices[id]; !ok {
				problem("progression.prices has no price for released Vanguard " + id)
			}
		}
	}

	if f.DevGrant == nil || f.DevGrant.Enabled == nil {
		missing("progression.devGrant.enabled")
	} else {
		p.DevGrant = *f.DevGrant.Enabled
		if p.DevGrant && environment != EnvironmentLocal {
			problem("progression.devGrant.enabled is only allowed when environment is \"" + EnvironmentLocal + "\"")
		}
	}
	return p
}
