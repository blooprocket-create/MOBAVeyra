package postgres

import (
	"context"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
)

// SelectionStore implements selection.Store.
type SelectionStore struct{ pool *pgxpool.Pool }

// Selection returns the champion-select store.
func (s *Store) Selection() *SelectionStore { return &SelectionStore{pool: s.pool} }

func (s *SelectionStore) InTx(ctx context.Context, fn func(context.Context, selection.Tx) error) error {
	return inTx(ctx, s.pool, func(ctx context.Context, tx pgx.Tx) error { return fn(ctx, selectionTx{ctx: ctx, q: tx}) })
}

type selectionTx struct {
	ctx context.Context
	q   querier
}

func (t selectionTx) CreateSession(s selection.Session) error {
	if _, err := t.q.Exec(t.ctx, `
		INSERT INTO selection.sessions (id, kind, mode, host_account_id, state, created_at, deadline, starting_at, ended_at, match_id, cancel_reason, left_by,
			lobby_id, custom_victory_enabled, custom_starting_gold)
		VALUES ($1::uuid, $2, $3, $4::uuid, $5, $6, $7, $8, $9, $10::uuid, $11, $12::uuid, $13::uuid, $14, $15)`,
		s.ID, string(s.Kind), s.Mode, nullableText(s.HostAccountID), string(s.State), s.CreatedAt, s.Deadline, nullableTime(s.StartingAt),
		nullableTime(s.EndedAt), nullableText(s.MatchID), nullableText(string(s.CancelReason)), nullableText(s.LeftBy),
		nullableText(s.LobbyID), customVictory(s.Custom), customGold(s.Custom)); err != nil {
		return err
	}
	for i, b := range s.Bots {
		if _, err := t.q.Exec(t.ctx, `INSERT INTO selection.bots (session_id, bot_order, side, vanguard_id, difficulty) VALUES ($1::uuid, $2, $3, $4, $5)`,
			s.ID, i, string(b.Side), b.VanguardID, string(b.Difficulty)); err != nil {
			return err
		}
	}
	for i, seat := range s.Seats {
		if _, err := t.q.Exec(t.ctx, `INSERT INTO selection.seats (session_id, account_id, display_name, side, seat_order, hover, locked, locked_at, last_seen,
			flux_spells, flux_spells_edited)
			VALUES ($1::uuid, $2::uuid, $3, $4, $5, $6, $7, $8, $9, $10, $11)`,
			s.ID, seat.AccountID, seat.DisplayName, string(seat.Side), i, nullableText(seat.Hover), nullableText(seat.Locked), nullableTime(seat.LockedAt),
			nullableTime(seat.LastSeen), seat.FluxSpells[:], seat.FluxSpellsEdited); err != nil {
			return err
		}
	}
	if !s.State.Active() {
		return nil
	}
	for _, seat := range s.Seats {
		_, err := t.q.Exec(t.ctx, `INSERT INTO selection.active_seats (account_id, session_id) VALUES ($1::uuid, $2::uuid)`, seat.AccountID, s.ID)
		var pgErr *pgconn.PgError
		if errors.As(err, &pgErr) && pgErr.Code == uniqueViolation {
			return selection.ErrAlreadySelecting
		}
		if err != nil {
			return err
		}
	}
	return nil
}

func (t selectionTx) LockSession(id string) (selection.Session, error) {
	return loadSession(t.ctx, t.q, id, true)
}

// SaveSession stores a session's changes. Seats never join or leave a session,
// so only their picks change.
func (t selectionTx) SaveSession(s selection.Session) error {
	tag, err := t.q.Exec(t.ctx, `UPDATE selection.sessions SET state = $2, starting_at = $3, ended_at = $4, match_id = $5::uuid, cancel_reason = $6,
		left_by = $7::uuid WHERE id = $1::uuid`,
		s.ID, string(s.State), nullableTime(s.StartingAt), nullableTime(s.EndedAt), nullableText(s.MatchID), nullableText(string(s.CancelReason)),
		nullableText(s.LeftBy))
	if err != nil {
		return err
	}
	if tag.RowsAffected() == 0 {
		return selection.ErrSelectNotFound
	}
	for _, seat := range s.Seats {
		if _, err := t.q.Exec(t.ctx, `UPDATE selection.seats SET hover = $3, locked = $4, locked_at = $5, last_seen = $6, flux_spells = $7,
			flux_spells_edited = $8
			WHERE session_id = $1::uuid AND account_id = $2::uuid`,
			s.ID, seat.AccountID, nullableText(seat.Hover), nullableText(seat.Locked), nullableTime(seat.LockedAt), nullableTime(seat.LastSeen),
			seat.FluxSpells[:], seat.FluxSpellsEdited); err != nil {
			return err
		}
	}
	if !s.State.Active() {
		_, err := t.q.Exec(t.ctx, `DELETE FROM selection.active_seats WHERE session_id = $1::uuid`, s.ID)
		return err
	}
	return nil
}

func (s *SelectionStore) ActiveFor(ctx context.Context, accountID string) (selection.Session, error) {
	if !uuidPattern.MatchString(accountID) {
		return selection.Session{}, selection.ErrSelectNotFound
	}
	q := querierFor(ctx, s.pool)
	var id string
	err := q.QueryRow(ctx, `SELECT session_id::text FROM selection.active_seats WHERE account_id = $1::uuid`, accountID).Scan(&id)
	if errors.Is(err, pgx.ErrNoRows) {
		return selection.Session{}, selection.ErrSelectNotFound
	}
	if err != nil {
		return selection.Session{}, err
	}
	return loadSession(ctx, q, id, false)
}

func (s *SelectionStore) ByID(ctx context.Context, id string) (selection.Session, error) {
	if !uuidPattern.MatchString(id) {
		return selection.Session{}, selection.ErrSelectNotFound
	}
	return loadSession(ctx, querierFor(ctx, s.pool), id, false)
}

func (s *SelectionStore) Active(ctx context.Context) ([]selection.Session, error) {
	q := querierFor(ctx, s.pool)
	rows, err := q.Query(ctx, `SELECT id::text FROM selection.sessions WHERE state IN ('picking', 'starting') ORDER BY created_at, id`)
	if err != nil {
		return nil, err
	}
	ids, err := pgx.CollectRows(rows, pgx.RowTo[string])
	if err != nil {
		return nil, err
	}
	out := make([]selection.Session, 0, len(ids))
	for _, id := range ids {
		session, err := loadSession(ctx, q, id, false)
		if err != nil {
			return nil, err
		}
		out = append(out, session)
	}
	return out, nil
}

func loadSession(ctx context.Context, q querier, id string, lock bool) (selection.Session, error) {
	sql := `SELECT id::text, kind, mode, coalesce(host_account_id::text, ''), state, created_at, deadline, starting_at, ended_at,
		coalesce(match_id::text, ''), coalesce(cancel_reason, ''), coalesce(left_by::text, ''), coalesce(lobby_id::text, ''),
		custom_victory_enabled, custom_starting_gold FROM selection.sessions WHERE id = $1::uuid`
	if lock {
		sql += ` FOR UPDATE`
	}
	var s selection.Session
	var kind, state, reason string
	var startingAt, endedAt *time.Time
	var victory *bool
	var gold *float64
	err := q.QueryRow(ctx, sql, id).Scan(&s.ID, &kind, &s.Mode, &s.HostAccountID, &state, &s.CreatedAt, &s.Deadline, &startingAt, &endedAt,
		&s.MatchID, &reason, &s.LeftBy, &s.LobbyID, &victory, &gold)
	if errors.Is(err, pgx.ErrNoRows) {
		return selection.Session{}, selection.ErrSelectNotFound
	}
	if err != nil {
		return selection.Session{}, err
	}
	s.Kind, s.State, s.CancelReason = selection.Kind(kind), selection.State(state), selection.CancelReason(reason)
	s.StartingAt, s.EndedAt = timeOrZero(startingAt), timeOrZero(endedAt)
	if victory != nil {
		s.Custom = &match.CustomSettings{VictoryEnabled: *victory, StartingGold: gold}
	}
	botRows, err := q.Query(ctx, `SELECT side, vanguard_id, difficulty FROM selection.bots WHERE session_id = $1::uuid ORDER BY bot_order`, id)
	if err != nil {
		return selection.Session{}, err
	}
	s.Bots, err = pgx.CollectRows(botRows, func(r pgx.CollectableRow) (match.Bot, error) {
		var b match.Bot
		var side, difficulty string
		err := r.Scan(&side, &b.VanguardID, &difficulty)
		b.Side, b.Difficulty = match.Side(side), match.BotDifficulty(difficulty)
		return b, err
	})
	if err != nil {
		return selection.Session{}, err
	}
	rows, err := q.Query(ctx, `SELECT account_id::text, display_name, side, coalesce(hover, ''), coalesce(locked, ''), locked_at, last_seen,
		flux_spells, flux_spells_edited
		FROM selection.seats WHERE session_id = $1::uuid ORDER BY seat_order`, id)
	if err != nil {
		return selection.Session{}, err
	}
	s.Seats, err = pgx.CollectRows(rows, func(r pgx.CollectableRow) (selection.Seat, error) {
		var seat selection.Seat
		var side string
		var lockedAt, lastSeen *time.Time
		var spells []string
		err := r.Scan(&seat.AccountID, &seat.DisplayName, &side, &seat.Hover, &seat.Locked, &lockedAt, &lastSeen, &spells, &seat.FluxSpellsEdited)
		seat.Side, seat.LockedAt, seat.LastSeen = match.Side(side), timeOrZero(lockedAt), timeOrZero(lastSeen)
		copy(seat.FluxSpells[:], spells)
		return seat, err
	})
	if err != nil {
		return selection.Session{}, err
	}
	return s, nil
}
