package config

import (
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
	"time"
)

const validJSON = `{
  "environment": "local",
  "listenAddress": ":8080",
  "requestBodyLimitBytes": 1024,
  "http": {"readTimeout": "5s", "writeTimeout": "5s", "idleTimeout": "30s", "shutdownTimeout": "5s"},
  "sessions": {"launcherLifetime": "720h", "gameLifetime": "24h"},
  "launchCodes": {"lifetime": "20s"},
  "devLogin": {"enabled": true, "accounts": ["DevOne", "DevTwo"]},
  "party": {"maxSize": 5, "inviteLifetime": "2m", "defaultPrivacy": "private"},
  "modes": [
    {"id": "casual_select", "enabled": true, "humanPlayersPerTeam": 5},
    {"id": "ranked", "enabled": false, "humanPlayersPerTeam": 5}
  ]
}`

func TestParseValid(t *testing.T) {
	c, err := Parse([]byte(validJSON))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	if c.LaunchCodeLifetime != 20*time.Second || c.Sessions.Game != 24*time.Hour {
		t.Fatalf("durations not parsed: %+v", c)
	}
	if !c.DevLogin.Enabled || len(c.DevLogin.Accounts) != 2 {
		t.Fatalf("dev login not parsed: %+v", c.DevLogin)
	}
}

func TestParseRejects(t *testing.T) {
	cases := map[string]struct {
		from, to, want string
	}{
		"unknown field":           {`"listenAddress"`, `"listenAddres"`, "unknown field"},
		"missing lifetime":        {`"launchCodes": {"lifetime": "20s"}`, `"launchCodes": {}`, "launchCodes.lifetime is required"},
		"launch code too long":    {`"lifetime": "20s"`, `"lifetime": "5m"`, "must not exceed"},
		"negative session":        {`"gameLifetime": "24h"`, `"gameLifetime": "-1h"`, "must be positive"},
		"dev login outside local": {`"environment": "local"`, `"environment": "staging"`, "only allowed when environment"},
		"duplicate dev account":   {`["DevOne", "DevTwo"]`, `["DevOne", "DevOne"]`, "duplicate"},
		"no dev accounts":         {`["DevOne", "DevTwo"]`, `[]`, "at least one account"},
		"party size zero":         {`"maxSize": 5`, `"maxSize": 0`, "party.maxSize must be at least 1"},
		"bad privacy":             {`"defaultPrivacy": "private"`, `"defaultPrivacy": "open"`, "party.defaultPrivacy must be"},
		"duplicate mode":          {`"id": "ranked"`, `"id": "casual_select"`, "duplicate id casual_select"},
		"mode missing team size":  {`"enabled": false, "humanPlayersPerTeam": 5`, `"enabled": false`, "humanPlayersPerTeam is required"},
	}
	for name, tc := range cases {
		t.Run(name, func(t *testing.T) {
			raw := strings.Replace(validJSON, tc.from, tc.to, 1)
			if raw == validJSON {
				t.Fatalf("test fixture did not change")
			}
			_, err := Parse([]byte(raw))
			if err == nil || !strings.Contains(err.Error(), tc.want) {
				t.Fatalf("want error containing %q, got %v", tc.want, err)
			}
		})
	}
}

// The committed local config must always load.
func TestCommittedLocalConfigLoads(t *testing.T) {
	_, file, _, _ := runtime.Caller(0)
	path := filepath.Join(filepath.Dir(file), "..", "..", "config", "local.json")
	if _, err := os.Stat(path); err != nil {
		t.Fatalf("local config missing: %v", err)
	}
	if _, err := Load(path); err != nil {
		t.Fatalf("Load(%s): %v", path, err)
	}
}
