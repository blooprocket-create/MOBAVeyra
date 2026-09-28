package matchmaking

import (
	"context"
	"crypto/rand"
	"errors"
	"fmt"
	"log/slog"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
)

// Mode is one matchmade mode's matchmaking settings.
type Mode struct {
	ID string
	// TeamSize is how many players each side of its matches holds.
	TeamSize int
}

// Settings are the validated settings matchmaking needs.
type Settings struct {
	// Modes are the modes with a matchmaker.
	Modes []Mode
	// AcceptDuration is how long players have to accept a match found.
	AcceptDuration time.Duration
	// SearchLimit is the most steps Group takes around one party in a pass.
	SearchLimit int
}

// Parties moves parties through matchmaking. The party package implements it;
// matchmaking never writes party state itself.
type Parties interface {
	// Get returns the account's party, or party.ErrNotInParty.
	Get(ctx context.Context, accountID string) (party.Party, error)
	LockQueued(ctx context.Context, mode string) ([]party.Party, error)
	MarkFound(ctx context.Context, ids []string) error
	MarkSelecting(ctx context.Context, ids []string) error
	Requeue(ctx context.Context, ids []string) error
	ReturnToIdle(ctx context.Context, ids []string) error
	Status(ctx context.Context, id string) (party.Status, error)
}

// Blocks answers whether accounts block each other. The social package
// implements it.
type Blocks interface {
	BlockedWithAny(ctx context.Context, account string, others []string) (bool, error)
	// BlockedAmong reports whether any two of accounts block each other, and
	// holds each pair's block lock until the caller's unit of work ends, so no
	// block between them can commit before what the caller does with the
	// answer.
	BlockedAmong(ctx context.Context, accounts []string) (bool, error)
}

// Activity answers whether any of accounts is in a match or a champion
// select, where no queued player may be (the main package joins the match
// and selection packages to implement it).
type Activity interface {
	Busy(ctx context.Context, accounts []string) (bool, error)
}

// SelectSeat is one player of an accepted match, for its champion select.
type SelectSeat struct {
	AccountID string
	Side      match.Side
}

// Selects opens the champion select of a match everyone accepted. The
// selection package implements it.
type Selects interface {
	OpenCasual(ctx context.Context, mode string, seats []SelectSeat) (selectID string, err error)
}

// Service runs the matchmaker and Match Found.
type Service struct {
	store    Store
	parties  Parties
	blocks   Blocks
	activity Activity
	selects  Selects
	settings Settings
	now      func() time.Time
	log      *slog.Logger
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, parties Parties, blocks Blocks, activity Activity, selects Selects, settings Settings, now func() time.Time,
	log *slog.Logger) *Service {
	return &Service{store: store, parties: parties, blocks: blocks, activity: activity, selects: selects, settings: settings, now: now, log: log}
}

// Current returns the account's pending match found, and false when it has none.
func (s *Service) Current(ctx context.Context, accountID string) (Found, bool, error) {
	f, err := s.store.PendingFor(ctx, accountID)
	if errors.Is(err, ErrFoundNotFound) {
		return Found{}, false, nil
	}
	if err != nil {
		return Found{}, false, err
	}
	return f, true, nil
}

// RemainingAccept returns how long a pending match found's timer has left, by
// the server's clock; zero once it has ended or the match is no longer pending.
func (s *Service) RemainingAccept(f Found) time.Duration {
	if f.State != Pending {
		return 0
	}
	return max(f.Deadline.Sub(s.now()), 0)
}

// Accept records the account accepting its match found. When everyone has,
// the match's champion select opens before it returns.
func (s *Service) Accept(ctx context.Context, accountID string) (Found, error) {
	return s.decide(ctx, accountID, Accept)
}

// Decline records the account declining its match found, which abandons it:
// the decliner's party leaves the queue, and the others return to it (§3).
func (s *Service) Decline(ctx context.Context, accountID string) (Found, error) {
	return s.decide(ctx, accountID, Decline)
}

func (s *Service) decide(ctx context.Context, accountID string, decision Decision) (Found, error) {
	var out Found
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		id, err := tx.PendingFoundOf(accountID)
		if err != nil {
			return err
		}
		f, err := tx.LockFound(id)
		if err != nil {
			return err
		}
		now := s.now()
		if err := f.Decide(accountID, decision, now); err != nil {
			return err
		}
		switch {
		case decision == Decline:
			err = s.abandon(ctx, &f, AbandonDeclined, nil, now)
		case f.AllAccepted():
			err = s.assemble(ctx, &f, now)
		}
		if err != nil {
			return err
		}
		out = f
		return tx.SaveFound(f)
	})
	return out, err
}

// abandon ends a proposed match: parties at fault, and changed ones, leave
// matchmaking Not Ready; the others return to the queue with their places kept.
func (s *Service) abandon(ctx context.Context, f *Found, reason AbandonReason, changed map[string]bool, now time.Time) error {
	f.Abandon(reason, now)
	var back, out []string
	for _, p := range f.Parties {
		if changed[p.PartyID] || f.PartyAtFault(p.PartyID, reason) {
			out = append(out, p.PartyID)
		} else {
			back = append(back, p.PartyID)
		}
	}
	s.log.Info("match found abandoned", "found", f.ID, "reason", reason, "requeued", len(back), "left", len(out))
	if err := s.parties.ReturnToIdle(ctx, out); err != nil {
		return err
	}
	return s.parties.Requeue(ctx, back)
}

// assemble opens the champion select of a match everyone accepted. If it
// cannot open, the match is abandoned. Blocks are checked again first, under
// their locks: one placed since the match was found forbids it (Parties &
// Social Bible §6), and nobody is at fault.
func (s *Service) assemble(ctx context.Context, f *Found, now time.Time) error {
	blocked, err := s.blocks.BlockedAmong(ctx, f.Accounts())
	if err != nil {
		return err
	}
	if blocked {
		return s.abandon(ctx, f, AbandonNoLongerMatched, nil, now)
	}
	seats := make([]SelectSeat, len(f.Seats))
	for i, seat := range f.Seats {
		seats[i] = SelectSeat{AccountID: seat.AccountID, Side: seat.Side}
	}
	selectID, err := s.selects.OpenCasual(ctx, f.Mode, seats)
	if err != nil {
		s.log.Error("an accepted match could not open its champion select", "found", f.ID, "err", err)
		return s.abandon(ctx, f, AbandonSelectFailed, nil, now)
	}
	f.Assemble(selectID, now)
	s.log.Info("match found accepted", "found", f.ID, "select", selectID)
	return s.parties.MarkSelecting(ctx, f.PartyIDs())
}

// SelectEnded settles the parties of a matchmade champion select that ended,
// inside the select's transaction. A select that started its match lets every
// party go, Not Ready (UX-15). Otherwise the leaving players' parties leave
// the queue, Not Ready, and the others return to it in their places (Match
// Flow Bible §2).
func (s *Service) SelectEnded(ctx context.Context, accounts, leaving []string, started bool) error {
	leavers := map[string]bool{}
	for _, account := range leaving {
		leavers[account] = true
	}
	faulty := map[string]bool{}
	var partyIDs []string
	seen := map[string]bool{}
	for _, account := range accounts {
		p, err := s.parties.Get(ctx, account)
		if errors.Is(err, party.ErrNotInParty) {
			continue
		}
		if err != nil {
			return err
		}
		if !seen[p.ID] {
			seen[p.ID] = true
			partyIDs = append(partyIDs, p.ID)
		}
		if started || leavers[account] {
			faulty[p.ID] = true
		}
	}
	var back, out []string
	for _, id := range partyIDs {
		if faulty[id] {
			out = append(out, id)
		} else {
			back = append(back, id)
		}
	}
	if err := s.parties.ReturnToIdle(ctx, out); err != nil {
		return err
	}
	return s.parties.Requeue(ctx, back)
}

// Run makes a matchmaking pass and a Match Found pass now and then every
// interval until ctx ends.
func (s *Service) Run(ctx context.Context, interval time.Duration) {
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	for {
		if err := errors.Join(s.MatchOnce(ctx), s.Tick(ctx)); err != nil && ctx.Err() == nil {
			s.log.Error("matchmaking", "err", err)
		}
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
		}
	}
}

// MatchOnce forms what matches it can from each mode's queue.
func (s *Service) MatchOnce(ctx context.Context) error {
	var errs []error
	for _, mode := range s.settings.Modes {
		if err := s.matchMode(ctx, mode); err != nil {
			errs = append(errs, fmt.Errorf("mode %s: %w", mode.ID, err))
		}
	}
	return errors.Join(errs...)
}

func (s *Service) matchMode(ctx context.Context, mode Mode) error {
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		queued, err := s.parties.LockQueued(ctx, mode.ID)
		if err != nil {
			return err
		}
		// A player already in a match or champion select cannot be matched
		// again: their party leaves the queue instead of costing the others a
		// select that could never start its match.
		var candidates []Candidate
		var busy []string
		for _, p := range queued {
			candidate := Candidate{PartyID: p.ID}
			for _, m := range p.Members {
				candidate.Accounts = append(candidate.Accounts, m.AccountID)
			}
			isBusy, err := s.activity.Busy(ctx, candidate.Accounts)
			if err != nil {
				return err
			}
			if isBusy {
				busy = append(busy, p.ID)
				continue
			}
			candidates = append(candidates, candidate)
		}
		if len(busy) > 0 {
			s.log.Info("parties left the queue: a member is in a match or champion select", "mode", mode.ID, "parties", len(busy))
			if err := s.parties.ReturnToIdle(ctx, busy); err != nil {
				return err
			}
		}
		if len(candidates) < 2 {
			return nil
		}
		blocks := &blockCache{ctx: ctx, blocks: s.blocks, known: map[[2]string]bool{}}
		groupings := Group(candidates, mode.TeamSize, s.settings.SearchLimit, blocks.blocked)
		if blocks.err != nil {
			return blocks.err
		}
		now := s.now()
		for _, grouping := range groupings {
			f := Found{ID: newID(), Mode: mode.ID, State: Pending, CreatedAt: now, Deadline: now.Add(s.settings.AcceptDuration)}
			for _, placed := range []struct {
				side    match.Side
				indexes []int
			}{{match.SideA, grouping.SideA}, {match.SideB, grouping.SideB}} {
				for _, i := range placed.indexes {
					f.Parties = append(f.Parties, FoundParty{PartyID: candidates[i].PartyID, Side: placed.side})
					for _, account := range candidates[i].Accounts {
						f.Seats = append(f.Seats, Seat{AccountID: account, PartyID: candidates[i].PartyID, Side: placed.side})
					}
				}
			}
			if err := tx.CreateFound(f); err != nil {
				return err
			}
			if err := s.parties.MarkFound(ctx, f.PartyIDs()); err != nil {
				return err
			}
			s.log.Info("match found", "found", f.ID, "mode", mode.ID, "players", len(f.Seats))
		}
		return nil
	})
}

// blockCache answers whether two accounts block each other, asking the social
// graph once per pair. A lookup that fails counts as blocked, so nothing is
// matched on a guess, and keeps its error for the caller.
type blockCache struct {
	ctx    context.Context
	blocks Blocks
	known  map[[2]string]bool
	err    error
}

func (c *blockCache) blocked(a, b string) bool {
	key := [2]string{a, b}
	if b < a {
		key = [2]string{b, a}
	}
	if known, ok := c.known[key]; ok {
		return known
	}
	isBlocked, err := c.blocks.BlockedWithAny(c.ctx, a, []string{b})
	if err != nil {
		c.err = errors.Join(c.err, err)
		isBlocked = true
	}
	c.known[key] = isBlocked
	return isBlocked
}

// Tick abandons each pending match found whose timer ran out before everyone
// accepted, or one of whose parties has left matchmaking.
func (s *Service) Tick(ctx context.Context) error {
	pending, err := s.store.Pending(ctx)
	if err != nil {
		return err
	}
	var errs []error
	for _, f := range pending {
		if err := s.tickOne(ctx, f.ID); err != nil {
			errs = append(errs, fmt.Errorf("match found %s: %w", f.ID, err))
		}
	}
	return errors.Join(errs...)
}

func (s *Service) tickOne(ctx context.Context, id string) error {
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		f, err := tx.LockFound(id)
		if err != nil || f.State != Pending {
			return err
		}
		now := s.now()
		changed := map[string]bool{}
		for _, p := range f.Parties {
			status, err := s.parties.Status(ctx, p.PartyID)
			if err != nil {
				return err
			}
			if status != party.Found {
				changed[p.PartyID] = true
			}
		}
		switch {
		case len(changed) > 0:
			err = s.abandon(ctx, &f, AbandonPartyChanged, changed, now)
		case !now.Before(f.Deadline):
			err = s.abandon(ctx, &f, AbandonTimedOut, nil, now)
		default:
			return nil
		}
		if err != nil {
			return err
		}
		return tx.SaveFound(f)
	})
}

// newID returns a random RFC 4122 version-4 UUID string.
func newID() string {
	var b [16]byte
	if _, err := rand.Read(b[:]); err != nil {
		panic(fmt.Sprintf("crypto/rand failed: %v", err))
	}
	b[6] = (b[6] & 0x0f) | 0x40
	b[8] = (b[8] & 0x3f) | 0x80
	return fmt.Sprintf("%x-%x-%x-%x-%x", b[0:4], b[4:6], b[6:8], b[8:10], b[10:16])
}
