// Package catalog lists the Vanguards players may own and pick: which are
// released, which a new player may choose as a starter, and which the weekly
// free rotation offers (ADR-010 §6; ADR-038 §1; Modes & Access Bible §3). It is
// configuration, read-only while the backend runs. Game/Tuning/Vanguards.json
// defines the Vanguards themselves, and a contract test keeps the released list
// equal to its Playable ones.
package catalog

import (
	"hash/fnv"
	"math/rand/v2"
	"slices"
	"sync"
	"time"
)

// RotationSettings configure the weekly free rotation (Modes & Access Bible
// §3; ADR-038 §1).
type RotationSettings struct {
	// Slots is how many distinct Vanguards each week offers.
	Slots int
	// Epoch is when the first rotation week begins; each week lasts Week.
	Epoch time.Time
	Week  time.Duration
	// Seed makes every week's draw reproducible: the same seed and week give
	// the same rotation on every service.
	Seed string
	// Releases maps a Vanguard to when it was released. One absent was
	// released before the epoch.
	Releases map[string]time.Time
}

// Settings are the catalog's validated configuration.
type Settings struct {
	// Released are the Vanguards players may own and pick, in display order.
	Released []string
	// Starters are the Vanguards a new player may choose from, all released.
	Starters []string
	Rotation RotationSettings
}

// Catalog answers questions about released Vanguards.
type Catalog struct {
	settings Settings
	released map[string]bool
	starters map[string]bool
	now      func() time.Time

	mu    sync.Mutex
	weeks map[int64][]string
}

// New builds a Catalog from validated settings, reading the time from now.
func New(s Settings, now func() time.Time) *Catalog {
	c := &Catalog{settings: s, released: map[string]bool{}, starters: map[string]bool{}, now: now, weeks: map[int64][]string{}}
	for _, id := range s.Released {
		c.released[id] = true
	}
	for _, id := range s.Starters {
		c.starters[id] = true
	}
	return c
}

// IsReleased reports whether players may own and pick a Vanguard.
func (c *Catalog) IsReleased(id string) bool { return c.released[id] }

// IsStarter reports whether a new player may choose a Vanguard as a starter.
func (c *Catalog) IsStarter(id string) bool { return c.starters[id] }

// Released returns the released Vanguards, in display order.
func (c *Catalog) Released() []string { return append([]string(nil), c.settings.Released...) }

// Starters returns the starters, in display order.
func (c *Catalog) Starters() []string { return append([]string(nil), c.settings.Starters...) }

// Rotation returns what the weekly rotation offers now, in display order.
func (c *Catalog) Rotation() []string { return c.RotationAt(c.now()) }

// RotationAt returns what the weekly rotation offers at t, in display order:
// nothing before the epoch, or in a week with fewer eligible Vanguards than
// its slots (a launch precondition, Modes & Access Bible §3).
func (c *Catalog) RotationAt(t time.Time) []string {
	week, ok := c.WeekAt(t)
	if !ok {
		return nil
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	return append([]string(nil), c.week(week)...)
}

// WeekAt returns the rotation week t falls in, counted from the epoch; false
// before the epoch.
func (c *Catalog) WeekAt(t time.Time) (int64, bool) {
	r := c.settings.Rotation
	if r.Week <= 0 || t.Before(r.Epoch) {
		return 0, false
	}
	return int64(t.Sub(r.Epoch) / r.Week), true
}

// week draws week w's rotation, each week from the one before it, and keeps
// it. Callers hold mu.
func (c *Catalog) week(w int64) []string {
	if drawn, ok := c.weeks[w]; ok {
		return drawn
	}
	// Oldest first, each from the one before it.
	var previous []string
	for earlier := int64(0); earlier <= w; earlier++ {
		drawn, ok := c.weeks[earlier]
		if !ok {
			drawn = c.draw(earlier, previous)
			c.weeks[earlier] = drawn
		}
		previous = drawn
	}
	return previous
}

// draw picks week w's Vanguards: Slots distinct eligible ones, none of last
// week's unless too few others remain, by a shuffle seeded with the seed and
// the week (ADR-038 §1).
func (c *Catalog) draw(w int64, previous []string) []string {
	r := c.settings.Rotation
	start := r.Epoch.Add(time.Duration(w) * r.Week)
	var fresh, repeats []string
	for _, id := range c.settings.Released {
		// Eligible from the first week that starts at least a week after its release.
		if released, ok := r.Releases[id]; ok && start.Before(released.Add(r.Week)) {
			continue
		}
		if slices.Contains(previous, id) {
			repeats = append(repeats, id)
		} else {
			fresh = append(fresh, id)
		}
	}
	if len(fresh)+len(repeats) < r.Slots {
		return nil
	}
	hash := fnv.New64a()
	hash.Write([]byte(r.Seed))
	random := rand.New(rand.NewPCG(hash.Sum64(), uint64(w)))
	random.Shuffle(len(fresh), func(i, j int) { fresh[i], fresh[j] = fresh[j], fresh[i] })
	random.Shuffle(len(repeats), func(i, j int) { repeats[i], repeats[j] = repeats[j], repeats[i] })
	picked := map[string]bool{}
	for _, id := range fresh[:min(r.Slots, len(fresh))] {
		picked[id] = true
	}
	// Too few new choices: last week's fill the rest, the no-repeat rule relaxed first.
	for _, id := range repeats[:r.Slots-len(picked)] {
		picked[id] = true
	}
	out := make([]string, 0, r.Slots)
	for _, id := range c.settings.Released {
		if picked[id] {
			out = append(out, id)
		}
	}
	return out
}
