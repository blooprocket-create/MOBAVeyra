package selection

import "time"

// Trade is a player's offer to swap locked Vanguards with a teammate
// (Battleground Bible, shared lock-in rules; ADR-041 §2).
type Trade struct {
	From, To string
}

// SeatAt returns the account in a seat, by its place in the select's order.
func (s *Session) SeatAt(index int) (string, bool) {
	if index < 0 || index >= len(s.Seats) {
		return "", false
	}
	return s.Seats[index].AccountID, true
}

// checkTrading checks that two players may trade: a Casual Select or a draft
// still picking, both seated humans on one side, each with a locked Vanguard.
func (s *Session) checkTrading(fromID, toID string, now time.Time) (*Seat, *Seat, error) {
	from, ok := s.seat(fromID)
	if !ok {
		return nil, nil, ErrSelectNotFound
	}
	to, ok := s.seat(toID)
	if !ok || fromID == toID || to.Side != from.Side || !s.Kind.Matchmade() {
		return nil, nil, ErrCannotTrade
	}
	if s.State != Picking {
		return nil, nil, ErrInvalidState
	}
	if !now.Before(s.Deadline) {
		return nil, nil, ErrExpired
	}
	if from.Locked == "" || to.Locked == "" {
		return nil, nil, ErrCannotTrade
	}
	return from, to, nil
}

// OfferTrade offers a teammate the player's locked Vanguard for theirs. A new
// offer replaces the player's earlier one (ADR-041 §7.5).
func (s *Session) OfferTrade(fromID, toID string, now time.Time) error {
	if _, _, err := s.checkTrading(fromID, toID, now); err != nil {
		return err
	}
	s.dropTrades(func(t Trade) bool { return t.From == fromID })
	s.Trades = append(s.Trades, Trade{From: fromID, To: toID})
	return nil
}

// Offered reports whether fromID offers toID a trade now.
func (s *Session) Offered(fromID, toID string) bool {
	for _, t := range s.Trades {
		if t.From == fromID && t.To == toID {
			return true
		}
	}
	return false
}

// AcceptTrade swaps two teammates' locked Vanguards on an offer between them.
// The caller has checked that each may play the other's Vanguard, which must
// still be the ones expected, and loads each player's saved Flux Spells for the
// Vanguard they receive (LoadSavedFluxSpells). The team's locked roster stays
// the same. Every offer either player had lapses.
func (s *Session) AcceptTrade(toID, fromID, fromVanguard, toVanguard string, now time.Time) error {
	from, to, err := s.checkTrading(fromID, toID, now)
	if err != nil {
		return err
	}
	if !s.Offered(fromID, toID) || from.Locked != fromVanguard || to.Locked != toVanguard {
		return ErrCannotTrade
	}
	from.Locked, to.Locked = to.Locked, from.Locked
	from.Hover, to.Hover = from.Locked, to.Locked
	s.dropTrades(func(t Trade) bool { return t.From == fromID || t.To == fromID || t.From == toID || t.To == toID })
	return nil
}

// DeclineTrade turns down a teammate's offer.
func (s *Session) DeclineTrade(toID, fromID string) error {
	if !s.Offered(fromID, toID) {
		return ErrCannotTrade
	}
	s.dropTrades(func(t Trade) bool { return t.From == fromID && t.To == toID })
	return nil
}

func (s *Session) dropTrades(drop func(Trade) bool) {
	kept := s.Trades[:0]
	for _, t := range s.Trades {
		if !drop(t) {
			kept = append(kept, t)
		}
	}
	s.Trades = kept
}
