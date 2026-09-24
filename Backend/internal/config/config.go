// Package config loads and validates the backend's configuration file.
//
// Every operational value (lifetimes, timeouts, limits, seeded accounts) comes
// from the file; nothing falls back to a silent default. Secrets such as the
// database URL come from the environment, never from the file.
package config

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"strings"
	"time"
)

// EnvironmentLocal is the only environment in which development login may be enabled.
const EnvironmentLocal = "local"

// MaxLaunchCodeLifetime bounds launch-code lifetime. ADR-005 requires launch
// codes to expire within seconds; this is a security invariant, not tuning.
const MaxLaunchCodeLifetime = time.Minute

// DatabaseURLEnv names the environment variable holding the Postgres URL.
const DatabaseURLEnv = "VEYRA_DATABASE_URL"

// Config is the validated backend configuration.
type Config struct {
	Environment           string
	ListenAddress         string
	RequestBodyLimitBytes int64
	HTTP                  HTTPTimeouts
	Sessions              SessionLifetimes
	LaunchCodeLifetime    time.Duration
	DevLogin              DevLogin
	Party                 Party
	Modes                 []Mode
}

// Party configures party rules (Parties & Social Bible §1).
type Party struct {
	MaxSize        int
	InviteLifetime time.Duration
	DefaultPrivacy string
}

// Mode is one matchmade mode's validated settings (Modes & Access Bible §1).
type Mode struct {
	ID                  string
	Enabled             bool
	HumanPlayersPerTeam int
}

// Party privacy values accepted in config.
const (
	PrivacyPrivate = "private"
	PrivacyPublic  = "public"
)

// HTTPTimeouts configures the HTTP server.
type HTTPTimeouts struct {
	Read     time.Duration
	Write    time.Duration
	Idle     time.Duration
	Shutdown time.Duration
}

// SessionLifetimes configures how long each session kind stays valid.
type SessionLifetimes struct {
	Launcher time.Duration
	Game     time.Duration
}

// DevLogin configures development-only passwordless login.
type DevLogin struct {
	Enabled  bool
	Accounts []string
}

// Duration is a JSON string such as "30s" or "24h".
type Duration time.Duration

// UnmarshalJSON parses a Go duration string.
func (d *Duration) UnmarshalJSON(b []byte) error {
	var s string
	if err := json.Unmarshal(b, &s); err != nil {
		return fmt.Errorf("duration must be a string such as \"30s\": %w", err)
	}
	parsed, err := time.ParseDuration(s)
	if err != nil {
		return err
	}
	*d = Duration(parsed)
	return nil
}

// fileConfig mirrors the JSON file. Pointers distinguish "missing" from "zero".
type fileConfig struct {
	Environment           *string `json:"environment"`
	ListenAddress         *string `json:"listenAddress"`
	RequestBodyLimitBytes *int64  `json:"requestBodyLimitBytes"`
	HTTP                  *struct {
		ReadTimeout     *Duration `json:"readTimeout"`
		WriteTimeout    *Duration `json:"writeTimeout"`
		IdleTimeout     *Duration `json:"idleTimeout"`
		ShutdownTimeout *Duration `json:"shutdownTimeout"`
	} `json:"http"`
	Sessions *struct {
		LauncherLifetime *Duration `json:"launcherLifetime"`
		GameLifetime     *Duration `json:"gameLifetime"`
	} `json:"sessions"`
	LaunchCodes *struct {
		Lifetime *Duration `json:"lifetime"`
	} `json:"launchCodes"`
	DevLogin *struct {
		Enabled  *bool    `json:"enabled"`
		Accounts []string `json:"accounts"`
	} `json:"devLogin"`
	Party *struct {
		MaxSize        *int      `json:"maxSize"`
		InviteLifetime *Duration `json:"inviteLifetime"`
		DefaultPrivacy *string   `json:"defaultPrivacy"`
	} `json:"party"`
	Modes []struct {
		ID                  *string `json:"id"`
		Enabled             *bool   `json:"enabled"`
		HumanPlayersPerTeam *int    `json:"humanPlayersPerTeam"`
	} `json:"modes"`
}

// Load reads and validates the configuration file at path.
func Load(path string) (Config, error) {
	raw, err := os.ReadFile(path)
	if err != nil {
		return Config{}, fmt.Errorf("read config: %w", err)
	}
	return Parse(raw)
}

// Parse validates configuration JSON. Unknown fields are rejected so typos fail loudly.
func Parse(raw []byte) (Config, error) {
	dec := json.NewDecoder(bytes.NewReader(raw))
	dec.DisallowUnknownFields()
	var f fileConfig
	if err := dec.Decode(&f); err != nil {
		return Config{}, fmt.Errorf("parse config: %w", err)
	}

	var problems []string
	missing := func(field string) { problems = append(problems, field+" is required") }
	positive := func(field string, d *Duration) time.Duration {
		if d == nil {
			missing(field)
			return 0
		}
		if *d <= 0 {
			problems = append(problems, field+" must be positive")
		}
		return time.Duration(*d)
	}

	var c Config
	if f.Environment == nil || *f.Environment == "" {
		missing("environment")
	} else {
		c.Environment = *f.Environment
	}
	if f.ListenAddress == nil || *f.ListenAddress == "" {
		missing("listenAddress")
	} else {
		c.ListenAddress = *f.ListenAddress
	}
	if f.RequestBodyLimitBytes == nil {
		missing("requestBodyLimitBytes")
	} else if *f.RequestBodyLimitBytes <= 0 {
		problems = append(problems, "requestBodyLimitBytes must be positive")
	} else {
		c.RequestBodyLimitBytes = *f.RequestBodyLimitBytes
	}

	if f.HTTP == nil {
		missing("http")
	} else {
		c.HTTP.Read = positive("http.readTimeout", f.HTTP.ReadTimeout)
		c.HTTP.Write = positive("http.writeTimeout", f.HTTP.WriteTimeout)
		c.HTTP.Idle = positive("http.idleTimeout", f.HTTP.IdleTimeout)
		c.HTTP.Shutdown = positive("http.shutdownTimeout", f.HTTP.ShutdownTimeout)
	}

	if f.Sessions == nil {
		missing("sessions")
	} else {
		c.Sessions.Launcher = positive("sessions.launcherLifetime", f.Sessions.LauncherLifetime)
		c.Sessions.Game = positive("sessions.gameLifetime", f.Sessions.GameLifetime)
	}

	if f.LaunchCodes == nil {
		missing("launchCodes")
	} else {
		c.LaunchCodeLifetime = positive("launchCodes.lifetime", f.LaunchCodes.Lifetime)
		if c.LaunchCodeLifetime > MaxLaunchCodeLifetime {
			problems = append(problems, fmt.Sprintf("launchCodes.lifetime must not exceed %s (ADR-005)", MaxLaunchCodeLifetime))
		}
	}

	if f.DevLogin == nil || f.DevLogin.Enabled == nil {
		missing("devLogin.enabled")
	} else {
		c.DevLogin.Enabled = *f.DevLogin.Enabled
		seen := map[string]bool{}
		for _, name := range f.DevLogin.Accounts {
			if strings.TrimSpace(name) == "" {
				problems = append(problems, "devLogin.accounts must not contain blank names")
				continue
			}
			if seen[name] {
				problems = append(problems, "devLogin.accounts contains duplicate "+name)
			}
			seen[name] = true
			c.DevLogin.Accounts = append(c.DevLogin.Accounts, name)
		}
		if c.DevLogin.Enabled && c.Environment != EnvironmentLocal {
			problems = append(problems, "devLogin.enabled is only allowed when environment is \""+EnvironmentLocal+"\"")
		}
		if c.DevLogin.Enabled && len(c.DevLogin.Accounts) == 0 {
			problems = append(problems, "devLogin.accounts must list at least one account when dev login is enabled")
		}
	}

	if f.Party == nil {
		missing("party")
	} else {
		switch {
		case f.Party.MaxSize == nil:
			missing("party.maxSize")
		case *f.Party.MaxSize < 1:
			problems = append(problems, "party.maxSize must be at least 1")
		default:
			c.Party.MaxSize = *f.Party.MaxSize
		}
		c.Party.InviteLifetime = positive("party.inviteLifetime", f.Party.InviteLifetime)
		switch {
		case f.Party.DefaultPrivacy == nil:
			missing("party.defaultPrivacy")
		case *f.Party.DefaultPrivacy != PrivacyPrivate && *f.Party.DefaultPrivacy != PrivacyPublic:
			problems = append(problems, "party.defaultPrivacy must be \""+PrivacyPrivate+"\" or \""+PrivacyPublic+"\"")
		default:
			c.Party.DefaultPrivacy = *f.Party.DefaultPrivacy
		}
	}

	if len(f.Modes) == 0 {
		missing("modes")
	}
	seenModes := map[string]bool{}
	for i, m := range f.Modes {
		field := fmt.Sprintf("modes[%d]", i)
		if m.ID == nil || strings.TrimSpace(*m.ID) == "" {
			missing(field + ".id")
			continue
		}
		if seenModes[*m.ID] {
			problems = append(problems, "modes contains duplicate id "+*m.ID)
		}
		seenModes[*m.ID] = true
		if m.Enabled == nil {
			missing(field + ".enabled")
			continue
		}
		if m.HumanPlayersPerTeam == nil {
			missing(field + ".humanPlayersPerTeam")
			continue
		}
		if *m.HumanPlayersPerTeam < 1 {
			problems = append(problems, field+".humanPlayersPerTeam must be at least 1")
		}
		c.Modes = append(c.Modes, Mode{ID: *m.ID, Enabled: *m.Enabled, HumanPlayersPerTeam: *m.HumanPlayersPerTeam})
	}

	if len(problems) > 0 {
		return Config{}, errors.New("invalid config: " + strings.Join(problems, "; "))
	}
	return c, nil
}
