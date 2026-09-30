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

const (
	// KindPractice is solo Custom practice: its host alone, picking from their
	// available Vanguards (ADR-010 §7).
	KindPractice Kind = "practice"
	// KindCasual is Casual Select (Battleground Bible §15): the players of a
	// match everyone accepted pick at once, with no bans.
	KindCasual Kind = "casual"
	// KindCustom is a custom lobby's select (ADR-021 §2): its humans pick at
	// once, blind, beside the bots the host placed; each Vanguard once per
	// side, bots included. It ends back in the lobby unless it starts a match.
	KindCustom Kind = "custom"
)

// CancelReason says why a select ended without a match.
type CancelReason string

const (
	// CancelTimedOut: the pick timer ended with a seat that had nothing to lock.
	CancelTimedOut CancelReason = "timed_out"
	// CancelAllocationFailed: the match could not be created.
	CancelAllocationFailed CancelReason = "allocation_failed"
	// CancelStartingTimedOut: creating the match never finished.
	CancelStartingTimedOut CancelReason = "starting_timed_out"
	// CancelLeft: a player left champion select, a dodge (Match Flow Bible §2).
	CancelLeft CancelReason = "left"
	// CancelPresenceLost: a player stopped answering, a disconnect (Match Flow
	// Bible §2).
	CancelPresenceLost CancelReason = "presence_lost"
	// CancelNoLongerMatched: two of its players now block each other (Parties
	// & Social Bible §6). Nobody is at fault; its name says no more, so no
	// player learns of another's block.
	CancelNoLongerMatched CancelReason = "no_longer_matched"
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
	ErrTaken            = errors.New("another player has locked that Vanguard")
	ErrCannotLeave      = errors.New("this champion select cannot be left")
)

// SetFluxSpells records the starting Flux Spells a seated player chose, while
// the select is picking, before or after lock-in; choosing never touches the
// timer (Pre-Game Client UX Bible 36). The caller has checked them against
// the roster.
func (s *Session) SetFluxSpells(accountID string, spells [2]string, now time.Time) error {
	seat, ok := s.seat(accountID)
	if !ok {
		return ErrSelectNotFound
	}
	if s.State != Picking {
		return ErrInvalidState
	}
	if !now.Before(s.Deadline) {
		return ErrExpired
	}
	if !match.ValidFluxSpells(spells) {
		return match.ErrInvalidFluxSpells
	}
	seat.FluxSpells, seat.FluxSpellsEdited = spells, true
	return nil
}

// FollowSavedFluxSpells gives a seat whose player has not chosen spells the
// saved loadout of the Vanguard it now hovers or locked.
func (s *Session) FollowSavedFluxSpells(accountID string, saved [2]string) {
	if seat, ok := s.seat(accountID); ok && !seat.FluxSpellsEdited {
		seat.FluxSpells = saved
	}
}

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
	// FluxSpells are the starting Flux Spells the player takes into the
	// match, in slot order, empty for an empty slot (ADR-015 §5). Free to
	// change until the match starts, before or after lock-in.
	FluxSpells [2]string
	// FluxSpellsEdited is whether the player chose them. Until they do, they
	// follow the saved loadout of the Vanguard the seat hovers or locked
	// (Pre-Game Client UX Bible 37).
	FluxSpellsEdited bool
	// LastSeen is when the player's client last asked about the select, its
	// presence while polling (ADR-010 §10).
	LastSeen time.Time
}

// Session is one champion select.
type Session struct {
	ID   string
	Kind Kind
	// Mode is the mode the match will record.
	Mode string
	// HostAccountID is the host of a practice or custom select; empty
	// otherwise.
	HostAccountID string
	// LobbyID is the custom lobby that launched the select; empty otherwise.
	LobbyID string
	// Bots are a custom select's bots, which the host chose in the lobby, in
	// each side's seat order.
	Bots []match.Bot
	// Custom is a custom select's session rules, for its match.
	Custom    *match.CustomSettings
	State     State
	Seats     []Seat
	CreatedAt time.Time
	// Deadline is when the pick timer ends, in server time.
	Deadline   time.Time
	StartingAt time.Time
	EndedAt    time.Time
	// MatchID is the match the select created, once started.
	MatchID      string
	CancelReason CancelReason
	// LeftBy is the player who left a select cancelled as CancelLeft: the
	// dodge's record. No penalty follows yet; Match Flow §2 sets no schedule.
	LeftBy string
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

// Taken reports whether a player other than accountID has locked the
// Vanguard. Picks are unique across both teams in PvP (Battleground Bible §15);
// in a custom select, within the picker's side, where its bots count too
// (ADR-021 §2).
func (s *Session) Taken(vanguardID, accountID string) bool {
	own, _ := s.seat(accountID)
	perSide := s.Kind == KindCustom && own != nil
	for _, seat := range s.Seats {
		if seat.AccountID != accountID && seat.Locked == vanguardID && (!perSide || seat.Side == own.Side) {
			return true
		}
	}
	for _, bot := range s.Bots {
		if bot.VanguardID == vanguardID && (!perSide || bot.Side == own.Side) {
			return true
		}
	}
	return false
}

// Hover records the Vanguard a player is considering. The caller has checked
// that they may pick it.
func (s *Session) Hover(accountID, vanguardID string, now time.Time) error {
	seat, err := s.checkPicking(accountID, now)
	if err != nil {
		return err
	}
	if s.Taken(vanguardID, accountID) {
		return ErrTaken
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
	if s.Taken(vanguardID, accountID) {
		return ErrTaken
	}
	seat.Hover, seat.Locked, seat.LockedAt = vanguardID, vanguardID, now
	return nil
}

// Seen records the player's client asking about the select.
func (s *Session) Seen(accountID string, now time.Time) {
	if seat, ok := s.seat(accountID); ok {
		seat.LastSeen = now
	}
}

// Absent returns the players whose clients have not asked about the select
// for longer than timeout.
func (s *Session) Absent(now time.Time, timeout time.Duration) []string {
	var absent []string
	for _, seat := range s.Seats {
		if now.Sub(seat.LastSeen) > timeout {
			absent = append(absent, seat.AccountID)
		}
	}
	return absent
}

// Unlocked returns the players who have not locked a Vanguard.
func (s *Session) Unlocked() []string {
	var out []string
	for _, seat := range s.Seats {
		if seat.Locked == "" {
			out = append(out, seat.AccountID)
		}
	}
	return out
}

// Accounts returns every seated player.
func (s *Session) Accounts() []string {
	out := make([]string, len(s.Seats))
	for i, seat := range s.Seats {
		out[i] = seat.AccountID
	}
	return out
}

// Leave cancels a Casual Select because a player left it: a dodge (Match Flow
// Bible §2). Leaving a custom select returns everyone to its lobby (ADR-021
// §2). A practice select cannot be left; only its timer ends it (ADR-010
// §11).
func (s *Session) Leave(accountID string, now time.Time) error {
	if _, ok := s.seat(accountID); !ok {
		return ErrSelectNotFound
	}
	if s.Kind != KindCasual && s.Kind != KindCustom {
		return ErrCannotLeave
	}
	if s.State != Picking {
		return ErrInvalidState
	}
	s.Cancel(CancelLeft, now)
	s.LeftBy = accountID
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
// locks it, unless someone locked it first, and a seat with nothing to lock
// cancels the select. It reports whether every seat is now locked.
func (s *Session) Expire(now time.Time) bool {
	for i := range s.Seats {
		if seat := &s.Seats[i]; seat.Locked == "" && seat.Hover != "" && !s.Taken(seat.Hover, seat.AccountID) {
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
