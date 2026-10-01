package selection

import (
	"context"
	"crypto/rand"
	"errors"
	"fmt"
	"log/slog"
	"slices"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// PracticeSettings configures solo Custom practice's select (ADR-010 §7).
type PracticeSettings struct {
	Enabled bool
	// Mode is the mode ID practice matches record.
	Mode string
	// HostSide is the side the practising player plays on.
	HostSide match.Side
	// PickDuration is how long the player has to lock a Vanguard.
	PickDuration time.Duration
}

// CasualSettings configures Casual Select (ADR-010 §10).
type CasualSettings struct {
	// PickDuration is how long the players have to lock their Vanguards.
	PickDuration time.Duration
	// PresenceTimeout cancels a select one of whose players' clients has not
	// asked about it for this long: a disconnect (Match Flow Bible §2).
	PresenceTimeout time.Duration
}

// CustomSettings configures custom lobbies' selects (ADR-021 §2).
type CustomSettings struct {
	// Mode is the mode ID custom matches record.
	Mode string
	// PickDuration is how long the humans have to lock their Vanguards.
	PickDuration time.Duration
}

// Settings are the validated settings the select service needs.
type Settings struct {
	Practice PracticeSettings
	Casual   CasualSettings
	Custom   CustomSettings
	// StartingTimeout cancels a select whose match creation never finished.
	// It must exceed how long creating a match can take.
	StartingTimeout time.Duration
	// FluxSpells is the roster of Flux Spells a player may choose (ADR-015 §5).
	FluxSpells []string
}

// CasualSeat is one player of a match everyone accepted, on its side.
type CasualSeat struct {
	AccountID string
	Side      match.Side
}

// Matchmaking learns how a matchmade select ended, inside the select's
// transaction. The matchmaking package implements it.
type Matchmaking interface {
	// SelectEnded lets every party go when the match started; otherwise the
	// leaving players' parties go and the others return to the queue.
	SelectEnded(ctx context.Context, accounts, leaving []string, started bool) error
}

// Lobbies learns how a custom select ended, inside the select's transaction.
// The lobby package implements it.
type Lobbies interface {
	// SelectEnded closes the lobby when its match started, and reopens it
	// otherwise.
	SelectEnded(ctx context.Context, lobbyID string, started bool) error
}

// CustomLaunch is what a custom lobby launches: its humans in their seats, in
// each side's seat order, its bots, and the session's rules.
type CustomLaunch struct {
	LobbyID       string
	HostAccountID string
	Seats         []CasualSeat
	Bots          []match.Bot
	Settings      match.CustomSettings
}

// Accounts answers the account questions select rules need. The account
// package implements it.
type Accounts interface {
	Profile(ctx context.Context, accountID string) (account.Profile, error)
	MayPick(ctx context.Context, accountID, vanguardID string) (bool, error)
}

// Names resolves display names; match.AccountsFunc implements it.
type Names interface {
	DisplayNames(ctx context.Context, ids []string) (map[string]string, error)
}

// Matches creates and reports matches. The match package implements it.
type Matches interface {
	Current(ctx context.Context, accountID string) (match.PlayerMatch, bool, error)
	Create(ctx context.Context, spec match.Spec) (match.Match, error)
	BySelect(ctx context.Context, selectID string) (match.Match, bool, error)
	// LastFluxSpells returns the starting Flux Spells the account last took
	// into a match with the Vanguard, or two empty slots (Pre-Game Client UX
	// Bible 37).
	LastFluxSpells(ctx context.Context, accountID, vanguardID string) ([2]string, error)
}

// Blocks answers whether players block each other. The social package
// implements it.
type Blocks interface {
	// BlockedAmong reports whether any two of accounts block each other, and
	// holds each pair's block lock until the caller's unit of work ends.
	BlockedAmong(ctx context.Context, accounts []string) (bool, error)
}

// Parties answers whether an account's party is queued.
type Parties interface {
	Queued(ctx context.Context, accountID string) (bool, error)
}

// PartiesFunc adapts a function to Parties.
type PartiesFunc func(ctx context.Context, accountID string) (bool, error)

// Queued calls f.
func (f PartiesFunc) Queued(ctx context.Context, accountID string) (bool, error) {
	return f(ctx, accountID)
}

// Service applies champion-select rules.
type Service struct {
	store       Store
	accounts    Accounts
	names       Names
	matches     Matches
	parties     Parties
	blocks      Blocks
	matchmaking Matchmaking
	lobbies     Lobbies
	settings    Settings
	now         func() time.Time
	log         *slog.Logger
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, accounts Accounts, names Names, matches Matches, parties Parties, blocks Blocks, settings Settings,
	now func() time.Time, log *slog.Logger) *Service {
	return &Service{store: store, accounts: accounts, names: names, matches: matches, parties: parties, blocks: blocks, settings: settings,
		now: now, log: log}
}

// SetMatchmaking connects the matchmaker, which in turn opens Casual Selects;
// matchmaking and select each need the other, so this is set after both exist.
func (s *Service) SetMatchmaking(m Matchmaking) { s.matchmaking = m }

// SetLobbies connects the custom lobbies, which in turn open custom selects.
func (s *Service) SetLobbies(l Lobbies) { s.lobbies = l }

// StartPractice opens a practice select for the account: its host alone,
// with no lobby (ADR-010 §7). The account must have finished the tutorial,
// and have no match, select or queued party.
func (s *Service) StartPractice(ctx context.Context, accountID string) (Session, error) {
	practice := s.settings.Practice
	if !practice.Enabled {
		return Session{}, ErrPracticeDisabled
	}
	profile, err := s.accounts.Profile(ctx, accountID)
	if err != nil {
		return Session{}, err
	}
	if !profile.TutorialCompleted {
		return Session{}, ErrTutorialRequired
	}
	if err := s.checkFree(ctx, accountID); err != nil {
		return Session{}, err
	}
	names, err := s.names.DisplayNames(ctx, []string{accountID})
	if err != nil {
		return Session{}, err
	}
	now := s.now()
	session := Session{
		ID:            newID(),
		Kind:          KindPractice,
		Mode:          practice.Mode,
		HostAccountID: accountID,
		State:         Picking,
		Seats:         []Seat{{AccountID: accountID, DisplayName: names[accountID], Side: practice.HostSide, LastSeen: now}},
		CreatedAt:     now,
		Deadline:      now.Add(practice.PickDuration),
	}
	if err := s.store.InTx(ctx, func(_ context.Context, tx Tx) error { return tx.CreateSession(session) }); err != nil {
		if errors.Is(err, ErrAlreadySelecting) {
			return Session{}, ErrBusy
		}
		return Session{}, err
	}
	return session, nil
}

// OpenCasual opens the Casual Select of a match everyone accepted
// (Battleground Bible §15): the players pick at once, each from their
// available Vanguards, with picks unique across both teams. Matchmaking calls
// it inside its own transaction, which this joins.
func (s *Service) OpenCasual(ctx context.Context, mode string, seats []CasualSeat) (string, error) {
	return s.openCasual(ctx, mode, seats, nil)
}

// OpenCoop opens the select of a co-op match everyone accepted (ADR-039 §3): a
// Casual Select whose bots, the enemy AI team, are seated from the start. The
// humans pick as in Casual Select, and may play what an enemy bot plays.
// Matchmaking calls it inside its own transaction, which this joins.
func (s *Service) OpenCoop(ctx context.Context, mode string, seats []CasualSeat, bots []match.Bot) (string, error) {
	if len(bots) == 0 {
		return "", ErrNoOpponents
	}
	return s.openCasual(ctx, mode, seats, bots)
}

func (s *Service) openCasual(ctx context.Context, mode string, seats []CasualSeat, bots []match.Bot) (string, error) {
	ids := make([]string, len(seats))
	for i, seat := range seats {
		ids[i] = seat.AccountID
		// Checked first, so a busy player is refused without failing the
		// caller's transaction.
		if _, busy, err := s.Current(ctx, seat.AccountID); err != nil || busy {
			if err != nil {
				return "", err
			}
			return "", ErrBusy
		}
	}
	names, err := s.names.DisplayNames(ctx, ids)
	if err != nil {
		return "", err
	}
	now := s.now()
	session := Session{ID: newID(), Kind: KindCasual, Mode: mode, State: Picking, CreatedAt: now, Deadline: now.Add(s.settings.Casual.PickDuration),
		Bots: append([]match.Bot(nil), bots...)}
	for _, seat := range seats {
		session.Seats = append(session.Seats, Seat{AccountID: seat.AccountID, DisplayName: names[seat.AccountID], Side: seat.Side, LastSeen: now})
	}
	if err := s.store.InTx(ctx, func(_ context.Context, tx Tx) error { return tx.CreateSession(session) }); err != nil {
		return "", err
	}
	return session.ID, nil
}

// OpenCustom opens a custom lobby's select (ADR-021 §2): its humans pick at
// once, each from what they own or the rotation offers, beside the lobby's
// bots. Every human must have finished the tutorial and have no match, select
// or queued party. The lobby calls it inside its own transaction, which this
// joins.
func (s *Service) OpenCustom(ctx context.Context, launch CustomLaunch) (Session, error) {
	ids := make([]string, len(launch.Seats))
	for i, seat := range launch.Seats {
		ids[i] = seat.AccountID
		profile, err := s.accounts.Profile(ctx, seat.AccountID)
		if err != nil {
			return Session{}, err
		}
		if !profile.TutorialCompleted {
			return Session{}, ErrTutorialRequired
		}
		if err := s.checkFree(ctx, seat.AccountID); err != nil {
			return Session{}, err
		}
	}
	names, err := s.names.DisplayNames(ctx, ids)
	if err != nil {
		return Session{}, err
	}
	now := s.now()
	settings := launch.Settings
	session := Session{
		ID:            newID(),
		Kind:          KindCustom,
		Mode:          s.settings.Custom.Mode,
		HostAccountID: launch.HostAccountID,
		LobbyID:       launch.LobbyID,
		Bots:          append([]match.Bot(nil), launch.Bots...),
		Custom:        &settings,
		State:         Picking,
		CreatedAt:     now,
		Deadline:      now.Add(s.settings.Custom.PickDuration),
	}
	for _, seat := range launch.Seats {
		session.Seats = append(session.Seats, Seat{AccountID: seat.AccountID, DisplayName: names[seat.AccountID], Side: seat.Side, LastSeen: now})
	}
	if err := s.store.InTx(ctx, func(_ context.Context, tx Tx) error { return tx.CreateSession(session) }); err != nil {
		if errors.Is(err, ErrAlreadySelecting) {
			return Session{}, ErrBusy
		}
		return Session{}, err
	}
	return session, nil
}

// Poll returns the account's active select, as Current does, and records its
// client asking: its presence in a Casual Select (ADR-010 §10).
func (s *Service) Poll(ctx context.Context, accountID string) (Session, bool, error) {
	session, ok, err := s.Current(ctx, accountID)
	if err != nil || !ok || session.State != Picking {
		return session, ok, err
	}
	seen, err := s.changeActive(ctx, accountID, func(_ context.Context, session *Session) error {
		session.Seen(accountID, s.now())
		return nil
	})
	if errors.Is(err, ErrSelectNotFound) {
		return Session{}, false, nil
	}
	return seen, err == nil, err
}

// Leave takes the account out of its Casual Select, which cancels it: a dodge
// (Match Flow Bible §2). The others return to the queue.
func (s *Service) Leave(ctx context.Context, accountID string) (Session, error) {
	return s.changeActive(ctx, accountID, func(ctx context.Context, session *Session) error {
		if err := session.Leave(accountID, s.now()); err != nil {
			return err
		}
		s.log.Info("a player left champion select", "select", session.ID, "account", accountID)
		return s.ended(ctx, *session, []string{accountID})
	})
}

// ended tells matchmaking how a matchmade select ended, and a custom lobby
// how its select did; leaving are the players matchmaking lets go, whose
// parties leave the queue.
func (s *Service) ended(ctx context.Context, session Session, leaving []string) error {
	switch {
	case session.Kind == KindCasual && s.matchmaking != nil:
		return s.matchmaking.SelectEnded(ctx, session.Accounts(), leaving, session.State == Started)
	case session.Kind == KindCustom && s.lobbies != nil:
		return s.lobbies.SelectEnded(ctx, session.LobbyID, session.State == Started)
	}
	return nil
}

// checkFree refuses an account that has an active match, select or queued party.
func (s *Service) checkFree(ctx context.Context, accountID string) error {
	_, inMatch, err := s.matches.Current(ctx, accountID)
	if err != nil {
		return err
	}
	if inMatch {
		return ErrBusy
	}
	if _, selecting, err := s.Current(ctx, accountID); err != nil || selecting {
		if err != nil {
			return err
		}
		return ErrBusy
	}
	queued, err := s.parties.Queued(ctx, accountID)
	if err != nil {
		return err
	}
	if queued {
		return ErrBusy
	}
	return nil
}

// RemainingPick returns how long a picking select's timer has left, by the
// server's clock; zero once it has ended or the select is no longer picking.
func (s *Service) RemainingPick(session Session) time.Duration {
	if session.State != Picking {
		return 0
	}
	return max(session.Deadline.Sub(s.now()), 0)
}

// Current returns the account's active select, and false when it has none.
func (s *Service) Current(ctx context.Context, accountID string) (Session, bool, error) {
	session, err := s.store.ActiveFor(ctx, accountID)
	if errors.Is(err, ErrSelectNotFound) {
		return Session{}, false, nil
	}
	if err != nil {
		return Session{}, false, err
	}
	return session, true, nil
}

// ForParticipant returns a select the account had a seat in, active or over,
// so a player learns how it ended. Anyone else's is ErrSelectNotFound.
func (s *Service) ForParticipant(ctx context.Context, accountID, id string) (Session, error) {
	session, err := s.store.ByID(ctx, id)
	if err != nil {
		return Session{}, err
	}
	if !session.Has(accountID) {
		return Session{}, ErrSelectNotFound
	}
	return session, nil
}

// Hover records the Vanguard the account is considering in its active select.
func (s *Service) Hover(ctx context.Context, accountID, vanguardID string) (Session, error) {
	if err := s.checkPick(ctx, accountID, vanguardID); err != nil {
		return Session{}, err
	}
	saved, err := s.savedFluxSpells(ctx, accountID, vanguardID)
	if err != nil {
		return Session{}, err
	}
	return s.changeActive(ctx, accountID, func(_ context.Context, session *Session) error {
		now := s.now()
		session.Seen(accountID, now)
		if err := session.Hover(accountID, vanguardID, now); err != nil {
			return err
		}
		session.FollowSavedFluxSpells(accountID, saved)
		return nil
	})
}

// SetFluxSpells records the starting Flux Spells the account chose in its
// active select: roster spells or empty slots, no spell twice (ADR-015 §5).
func (s *Service) SetFluxSpells(ctx context.Context, accountID string, spells [2]string) (Session, error) {
	for _, spell := range spells {
		if spell != "" && !slices.Contains(s.settings.FluxSpells, spell) {
			return Session{}, match.ErrInvalidFluxSpells
		}
	}
	return s.changeActive(ctx, accountID, func(_ context.Context, session *Session) error {
		now := s.now()
		session.Seen(accountID, now)
		return session.SetFluxSpells(accountID, spells, now)
	})
}

// savedFluxSpells is the account's saved loadout for the Vanguard, keeping
// only spells still on the roster.
func (s *Service) savedFluxSpells(ctx context.Context, accountID, vanguardID string) ([2]string, error) {
	saved, err := s.matches.LastFluxSpells(ctx, accountID, vanguardID)
	if err != nil {
		return [2]string{}, err
	}
	for i, spell := range saved {
		if !slices.Contains(s.settings.FluxSpells, spell) {
			saved[i] = ""
		}
	}
	return saved, nil
}

// Lock locks the account's Vanguard in its active select. When every seat has
// locked, the select creates its match before returning.
func (s *Service) Lock(ctx context.Context, accountID, vanguardID string) (Session, error) {
	if err := s.checkPick(ctx, accountID, vanguardID); err != nil {
		return Session{}, err
	}
	saved, err := s.savedFluxSpells(ctx, accountID, vanguardID)
	if err != nil {
		return Session{}, err
	}
	session, err := s.changeActive(ctx, accountID, func(ctx context.Context, session *Session) error {
		now := s.now()
		session.Seen(accountID, now)
		if err := session.Lock(accountID, vanguardID, now); err != nil {
			return err
		}
		session.FollowSavedFluxSpells(accountID, saved)
		if session.AllLocked() {
			return s.beginStarting(ctx, session, now)
		}
		return nil
	})
	if err != nil || session.State != Starting {
		return session, err
	}
	return s.startMatch(ctx, session)
}

// checkPick checks that the account may pick the Vanguard (ADR-010 §6).
func (s *Service) checkPick(ctx context.Context, accountID, vanguardID string) error {
	may, err := s.accounts.MayPick(ctx, accountID, vanguardID)
	if err != nil {
		return err
	}
	if !may {
		return ErrNotAvailable
	}
	return nil
}

// changeActive changes the account's active select under its lock.
func (s *Service) changeActive(ctx context.Context, accountID string, change func(context.Context, *Session) error) (Session, error) {
	active, err := s.store.ActiveFor(ctx, accountID)
	if err != nil {
		return Session{}, err
	}
	var out Session
	err = s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		session, err := tx.LockSession(active.ID)
		if err != nil {
			return err
		}
		if err := change(ctx, &session); err != nil {
			return err
		}
		out = session
		return tx.SaveSession(session)
	})
	return out, err
}

// beginStarting moves a select whose every pick is locked to starting, inside
// its transaction. A matchmade select checks blocks again first, under their
// locks: a block placed since its match was found forbids the match (Parties &
// Social Bible §6), so the select is cancelled, nobody at fault, and everyone
// returns to the queue. Once starting, the match goes ahead.
func (s *Service) beginStarting(ctx context.Context, session *Session, now time.Time) error {
	if session.Kind == KindCasual {
		blocked, err := s.blocks.BlockedAmong(ctx, session.Accounts())
		if err != nil {
			return err
		}
		if blocked {
			session.Cancel(CancelNoLongerMatched, now)
			s.log.Info("champion select can no longer make its match", "select", session.ID)
			return s.ended(ctx, *session, nil)
		}
	}
	session.BeginStarting(now)
	return nil
}

// startMatch creates the match of a select that has just begun starting, and
// records how that went. The match service refuses a second match for one
// select, so the select creates at most one.
func (s *Service) startMatch(ctx context.Context, session Session) (Session, error) {
	spec := match.Spec{Mode: session.Mode, Rules: rulesFor(session.Kind), HostAccountID: session.HostAccountID, SelectID: session.ID,
		Bots: session.Bots, Custom: session.Custom}
	for _, seat := range session.Seats {
		spec.Seats = append(spec.Seats, match.Seat{AccountID: seat.AccountID, Side: seat.Side, VanguardID: seat.Locked, FluxSpells: seat.FluxSpells})
	}
	created, createErr := s.matches.Create(ctx, spec)
	if createErr != nil {
		s.log.Error("champion select could not create its match", "select", session.ID, "err", createErr)
	}
	var out Session
	err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		locked, err := tx.LockSession(session.ID)
		if err != nil {
			return err
		}
		out = locked
		if locked.State != Starting {
			return nil
		}
		var leaving []string
		if createErr != nil {
			// Nobody is at fault, but nobody is sent back to wait on a match
			// that cannot be created either.
			locked.Cancel(CancelAllocationFailed, s.now())
			leaving = locked.Accounts()
		} else {
			locked.Start(created.ID, s.now())
		}
		if err := s.ended(ctx, locked, leaving); err != nil {
			return err
		}
		out = locked
		return tx.SaveSession(locked)
	})
	return out, err
}

func rulesFor(kind Kind) match.Rules {
	switch kind {
	case KindPractice:
		return match.RulesPractice
	case KindCustom:
		return match.RulesCustom
	}
	return match.RulesStandard
}

// RunTicker runs Tick now and then every interval until ctx ends.
func (s *Service) RunTicker(ctx context.Context, interval time.Duration) {
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	for {
		if err := s.Tick(ctx); err != nil && ctx.Err() == nil {
			s.log.Error("champion select ticker", "err", err)
		}
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
		}
	}
}

// Tick makes one pass over the active selects: it ends each pick timer that
// has run out, and settles each select whose match creation stopped short.
func (s *Service) Tick(ctx context.Context) error {
	active, err := s.store.Active(ctx)
	if err != nil {
		return err
	}
	var errs []error
	for _, session := range active {
		if err := s.tickOne(ctx, session); err != nil {
			errs = append(errs, fmt.Errorf("select %s: %w", session.ID, err))
		}
	}
	return errors.Join(errs...)
}

func (s *Service) tickOne(ctx context.Context, listed Session) error {
	now := s.now()
	switch {
	case listed.State == Picking && !now.Before(listed.Deadline):
		var starting Session
		err := s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
			session, err := tx.LockSession(listed.ID)
			if err != nil || session.State != Picking {
				return err
			}
			if session.Expire(now) {
				if err := s.beginStarting(ctx, &session, now); err != nil {
					return err
				}
				starting = session
			} else if err := s.ended(ctx, session, session.Unlocked()); err != nil {
				return err
			}
			return tx.SaveSession(session)
		})
		if err != nil || starting.State != Starting {
			return err
		}
		_, err = s.startMatch(ctx, starting)
		return err
	case listed.State == Picking && listed.Kind == KindCasual && len(listed.Absent(now, s.settings.Casual.PresenceTimeout)) > 0:
		// A player stopped answering: a disconnect cancels the select, and
		// the others return to the queue (Match Flow Bible §2).
		return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
			session, err := tx.LockSession(listed.ID)
			if err != nil || session.State != Picking {
				return err
			}
			absent := session.Absent(now, s.settings.Casual.PresenceTimeout)
			if len(absent) == 0 {
				return nil
			}
			session.Cancel(CancelPresenceLost, now)
			s.log.Info("champion select lost a player", "select", session.ID, "absent", len(absent))
			if err := s.ended(ctx, session, absent); err != nil {
				return err
			}
			return tx.SaveSession(session)
		})
	case listed.State == Starting && !now.Before(listed.StartingAt.Add(s.settings.StartingTimeout)):
		// A crash between starting and recording the match leaves it here; the
		// match, if it was created, is found by its select.
		created, found, err := s.matches.BySelect(ctx, listed.ID)
		if err != nil {
			return err
		}
		return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
			session, err := tx.LockSession(listed.ID)
			if err != nil || session.State != Starting {
				return err
			}
			var leaving []string
			if found {
				session.Start(created.ID, now)
			} else {
				session.Cancel(CancelStartingTimedOut, now)
				leaving = session.Accounts()
			}
			if err := s.ended(ctx, session, leaving); err != nil {
				return err
			}
			return tx.SaveSession(session)
		})
	}
	return nil
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
