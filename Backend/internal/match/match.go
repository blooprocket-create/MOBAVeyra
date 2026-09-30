// Package match owns matches: who plays in one and on which side, the server
// that hosts it, and how it ended (ADR-007). Rules are applied to an in-memory
// Match value inside a storage transaction, so every rule here is a pure
// function that is unit-tested without a database or an allocator.
package match

import (
	"errors"
	"math"
	"regexp"
	"slices"
	"sort"
	"time"
)

// State is where a match is in its life.
type State string

const (
	// Allocating matches have a reserved server that has not reported ready.
	Allocating State = "allocating"
	// Ready matches have a server accepting their participants.
	Ready State = "ready"
	// Ended matches have a reported result.
	Ended State = "ended"
	// Failed matches ended without a result.
	Failed State = "failed"
)

// Active reports whether the match still holds its participants.
func (s State) Active() bool { return s == Allocating || s == Ready }

// Side is one of the battleground's two teams.
type Side string

const (
	SideA Side = "A"
	SideB Side = "B"
)

// Rules says which rules a match plays by (ADR-010 §7, §9).
type Rules string

const (
	// RulesStandard is a matchmade or developer match.
	RulesStandard Rules = "standard"
	// RulesPractice is solo Custom practice: its host alone, open-ended, and
	// ended by the host (Custom Matches Bible §1, §4).
	RulesPractice Rules = "practice"
	// RulesCustom is a custom lobby's match: its host, the humans and bots the
	// host placed, and the session's own rules (ADR-021 §3).
	RulesCustom Rules = "custom"
)

// HasHost reports whether matches under these rules have a host, who may end
// them (ADR-021 §3).
func (r Rules) HasHost() bool { return r == RulesPractice || r == RulesCustom }

// CustomSettings are the rules a custom lobby's host set for its one match
// (Custom Matches Bible §4; ADR-021 §1).
type CustomSettings struct {
	// VictoryEnabled is whether a fallen Prime Well wins; it needs a Vanguard
	// on each side.
	VictoryEnabled bool
	// StartingGold is each Vanguard's starting Gold; nil plays the game's own.
	StartingGold *float64
}

// CustomModeSettings configures custom matches: whether they may be created,
// the mode ID they record, a side's size, and the starting Gold a host may
// set.
type CustomModeSettings struct {
	Enabled         bool
	Mode            string
	PlayersPerSide  int
	StartingGoldMin float64
	StartingGoldMax float64
}

// MapKind names which map a match's server loads (ADR-011 §12). Configuration
// gives each kind its map, so a request never names a map path itself.
type MapKind string

const (
	// MapPlay is the battleground, where every player-made match plays.
	MapPlay MapKind = "play"
	// MapDevelopment is the one-lane grey box development matches keep, so the
	// smoke runs built on it keep their meaning.
	MapDevelopment MapKind = "development"
)

// EndReason says why a match ended with a result (ADR-007 §7–8).
type EndReason string

const (
	EndDeveloperRequest EndReason = "developer_request"
	EndAbandoned        EndReason = "abandoned"
	// EndHostEnded is a practice match its host ended (ADR-010 §7).
	EndHostEnded EndReason = "host_ended"
	// EndPrimeWellDestroyed is a standard match won by destroying the other
	// side's Prime Well (ADR-011 §13).
	EndPrimeWellDestroyed EndReason = "prime_well_destroyed"
	// EndSurrender is a standard match a side surrendered: the other side
	// wins (Match Flow Bible §8; ADR-019 §5).
	EndSurrender EndReason = "surrender"
	// EndRemake is a standard match its players voted to remake: no contest,
	// no winner (Match Flow Bible §7; ADR-019 §5).
	EndRemake EndReason = "remake"
)

// FailureReason says why a match ended without a result.
type FailureReason string

const (
	FailAllocation   FailureReason = "allocation_failed"
	FailReadyTimeout FailureReason = "ready_timeout"
	FailMaxDuration  FailureReason = "max_duration"
	FailServerExited FailureReason = "server_exited"
)

// Errors describing rule violations. Callers map them to response codes.
var (
	ErrUnknownMode     = errors.New("unknown or unavailable mode")
	ErrInvalidRules    = errors.New("unknown match rules")
	ErrInvalidMap      = errors.New("unknown map kind")
	ErrInvalidRoster   = errors.New("invalid roster")
	ErrInvalidVanguard = errors.New("invalid Vanguard")
	// ErrInvalidFluxSpells: a starting loadout off the roster, malformed, or
	// holding one spell twice (ADR-015 §5).
	ErrInvalidFluxSpells = errors.New("invalid Flux Spells")
	ErrAccountNotFound   = errors.New("account not found")
	ErrAlreadyInMatch    = errors.New("an account is already in an active match")
	ErrSelectHasMatch    = errors.New("the champion select already created its match")
	ErrNoServerCapacity  = errors.New("no match-server port is free")
	ErrAllocationFailed  = errors.New("the match server could not be started")
	ErrMatchNotFound     = errors.New("match not found")
	ErrUnauthorized      = errors.New("invalid match-server credential")
	ErrInvalidState      = errors.New("the match is not in a state that allows this")
	ErrInvalidResult     = errors.New("invalid result")
	ErrResultConflict    = errors.New("a different result was already reported")
)

// Participant is one rostered account.
type Participant struct {
	AccountID   string
	DisplayName string
	Side        Side
	// VanguardID is the content ID of the Vanguard the participant plays. It
	// is empty only for matches created before matches carried Vanguards.
	VanguardID string
	// FluxSpells are the participant's starting Flux Spells in slot order,
	// each a content ID or empty for an empty slot (ADR-015 §5).
	FluxSpells [2]string
}

// contentIDPattern is the content ID format the game's tuning uses
// (Game/Tuning/README.md).
var contentIDPattern = regexp.MustCompile(`^[a-z][a-z0-9]*(_[a-z0-9]+)*$`)

// IsContentID reports whether s has the content ID format.
func IsContentID(s string) bool { return contentIDPattern.MatchString(s) }

// ValidFluxSpells reports whether spells has the shape of a starting loadout:
// each slot a content ID or empty, and no spell in both (ADR-015 §5).
func ValidFluxSpells(spells [2]string) bool {
	for _, spell := range spells {
		if spell != "" && !IsContentID(spell) {
			return false
		}
	}
	return spells[0] == "" || spells[0] != spells[1]
}

// ParticipantResult is what the server reports about one participant.
type ParticipantResult struct {
	AccountID      string
	Joined         bool
	ConnectedAtEnd bool
	// PersonalLoss is a loss the player's own absence earned them, whatever
	// their team's result (Match Flow Bible §6; ADR-019 §3).
	PersonalLoss bool
	// AbsentSeconds is the player's total absence, AFK and disconnected, in
	// match seconds.
	AbsentSeconds float64
}

// Result is a match server's report of how its match ended (ADR-007 §7).
type Result struct {
	EndReason EndReason
	// Winner is the side that destroyed the other's Prime Well, and "" for
	// every other end.
	Winner Side
	// DurationSeconds is the match clock, which excludes pauses.
	DurationSeconds float64
	Participants    []ParticipantResult
	// Players is the scoreboard: every player's statistics and final
	// equipment, humans and bots (ADR-017 §5). Nil when the server sent none.
	Players []PlayerResult
	// Wells are the Flux Wells secured, in order; nil when the server sent none.
	Wells []WellCapture
}

// Server is the match server a match was given.
type Server struct {
	HostPort int
	// RemovedAt is when the server was removed; zero while it may still run.
	// Its port stays reserved until then.
	RemovedAt time.Time
}

// Mode is the match-relevant part of a mode's validated configuration.
type Mode struct {
	ID                  string
	Enabled             bool
	HumanPlayersPerTeam int
}

// PracticeSettings configures solo Custom practice (ADR-010 §7).
type PracticeSettings struct {
	Enabled bool
	// Mode is the mode ID practice matches record. No matchmade mode uses it.
	Mode string
	// HostSide is the side the practising player plays on.
	HostSide Side
	// Bots are the AI participants every practice match adds (Custom Matches
	// Bible §1), validated with the configuration: known sides, released
	// Vanguards, and room on each side.
	Bots []Bot
}

// Bot is an AI participant: a side and the Vanguard it plays. It is not an
// account, so it has no join ticket and no result; the match server adds it
// when the match starts.
type Bot struct {
	Side Side
	// VanguardID is the content ID of the Vanguard the bot plays.
	VanguardID string
	// Difficulty is how the bot plays (Custom Matches Bible §3; ADR-013 §6).
	Difficulty BotDifficulty
}

// BotDifficulty is one of the AI behaviours canon defines (Custom Matches
// Bible §3). It changes how a bot plays, never the rules (Modes & Access §4).
type BotDifficulty string

const (
	BotBeginner     BotDifficulty = "beginner"
	BotIntermediate BotDifficulty = "intermediate"
)

// Valid reports whether d is a defined difficulty.
func (d BotDifficulty) Valid() bool {
	return d == BotBeginner || d == BotIntermediate
}

// Match is the authoritative state of one match.
type Match struct {
	ID    string
	Mode  string
	Rules Rules
	// HostAccountID is the practice match's host; empty for standard rules.
	HostAccountID string
	// SelectID is the champion select that created the match, which creates
	// at most one; empty for development matches.
	SelectID     string
	State        State
	Participants []Participant
	// Bots are the match's AI participants; only hosted matches have any.
	Bots []Bot
	// Custom is the session's rules, present exactly for custom matches.
	Custom    *CustomSettings
	CreatedAt time.Time
	ReadyAt   time.Time
	EndedAt   time.Time
	// JoinKey derives the participants' join tickets. It is nil once the
	// match is over, which invalidates every ticket (ADR-007 §3).
	JoinKey []byte
	// ServerCredentialHash is the SHA-256 of the match server's credential.
	ServerCredentialHash []byte
	Server               Server
	FailureReason        FailureReason
	Result               *Result
}

// Participant returns the rostered participant for an account.
func (m *Match) Participant(accountID string) (Participant, bool) {
	for _, p := range m.Participants {
		if p.AccountID == accountID {
			return p, true
		}
	}
	return Participant{}, false
}

// ValidateRoster checks a requested standard roster against its mode: an
// enabled mode, at least one participant, known sides, no account twice, no
// side larger than the mode's team (Battleground Bible: two teams of up to
// five), and a Vanguard for everyone.
func ValidateRoster(mode Mode, participants []Participant) error {
	if !mode.Enabled {
		return ErrUnknownMode
	}
	if len(participants) == 0 {
		return ErrInvalidRoster
	}
	seen := map[string]bool{}
	perSide := map[Side]int{}
	for _, p := range participants {
		if p.AccountID == "" || seen[p.AccountID] {
			return ErrInvalidRoster
		}
		seen[p.AccountID] = true
		if p.Side != SideA && p.Side != SideB {
			return ErrInvalidRoster
		}
		perSide[p.Side]++
		if perSide[p.Side] > mode.HumanPlayersPerTeam {
			return ErrInvalidRoster
		}
		if !IsContentID(p.VanguardID) {
			return ErrInvalidVanguard
		}
	}
	return nil
}

// ValidatePractice checks a requested practice roster: practice is enabled,
// the mode is practice's, and the roster is its host alone on the host's side
// with a Vanguard (Custom Matches Bible §1).
func ValidatePractice(practice PracticeSettings, modeID, hostAccountID string, participants []Participant) error {
	if !practice.Enabled || modeID != practice.Mode {
		return ErrUnknownMode
	}
	if hostAccountID == "" || len(participants) != 1 {
		return ErrInvalidRoster
	}
	p := participants[0]
	if p.AccountID != hostAccountID || p.Side != practice.HostSide {
		return ErrInvalidRoster
	}
	if !IsContentID(p.VanguardID) {
		return ErrInvalidVanguard
	}
	return nil
}

// ValidateCustom checks a requested custom match (ADR-021 §2–§3): custom
// matches are enabled and the mode is theirs; the host plays; at least one
// human; each account once; known sides no fuller than a side; every
// Vanguard a content ID and each once on its side, bots included; known bot
// difficulties; and the session's rules, with starting Gold in range and
// victory only with a Vanguard on each side.
func ValidateCustom(custom CustomModeSettings, modeID, hostAccountID string, participants []Participant, bots []Bot, settings *CustomSettings) error {
	if !custom.Enabled || modeID != custom.Mode {
		return ErrUnknownMode
	}
	if settings == nil || hostAccountID == "" || len(participants) == 0 {
		return ErrInvalidRoster
	}
	seen := map[string]bool{}
	perSide := map[Side]int{}
	vanguards := map[Side]map[string]bool{SideA: {}, SideB: {}}
	place := func(side Side, vanguardID string) error {
		if side != SideA && side != SideB {
			return ErrInvalidRoster
		}
		if !IsContentID(vanguardID) {
			return ErrInvalidVanguard
		}
		perSide[side]++
		if perSide[side] > custom.PlayersPerSide || vanguards[side][vanguardID] {
			return ErrInvalidRoster
		}
		vanguards[side][vanguardID] = true
		return nil
	}
	hostPlays := false
	for _, p := range participants {
		if p.AccountID == "" || seen[p.AccountID] {
			return ErrInvalidRoster
		}
		seen[p.AccountID] = true
		hostPlays = hostPlays || p.AccountID == hostAccountID
		if err := place(p.Side, p.VanguardID); err != nil {
			return err
		}
	}
	if !hostPlays {
		return ErrInvalidRoster
	}
	for _, b := range bots {
		if !b.Difficulty.Valid() {
			return ErrInvalidRoster
		}
		if err := place(b.Side, b.VanguardID); err != nil {
			return err
		}
	}
	if g := settings.StartingGold; g != nil && (math.IsNaN(*g) || *g < custom.StartingGoldMin || *g > custom.StartingGoldMax) {
		return ErrInvalidRoster
	}
	if settings.VictoryEnabled && (perSide[SideA] == 0 || perSide[SideB] == 0) {
		return ErrInvalidRoster
	}
	return nil
}

// MarkReady records that the match server accepts players. Reporting ready
// again is harmless.
func (m *Match) MarkReady(now time.Time) error {
	switch m.State {
	case Allocating:
		m.State = Ready
		m.ReadyAt = now
		return nil
	case Ready:
		return nil
	default:
		return ErrInvalidState
	}
}

// End records the server's result. The same result reported again is
// accepted without change (replayed reports are harmless); a different one is
// a conflict. Ending erases the join key (ADR-007 §3, §6).
func (m *Match) End(r Result, now time.Time) error {
	if err := m.validateResult(r); err != nil {
		return err
	}
	switch m.State {
	case Ready:
	case Ended:
		if m.Result != nil && sameResult(*m.Result, r) {
			return nil
		}
		return ErrResultConflict
	default:
		return ErrInvalidState
	}
	stored := r
	stored.Participants = sortedResults(r.Participants)
	stored.Players = copyPlayers(r.Players)
	stored.Wells = slices.Clone(r.Wells)
	m.State = Ended
	m.EndedAt = now
	m.JoinKey = nil
	m.Result = &stored
	return nil
}

// Fail ends an active match without a result. It does nothing to a match
// that is already over.
func (m *Match) Fail(reason FailureReason, now time.Time) {
	if !m.State.Active() {
		return
	}
	m.State = Failed
	m.EndedAt = now
	m.JoinKey = nil
	m.FailureReason = reason
}

func (m *Match) validateResult(r Result) error {
	switch r.EndReason {
	case EndDeveloperRequest, EndAbandoned:
	case EndHostEnded:
		// Only a match with a host can be ended by it (ADR-010 §7; ADR-021 §3).
		if !m.Rules.HasHost() {
			return ErrInvalidResult
		}
	case EndPrimeWellDestroyed, EndSurrender:
		// A win needs a victory condition: a standard match's, or a custom
		// match's when its host left victory on (ADR-021 §3). Practice has
		// none (ADR-011 §14).
		if m.Rules != RulesStandard && (m.Rules != RulesCustom || m.Custom == nil || !m.Custom.VictoryEnabled) {
			return ErrInvalidResult
		}
	case EndRemake:
		// Remake votes are for matchmade matches (ADR-019 §9; ADR-021 §3).
		if m.Rules != RulesStandard {
			return ErrInvalidResult
		}
	default:
		return ErrInvalidResult
	}
	// A side wins exactly when it destroyed the other's Prime Well or the
	// other side surrendered.
	if (r.EndReason == EndPrimeWellDestroyed || r.EndReason == EndSurrender) != (r.Winner != "") {
		return ErrInvalidResult
	}
	if r.Winner != "" && r.Winner != SideA && r.Winner != SideB {
		return ErrInvalidResult
	}
	if math.IsNaN(r.DurationSeconds) || math.IsInf(r.DurationSeconds, 0) || r.DurationSeconds < 0 {
		return ErrInvalidResult
	}
	// Exactly the roster, each participant once.
	if len(r.Participants) != len(m.Participants) {
		return ErrInvalidResult
	}
	seen := map[string]bool{}
	for _, p := range r.Participants {
		if _, ok := m.Participant(p.AccountID); !ok || seen[p.AccountID] {
			return ErrInvalidResult
		}
		seen[p.AccountID] = true
		if p.ConnectedAtEnd && !p.Joined {
			return ErrInvalidResult
		}
		// Only one who joined can be absent, and never longer than the match.
		if math.IsNaN(p.AbsentSeconds) || p.AbsentSeconds < 0 || p.AbsentSeconds > r.DurationSeconds || (p.PersonalLoss && !p.Joined) {
			return ErrInvalidResult
		}
	}
	if err := validateWells(r.Wells, r.DurationSeconds); err != nil {
		return err
	}
	return m.validatePlayers(r.Players)
}

func sortedResults(in []ParticipantResult) []ParticipantResult {
	out := append([]ParticipantResult(nil), in...)
	sort.Slice(out, func(i, j int) bool { return out[i].AccountID < out[j].AccountID })
	return out
}

func sameResult(a, b Result) bool {
	if a.EndReason != b.EndReason || a.Winner != b.Winner || a.DurationSeconds != b.DurationSeconds {
		return false
	}
	pa, pb := sortedResults(a.Participants), sortedResults(b.Participants)
	if len(pa) != len(pb) {
		return false
	}
	for i := range pa {
		if pa[i] != pb[i] {
			return false
		}
	}
	return samePlayers(a.Players, b.Players) && (a.Wells == nil) == (b.Wells == nil) && slices.Equal(a.Wells, b.Wells)
}
