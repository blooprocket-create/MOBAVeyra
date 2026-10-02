package config

import (
	"strings"
	"testing"
	"time"
)

func TestParseNames(t *testing.T) {
	c, err := Parse([]byte(validJSON))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if c.Names.RenameCooldown != 24*time.Hour || c.Names.ClaimAfter != 8760*time.Hour || c.Names.PriceFlux != 6000 || c.Names.PriceRefinedFlux != 600 {
		t.Fatalf("names: %+v", c.Names)
	}
}

func TestNamesRejectsBadTuning(t *testing.T) {
	cases := []struct {
		name, old, new, want string
	}{
		{"no cooldown", `"renameCooldown": "24h"`, `"renameCooldown": "0s"`, "names.renameCooldown must be positive"},
		{"no claim threshold", `"claimAfter": "8760h", `, ``, "names.claimAfter is required"},
		{"a free price", `"flux": 6000`, `"flux": 0`, "names.renamePrice.flux must be positive"},
		{"no price", `, "renamePrice": {"flux": 6000, "refinedFlux": 600}`, ``, "names.renamePrice is required"},
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
