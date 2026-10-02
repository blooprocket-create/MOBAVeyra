package config

import (
	"slices"
	"strings"
	"testing"
	"time"
)

func TestParseConduct(t *testing.T) {
	c, err := Parse([]byte(validJSON))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if !slices.Equal(c.Conduct.Reasons, []string{"afk", "other"}) || c.Conduct.DetailsMaxCharacters != 500 || c.Conduct.ReportWindow != 336*time.Hour ||
		c.Conduct.CommendWindow != 10*time.Minute {
		t.Fatalf("conduct: %+v", c.Conduct)
	}
}

func TestConductRejectsBadTuning(t *testing.T) {
	cases := []struct {
		name, old, new, want string
	}{
		{"no reasons", `"reasons": ["afk", "other"]`, `"reasons": []`, "conduct.reasons must name at least one reason"},
		{"a reason that is not a code", `"reasons": ["afk", "other"]`, `"reasons": ["afk", "Other Thing"]`, "lowercase codes"},
		{"a repeated reason", `"reasons": ["afk", "other"]`, `"reasons": ["afk", "afk"]`, "must not repeat afk"},
		{"negative details", `"detailsMaxCharacters": 500`, `"detailsMaxCharacters": -1`, "conduct.detailsMaxCharacters must not be negative"},
		{"no report window", `"reportWindow": "336h"`, `"reportWindow": "0s"`, "conduct.reportWindow must be positive"},
		{"no commend window", `, "commendWindow": "10m"`, ``, "conduct.commendWindow is required"},
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
