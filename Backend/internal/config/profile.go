package config

import (
	"regexp"
	"slices"
	"strings"
)

// Profile is the official profile icons and backgrounds every account may
// choose (ADR-048 §2). Every value is Provisional.
type Profile struct {
	Icons             []string
	Backgrounds       []string
	DefaultIcon       string
	DefaultBackground string
}

// fileProfile is the profile catalog's file form (ADR-048 §2).
type fileProfile struct {
	Icons             []string `json:"icons"`
	Backgrounds       []string `json:"backgrounds"`
	DefaultIcon       string   `json:"defaultIcon"`
	DefaultBackground string   `json:"defaultBackground"`
}

// catalogEntryPattern is an icon's or background's ID.
var catalogEntryPattern = regexp.MustCompile(`^[a-z][a-z0-9_]{0,63}$`)

// vanguardEntryPrefix marks an entry drawn from a released Vanguard's art.
const vanguardEntryPrefix = "vanguard_"

// parseProfile validates the profile catalog: unique, well-formed IDs, each
// list holding its default, and every Vanguard entry naming a released
// Vanguard, reporting through missing and problem.
func parseProfile(f *fileProfile, released []string, missing, problem func(string)) Profile {
	var c Profile
	if f == nil {
		missing("profile")
		return c
	}
	entries := func(field string, list []string, def string) []string {
		if len(list) == 0 {
			problem("profile." + field + " must list at least one entry")
		}
		for i, id := range list {
			if !catalogEntryPattern.MatchString(id) {
				problem("profile." + field + " entries must be lowercase IDs such as \"default\"")
			}
			if slices.Contains(list[:i], id) {
				problem("profile." + field + " must not repeat " + id)
			}
			if v, ok := strings.CutPrefix(id, vanguardEntryPrefix); ok && !slices.Contains(released, v) {
				problem("profile." + field + " entry " + id + " must name a released Vanguard")
			}
		}
		if !slices.Contains(list, def) {
			problem("profile." + field + " must list its default, " + def)
		}
		return slices.Clone(list)
	}
	c.Icons = entries("icons", f.Icons, f.DefaultIcon)
	c.Backgrounds = entries("backgrounds", f.Backgrounds, f.DefaultBackground)
	c.DefaultIcon = f.DefaultIcon
	c.DefaultBackground = f.DefaultBackground
	return c
}
