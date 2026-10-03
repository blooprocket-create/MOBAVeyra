package config

import (
	"strings"
	"testing"
	"time"
)

func TestParsePresence(t *testing.T) {
	c, err := Parse([]byte(validJSON))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	want := Presence{OfflineAfter: 30 * time.Second, TouchEvery: 5 * time.Second, SweepInterval: 5 * time.Second, PostMatchGrace: 2 * time.Minute}
	if c.Presence != want {
		t.Fatalf("presence: %+v", c.Presence)
	}
}

func TestPresenceRejectsBadTuning(t *testing.T) {
	cases := []struct {
		name, old, new, want string
	}{
		{"no threshold", `"offlineAfter": "30s"`, `"offlineAfter": "0s"`, "presence.offlineAfter must be positive"},
		{"no sweep", `"sweepInterval": "5s"`, `"sweepInterval": "0s"`, "presence.sweepInterval must be positive"},
		{"writes too far apart", `"touchEvery": "5s"`, `"touchEvery": "30s"`, "presence.touchEvery must be shorter than presence.offlineAfter"},
		{"no grace", `"postMatchGrace": "2m"`, `"postMatchGrace": "0s"`, "presence.postMatchGrace must be positive"},
		{"no section", `"presence": {"offlineAfter": "30s", "touchEvery": "5s", "sweepInterval": "5s", "postMatchGrace": "2m"},`, ``, "presence is required"},
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
