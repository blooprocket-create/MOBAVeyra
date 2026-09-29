// Package match owns matches: who plays in one and on which side, the server
// that hosts it, and how it ended (ADR-007). Rules are applied to an in-memory
// Match value inside a storage transaction, so every rule here is a pure
// function that is unit-tested without a database or an allocator.
package match

import (
	"errors"
	"math"
	"regexp"
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
)

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
	// side's Prime Well (ADR-011 §13), the one end with a winner.
	EndPrimeWellDestroyed EndReason = "prime_well_destroyed"
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
	ErrUnknownMode      = errors.New("unknown or unavailable mode")
	ErrInvalidRules     = errors.New("unknown match rules")
	ErrInvalidMap       = errors.New("unknown map kind")
	ErrInvalidRoster    = errors.New("invalid roster")
	ErrInvalidVanguard  = errors.New("invalid Vanguard")
	ErrAccountNotFound  = errors.New("account not found")
	ErrAlreadyInMatch   = errors.New("an account is already in an active match")
	ErrSelectHasMatch   = errors.New("the champion select already created its match")
	ErrNoServerCapacity = errors.New("no match-server port is free")
	ErrAllocationFailed = errors.New("the match server could not be started")
	ErrMatchNotFound    = errors.New("match not found")
	ErrUnauthorized     = errors.New("invalid match-server credential")
	ErrInvalidState     = errors.New("the match is not in a state that allows this")
	ErrInvalidResult    = errors.New("invalid result")
	ErrResultConflict   = errors.New("a different result was already reported")
)

// Participant is one rostered account.
type Participant struct {
	AccountID   string
	DisplayName string
	Side        Side
	// VanguardID is the content ID of the Vanguard the participant plays. It
	// is empty only for matches created before matches carried Vanguards.
	VanguardID string
}

// contentIDPattern is the content ID format the game's tuning uses
// (Game/Tuning/README.md).
var contentIDPattern = regexp.MustCompile(`^[a-z][a-z0-9]*(_[a-z0-9]+)*$`)

// IsContentID reports whether s has the content ID format.
func IsContentID(s string) bool { return contentIDPattern.MatchString(s) }

// ParticipantResult is what the server reports about one participant.
type ParticipantResult struct {
	AccountID      string
	Joined         bool
	ConnectedAtEnd bool
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
	// Bots are the match's AI participants; only practice matches have any.
	Bots      []Bot
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
		// Only practice has a host to end it (ADR-010 §7).
		if m.Rules != RulesPractice {
			return ErrInvalidResult
		}
	case EndPrimeWellDestroyed:
		// Practice has no victory condition (ADR-011 §14).
		if m.Rules != RulesStandard {
			return ErrInvalidResult
		}
	default:
		return ErrInvalidResult
	}
	// A side wins exactly when it destroyed the other's Prime Well.
	if (r.EndReason == EndPrimeWellDestroyed) != (r.Winner != "") {
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
	}
	return nil
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
	return true
}
