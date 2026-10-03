package config

import (
	"strings"
	"testing"
)

func TestParseFavorites(t *testing.T) {
	c, err := Parse([]byte(validJSON))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if c.Favorites.MaxPerAccount != 64 {
		t.Fatalf("favorites: %+v", c.Favorites)
	}
}

func TestFavoritesRejectsBadTuning(t *testing.T) {
	cases := []struct {
		name, old, new, want string
	}{
		{"none allowed", `"maxPerAccount": 64`, `"maxPerAccount": 0`, "favorites.maxPerAccount must be at least 1"},
		{"no most", `"maxPerAccount": 64`, ``, "favorites.maxPerAccount is required"},
		{"no section", `"favorites": {"maxPerAccount": 64},`, ``, "favorites is required"},
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
