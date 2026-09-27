package match

import (
	"context"
	"crypto/rand"
	"errors"
	"fmt"
	"log/slog"
	"strings"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/secret"
)

// Settings are the validated settings the match service needs.
type Settings struct {
	Modes map[string]Mode
	// Practice configures solo Custom practice matches.
	Practice PracticeSettings
	// ReadyTimeout fails a match whose server has not reported ready.
	ReadyTimeout time.Duration
	// MaxDuration fails a ready match that has not ended.
	MaxDuration time.Duration
	// RemoveServerAfter keeps a finished match's server, and its logs, this
	// long before removing it.
	RemoveServerAfter time.Duration
	// HostPortMin and HostPortMax bound the ports players connect to.
	HostPortMin, HostPortMax int
	// PublicHost is the host players connect to.
	PublicHost string
	// BackendURL is the backend's address as match servers reach it.
	BackendURL string
}

// Accounts answers the account questions match rules need. The identity
// package implements it; match never writes identity state.
type Accounts interface {
	// DisplayNames returns the display names of the accounts that exist
	// among ids, keyed by account ID.
	DisplayNames(ctx context.Context, ids []string) (map[string]string, error)
}

// AccountsFunc adapts a function to Accounts.
type AccountsFunc func(ctx context.Context, ids []string) (map[string]string, error)

// DisplayNames calls f.
func (f AccountsFunc) DisplayNames(ctx context.Context, ids []string) (map[string]string, error) {
	return f(ctx, ids)
}

// Seat is one requested roster entry.
type Seat struct {
	AccountID string
	Side      Side
	// VanguardID is the Vanguard the account plays, a content ID.
	VanguardID string
}

// Spec is a requested match (ADR-010 §9).
type Spec struct {
	Mode  string
	Rules Rules
	// HostAccountID is the practice match's host, who must be its only seat.
	// Standard matches have none.
	HostAccountID string
	Seats         []Seat
	// SelectID is the champion select creating the match; a select creates at
	// most one (ErrSelectHasMatch). Development matches have none.
	SelectID string
}

// PlayerMatch is a player's view of their active match. The server address
// and ticket are set only once the match is ready.
type PlayerMatch struct {
	MatchID    string
	Mode       string
	Rules      Rules
	State      State
	Side       Side
	VanguardID string
	ServerHost string
	ServerPort int
	// Ticket is the player's join ticket (ADR-007 §3–4).
	Ticket string
}

// Service applies match rules.
type Service struct {
	store     Store
	accounts  Accounts
	allocator Allocator
	settings  Settings
	now       func() time.Time
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, accounts Accounts, allocator Allocator, settings Settings, now func() time.Time) *Service {
	return &Service{store: store, accounts: accounts, allocator: allocator, settings: settings, now: now}
}

// Create creates a match from a specification and starts its server. Champion
// select creates matches through it, and the development route stands in for
// select in scripts (ADR-010 §8–9). Callers validate each Vanguard against
// what the player may pick; Create checks only the roster's shape. The
// returned match carries no secrets.
func (s *Service) Create(ctx context.Context, spec Spec) (Match, error) {
	participants := make([]Participant, len(spec.Seats))
	ids := make([]string, len(spec.Seats))
	for i, seat := range spec.Seats {
		participants[i] = Participant{AccountID: seat.AccountID, Side: seat.Side, VanguardID: seat.VanguardID}
		ids[i] = seat.AccountID
	}
	switch spec.Rules {
	case RulesStandard:
		mode, ok := s.settings.Modes[spec.Mode]
		if !ok {
			return Match{}, ErrUnknownMode
		}
		if spec.HostAccountID != "" {
			return Match{}, ErrInvalidRoster
		}
		if err := ValidateRoster(mode, participants); err != nil {
			return Match{}, err
		}
	case RulesPractice:
		if err := ValidatePractice(s.settings.Practice, spec.Mode, spec.HostAccountID, participants); err != nil {
			return Match{}, err
		}
	default:
		return Match{}, ErrInvalidRules
	}
	names, err := s.accounts.DisplayNames(ctx, ids)
	if err != nil {
		return Match{}, err
	}
	for i := range participants {
		name, ok := names[participants[i].AccountID]
		if !ok {
			return Match{}, ErrAccountNotFound
		}
		participants[i].DisplayName = name
	}

	key, err := NewJoinKey()
	if err != nil {
		return Match{}, err
	}
	credential, credentialHash, err := secret.New(serverCredentialPrefix)
	if err != nil {
		return Match{}, err
	}
	m := Match{
		ID:                   newID(),
		Mode:                 spec.Mode,
		Rules:                spec.Rules,
		HostAccountID:        spec.HostAccountID,
		SelectID:             spec.SelectID,
		State:                Allocating,
		Participants:         participants,
		CreatedAt:            s.now(),
		JoinKey:              key,
		ServerCredentialHash: credentialHash,
	}
	err = s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		port, err := tx.FreePort(s.settings.HostPortMin, s.settings.HostPortMax)
		if err != nil {
			return err
		}
		m.Server.HostPort = port
		return tx.CreateMatch(m)
	})
	if err != nil {
		return Match{}, err
	}

	// The match is stored before its server starts, so a crash in between
	// leaves a record the reaper fails at its ready timeout.
	assignment, err := BuildAssignment(m, credential, s.settings.BackendURL)
	if err == nil {
		err = s.allocator.Start(ctx, ServerSpec{MatchID: m.ID, HostPort: m.Server.HostPort, Assignment: assignment})
	}
	if err != nil {
		return Match{}, s.failAllocation(ctx, m.ID, err)
	}
	return withoutSecrets(m), nil
}

// failAllocation fails a match whose server did not start, removing anything
// the allocator may have left behind.
func (s *Service) failAllocation(ctx context.Context, matchID string, cause error) error {
	removeErr := s.allocator.Remove(ctx, matchID)
	saveErr := s.update(ctx, matchID, func(m *Match) bool {
		m.Fail(FailAllocation, s.now())
		if removeErr == nil {
			m.Server.RemovedAt = s.now()
		}
		return true
	})
	return errors.Join(fmt.Errorf("%w: %v", ErrAllocationFailed, cause), removeErr, saveErr)
}

// Current returns the account's active match, and false when it has none.
// It is also the "is my match live" query for reconnect (ADR-005 L4): the
// ticket is derived again, so a crashed client gets the same one back.
func (s *Service) Current(ctx context.Context, accountID string) (PlayerMatch, bool, error) {
	m, err := s.store.ActiveMatchFor(ctx, accountID)
	if errors.Is(err, ErrMatchNotFound) {
		return PlayerMatch{}, false, nil
	}
	if err != nil {
		return PlayerMatch{}, false, err
	}
	p, _ := m.Participant(accountID)
	out := PlayerMatch{MatchID: m.ID, Mode: m.Mode, Rules: m.Rules, State: m.State, Side: p.Side, VanguardID: p.VanguardID}
	if m.State == Ready && len(m.JoinKey) > 0 {
		out.ServerHost = s.settings.PublicHost
		out.ServerPort = m.Server.HostPort
		out.Ticket = DeriveTicket(m.JoinKey, m.ID, accountID)
	}
	return out, true, nil
}

// ForParticipant returns a match, without its secrets, and the account's place
// in it. A match the account did not play in is ErrMatchNotFound, so its
// existence is not disclosed. Players read their verified result through it
// (ADR-010 §3).
func (s *Service) ForParticipant(ctx context.Context, accountID, matchID string) (Match, Participant, error) {
	m, err := s.store.MatchByID(ctx, matchID)
	if err != nil {
		return Match{}, Participant{}, err
	}
	p, ok := m.Participant(accountID)
	if !ok {
		return Match{}, Participant{}, ErrMatchNotFound
	}
	return withoutSecrets(m), p, nil
}

// BySelect returns the match a champion select created, without its secrets,
// and false if it created none.
func (s *Service) BySelect(ctx context.Context, selectID string) (Match, bool, error) {
	m, err := s.store.MatchBySelectID(ctx, selectID)
	if errors.Is(err, ErrMatchNotFound) {
		return Match{}, false, nil
	}
	if err != nil {
		return Match{}, false, err
	}
	return withoutSecrets(m), true, nil
}

// Get returns a match without its secrets.
func (s *Service) Get(ctx context.Context, id string) (Match, error) {
	m, err := s.store.MatchByID(ctx, id)
	if err != nil {
		return Match{}, err
	}
	return withoutSecrets(m), nil
}

// ServerReady records that a match's server accepts players.
func (s *Service) ServerReady(ctx context.Context, credential, matchID string) error {
	if err := s.authenticateServer(ctx, credential, matchID); err != nil {
		return err
	}
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		m, err := tx.LockMatch(matchID)
		if err != nil {
			return err
		}
		if err := m.MarkReady(s.now()); err != nil {
			return err
		}
		return tx.SaveMatch(m)
	})
}

// ServerResult records a match's result, reported by its server.
func (s *Service) ServerResult(ctx context.Context, credential, matchID string, r Result) error {
	if err := s.authenticateServer(ctx, credential, matchID); err != nil {
		return err
	}
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		m, err := tx.LockMatch(matchID)
		if err != nil {
			return err
		}
		if err := m.End(r, s.now()); err != nil {
			return err
		}
		return tx.SaveMatch(m)
	})
}

// authenticateServer checks a match-server credential against the match it
// claims. Every failure looks the same to the caller.
func (s *Service) authenticateServer(ctx context.Context, credential, matchID string) error {
	if !strings.HasPrefix(credential, serverCredentialPrefix) {
		return ErrUnauthorized
	}
	m, err := s.store.MatchByServerCredential(ctx, secret.Hash(credential))
	if errors.Is(err, ErrMatchNotFound) {
		return ErrUnauthorized
	}
	if err != nil {
		return err
	}
	if m.ID != matchID {
		return ErrUnauthorized
	}
	return nil
}

// RunReaper runs Reap now and then every interval until ctx ends. Running at
// once reconciles the stored matches with their servers after a restart.
func (s *Service) RunReaper(ctx context.Context, interval time.Duration, log *slog.Logger) {
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	for {
		if err := s.Reap(ctx); err != nil && ctx.Err() == nil {
			log.Error("match reaper", "err", err)
		}
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
		}
	}
}

// Reap makes one pass over the matches that need attention. It fails active
// matches that timed out or whose server stopped, and removes finished
// matches' servers once RemoveServerAfter has passed.
func (s *Service) Reap(ctx context.Context) error {
	matches, err := s.store.MatchesNeedingAttention(ctx)
	if err != nil {
		return err
	}
	var errs []error
	for _, m := range matches {
		if err := s.reapOne(ctx, m); err != nil {
			errs = append(errs, fmt.Errorf("match %s: %w", m.ID, err))
		}
	}
	return errors.Join(errs...)
}

func (s *Service) reapOne(ctx context.Context, m Match) error {
	now := s.now()
	if m.State.Active() {
		reason, fail, err := s.failureFor(ctx, m, now)
		if err != nil || !fail {
			return err
		}
		// The match may have ended since it was listed; Fail ignores that.
		return s.update(ctx, m.ID, func(locked *Match) bool {
			if !locked.State.Active() {
				return false
			}
			locked.Fail(reason, now)
			return true
		})
	}
	if m.Server.RemovedAt.IsZero() && !now.Before(m.EndedAt.Add(s.settings.RemoveServerAfter)) {
		if err := s.allocator.Remove(ctx, m.ID); err != nil {
			return err
		}
		return s.update(ctx, m.ID, func(locked *Match) bool {
			if !locked.Server.RemovedAt.IsZero() {
				return false
			}
			locked.Server.RemovedAt = now
			return true
		})
	}
	return nil
}

// failureFor decides whether an active match has failed.
func (s *Service) failureFor(ctx context.Context, m Match, now time.Time) (FailureReason, bool, error) {
	if m.State == Allocating && !now.Before(m.CreatedAt.Add(s.settings.ReadyTimeout)) {
		return FailReadyTimeout, true, nil
	}
	if m.State == Ready && !now.Before(m.ReadyAt.Add(s.settings.MaxDuration)) {
		return FailMaxDuration, true, nil
	}
	status, err := s.allocator.Status(ctx, m.ID)
	if err != nil {
		return "", false, err
	}
	// A missing server only counts once the match is ready: while allocating,
	// the server may not have been created yet, and the ready timeout covers
	// a server that never appears.
	if status.Exited || (status.Missing && m.State == Ready) {
		return FailServerExited, true, nil
	}
	return "", false, nil
}

// update changes one locked match; change reports whether to save.
func (s *Service) update(ctx context.Context, id string, change func(*Match) bool) error {
	return s.store.InTx(ctx, func(ctx context.Context, tx Tx) error {
		m, err := tx.LockMatch(id)
		if err != nil {
			return err
		}
		if !change(&m) {
			return nil
		}
		return tx.SaveMatch(m)
	})
}

// withoutSecrets returns a copy of a match that is safe to hand outside the
// service.
func withoutSecrets(m Match) Match {
	m = copyMatch(m)
	m.JoinKey = nil
	m.ServerCredentialHash = nil
	return m
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
