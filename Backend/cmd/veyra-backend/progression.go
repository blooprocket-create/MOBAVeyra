package main

import (
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/config"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/progression"
)

// progressionTuning converts the validated configuration to the progression
// domain's tuning (ADR-045 §10).
func progressionTuning(c config.Progression) progression.Tuning {
	curve := func(c config.ProgressionCurve) progression.Curve {
		return progression.Curve{First: c.First, Growth: c.Growth, GrowthUntil: c.GrowthUntil}
	}
	t := progression.Tuning{
		AccountXP:     progression.AccountXP{PerMinute: c.AccountXP.PerMinute, WinBonus: c.AccountXP.WinBonus, CoopBelowLevel: c.AccountXP.CoopBelowLevel},
		AccountLevels: curve(c.AccountLevels),
		FluxPerLevel:  c.FluxPerLevel,
		RefinedFlux:   progression.RefinedFlux{EveryLevels: c.RefinedFlux.EveryLevels, Amount: c.RefinedFlux.Amount},
		Mastery: progression.MasteryTuning{
			PerMinute:      c.Mastery.PerMinute,
			WinBonus:       c.Mastery.WinBonus,
			PerformanceCap: c.Mastery.PerformanceCap,
			Weights: progression.Weights{
				Kills: c.Mastery.Weights.Kills, Assists: c.Mastery.Weights.Assists, VanguardDamage: c.Mastery.Weights.VanguardDamage,
				DamageShielded: c.Mastery.Weights.DamageShielded, TeammateHealing: c.Mastery.Weights.TeammateHealing,
				CrowdControlSeconds: c.Mastery.Weights.CrowdControlSeconds, TowerDamage: c.Mastery.Weights.TowerDamage,
				WellsSecured: c.Mastery.Weights.WellsSecured, WardsPlaced: c.Mastery.Weights.WardsPlaced, WardsDestroyed: c.Mastery.Weights.WardsDestroyed,
			},
			Levels:          curve(c.Mastery.Levels),
			EmoteTierLevels: append([]int(nil), c.Mastery.EmoteTierLevels...),
		},
		Prices:   map[string]progression.Price{},
		DevGrant: c.DevGrant,
	}
	for _, m := range c.RefinedFlux.Milestones {
		t.RefinedFlux.Milestones = append(t.RefinedFlux.Milestones, progression.Milestone{Level: m.Level, Amount: m.Amount})
	}
	for id, p := range c.Prices {
		t.Prices[id] = progression.Price{Flux: p.Flux, RefinedFlux: p.RefinedFlux}
	}
	return t
}

// modeCategories maps each configured mode to its Play-page category, which
// decides what its matches reward (ADR-045 §3).
func modeCategories(modes []config.Mode) map[string]string {
	out := map[string]string{}
	for _, m := range modes {
		out[m.ID] = m.Category
	}
	return out
}
