package selection

import (
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// Phase is where a select is within its picking (ADR-042 §1): a draft's ban
// and pick turns, then a final window before the match starts, in which locked
// teammates may still trade.
type Phase string

const (
	// PhaseBanning: a draft's ban turn.
	PhaseBanning Phase = "banning"
	// PhasePicking: picks are taken; a draft's pick turn, or a single-phase
	// select's whole time.
	PhasePicking Phase = "picking"
	// PhaseFinal: every pick is locked; the select starts its match when the
	// window ends.
	PhaseFinal Phase = "final"
)

// Turn is one turn of a draft (Battleground Bible, Draft Pick): Count bans or
// picks by one side.
type Turn struct {
	Ban   bool       `json:"ban"`
	Side  match.Side `json:"side"`
	Count int        `json:"count"`
}

// Timing is a select's turns and how long each phase lasts, fixed when it
// opens. A single-phase select has no turns; its Pick is its whole time.
type Timing struct {
	Turns []Turn        `json:"turns,omitempty"`
	Ban   time.Duration `json:"ban,omitempty"`
	Pick  time.Duration `json:"pick,omitempty"`
	// Final is the window after the last lock; zero starts the match at once.
	Final time.Duration `json:"final,omitempty"`
}

// Ban is a Vanguard a draft's side banned: neither team may pick it.
type Ban struct {
	Side       match.Side
	AccountID  string
	VanguardID string
}

// PhaseLength is the full length of the select's current phase or turn, whose
// end is its Deadline.
func (s *Session) PhaseLength() time.Duration {
	switch {
	case s.Phase == PhaseFinal:
		return s.Timing.Final
	case s.Phase == PhaseBanning:
		return s.Timing.Ban
	case s.Kind == KindDraft || s.Timing.Pick > 0:
		return s.Timing.Pick
	}
	return s.Deadline.Sub(s.CreatedAt)
}

// CurrentTurn is the draft's turn now, if it is in one.
func (s *Session) CurrentTurn() (Turn, bool) {
	if s.Kind != KindDraft || s.Phase == PhaseFinal || s.Turn >= len(s.Timing.Turns) {
		return Turn{}, false
	}
	return s.Timing.Turns[s.Turn], true
}

// sideSeats returns the indices of a side's seats, in seat order.
func (s *Session) sideSeats(side match.Side) []int {
	var out []int
	for i, seat := range s.Seats {
		if seat.Side == side {
			out = append(out, i)
		}
	}
	return out
}

// Acting returns the players who act in the current turn, in order, each once
// per ban or pick still owed (ADR-042 §1). A ban turn's bans go round the
// side's seats in order, so a side with fewer players than bans bans again; a
// pick turn's picks go to the side's next seats that have not locked.
func (s *Session) Acting() []string {
	turn, ok := s.CurrentTurn()
	if !ok || s.State != Picking {
		return nil
	}
	seats := s.sideSeats(turn.Side)
	if len(seats) == 0 {
		return nil
	}
	var out []string
	if turn.Ban {
		before := 0
		for _, t := range s.Timing.Turns[:s.Turn] {
			if t.Ban && t.Side == turn.Side {
				before += t.Count
			}
		}
		for slot := before + s.TurnDone; slot < before+turn.Count; slot++ {
			out = append(out, s.Seats[seats[slot%len(seats)]].AccountID)
		}
		return out
	}
	for _, i := range seats {
		if len(out) == turn.Count-s.TurnDone {
			break
		}
		if s.Seats[i].Locked == "" {
			out = append(out, s.Seats[i].AccountID)
		}
	}
	return out
}

// Acts reports whether the player acts in the current turn.
func (s *Session) Acts(accountID string) bool {
	for _, id := range s.Acting() {
		if id == accountID {
			return true
		}
	}
	return false
}

// BanTurnFor reports whether the player bans now: a draft's ban turn names
// them.
func (s *Session) BanTurnFor(accountID string) bool {
	turn, ok := s.CurrentTurn()
	return ok && turn.Ban && s.Acts(accountID)
}

// Banned reports whether a side banned the Vanguard.
func (s *Session) Banned(vanguardID string) bool {
	for _, b := range s.Bans {
		if b.VanguardID == vanguardID {
			return true
		}
	}
	return false
}

// checkBanning checks that the player bans now.
func (s *Session) checkBanning(accountID string, now time.Time) (*Seat, error) {
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
	if !s.BanTurnFor(accountID) {
		return nil, ErrNotYourTurn
	}
	return seat, nil
}

// HoverBan records the Vanguard a banning player is considering, which only
// their team sees. The caller has checked that it is released.
func (s *Session) HoverBan(accountID, vanguardID string, now time.Time) error {
	seat, err := s.checkBanning(accountID, now)
	if err != nil {
		return err
	}
	if s.Banned(vanguardID) {
		return ErrTaken
	}
	seat.BanHover = vanguardID
	return nil
}

// LockBan bans a Vanguard for the player's side. The caller has checked that it
// is released.
func (s *Session) LockBan(accountID, vanguardID string, now time.Time) error {
	seat, err := s.checkBanning(accountID, now)
	if err != nil {
		return err
	}
	if s.Banned(vanguardID) {
		return ErrTaken
	}
	s.Bans = append(s.Bans, Ban{Side: seat.Side, AccountID: accountID, VanguardID: vanguardID})
	seat.BanHover = ""
	s.acted(now)
	return nil
}

// acted counts a ban or pick toward the current turn, and moves on when the
// turn has all it owes.
func (s *Session) acted(now time.Time) {
	turn, ok := s.CurrentTurn()
	if !ok {
		return
	}
	s.TurnDone++
	if s.TurnDone >= turn.Count || len(s.Acting()) == 0 {
		s.advance(now)
	}
}

// advance starts the draft's next turn that has someone to act, or its final
// window after the last.
func (s *Session) advance(now time.Time) {
	s.Turn++
	s.TurnDone = 0
	s.beginTurn(now)
}

// beginTurn starts the turn at s.Turn, skipping a pick turn whose side has no
// seat left to lock, as a side with fewer players than a full team has.
func (s *Session) beginTurn(now time.Time) {
	for ; s.Turn < len(s.Timing.Turns); s.Turn++ {
		turn := s.Timing.Turns[s.Turn]
		if turn.Ban {
			s.Phase, s.Deadline = PhaseBanning, now.Add(s.Timing.Ban)
			if len(s.sideSeats(turn.Side)) > 0 {
				return
			}
			continue
		}
		s.Phase, s.Deadline = PhasePicking, now.Add(s.Timing.Pick)
		if len(s.Acting()) > 0 {
			return
		}
	}
	s.enterFinal(now)
}

// enterFinal opens the window after the last lock (ADR-042 §2).
func (s *Session) enterFinal(now time.Time) {
	s.Phase, s.Deadline = PhaseFinal, now.Add(s.Timing.Final)
}

// ReadyToStart reports whether the select's final window has ended, so its
// match may start.
func (s *Session) ReadyToStart(now time.Time) bool {
	return s.State == Picking && s.Phase == PhaseFinal && !now.Before(s.Deadline)
}

// expireTurn ends a draft turn whose time ran out (ADR-042 §7.2): a banning
// player who banned nothing bans their hover, or nothing; a picking player who
// picked nothing locks their hover if they may, or the select is cancelled. It
// reports whether the select goes on.
func (s *Session) expireTurn(turn Turn, now time.Time) bool {
	for _, id := range s.Acting() {
		seat, _ := s.seat(id)
		switch {
		case turn.Ban:
			if seat.BanHover != "" && !s.Banned(seat.BanHover) {
				s.Bans = append(s.Bans, Ban{Side: seat.Side, AccountID: id, VanguardID: seat.BanHover})
			}
			seat.BanHover = ""
		case seat.Hover != "" && !s.Taken(seat.Hover, id):
			seat.Locked, seat.LockedAt = seat.Hover, now
		default:
			s.Cancel(CancelTimedOut, now)
			return false
		}
	}
	s.advance(now)
	return true
}
