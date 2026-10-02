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
	"slices"
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

// MinStarters and MaxStarters bound the starter pool: a new player tries three
// to five starter Vanguards and unlocks one (Account, Collection & Mastery
// Bible §1).
const (
	MinStarters = 3
	MaxStarters = 5
)

// RotationWeekSeconds is the length of a rotation week the catalog accepts
// at least: one second, so a week always moves on.
const (
	MinRotationWeekSeconds = 1
)

// DatabaseURLEnv names the environment variable holding the Postgres URL.
const DatabaseURLEnv = "VEYRA_DATABASE_URL"

// Matchmaking kinds a mode may have (ADR-010 §10).
const (
	// MatchmakingCasualSelect: a matchmaker, then Match Found and Casual Select.
	MatchmakingCasualSelect = "casualSelect"
	// The Play page's categories a mode is listed under (ADR-039 §6).
	CategoryRanked = "ranked"
	CategoryCasual = "casual"
	CategoryAI     = "ai"

	// MatchmakingCoop: a matchmaker for one side of humans against an enemy AI
	// team, then Match Found and a Casual Select with the bots seated (ADR-039 §2).
	MatchmakingCoop = "coop"
	// MatchmakingDraftPick: a matchmaker, then Match Found and a Draft Pick
	// select of bans and picks in turns (ADR-042 §3).
	MatchmakingDraftPick = "draftPick"
	// MatchmakingNotImplemented: the mode may be selected but not queued yet.
	MatchmakingNotImplemented = "notImplemented"
)

// Player-login providers (ADR-038).
const (
	PlayerLoginFirebase = "firebase"
	PlayerLoginNone     = "none"
)

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
	PlayerLogin           PlayerLogin
	Party                 Party
	Modes                 []Mode
	Vanguards             Vanguards
	FluxSpells            FluxSpells
	CustomPractice        CustomPractice
	CustomLobby           CustomLobby
	Settings              Settings
	Matchmaking           Matchmaking
	MatchFound            MatchFound
	CasualSelect          CasualSelect
	// DraftPick is set when a mode uses draftPick matchmaking.
	DraftPick DraftPick
	Selection Selection
	Matches   Matches
	Allocator Allocator
	// Progression is account progression, the account currencies, the
	// storefront and Mastery (ADR-045).
	Progression Progression
	// Chat is Party Chat, friend messages, champion-select chat and
	// post-match chat (ADR-046).
	Chat Chat
	// Conduct is reports and commendation (ADR-047).
	Conduct Conduct
	// Profile is the official profile icons and backgrounds (ADR-048).
	Profile Profile
}

// Vanguards configures the catalog of Vanguards players may own and pick
// (ADR-010 §6). Released must equal Game/Tuning/Vanguards.json's Playable
// Vanguards; a contract test checks it.
type Vanguards struct {
	Released []string
	// Starters are the Vanguards a new player may choose, all released.
	Starters []string
	// RotationSlots is how many Vanguards the weekly rotation offers.
	RotationSlots int
	// RotationEpoch begins the first rotation week, and each week lasts
	// RotationWeek (ADR-039 §1).
	RotationEpoch time.Time
	RotationWeek  time.Duration
	// RotationSeed makes every week's draw reproducible.
	RotationSeed string
	// RotationReleases maps a Vanguard to its release time; one absent was
	// released before the epoch.
	RotationReleases map[string]time.Time
}

// FluxSpells configures the Flux Spells a player may choose in champion select
// (ADR-015 §5). Roster must equal Game/Tuning/Abilities.json's fluxSpells
// roster; a contract test checks it.
type FluxSpells struct {
	Roster []string
}

// CustomPractice configures solo Custom practice (ADR-010 §7). It is a custom
// match, not a matchmade mode, so its mode ID must not be one of modes.
type CustomPractice struct {
	Enabled bool
	// Mode is the mode ID practice matches record.
	Mode string
	// HostSide is the side the practising player plays on: "A" or "B".
	HostSide string
	// PickDuration is how long the player has to lock a Vanguard.
	PickDuration time.Duration
	// PlayersPerSide is how many Vanguards, the host and bots together, one
	// side of a practice match may hold.
	PlayersPerSide int
	// Bots are the AI participants every practice match adds, so its player
	// has targets (ADR-010 §7). Until custom lobbies let the host choose them,
	// they come from here.
	Bots []PracticeBot
}

// CustomLobby configures invite-only custom lobbies (ADR-021 §1). A custom
// match is not a matchmade mode, so its mode ID must not be one of modes.
type CustomLobby struct {
	Enabled bool
	// Mode is the mode ID custom matches record.
	Mode string
	// PlayersPerSide is how many Vanguards, humans and bots together, one side
	// of a lobby holds.
	PlayersPerSide int
	// PickDuration is how long the humans have to lock their Vanguards.
	PickDuration time.Duration
	// InviteLifetime is how long an invitation to a lobby stands.
	InviteLifetime time.Duration
	// StartingGold bounds the starting Gold a host may set. A lobby that sets
	// none plays with the game's own (Economy.json gold.starting).
	StartingGold GoldRange
}

// Settings configures the account settings documents (ADR-024 §1).
type Settings struct {
	// MaxDocumentBytes is the most JSON one account's settings may take. It
	// stays under RequestBodyLimitBytes, so a document that fits can be sent.
	MaxDocumentBytes int
}

// GoldRange is an inclusive range of Gold.
type GoldRange struct {
	Min float64
	Max float64
}

// PracticeBot is one AI participant in a practice match.
type PracticeBot struct {
	// Side is "A" or "B".
	Side string
	// VanguardID is the released Vanguard the bot plays.
	VanguardID string
	// Difficulty is "beginner" or "intermediate" (Custom Matches Bible §3).
	Difficulty string
}

// Matchmaking configures the matchmaker (ADR-010 §10).
type Matchmaking struct {
	// Interval is how often the matchmaker forms matches and settles Match Found.
	Interval time.Duration
	// SearchLimit is the most placements the matchmaker tries around one party
	// in a pass. It bounds the search when blocks make grouping combinatorial
	// (Parties & Social Bible §6: scale controls are future design).
	SearchLimit int
}

// MatchFound configures Match Found (Parties & Social Bible §3).
type MatchFound struct {
	// AcceptDuration is how long players have to accept a match found.
	AcceptDuration time.Duration
}

// CasualSelect configures Casual Select (ADR-010 §10; Battleground Bible §15).
type CasualSelect struct {
	// PickDuration is how long its players have to lock their Vanguards.
	PickDuration time.Duration
	// PresenceTimeout cancels a select a player's client has stopped polling
	// for this long: a disconnect.
	PresenceTimeout time.Duration
	// FinalDuration is the window after the last lock, in which locked
	// teammates may still trade (ADR-042 §2); zero starts the match at once.
	FinalDuration time.Duration
}

// DraftTurn is one turn of Draft Pick: Count bans or picks by Side.
type DraftTurn struct {
	Ban   bool
	Side  string
	Count int
}

// DraftPick configures Draft Pick (ADR-042 §1, §3; Battleground Bible).
type DraftPick struct {
	// Turns are the bans, then the picks, in order.
	Turns []DraftTurn
	// BanDuration and PickDuration are how long each ban or pick turn lasts;
	// FinalDuration is the window after the last pick.
	BanDuration     time.Duration
	PickDuration    time.Duration
	FinalDuration   time.Duration
	PresenceTimeout time.Duration
}

// Selection configures champion select's own upkeep (ADR-010 §8).
type Selection struct {
	// TickInterval is how often deadlines are checked.
	TickInterval time.Duration
	// StartingTimeout cancels a select whose match creation never finished;
	// it must exceed the allocator's request timeout.
	StartingTimeout time.Duration
}

// maxHistoryPageSize bounds a page of Match History, so one request stays
// small whatever the configuration.
const maxHistoryPageSize = 100

// Matches configures match lifecycles (ADR-007 §8, §10).
type Matches struct {
	// DevCreate enables the development-only match-creation endpoint.
	DevCreate         bool
	ReadyTimeout      time.Duration
	MaxDuration       time.Duration
	ReapInterval      time.Duration
	RemoveServerAfter time.Duration
	// HistoryPageSize is how many matches a page of Match History holds
	// (Pre-Game Client UX Bible 67).
	HistoryPageSize int
	// Maps are the maps match servers load (ADR-011 §12).
	Maps Maps
}

// Maps name the map each kind of match loads, as /Game/ paths.
type Maps struct {
	// Play is the battleground, for every match players make.
	Play string
	// Development is the grey box development matches keep by default.
	Development string
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
	ID      string
	Enabled bool
	// Category is the Play page's group for the mode: CategoryRanked,
	// CategoryCasual or CategoryAI. A co-op mode is always CategoryAI.
	Category            string
	HumanPlayersPerTeam int
	// Matchmaking is MatchmakingCasualSelect, MatchmakingCoop or
	// MatchmakingNotImplemented.
	Matchmaking string
	// AIPerTeam and AIDifficulty are a co-op mode's enemy AI team: how many
	// bots, and how they play ("beginner" or "intermediate"). Zero and empty for
	// any other mode.
	AIPerTeam    int
	AIDifficulty string
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

// PlayerLogin configures registration and sign-in through an external
// identity provider (ADR-038). Firebase is nil when Provider is "none".
type PlayerLogin struct {
	Provider string
	Firebase *FirebaseLogin
}

// FirebaseLogin configures verification of Firebase ID tokens.
type FirebaseLogin struct {
	// ProjectID is the Firebase project whose tokens are accepted.
	ProjectID string
	// KeysURL serves Google's certificates that sign ID tokens.
	KeysURL string
	// KeysFetchTimeout bounds one fetch of KeysURL.
	KeysFetchTimeout time.Duration
	// ClockSkew is the leeway on a token's expiry and issue times.
	ClockSkew time.Duration
}

// MaxClockSkew bounds firebase.clockSkew: more would accept long-expired tokens.
const MaxClockSkew = 5 * time.Minute

var firebaseProjectIDPattern = regexp.MustCompile(`^[a-z][a-z0-9-]{4,28}[a-z0-9]$`)

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
	PlayerLogin *struct {
		Provider *string `json:"provider"`
		Firebase *struct {
			ProjectID        *string   `json:"projectId"`
			KeysURL          *string   `json:"keysUrl"`
			KeysFetchTimeout *Duration `json:"keysFetchTimeout"`
			ClockSkew        *Duration `json:"clockSkew"`
		} `json:"firebase"`
	} `json:"playerLogin"`
	Party *struct {
		MaxSize        *int      `json:"maxSize"`
		InviteLifetime *Duration `json:"inviteLifetime"`
		DefaultPrivacy *string   `json:"defaultPrivacy"`
	} `json:"party"`
	Modes []struct {
		ID                  *string `json:"id"`
		Category            *string `json:"category"`
		Enabled             *bool   `json:"enabled"`
		HumanPlayersPerTeam *int    `json:"humanPlayersPerTeam"`
		Matchmaking         *string `json:"matchmaking"`
		AIPerTeam           *int    `json:"aiPerTeam"`
		AIDifficulty        *string `json:"aiDifficulty"`
	} `json:"modes"`
	Vanguards *struct {
		Released []string `json:"released"`
		Starters []string `json:"starters"`
		Rotation *struct {
			Slots       *int              `json:"slots"`
			Epoch       *string           `json:"epoch"`
			WeekSeconds *int64            `json:"weekSeconds"`
			Seed        *string           `json:"seed"`
			Releases    map[string]string `json:"releases"`
		} `json:"rotation"`
	} `json:"vanguards"`
	FluxSpells *struct {
		Roster []string `json:"roster"`
	} `json:"fluxSpells"`
	CustomPractice *struct {
		Enabled        *bool     `json:"enabled"`
		Mode           *string   `json:"mode"`
		HostSide       *string   `json:"hostSide"`
		PickDuration   *Duration `json:"pickDuration"`
		PlayersPerSide *int      `json:"playersPerSide"`
		Bots           *[]struct {
			Side       *string `json:"side"`
			VanguardID *string `json:"vanguardId"`
			Difficulty *string `json:"difficulty"`
		} `json:"bots"`
	} `json:"customPractice"`
	Settings *struct {
		MaxDocumentBytes *int `json:"maxDocumentBytes"`
	} `json:"settings"`
	CustomLobby *struct {
		Enabled        *bool     `json:"enabled"`
		Mode           *string   `json:"mode"`
		PlayersPerSide *int      `json:"playersPerSide"`
		PickDuration   *Duration `json:"pickDuration"`
		InviteLifetime *Duration `json:"inviteLifetime"`
		StartingGold   *struct {
			Min *float64 `json:"min"`
			Max *float64 `json:"max"`
		} `json:"startingGold"`
	} `json:"customLobby"`
	Matchmaking *struct {
		Interval    *Duration `json:"interval"`
		SearchLimit *int      `json:"searchLimit"`
	} `json:"matchmaking"`
	MatchFound *struct {
		AcceptDuration *Duration `json:"acceptDuration"`
	} `json:"matchFound"`
	CasualSelect *struct {
		PickDuration    *Duration `json:"pickDuration"`
		PresenceTimeout *Duration `json:"presenceTimeout"`
		FinalDuration   *Duration `json:"finalDuration"`
	} `json:"casualSelect"`
	DraftPick *struct {
		Turns []struct {
			Phase *string `json:"phase"`
			Side  *string `json:"side"`
			Count *int    `json:"count"`
		} `json:"turns"`
		BanDuration     *Duration `json:"banDuration"`
		PickDuration    *Duration `json:"pickDuration"`
		FinalDuration   *Duration `json:"finalDuration"`
		PresenceTimeout *Duration `json:"presenceTimeout"`
	} `json:"draftPick"`
	Selection *struct {
		TickInterval    *Duration `json:"tickInterval"`
		StartingTimeout *Duration `json:"startingTimeout"`
	} `json:"selection"`
	Matches *struct {
		DevCreate *struct {
			Enabled *bool `json:"enabled"`
		} `json:"devCreate"`
		ReadyTimeout      *Duration `json:"readyTimeout"`
		MaxDuration       *Duration `json:"maxDuration"`
		ReapInterval      *Duration `json:"reapInterval"`
		RemoveServerAfter *Duration `json:"removeServerAfter"`
		HistoryPageSize   *int      `json:"historyPageSize"`
		Maps              *struct {
			Play        *string `json:"play"`
			Development *string `json:"development"`
		} `json:"maps"`
	} `json:"matches"`
	Allocator *struct {
		Kind   *string           `json:"kind"`
		Docker *fileDockerConfig `json:"docker"`
	} `json:"allocator"`
	Progression *fileProgression `json:"progression"`
	Chat        *fileChat        `json:"chat"`
	Conduct     *fileConduct     `json:"conduct"`
	Profile     *fileProfile     `json:"profile"`
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

	// The formats the game accepts, so the backend refuses at startup what
	// every client or match server would refuse at the handoff: a server
	// address is a bare host name or IPv4 address (VeyraBackendProtocol.cpp),
	// and a backend URL is a scheme, a host and an optional port, with no
	// path, query, fragment or user info (MatchAssignment.schema.json).
	publicHostPattern = regexp.MustCompile(`^[A-Za-z0-9.-]+$`)
	backendURLPattern = regexp.MustCompile(`^https?://[A-Za-z0-9.-]+(:[0-9]{1,5})?$`)

	// contentIDPattern is the game's content ID format (Game/Tuning/README.md).
	contentIDPattern = regexp.MustCompile(`^[a-z][a-z0-9]*(_[a-z0-9]+)*$`)

	// mapPathPattern is an Unreal map's package path, which a server loads
	// first on its command line.
	mapPathPattern = regexp.MustCompile(`^/Game/[A-Za-z0-9_/]+$`)
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
	notNegative := func(field string, d *Duration) time.Duration {
		if d == nil {
			missing(field)
			return 0
		}
		if *d < 0 {
			problems = append(problems, field+" must not be negative")
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

	if f.PlayerLogin == nil || f.PlayerLogin.Provider == nil {
		missing("playerLogin.provider")
	} else {
		c.PlayerLogin.Provider = *f.PlayerLogin.Provider
		fb := f.PlayerLogin.Firebase
		switch c.PlayerLogin.Provider {
		case PlayerLoginNone:
			if fb != nil {
				problems = append(problems, "playerLogin.firebase must be absent when playerLogin.provider is \""+PlayerLoginNone+"\"")
			}
		case PlayerLoginFirebase:
			if fb == nil {
				missing("playerLogin.firebase")
				break
			}
			login := &FirebaseLogin{}
			switch {
			case fb.ProjectID == nil:
				missing("playerLogin.firebase.projectId")
			case !firebaseProjectIDPattern.MatchString(*fb.ProjectID):
				problems = append(problems, "playerLogin.firebase.projectId must be a Firebase project ID such as veyra-58ea4")
			default:
				login.ProjectID = *fb.ProjectID
			}
			if fb.KeysURL == nil {
				missing("playerLogin.firebase.keysUrl")
			} else if u, err := url.Parse(*fb.KeysURL); err != nil || u.Scheme != "https" || u.Host == "" || u.User != nil {
				problems = append(problems, "playerLogin.firebase.keysUrl must be an https URL")
			} else {
				login.KeysURL = *fb.KeysURL
			}
			login.KeysFetchTimeout = positive("playerLogin.firebase.keysFetchTimeout", fb.KeysFetchTimeout)
			if fb.ClockSkew == nil {
				missing("playerLogin.firebase.clockSkew")
			} else if d := time.Duration(*fb.ClockSkew); d < 0 || d > MaxClockSkew {
				problems = append(problems, fmt.Sprintf("playerLogin.firebase.clockSkew must be from 0s to %s", MaxClockSkew))
			} else {
				login.ClockSkew = d
			}
			c.PlayerLogin.Firebase = login
		default:
			problems = append(problems, "playerLogin.provider must be \""+PlayerLoginFirebase+"\" or \""+PlayerLoginNone+"\"")
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
		if m.Matchmaking == nil {
			missing(field + ".matchmaking")
			continue
		}
		if *m.Matchmaking != MatchmakingCasualSelect && *m.Matchmaking != MatchmakingCoop && *m.Matchmaking != MatchmakingDraftPick &&
			*m.Matchmaking != MatchmakingNotImplemented {
			problems = append(problems, field+".matchmaking must be \""+MatchmakingCasualSelect+"\", \""+MatchmakingCoop+"\", \""+MatchmakingDraftPick+
				"\" or \""+MatchmakingNotImplemented+"\"")
		}
		switch {
		case m.Category == nil:
			missing(field + ".category")
		case *m.Category != CategoryRanked && *m.Category != CategoryCasual && *m.Category != CategoryAI:
			problems = append(problems, field+".category must be \""+CategoryRanked+"\", \""+CategoryCasual+"\" or \""+CategoryAI+"\"")
		case (*m.Matchmaking == MatchmakingCoop) != (*m.Category == CategoryAI) && *m.Matchmaking != MatchmakingNotImplemented:
			// Humans against an enemy AI team are the AI category, and only they are.
			problems = append(problems, field+".category must be \""+CategoryAI+"\" exactly when its matchmaking is \""+MatchmakingCoop+"\"")
		}
		mode := Mode{ID: *m.ID, Enabled: *m.Enabled, HumanPlayersPerTeam: *m.HumanPlayersPerTeam, Matchmaking: *m.Matchmaking}
		if m.Category != nil {
			mode.Category = *m.Category
		}
		// A co-op mode's enemy AI team, and only a co-op mode's (ADR-039 §2).
		if *m.Matchmaking == MatchmakingCoop {
			switch {
			case m.AIPerTeam == nil:
				missing(field + ".aiPerTeam")
			case *m.AIPerTeam < 1:
				problems = append(problems, field+".aiPerTeam must be at least 1")
			default:
				mode.AIPerTeam = *m.AIPerTeam
			}
			switch {
			case m.AIDifficulty == nil:
				missing(field + ".aiDifficulty")
			case *m.AIDifficulty != "beginner" && *m.AIDifficulty != "intermediate":
				problems = append(problems, field+".aiDifficulty must be \"beginner\" or \"intermediate\"")
			default:
				mode.AIDifficulty = *m.AIDifficulty
			}
		} else if m.AIPerTeam != nil || m.AIDifficulty != nil {
			problems = append(problems, field+" has an AI team, which only a co-op mode has")
		}
		c.Modes = append(c.Modes, mode)
	}

	if f.Vanguards == nil {
		missing("vanguards")
	} else {
		ids := func(field string, list []string) []string {
			if len(list) == 0 {
				missing(field)
			}
			seen := map[string]bool{}
			for _, id := range list {
				if !contentIDPattern.MatchString(id) {
					problems = append(problems, field+" must hold content IDs such as cairn, not "+strconv.Quote(id))
				}
				if seen[id] {
					problems = append(problems, field+" contains duplicate "+id)
				}
				seen[id] = true
			}
			return append([]string(nil), list...)
		}
		c.Vanguards.Released = ids("vanguards.released", f.Vanguards.Released)
		c.Vanguards.Starters = ids("vanguards.starters", f.Vanguards.Starters)
		if n := len(c.Vanguards.Starters); n > 0 && (n < MinStarters || n > MaxStarters) {
			problems = append(problems, fmt.Sprintf("vanguards.starters must list %d to %d Vanguards (Account, Collection & Mastery Bible §1)", MinStarters, MaxStarters))
		}
		for _, id := range c.Vanguards.Starters {
			if !slices.Contains(c.Vanguards.Released, id) {
				problems = append(problems, "vanguards.starters must be released, and "+id+" is not in vanguards.released")
			}
		}
		if f.Vanguards.Rotation == nil {
			missing("vanguards.rotation")
		} else {
			switch {
			case f.Vanguards.Rotation.Slots == nil:
				missing("vanguards.rotation.slots")
			case *f.Vanguards.Rotation.Slots < 1:
				problems = append(problems, "vanguards.rotation.slots must be at least 1")
			default:
				c.Vanguards.RotationSlots = *f.Vanguards.Rotation.Slots
			}
			r := f.Vanguards.Rotation
			switch {
			case r.Epoch == nil:
				missing("vanguards.rotation.epoch")
			default:
				epoch, err := time.Parse(time.RFC3339, *r.Epoch)
				if err != nil {
					problems = append(problems, "vanguards.rotation.epoch must be an RFC 3339 time")
				}
				c.Vanguards.RotationEpoch = epoch
			}
			switch {
			case r.WeekSeconds == nil:
				missing("vanguards.rotation.weekSeconds")
			case *r.WeekSeconds < MinRotationWeekSeconds:
				problems = append(problems, fmt.Sprintf("vanguards.rotation.weekSeconds must be at least %d", MinRotationWeekSeconds))
			default:
				c.Vanguards.RotationWeek = time.Duration(*r.WeekSeconds) * time.Second
			}
			switch {
			case r.Seed == nil || *r.Seed == "":
				missing("vanguards.rotation.seed")
			default:
				c.Vanguards.RotationSeed = *r.Seed
			}
			if r.Releases == nil {
				missing("vanguards.rotation.releases")
			}
			c.Vanguards.RotationReleases = map[string]time.Time{}
			for id, at := range r.Releases {
				released, err := time.Parse(time.RFC3339, at)
				switch {
				case !slices.Contains(c.Vanguards.Released, id):
					problems = append(problems, "vanguards.rotation.releases names "+id+", which is not in vanguards.released")
				case err != nil:
					problems = append(problems, "vanguards.rotation.releases."+id+" must be an RFC 3339 time")
				default:
					c.Vanguards.RotationReleases[id] = released
				}
			}
		}
	}

	if f.FluxSpells == nil {
		missing("fluxSpells")
	} else {
		if len(f.FluxSpells.Roster) == 0 {
			missing("fluxSpells.roster")
		}
		seen := map[string]bool{}
		for _, id := range f.FluxSpells.Roster {
			if !contentIDPattern.MatchString(id) {
				problems = append(problems, "fluxSpells.roster must hold content IDs such as blink, not "+strconv.Quote(id))
			}
			if seen[id] {
				problems = append(problems, "fluxSpells.roster contains duplicate "+id)
			}
			seen[id] = true
		}
		c.FluxSpells.Roster = append([]string(nil), f.FluxSpells.Roster...)
	}

	if f.CustomPractice == nil {
		missing("customPractice")
	} else {
		if f.CustomPractice.Enabled == nil {
			missing("customPractice.enabled")
		} else {
			c.CustomPractice.Enabled = *f.CustomPractice.Enabled
		}
		switch {
		case f.CustomPractice.Mode == nil:
			missing("customPractice.mode")
		case !contentIDPattern.MatchString(*f.CustomPractice.Mode):
			problems = append(problems, "customPractice.mode must be a content ID such as custom_practice")
		case seenModes[*f.CustomPractice.Mode]:
			problems = append(problems, "customPractice.mode must not be a matchmade mode's id, so parties can never queue for it")
		default:
			c.CustomPractice.Mode = *f.CustomPractice.Mode
		}
		switch {
		case f.CustomPractice.HostSide == nil:
			missing("customPractice.hostSide")
		case *f.CustomPractice.HostSide != "A" && *f.CustomPractice.HostSide != "B":
			problems = append(problems, "customPractice.hostSide must be \"A\" or \"B\"")
		default:
			c.CustomPractice.HostSide = *f.CustomPractice.HostSide
		}
		c.CustomPractice.PickDuration = positive("customPractice.pickDuration", f.CustomPractice.PickDuration)
		switch {
		case f.CustomPractice.PlayersPerSide == nil:
			missing("customPractice.playersPerSide")
		case *f.CustomPractice.PlayersPerSide < 1:
			problems = append(problems, "customPractice.playersPerSide must be at least 1")
		default:
			c.CustomPractice.PlayersPerSide = *f.CustomPractice.PlayersPerSide
		}
		if f.CustomPractice.Bots == nil {
			missing("customPractice.bots")
		} else {
			// The host takes a place on its side; each bot plays a released Vanguard.
			perSide := map[string]int{c.CustomPractice.HostSide: 1}
			for i, b := range *f.CustomPractice.Bots {
				field := fmt.Sprintf("customPractice.bots[%d]", i)
				switch {
				case b.Side == nil:
					missing(field + ".side")
				case *b.Side != "A" && *b.Side != "B":
					problems = append(problems, field+".side must be \"A\" or \"B\"")
				case b.VanguardID == nil:
					missing(field + ".vanguardId")
				case !slices.Contains(c.Vanguards.Released, *b.VanguardID):
					problems = append(problems, field+".vanguardId must be in vanguards.released, and "+strconv.Quote(*b.VanguardID)+" is not")
				case b.Difficulty == nil:
					missing(field + ".difficulty")
				case *b.Difficulty != "beginner" && *b.Difficulty != "intermediate":
					problems = append(problems, field+".difficulty must be \"beginner\" or \"intermediate\"")
				default:
					perSide[*b.Side]++
					c.CustomPractice.Bots = append(c.CustomPractice.Bots, PracticeBot{Side: *b.Side, VanguardID: *b.VanguardID, Difficulty: *b.Difficulty})
				}
			}
			for _, side := range []string{"A", "B"} {
				if limit := c.CustomPractice.PlayersPerSide; limit > 0 && perSide[side] > limit {
					problems = append(problems, fmt.Sprintf("customPractice.bots put %d Vanguards on side %s, the host included, more than customPractice.playersPerSide (%d)",
						perSide[side], side, limit))
				}
			}
		}
	}

	switch {
	case f.Settings == nil || f.Settings.MaxDocumentBytes == nil:
		missing("settings.maxDocumentBytes")
	case *f.Settings.MaxDocumentBytes <= 0:
		problems = append(problems, "settings.maxDocumentBytes must be above 0")
	case c.RequestBodyLimitBytes > 0 && int64(*f.Settings.MaxDocumentBytes) >= c.RequestBodyLimitBytes:
		problems = append(problems, "settings.maxDocumentBytes must stay under requestBodyLimitBytes, so a document that fits can be sent")
	default:
		c.Settings.MaxDocumentBytes = *f.Settings.MaxDocumentBytes
	}

	if f.CustomLobby == nil {
		missing("customLobby")
	} else {
		if f.CustomLobby.Enabled == nil {
			missing("customLobby.enabled")
		} else {
			c.CustomLobby.Enabled = *f.CustomLobby.Enabled
		}
		switch {
		case f.CustomLobby.Mode == nil:
			missing("customLobby.mode")
		case !contentIDPattern.MatchString(*f.CustomLobby.Mode):
			problems = append(problems, "customLobby.mode must be a content ID such as custom_game")
		case seenModes[*f.CustomLobby.Mode]:
			problems = append(problems, "customLobby.mode must not be a matchmade mode's id, so parties can never queue for it")
		case c.CustomPractice.Mode != "" && *f.CustomLobby.Mode == c.CustomPractice.Mode:
			problems = append(problems, "customLobby.mode must differ from customPractice.mode, so results tell them apart")
		default:
			c.CustomLobby.Mode = *f.CustomLobby.Mode
		}
		switch {
		case f.CustomLobby.PlayersPerSide == nil:
			missing("customLobby.playersPerSide")
		case *f.CustomLobby.PlayersPerSide < 1:
			problems = append(problems, "customLobby.playersPerSide must be at least 1")
		default:
			c.CustomLobby.PlayersPerSide = *f.CustomLobby.PlayersPerSide
		}
		c.CustomLobby.PickDuration = positive("customLobby.pickDuration", f.CustomLobby.PickDuration)
		c.CustomLobby.InviteLifetime = positive("customLobby.inviteLifetime", f.CustomLobby.InviteLifetime)
		switch {
		case f.CustomLobby.StartingGold == nil || f.CustomLobby.StartingGold.Min == nil || f.CustomLobby.StartingGold.Max == nil:
			missing("customLobby.startingGold.min and .max")
		case *f.CustomLobby.StartingGold.Min < 0 || *f.CustomLobby.StartingGold.Max < *f.CustomLobby.StartingGold.Min:
			problems = append(problems, "customLobby.startingGold must have 0 <= min <= max")
		default:
			c.CustomLobby.StartingGold = GoldRange{Min: *f.CustomLobby.StartingGold.Min, Max: *f.CustomLobby.StartingGold.Max}
		}
	}

	if f.Matchmaking == nil {
		missing("matchmaking")
	} else {
		c.Matchmaking.Interval = positive("matchmaking.interval", f.Matchmaking.Interval)
		switch {
		case f.Matchmaking.SearchLimit == nil:
			missing("matchmaking.searchLimit")
		case *f.Matchmaking.SearchLimit < 1:
			problems = append(problems, "matchmaking.searchLimit must be at least 1")
		default:
			c.Matchmaking.SearchLimit = *f.Matchmaking.SearchLimit
		}
	}
	if f.MatchFound == nil {
		missing("matchFound")
	} else {
		c.MatchFound.AcceptDuration = positive("matchFound.acceptDuration", f.MatchFound.AcceptDuration)
	}
	if f.CasualSelect == nil {
		missing("casualSelect")
	} else {
		c.CasualSelect.PickDuration = positive("casualSelect.pickDuration", f.CasualSelect.PickDuration)
		c.CasualSelect.PresenceTimeout = positive("casualSelect.presenceTimeout", f.CasualSelect.PresenceTimeout)
		c.CasualSelect.FinalDuration = notNegative("casualSelect.finalDuration", f.CasualSelect.FinalDuration)
	}

	// Draft Pick's turns, for its modes (ADR-042 §1, §3): each side's picks
	// cover its whole team, so no seat waits for a turn that never comes.
	draftTeam := 0
	for _, m := range c.Modes {
		if m.Matchmaking == MatchmakingDraftPick {
			draftTeam = max(draftTeam, m.HumanPlayersPerTeam)
		}
	}
	switch {
	case f.DraftPick == nil && draftTeam > 0:
		missing("draftPick")
	case f.DraftPick != nil:
		d := f.DraftPick
		c.DraftPick.BanDuration = positive("draftPick.banDuration", d.BanDuration)
		c.DraftPick.PickDuration = positive("draftPick.pickDuration", d.PickDuration)
		c.DraftPick.FinalDuration = notNegative("draftPick.finalDuration", d.FinalDuration)
		c.DraftPick.PresenceTimeout = positive("draftPick.presenceTimeout", d.PresenceTimeout)
		if len(d.Turns) == 0 {
			missing("draftPick.turns")
		}
		picks := map[string]int{}
		for i, turn := range d.Turns {
			field := fmt.Sprintf("draftPick.turns[%d]", i)
			switch {
			case turn.Phase == nil || turn.Side == nil || turn.Count == nil:
				missing(field + ".phase, .side and .count")
				continue
			case *turn.Phase != "ban" && *turn.Phase != "pick":
				problems = append(problems, field+".phase must be \"ban\" or \"pick\"")
			case *turn.Side != "A" && *turn.Side != "B":
				problems = append(problems, field+".side must be \"A\" or \"B\"")
			case *turn.Count < 1:
				problems = append(problems, field+".count must be at least 1")
			}
			if *turn.Phase == "pick" {
				picks[*turn.Side] += *turn.Count
			}
			c.DraftPick.Turns = append(c.DraftPick.Turns, DraftTurn{Ban: *turn.Phase == "ban", Side: *turn.Side, Count: *turn.Count})
		}
		for _, side := range []string{"A", "B"} {
			if picks[side] < max(draftTeam, 1) {
				problems = append(problems, fmt.Sprintf("draftPick.turns give side %s %d pick(s), fewer than its team of %d", side, picks[side], max(draftTeam, 1)))
			}
		}
	}

	if f.Selection == nil {
		missing("selection")
	} else {
		c.Selection.TickInterval = positive("selection.tickInterval", f.Selection.TickInterval)
		c.Selection.StartingTimeout = positive("selection.startingTimeout", f.Selection.StartingTimeout)
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
		switch {
		case f.Matches.HistoryPageSize == nil:
			missing("matches.historyPageSize")
		case *f.Matches.HistoryPageSize < 1 || *f.Matches.HistoryPageSize > maxHistoryPageSize:
			problems = append(problems, fmt.Sprintf("matches.historyPageSize must be from 1 to %d", maxHistoryPageSize))
		default:
			c.Matches.HistoryPageSize = *f.Matches.HistoryPageSize
		}
		if f.Matches.Maps == nil {
			missing("matches.maps")
		} else {
			mapPath := func(field string, v *string) string {
				switch {
				case v == nil:
					missing("matches.maps." + field)
				case !mapPathPattern.MatchString(*v):
					problems = append(problems, "matches.maps."+field+" must be a map path such as /Game/Veyra/World/Maps/L_Battleground")
				default:
					return *v
				}
				return ""
			}
			c.Matches.Maps.Play = mapPath("play", f.Matches.Maps.Play)
			c.Matches.Maps.Development = mapPath("development", f.Matches.Maps.Development)
		}
	}

	c.Progression = parseProgression(f.Progression, c.Vanguards.Released, c.Environment, missing, func(s string) { problems = append(problems, s) })
	c.Chat = parseChat(f.Chat, missing, func(s string) { problems = append(problems, s) }, positive)
	c.Conduct = parseConduct(f.Conduct, missing, func(s string) { problems = append(problems, s) }, positive)
	c.Profile = parseProfile(f.Profile, c.Vanguards.Released, missing, func(s string) { problems = append(problems, s) })

	// A select waits for its match's creation, which waits for the allocator.
	if d := c.Allocator.Docker; d != nil && c.Selection.StartingTimeout > 0 && c.Selection.StartingTimeout <= d.RequestTimeout {
		problems = append(problems, "selection.startingTimeout must exceed allocator.docker.requestTimeout, or a slow server start would cancel its select")
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
	if d.PublicHost != "" && !publicHostPattern.MatchString(d.PublicHost) {
		problem(prefix + "publicHost must be a host name or IPv4 address, with no port")
	}
	d.BackendURL = text("backendUrl", f.BackendURL)
	if d.BackendURL != "" && !backendURLPattern.MatchString(d.BackendURL) {
		problem(prefix + "backendUrl must be an http or https URL with a host and an optional port only, such as http://backend:8080")
	}
	if len(f.ServerArgs) == 0 {
		missing(prefix + "serverArgs")
	}
	for _, arg := range f.ServerArgs {
		if strings.TrimSpace(arg) == "" {
			problem(prefix + "serverArgs must not contain blank arguments")
		}
		if mapPathPattern.MatchString(arg) {
			problem(prefix + "serverArgs must not name a map; matches.maps does, and the allocator puts it first")
		}
		d.ServerArgs = append(d.ServerArgs, arg)
	}
	d.StopTimeout = positive(prefix+"stopTimeout", f.StopTimeout)
	return &d
}
