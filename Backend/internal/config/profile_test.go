package config

import (
	"slices"
	"strings"
	"testing"
)

func TestParseProfile(t *testing.T) {
	c, err := Parse([]byte(validJSON))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if !slices.Equal(c.Profile.Icons, []string{"default", "vanguard_cairn"}) || !slices.Equal(c.Profile.Backgrounds, []string{"default", "vanguard_oriel"}) ||
		c.Profile.DefaultIcon != "default" || c.Profile.DefaultBackground != "default" {
		t.Fatalf("profile: %+v", c.Profile)
	}
}

func TestProfileRejectsABadCatalog(t *testing.T) {
	cases := []struct {
		name, old, new, want string
	}{
		{"no icons", `"icons": ["default", "vanguard_cairn"]`, `"icons": []`, "profile.icons must list at least one entry"},
		{"an ID that is not one", `"icons": ["default", "vanguard_cairn"]`, `"icons": ["default", "My Icon"]`, "lowercase IDs"},
		{"a repeated icon", `"icons": ["default", "vanguard_cairn"]`, `"icons": ["default", "default"]`, "must not repeat default"},
		{"an unreleased Vanguard", `"backgrounds": ["default", "vanguard_oriel"]`, `"backgrounds": ["default", "vanguard_nobody"]`, "vanguard_nobody must name a released Vanguard"},
		{"a default not listed", `"defaultIcon": "default"`, `"defaultIcon": "vanguard_oriel"`, "profile.icons must list its default, vanguard_oriel"},
		{"no section", `"profile": {"icons": ["default", "vanguard_cairn"], "backgrounds": ["default", "vanguard_oriel"], "defaultIcon": "default", "defaultBackground": "default"},`, ``, "profile is required"},
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
