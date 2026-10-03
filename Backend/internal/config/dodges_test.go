package config

import (
	"strings"
	"testing"
	"time"
)

func TestParseDodges(t *testing.T) {
	c, err := Parse([]byte(validJSON))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if c.Dodges.Restriction != 5*time.Minute {
		t.Fatalf("dodges: %+v", c.Dodges)
	}
}

func TestDodgesRejectsBadTuning(t *testing.T) {
	cases := []struct {
		name, old, new, want string
	}{
		{"no length", `"restriction": "5m"`, `"restriction": "0s"`, "dodges.restriction must be positive"},
		{"no section", `"dodges": {"restriction": "5m"},`, ``, "dodges is required"},
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
