package match

import (
	"math"
	"slices"
	"strings"
)

// PlayerStatistics is one player's record of a match as its server kept it
// (Match Statistics Bible §2–§8; ADR-017 §3). The backend checks its shape and
// keeps it; it computes nothing from it. Its JSON is both the result's wire
// format and the stored document.
type PlayerStatistics struct {
	Kills   int `json:"kills"`
	Deaths  int `json:"deaths"`
	Assists int `json:"assists"`
	Level   int `json:"level"`
	// VanguardDamage is the Health removed from enemy Vanguards.
	VanguardDamage float64      `json:"vanguardDamage"`
	DamageDealt    DamageByType `json:"damageDealt"`
	DamageTaken    DamageByType `json:"damageTaken"`
	// DamageShielded is what the shields this player gave absorbed, on anyone.
	DamageShielded  float64      `json:"damageShielded"`
	SelfHealing     float64      `json:"selfHealing"`
	TeammateHealing float64      `json:"teammateHealing"`
	CrowdControl    CrowdControl `json:"crowdControl"`
	GoldEarned      float64      `json:"goldEarned"`
	GoldBySource    GoldBySource `json:"goldBySource"`
	MinionKills     int          `json:"minionKills"`
	JungleKills     int          `json:"jungleKills"`
	// TowerDamage is the Health removed from enemy lane Spires and base towers.
	TowerDamage    float64 `json:"towerDamage"`
	WellsSecured   int     `json:"wellsSecured"`
	WellDamage     float64 `json:"wellDamage"`
	WellFinalHits  int     `json:"wellFinalHits"`
	WardsPlaced    int     `json:"wardsPlaced"`
	WardsDestroyed int     `json:"wardsDestroyed"`
}

// DamageByType is Health removed, by damage type.
type DamageByType struct {
	Physical float64 `json:"physical"`
	Magic    float64 `json:"magic"`
	True     float64 `json:"true"`
}

// CrowdControl is the effective seconds of each kind applied to enemy
// Vanguards, and of any kind: a stun and a slow at once count once in Total.
type CrowdControl struct {
	Stun  float64 `json:"stun"`
	Slow  float64 `json:"slow"`
	Total float64 `json:"total"`
}

// GoldBySource is the Gold earned, by where it came from (Match Statistics
// Bible §7).
type GoldBySource struct {
	Starting   float64 `json:"starting"`
	Kills      float64 `json:"kills"`
	Assists    float64 `json:"assists"`
	Minions    float64 `json:"minions"`
	Jungle     float64 `json:"jungle"`
	Objectives float64 `json:"objectives"`
	Wards      float64 `json:"wards"`
	Passive    float64 `json:"passive"`
}

// WellCapture is a Flux Well secured: which of its sites, by which side, and
// when on the match clock (Match Statistics Bible §5). The team summary counts
// each capture once (Pre-Game Client UX Bible 53).
type WellCapture struct {
	Site      int     `json:"site"`
	Side      Side    `json:"side"`
	AtSeconds float64 `json:"atSeconds"`
}

// validateWells checks that each capture is on a side that exists, at a site,
// within the match.
func validateWells(wells []WellCapture, durationSeconds float64) error {
	for _, w := range wells {
		if (w.Side != SideA && w.Side != SideB) || w.Site < 0 || math.IsNaN(w.AtSeconds) || w.AtSeconds < 0 || w.AtSeconds > durationSeconds {
			return ErrInvalidResult
		}
	}
	return nil
}

// PlayerResult is one player's line on a result's scoreboard, a human's or a
// bot's (ADR-017 §5).
type PlayerResult struct {
	Side Side   `json:"side"`
	Name string `json:"name"`
	// AccountID is the rostered account; empty for a bot.
	AccountID  string           `json:"accountId,omitempty"`
	VanguardID string           `json:"vanguardId"`
	Statistics PlayerStatistics `json:"statistics"`
	// Items are the inventory's slots in order as the match ended, each a
	// content ID or "" for an empty slot.
	Items []string `json:"items"`
	// FluxSpells are the spell slots as the match ended, each a content ID or
	// "" for an empty slot.
	FluxSpells [2]string `json:"fluxSpells"`
}

// validatePlayers checks a scoreboard against the match (ADR-017 §5): each
// line on a side that exists with a name and a Vanguard; each account a
// rostered participant, once, on its side and with its Vanguard; each bot one
// of the match's bots; every number a count or amount; every slot empty or a
// content ID. A nil scoreboard, from a server that sends none, is valid.
func (m *Match) validatePlayers(players []PlayerResult) error {
	accounts := map[string]bool{}
	bots := slices.Clone(m.Bots)
	for _, p := range players {
		if (p.Side != SideA && p.Side != SideB) || strings.TrimSpace(p.Name) == "" || !IsContentID(p.VanguardID) {
			return ErrInvalidResult
		}
		if p.AccountID != "" {
			rostered, ok := m.Participant(p.AccountID)
			if !ok || accounts[p.AccountID] || rostered.Side != p.Side || (rostered.VanguardID != "" && rostered.VanguardID != p.VanguardID) {
				return ErrInvalidResult
			}
			accounts[p.AccountID] = true
		} else {
			// Each of the match's bots appears at most once.
			i := slices.IndexFunc(bots, func(b Bot) bool { return b.Side == p.Side && b.VanguardID == p.VanguardID })
			if i < 0 {
				return ErrInvalidResult
			}
			bots = slices.Delete(bots, i, i+1)
		}
		if !p.Statistics.valid() || !ValidFluxSpells(p.FluxSpells) {
			return ErrInvalidResult
		}
		for _, item := range p.Items {
			if item != "" && !IsContentID(item) {
				return ErrInvalidResult
			}
		}
	}
	return nil
}

// valid reports whether every count and amount is finite and not negative.
func (s PlayerStatistics) valid() bool {
	counts := []int{s.Kills, s.Deaths, s.Assists, s.Level, s.MinionKills, s.JungleKills, s.WellsSecured, s.WellFinalHits, s.WardsPlaced, s.WardsDestroyed}
	amounts := []float64{
		s.VanguardDamage,
		s.DamageDealt.Physical, s.DamageDealt.Magic, s.DamageDealt.True,
		s.DamageTaken.Physical, s.DamageTaken.Magic, s.DamageTaken.True,
		s.DamageShielded, s.SelfHealing, s.TeammateHealing,
		s.CrowdControl.Stun, s.CrowdControl.Slow, s.CrowdControl.Total,
		s.GoldEarned,
		s.GoldBySource.Starting, s.GoldBySource.Kills, s.GoldBySource.Assists, s.GoldBySource.Minions,
		s.GoldBySource.Jungle, s.GoldBySource.Objectives, s.GoldBySource.Wards, s.GoldBySource.Passive,
		s.TowerDamage, s.WellDamage,
	}
	for _, c := range counts {
		if c < 0 {
			return false
		}
	}
	for _, a := range amounts {
		if math.IsNaN(a) || math.IsInf(a, 0) || a < 0 {
			return false
		}
	}
	return true
}

// samePlayers reports whether two scoreboards say the same, so a replayed
// result is recognised (ADR-007 §7).
func samePlayers(a, b []PlayerResult) bool {
	if (a == nil) != (b == nil) || len(a) != len(b) {
		return false
	}
	for i := range a {
		x, y := a[i], b[i]
		if x.Side != y.Side || x.Name != y.Name || x.AccountID != y.AccountID || x.VanguardID != y.VanguardID ||
			x.Statistics != y.Statistics || x.FluxSpells != y.FluxSpells || !slices.Equal(x.Items, y.Items) {
			return false
		}
	}
	return true
}

func copyPlayers(in []PlayerResult) []PlayerResult {
	if in == nil {
		return nil
	}
	out := make([]PlayerResult, len(in))
	for i, p := range in {
		p.Items = slices.Clone(p.Items)
		out[i] = p
	}
	return out
}
