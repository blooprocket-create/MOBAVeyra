// Package party owns pre-game parties: membership, leader, privacy, selected
// mode, readiness, invitations and the queue lock (Parties & Social Bible
// §1–2). Rules are applied to an in-memory Party value inside a storage
// transaction, so every rule here is a pure function that is unit-tested
// without a database.
package party

import (
	"errors"
	"sort"
	"time"
)

// Privacy controls whether friends may join without an invitation.
type Privacy string

const (
	Private Privacy = "private"
	Public  Privacy = "public"
)

// Status is the party's matchmaking state.
type Status string

const (
	// Idle parties can change membership, mode and readiness.
	Idle Status = "idle"
	// Queued parties are locked: membership composition is fixed from the
	// moment the leader presses Find Match (§2).
	Queued Status = "queued"
)

// Errors describing rule violations. Callers map them to user-facing codes.
var (
	ErrNotInParty       = errors.New("not in a party")
	ErrAlreadyInParty   = errors.New("already in a party")
	ErrNotLeader        = errors.New("only the leader can do that")
	ErrNotMember        = errors.New("account is not a party member")
	ErrPartyFull        = errors.New("party is full")
	ErrPartyLocked      = errors.New("party is locked while queued")
	ErrNotAllReady      = errors.New("every member must be Ready")
	ErrNoMode           = errors.New("select a mode first")
	ErrUnknownMode      = errors.New("unknown or unavailable mode")
	ErrTooManyForMode   = errors.New("party is larger than the mode's team")
	ErrInvalidPrivacy   = errors.New("invalid privacy setting")
	ErrSelf             = errors.New("cannot target yourself")
	ErrNotFriends       = errors.New("not friends")
	ErrBlocked          = errors.New("blocked")
	ErrInviteNotFound   = errors.New("invitation not found or expired")
	ErrPartyNotJoinable = errors.New("party is not open to join")
	ErrPartyNotFound    = errors.New("party not found")
)

// Member is one party member.
type Member struct {
	AccountID string
	Ready     bool
	JoinedAt  time.Time
}

// Party is the authoritative party state.
type Party struct {
	ID       string
	LeaderID string
	Mode     string // "" until the leader selects one
	Privacy  Privacy
	Status   Status
	Members  []Member

	isNew bool // created in this transaction and not yet stored
}

// Mode is the party-relevant part of a mode's validated configuration.
type Mode struct {
	ID                  string
	Enabled             bool
	HumanPlayersPerTeam int
}

// Rules are the validated party settings the pure rules need.
type Rules struct {
	MaxSize int
	Modes   map[string]Mode
}

// IsMember reports whether accountID belongs to the party.
func (p *Party) IsMember(accountID string) bool { return p.index(accountID) >= 0 }

func (p *Party) index(accountID string) int {
	for i, m := range p.Members {
		if m.AccountID == accountID {
			return i
		}
	}
	return -1
}

func (p *Party) requireLeader(accountID string) error {
	if p.LeaderID != accountID {
		return ErrNotLeader
	}
	return nil
}

func (p *Party) resetReady() {
	for i := range p.Members {
		p.Members[i].Ready = false
	}
}

// Add puts a new member into an idle party with room. Adding a member resets
// everyone's Ready (§2). Capacity is enforced here, at acceptance (§1).
func (p *Party) Add(accountID string, rules Rules, now time.Time) error {
	if p.Status != Idle {
		return ErrPartyLocked
	}
	if p.IsMember(accountID) {
		return ErrAlreadyInParty
	}
	if len(p.Members) >= rules.MaxSize {
		return ErrPartyFull
	}
	p.Members = append(p.Members, Member{AccountID: accountID, JoinedAt: now})
	p.resetReady()
	return nil
}

// Remove takes a member out for any reason (leave, removal, offline). A
// departure while queued cancels the whole party's queue and resets
// readiness (§2); a departing leader hands leadership on (§1). It reports
// whether the party is now empty.
func (p *Party) Remove(accountID string) (empty bool, err error) {
	i := p.index(accountID)
	if i < 0 {
		return false, ErrNotMember
	}
	p.Members = append(p.Members[:i], p.Members[i+1:]...)
	if len(p.Members) == 0 {
		return true, nil
	}
	if p.Status == Queued {
		p.Status = Idle
		p.resetReady()
	}
	if p.LeaderID == accountID {
		p.LeaderID = replacementLeader(p.Members)
	}
	return false, nil
}

// replacementLeader picks who leads after the leader departs.
//
// PROVISIONAL: the Parties & Social Bible §1 leaves the selection rule open.
// Until it is ruled on, the longest-standing remaining member leads.
func replacementLeader(members []Member) string {
	sorted := append([]Member(nil), members...)
	sort.SliceStable(sorted, func(i, j int) bool { return sorted[i].JoinedAt.Before(sorted[j].JoinedAt) })
	return sorted[0].AccountID
}

// Kick lets the leader remove another member. Removal carries no penalty.
func (p *Party) Kick(leaderID, targetID string) error {
	if err := p.requireLeader(leaderID); err != nil {
		return err
	}
	if leaderID == targetID {
		return ErrSelf
	}
	_, err := p.Remove(targetID)
	return err
}

// SetReady marks a member Ready or not. Readiness is fixed while queued.
func (p *Party) SetReady(accountID string, ready bool) error {
	i := p.index(accountID)
	if i < 0 {
		return ErrNotMember
	}
	if p.Status != Idle {
		return ErrPartyLocked
	}
	p.Members[i].Ready = ready
	return nil
}

// SetMode lets the leader choose an enabled mode; it resets everyone's Ready.
func (p *Party) SetMode(leaderID, modeID string, rules Rules) error {
	if err := p.requireLeader(leaderID); err != nil {
		return err
	}
	if p.Status != Idle {
		return ErrPartyLocked
	}
	if m, ok := rules.Modes[modeID]; !ok || !m.Enabled {
		return ErrUnknownMode
	}
	if p.Mode != modeID {
		p.Mode = modeID
		p.resetReady()
	}
	return nil
}

// SetPrivacy lets the leader switch between Public and Private.
func (p *Party) SetPrivacy(leaderID string, privacy Privacy) error {
	if err := p.requireLeader(leaderID); err != nil {
		return err
	}
	if privacy != Public && privacy != Private {
		return ErrInvalidPrivacy
	}
	p.Privacy = privacy
	return nil
}

// TransferLeader hands leadership to another member. Membership, mode and
// Ready states are preserved; it is unavailable while queued (UX-14).
func (p *Party) TransferLeader(leaderID, targetID string) error {
	if err := p.requireLeader(leaderID); err != nil {
		return err
	}
	if p.Status != Idle {
		return ErrPartyLocked
	}
	if leaderID == targetID {
		return ErrSelf
	}
	if !p.IsMember(targetID) {
		return ErrNotMember
	}
	p.LeaderID = targetID
	return nil
}

// StartQueue is the leader pressing Find Match: every member Ready, a mode
// selected and the party no larger than one team. It locks membership (§2).
func (p *Party) StartQueue(leaderID string, rules Rules) error {
	if err := p.requireLeader(leaderID); err != nil {
		return err
	}
	if p.Status != Idle {
		return ErrPartyLocked
	}
	if p.Mode == "" {
		return ErrNoMode
	}
	mode, ok := rules.Modes[p.Mode]
	if !ok || !mode.Enabled {
		return ErrUnknownMode
	}
	if len(p.Members) > mode.HumanPlayersPerTeam {
		return ErrTooManyForMode
	}
	for _, m := range p.Members {
		if !m.Ready {
			return ErrNotAllReady
		}
	}
	p.Status = Queued
	return nil
}

// CancelQueue is the leader cancelling matchmaking.
//
// PROVISIONAL: the bible requires re-readying after cancellations caused by
// departures or failed acceptance but does not say whether a leader's manual
// cancel does. Until ruled on, it resets Ready the same way.
func (p *Party) CancelQueue(leaderID string) error {
	if err := p.requireLeader(leaderID); err != nil {
		return err
	}
	if p.Status != Queued {
		return nil
	}
	p.Status = Idle
	p.resetReady()
	return nil
}
