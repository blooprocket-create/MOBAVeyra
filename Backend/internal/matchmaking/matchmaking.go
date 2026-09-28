// Package matchmaking owns the queue's matchmaker and Match Found: it forms
// proposed matches from queued parties, asks every player to accept, and on
// full acceptance opens the match's champion select (ADR-010 §10; Parties &
// Social Bible §2–3, §6). Parties stay the party package's: matchmaking moves
// them through its states through a small interface. Rules are applied to an
// in-memory Found inside a storage transaction, so each rule here is a pure
// function.
package matchmaking

import (
	"errors"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// State is where a proposed match is.
type State string

const (
	// Pending proposed matches wait for every player to accept.
	Pending State = "pending"
	// Accepted proposed matches had every player accept; their champion
	// select is open.
	Accepted State = "accepted"
	// Abandoned proposed matches fell through before champion select.
	Abandoned State = "abandoned"
)

// Decision is a player's answer to Match Found.
type Decision string

const (
	Undecided Decision = ""
	Accept    Decision = "accepted"
	Decline   Decision = "declined"
)

// AbandonReason says why a proposed match fell through.
type AbandonReason string

const (
	// AbandonDeclined: a player declined.
	AbandonDeclined AbandonReason = "declined"
	// AbandonTimedOut: the acceptance timer ended before everyone accepted.
	AbandonTimedOut AbandonReason = "timed_out"
	// AbandonPartyChanged: a party left matchmaking, for example because a
	// member left it.
	AbandonPartyChanged AbandonReason = "party_changed"
	// AbandonSelectFailed: champion select could not open.
	AbandonSelectFailed AbandonReason = "select_failed"
	// AbandonNoLongerMatched: two of its players now block each other
	// (Parties & Social Bible §6). Nobody is at fault; its name says no more,
	// so no player learns of another's block.
	AbandonNoLongerMatched AbandonReason = "no_longer_matched"
)

// Errors describing rule violations. Callers map them to response codes.
var (
	ErrFoundNotFound  = errors.New("no match found for the player")
	ErrAlreadyDecided = errors.New("the player already answered the other way")
	ErrFoundOver      = errors.New("the match found is over")
	ErrExpired        = errors.New("the acceptance timer has ended")
	ErrAlreadyFound   = errors.New("an account is already in a match found")
)

// FoundParty is one party in a proposed match, on its side.
type FoundParty struct {
	PartyID string
	Side    match.Side
}

// Seat is one player in a proposed match.
type Seat struct {
	AccountID string
	PartyID   string
	Side      match.Side
	Decision  Decision
}

// Found is one proposed match: Match Found (Parties & Social Bible §3).
type Found struct {
	ID   string
	Mode string
	// Parties and Seats are in side order: side A's, then side B's.
	Parties []FoundParty
	Seats   []Seat
	State   State
	// Deadline is when the acceptance timer ends, in server time.
	CreatedAt time.Time
	Deadline  time.Time
	EndedAt   time.Time
	// SelectID is the champion select an accepted match opened.
	SelectID      string
	AbandonReason AbandonReason
}

func (f *Found) seat(accountID string) (*Seat, bool) {
	for i := range f.Seats {
		if f.Seats[i].AccountID == accountID {
			return &f.Seats[i], true
		}
	}
	return nil, false
}

// Decide records a player's answer. Answering the same way twice changes
// nothing; the other way is refused.
func (f *Found) Decide(accountID string, decision Decision, now time.Time) error {
	seat, ok := f.seat(accountID)
	if !ok {
		return ErrFoundNotFound
	}
	if f.State != Pending {
		return ErrFoundOver
	}
	if !now.Before(f.Deadline) {
		return ErrExpired
	}
	switch seat.Decision {
	case decision:
		return nil
	case Undecided:
		seat.Decision = decision
		return nil
	default:
		return ErrAlreadyDecided
	}
}

// AllAccepted reports whether every player has accepted.
func (f *Found) AllAccepted() bool {
	for _, seat := range f.Seats {
		if seat.Decision != Accept {
			return false
		}
	}
	return true
}

// Counts returns how many players have accepted, and how many there are.
func (f *Found) Counts() (accepted, total int) {
	for _, seat := range f.Seats {
		if seat.Decision == Accept {
			accepted++
		}
	}
	return accepted, len(f.Seats)
}

// PartyAtFault reports whether a party is why the match fell through, so it
// leaves the queue instead of returning to it (§3): a member declined or, when
// the timer ran out, had not accepted. A failed champion select faults no one
// but returns everyone to idle; a new block faults no one, and everyone
// returns to the queue.
func (f *Found) PartyAtFault(partyID string, reason AbandonReason) bool {
	if reason == AbandonSelectFailed {
		return true
	}
	for _, seat := range f.Seats {
		if seat.PartyID != partyID {
			continue
		}
		if seat.Decision == Decline || (reason == AbandonTimedOut && seat.Decision != Accept) {
			return true
		}
	}
	return false
}

// Abandon ends a pending proposed match without a champion select.
func (f *Found) Abandon(reason AbandonReason, now time.Time) {
	if f.State != Pending {
		return
	}
	f.State, f.AbandonReason, f.EndedAt = Abandoned, reason, now
}

// Assemble records the champion select a fully accepted match opened.
func (f *Found) Assemble(selectID string, now time.Time) {
	f.State, f.SelectID, f.EndedAt = Accepted, selectID, now
}

// Accounts returns every player in the proposed match.
func (f *Found) Accounts() []string {
	ids := make([]string, len(f.Seats))
	for i, seat := range f.Seats {
		ids[i] = seat.AccountID
	}
	return ids
}

// PartyIDs returns the ID of each party in the proposed match.
func (f *Found) PartyIDs() []string {
	ids := make([]string, len(f.Parties))
	for i, p := range f.Parties {
		ids[i] = p.PartyID
	}
	return ids
}
