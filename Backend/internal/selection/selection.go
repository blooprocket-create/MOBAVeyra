// Package selection owns champion select: a session in which each seated
// player hovers and locks a Vanguard before a server-side deadline, after
// which the backend creates the match exactly once (ADR-010 §8; Battleground
// Bible §15). Locks are permanent, and every choice is checked against what
// the player may pick. Rules are applied to an in-memory Session inside a
// storage transaction, so each rule here is a pure function.
package selection

import (
	"errors"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// State is where a champion select is in its life.
type State string

const (
	// Picking selects take hovers and locks until their deadline.
	Picking State = "picking"
	// Starting selects have every pick locked and are creating their match.
	Starting State = "starting"
	// Started selects created their match.
	Started State = "started"
	// Cancelled selects ended without a match.
	Cancelled State = "cancelled"
)

// Active reports whether the select still holds its players.
func (s State) Active() bool { return s == Picking || s == Starting }

// Kind says which rules a select follows.
type Kind string

// KindPractice is solo Custom practice: its host alone, picking from their
// available Vanguards (ADR-010 §7).
const KindPractice Kind = "practice"

// CancelReason says why a select ended without a match.
type CancelReason string

const (
	// CancelTimedOut: the pick timer ended with a seat that had nothing to lock.
	CancelTimedOut CancelReason = "timed_out"
	// CancelAllocationFailed: the match could not be created.
	CancelAllocationFailed CancelReason = "allocation_failed"
	// CancelStartingTimedOut: creating the match never finished.
	CancelStartingTimedOut CancelReason = "starting_timed_out"
)

// Errors describing rule violations. Callers map them to response codes.
var (
	ErrTutorialRequired = errors.New("the first-time tutorial comes first")
	ErrBusy             = errors.New("the player already has a match, a champion select or a queued party")
	ErrPracticeDisabled = errors.New("custom practice is disabled")
	ErrSelectNotFound   = errors.New("champion select not found")
	ErrNotAvailable     = errors.New("the player may not pick that Vanguard")
	ErrAlreadyLocked    = errors.New("the pick is already locked")
	ErrExpired          = errors.New("the pick timer has ended")
	ErrInvalidState     = errors.New("the champion select is not picking")
	ErrAlreadySelecting = errors.New("an account is already in a champion select")
)

// Seat is one player's place in a select.
type Seat struct {
	AccountID   string
	DisplayName string
	Side        match.Side
	// Hover is the Vanguard the player is considering; visible only to their
	// team (Battleground Bible §15).
	Hover string
	// Locked is the Vanguard the player locked in; permanent once set.
	Locked   string
	LockedAt time.Time
}

// Session is one champion select.
type Session struct {
	ID   string
	Kind Kind
	// Mode is the mode the match will record.
	Mode string
	// HostAccountID is the practice host; empty otherwise.
	HostAccountID string
	State         State
	Seats         []Seat
	CreatedAt     time.Time
	// Deadline is when the pick timer ends, in server time.
	Deadline   time.Time
	StartingAt time.Time
	EndedAt    time.Time
	// MatchID is the match the select created, once started.
	MatchID      string
	CancelReason CancelReason
}

// seat returns the account's seat.
func (s *Session) seat(accountID string) (*Seat, bool) {
	for i := range s.Seats {
		if s.Seats[i].AccountID == accountID {
			return &s.Seats[i], true
		}
	}
	return nil, false
}

// Has reports whether the account has a seat.
func (s *Session) Has(accountID string) bool {
	_, ok := s.seat(accountID)
	return ok
}

// checkPicking checks that a seated player may still change their pick.
func (s *Session) checkPicking(accountID string, now time.Time) (*Seat, error) {
	seat, ok := s.seat(accountID)
	if !ok {
		return nil, ErrSelectNotFound
	}
	if s.State != Picking {
		return nil, ErrInvalidState
	}
	if !now.Before(s.Deadline) {
		return nil, ErrExpired
	}
	if seat.Locked != "" {
		return nil, ErrAlreadyLocked
	}
	return seat, nil
}

// Hover records the Vanguard a player is considering. The caller has checked
// that they may pick it.
func (s *Session) Hover(accountID, vanguardID string, now time.Time) error {
	seat, err := s.checkPicking(accountID, now)
	if err != nil {
		return err
	}
	seat.Hover = vanguardID
	return nil
}

// Lock locks a player's Vanguard, permanently. The caller has checked that
// they may pick it.
func (s *Session) Lock(accountID, vanguardID string, now time.Time) error {
	seat, err := s.checkPicking(accountID, now)
	if err != nil {
		return err
	}
	seat.Hover, seat.Locked, seat.LockedAt = vanguardID, vanguardID, now
	return nil
}

// AllLocked reports whether every seat has locked a Vanguard.
func (s *Session) AllLocked() bool {
	for _, seat := range s.Seats {
		if seat.Locked == "" {
			return false
		}
	}
	return true
}

// Expire ends the pick timer (provisional, ADR-010 §11): a seat with a hover
// locks it, and a seat with nothing to lock cancels the select. It reports
// whether every seat is now locked.
func (s *Session) Expire(now time.Time) bool {
	for i := range s.Seats {
		if seat := &s.Seats[i]; seat.Locked == "" && seat.Hover != "" {
			seat.Locked, seat.LockedAt = seat.Hover, now
		}
	}
	if !s.AllLocked() {
		s.Cancel(CancelTimedOut, now)
		return false
	}
	return true
}

// BeginStarting moves a fully locked select on to creating its match.
func (s *Session) BeginStarting(now time.Time) {
	s.State, s.StartingAt = Starting, now
}

// Start records the match the select created.
func (s *Session) Start(matchID string, now time.Time) {
	s.State, s.MatchID, s.EndedAt = Started, matchID, now
}

// Cancel ends an active select without a match. It does nothing to a select
// that is already over.
func (s *Session) Cancel(reason CancelReason, now time.Time) {
	if !s.State.Active() {
		return
	}
	s.State, s.CancelReason, s.EndedAt = Cancelled, reason, now
}
