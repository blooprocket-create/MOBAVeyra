package config

import (
	"strings"
	"testing"
)

func TestParseProgression(t *testing.T) {
	c, err := Parse([]byte(validJSON))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	p := c.Progression
	if p.AccountXP != (ProgressionAccountXP{PerMinute: 6, WinBonus: 30, CoopBelowLevel: 10}) || p.AccountLevels != (ProgressionCurve{First: 150, Growth: 20, GrowthUntil: 100}) {
		t.Fatalf("account XP: %+v %+v", p.AccountXP, p.AccountLevels)
	}
	if p.FluxPerLevel != 400 || len(p.RefinedFlux.Milestones) != 2 || p.RefinedFlux.Milestones[1] != (ProgressionMilestone{Level: 50, Amount: 400}) ||
		p.RefinedFlux.EveryLevels != 25 || p.RefinedFlux.Amount != 500 {
		t.Fatalf("currencies: %+v", p)
	}
	if m := p.Mastery; m.PerformanceCap != 300 || m.Weights.CrowdControlSeconds != 2 || m.Levels.GrowthUntil != 5 || len(m.EmoteTierLevels) != 3 {
		t.Fatalf("mastery: %+v", m)
	}
	if len(p.Prices) != 4 || p.Prices["bryn"] != (ProgressionPrice{Flux: 3000, RefinedFlux: 550}) || !p.DevGrant {
		t.Fatalf("prices: %+v", p.Prices)
	}
}

func TestProgressionRejectsBadTuning(t *testing.T) {
	cases := []struct {
		name, old, new, want string
	}{
		{"a released Vanguard without a price", `"bryn": {"flux": 3000, "refinedFlux": 550}`, `"cairn_two": {"flux": 3000, "refinedFlux": 550}`,
			"no price for released Vanguard bryn"},
		{"a price that is free", `"bryn": {"flux": 3000`, `"bryn": {"flux": 0`, "must be positive in both currencies"},
		{"milestones out of order", `{"level": 50, "amount": 400}`, `{"level": 20, "amount": 400}`, "above the milestone before it"},
		{"emote tiers out of order", `"emoteTierLevels": [1, 5, 10]`, `"emoteTierLevels": [1, 10, 5]`, "ascending"},
		{"a level that takes nothing", `"firstLevel": 150`, `"firstLevel": 0`, "firstLevel must be positive"},
		{"a negative weight", `"wardsPlaced": 3`, `"wardsPlaced": -3`, "wardsPlaced must not be negative"},
		{"a missing weight", `"wardsDestroyed": 5`, `"wardsDestroyedTypo": 5`, "unknown field"},
		{"the co-op gate below level 1", `"coopBelowLevel": 10`, `"coopBelowLevel": 0`, "coopBelowLevel must be at least 1"},
	}
	for _, c := range cases {
		raw := strings.Replace(validJSON, c.old, c.new, 1)
		if raw == validJSON {
			t.Fatalf("%s: the fixture has no %q", c.name, c.old)
		}
		if _, err := Parse([]byte(raw)); err == nil || !strings.Contains(err.Error(), c.want) {
			t.Errorf("%s: %v", c.name, err)
		}
	}
}

func TestTheDevelopmentGrantIsLocalOnly(t *testing.T) {
	raw := strings.Replace(validJSON, `"environment": "local"`, `"environment": "production"`, 1)
	raw = strings.Replace(raw, `"devLogin": {"enabled": true`, `"devLogin": {"enabled": false`, 1)
	raw = strings.Replace(raw, `"devCreate": {"enabled": true}`, `"devCreate": {"enabled": false}`, 1)
	if _, err := Parse([]byte(raw)); err == nil || !strings.Contains(err.Error(), "progression.devGrant.enabled is only allowed") {
		t.Fatalf("a development grant outside local: %v", err)
	}
}
