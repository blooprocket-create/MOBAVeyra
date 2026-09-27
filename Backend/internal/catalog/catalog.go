// Package catalog lists the Vanguards players may own and pick: which are
// released, which a new player may choose as a starter, and which the
// rotation offers (ADR-010 §6; Modes & Access Bible §3). It is configuration,
// read-only while the backend runs. Game/Tuning/Vanguards.json defines the
// Vanguards themselves, and a contract test keeps the released list equal to
// its Playable ones.
package catalog

// StandIn says what the rotation holds until the weekly rotation exists.
type StandIn string

const (
	// StandInAllReleased offers every released Vanguard while fewer than a
	// full rotation are released (provisional, ADR-010 §11).
	StandInAllReleased StandIn = "allReleased"
	// StandInNone offers nothing through rotation.
	StandInNone StandIn = "none"
)

// Settings are the catalog's validated configuration.
type Settings struct {
	// Released are the Vanguards players may own and pick, in display order.
	Released []string
	// Starters are the Vanguards a new player may choose from, all released.
	Starters []string
	// RotationSlots is how many Vanguards the weekly rotation offers
	// (Modes & Access Bible §3).
	RotationSlots int
	StandIn       StandIn
}

// Catalog answers questions about released Vanguards.
type Catalog struct {
	settings Settings
	released map[string]bool
	starters map[string]bool
}

// New builds a Catalog from validated settings.
func New(s Settings) *Catalog {
	c := &Catalog{settings: s, released: map[string]bool{}, starters: map[string]bool{}}
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

// Rotation returns what the rotation offers now. The weekly rotation needs
// at least RotationSlots released Vanguards (Modes & Access Bible §3); until
// it exists, the stand-in decides, and it offers nothing once that many are
// released, since a stand-in must not pose as the weekly rotation.
func (c *Catalog) Rotation() []string {
	if c.settings.StandIn == StandInAllReleased && len(c.settings.Released) < c.settings.RotationSlots {
		return c.Released()
	}
	return nil
}
