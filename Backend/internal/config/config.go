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
	"net"
	"net/url"
	"os"
	"regexp"
	"strconv"
	"strings"
	"time"
)

// EnvironmentLocal is the only environment in which development login may be enabled.
const EnvironmentLocal = "local"

// MaxLaunchCodeLifetime bounds launch-code lifetime. ADR-005 requires launch
// codes to expire within seconds; this is a security invariant, not tuning.
const MaxLaunchCodeLifetime = time.Minute

// MaxPartySize is the largest party the Parties & Social Bible §1 allows
// ("one through five players"). party.maxSize may be lower, never higher.
const MaxPartySize = 5

// DatabaseURLEnv names the environment variable holding the Postgres URL.
const DatabaseURLEnv = "VEYRA_DATABASE_URL"

// Allocator kinds (ADR-007 §11).
const (
	AllocatorDocker = "docker"
	AllocatorNone   = "none"
)

// MinDockerAPIMinor is the oldest Docker Engine API version (1.44) the
// allocator speaks. It is a protocol requirement of its requests, not tuning.
const MinDockerAPIMinor = 44

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
	Matches               Matches
	Allocator             Allocator
}

// Matches configures match lifecycles (ADR-007 §8, §10).
type Matches struct {
	// DevCreate enables the development-only match-creation endpoint.
	DevCreate         bool
	ReadyTimeout      time.Duration
	MaxDuration       time.Duration
	ReapInterval      time.Duration
	RemoveServerAfter time.Duration
}

// Allocator configures how match servers are started (ADR-007 §11).
type Allocator struct {
	Kind string
	// Docker is set when Kind is AllocatorDocker.
	Docker *DockerAllocator
}

// DockerAllocator configures the local Docker allocator.
type DockerAllocator struct {
	// Endpoint is the Docker Engine API address: unix:///path or tcp://host:port.
	Endpoint            string
	APIVersion          string
	RequestTimeout      time.Duration
	Image               string
	Network             string
	ContainerNamePrefix string
	ContainerPort       int
	HostPortMin         int
	HostPortMax         int
	HostIP              string
	// PublicHost is the host players connect to.
	PublicHost string
	// BackendURL is the backend's address as match servers reach it.
	BackendURL string
	// ServerArgs are the server's command-line arguments; the allocator adds
	// the switch that tells it to read its assignment from standard input.
	ServerArgs  []string
	StopTimeout time.Duration
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
	Matches *struct {
		DevCreate *struct {
			Enabled *bool `json:"enabled"`
		} `json:"devCreate"`
		ReadyTimeout      *Duration `json:"readyTimeout"`
		MaxDuration       *Duration `json:"maxDuration"`
		ReapInterval      *Duration `json:"reapInterval"`
		RemoveServerAfter *Duration `json:"removeServerAfter"`
	} `json:"matches"`
	Allocator *struct {
		Kind   *string           `json:"kind"`
		Docker *fileDockerConfig `json:"docker"`
	} `json:"allocator"`
}

type fileDockerConfig struct {
	Endpoint            *string   `json:"endpoint"`
	APIVersion          *string   `json:"apiVersion"`
	RequestTimeout      *Duration `json:"requestTimeout"`
	Image               *string   `json:"image"`
	Network             *string   `json:"network"`
	ContainerNamePrefix *string   `json:"containerNamePrefix"`
	ContainerPort       *int      `json:"containerPort"`
	HostPorts           *struct {
		Min *int `json:"min"`
		Max *int `json:"max"`
	} `json:"hostPorts"`
	HostIP      *string   `json:"hostIp"`
	PublicHost  *string   `json:"publicHost"`
	BackendURL  *string   `json:"backendUrl"`
	ServerArgs  []string  `json:"serverArgs"`
	StopTimeout *Duration `json:"stopTimeout"`
}

var (
	dockerAPIVersionPattern    = regexp.MustCompile(`^1\.(\d+)$`)
	containerNamePrefixPattern = regexp.MustCompile(`^[a-zA-Z0-9][a-zA-Z0-9_.-]*$`)
)

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
		case *f.Party.MaxSize < 1 || *f.Party.MaxSize > MaxPartySize:
			problems = append(problems, fmt.Sprintf("party.maxSize must be between 1 and %d", MaxPartySize))
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

	if f.Allocator == nil || f.Allocator.Kind == nil {
		missing("allocator.kind")
	} else {
		c.Allocator.Kind = *f.Allocator.Kind
		switch c.Allocator.Kind {
		case AllocatorNone:
			if f.Allocator.Docker != nil {
				problems = append(problems, "allocator.docker must be absent when allocator.kind is \""+AllocatorNone+"\"")
			}
		case AllocatorDocker:
			if f.Allocator.Docker == nil {
				missing("allocator.docker")
			} else {
				c.Allocator.Docker = parseDocker(f.Allocator.Docker, missing, positive, func(p string) { problems = append(problems, p) })
			}
		default:
			problems = append(problems, "allocator.kind must be \""+AllocatorDocker+"\" or \""+AllocatorNone+"\"")
		}
	}

	if f.Matches == nil {
		missing("matches")
	} else {
		if f.Matches.DevCreate == nil || f.Matches.DevCreate.Enabled == nil {
			missing("matches.devCreate.enabled")
		} else {
			c.Matches.DevCreate = *f.Matches.DevCreate.Enabled
			if c.Matches.DevCreate && c.Environment != EnvironmentLocal {
				problems = append(problems, "matches.devCreate.enabled is only allowed when environment is \""+EnvironmentLocal+"\"")
			}
			if c.Matches.DevCreate && c.Allocator.Kind == AllocatorNone {
				problems = append(problems, "matches.devCreate.enabled needs an allocator that starts servers")
			}
		}
		c.Matches.ReadyTimeout = positive("matches.readyTimeout", f.Matches.ReadyTimeout)
		c.Matches.MaxDuration = positive("matches.maxDuration", f.Matches.MaxDuration)
		c.Matches.ReapInterval = positive("matches.reapInterval", f.Matches.ReapInterval)
		c.Matches.RemoveServerAfter = positive("matches.removeServerAfter", f.Matches.RemoveServerAfter)
	}

	if len(problems) > 0 {
		return Config{}, errors.New("invalid config: " + strings.Join(problems, "; "))
	}
	return c, nil
}

// parseDocker validates the Docker allocator section. It reports problems
// through the callbacks and returns what it could read.
func parseDocker(f *fileDockerConfig, missing func(string), positive func(string, *Duration) time.Duration, problem func(string)) *DockerAllocator {
	const prefix = "allocator.docker."
	var d DockerAllocator
	text := func(field string, v *string) string {
		if v == nil || strings.TrimSpace(*v) == "" {
			missing(prefix + field)
			return ""
		}
		return *v
	}
	port := func(field string, v *int) int {
		if v == nil {
			missing(prefix + field)
			return 0
		}
		if *v < 1 || *v > 65535 {
			problem(prefix + field + " must be a port between 1 and 65535")
		}
		return *v
	}

	d.Endpoint = text("endpoint", f.Endpoint)
	if d.Endpoint != "" {
		u, err := url.Parse(d.Endpoint)
		switch {
		case err != nil:
			problem(prefix + "endpoint is not a URL")
		case u.Scheme == "unix" && u.Path != "":
		case u.Scheme == "tcp" && u.Host != "":
		default:
			problem(prefix + "endpoint must be unix:///path or tcp://host:port")
		}
	}
	d.APIVersion = text("apiVersion", f.APIVersion)
	if d.APIVersion != "" {
		m := dockerAPIVersionPattern.FindStringSubmatch(d.APIVersion)
		if m == nil {
			problem(prefix + "apiVersion must look like 1.44")
		} else if minor, _ := strconv.Atoi(m[1]); minor < MinDockerAPIMinor {
			problem(fmt.Sprintf("%sapiVersion must be at least 1.%d", prefix, MinDockerAPIMinor))
		}
	}
	d.RequestTimeout = positive(prefix+"requestTimeout", f.RequestTimeout)
	d.Image = text("image", f.Image)
	d.Network = text("network", f.Network)
	d.ContainerNamePrefix = text("containerNamePrefix", f.ContainerNamePrefix)
	if d.ContainerNamePrefix != "" && !containerNamePrefixPattern.MatchString(d.ContainerNamePrefix) {
		problem(prefix + "containerNamePrefix must start with a letter or digit and use only letters, digits, '_', '.' and '-'")
	}
	d.ContainerPort = port("containerPort", f.ContainerPort)
	if f.HostPorts == nil {
		missing(prefix + "hostPorts")
	} else {
		d.HostPortMin = port("hostPorts.min", f.HostPorts.Min)
		d.HostPortMax = port("hostPorts.max", f.HostPorts.Max)
		if f.HostPorts.Min != nil && f.HostPorts.Max != nil && d.HostPortMin > d.HostPortMax {
			problem(prefix + "hostPorts.min must not exceed hostPorts.max")
		}
	}
	d.HostIP = text("hostIp", f.HostIP)
	if d.HostIP != "" && net.ParseIP(d.HostIP) == nil {
		problem(prefix + "hostIp must be an IP address")
	}
	d.PublicHost = text("publicHost", f.PublicHost)
	d.BackendURL = text("backendUrl", f.BackendURL)
	if d.BackendURL != "" {
		u, err := url.Parse(d.BackendURL)
		if err != nil || (u.Scheme != "http" && u.Scheme != "https") || u.Host == "" || (u.Path != "" && u.Path != "/") {
			problem(prefix + "backendUrl must be an http or https URL with no path")
		}
	}
	if len(f.ServerArgs) == 0 {
		missing(prefix + "serverArgs")
	}
	for _, arg := range f.ServerArgs {
		if strings.TrimSpace(arg) == "" {
			problem(prefix + "serverArgs must not contain blank arguments")
		}
		d.ServerArgs = append(d.ServerArgs, arg)
	}
	d.StopTimeout = positive(prefix+"stopTimeout", f.StopTimeout)
	return &d
}
