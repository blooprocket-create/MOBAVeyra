package postgres

import (
	"context"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/matchmaking"
)

// MatchmakingStore implements matchmaking.Store.
type MatchmakingStore struct{ pool *pgxpool.Pool }

// Matchmaking returns the matchmaking store.
func (s *Store) Matchmaking() *MatchmakingStore { return &MatchmakingStore{pool: s.pool} }

func (s *MatchmakingStore) InTx(ctx context.Context, fn func(context.Context, matchmaking.Tx) error) error {
	return inTx(ctx, s.pool, func(ctx context.Context, tx pgx.Tx) error { return fn(ctx, matchmakingTx{ctx: ctx, q: tx}) })
}

type matchmakingTx struct {
	ctx context.Context
	q   querier
}

func (t matchmakingTx) CreateFound(f matchmaking.Found) error {
	if _, err := t.q.Exec(t.ctx, `INSERT INTO matchmaking.found (id, mode, state, created_at, deadline, ended_at, select_id, abandon_reason)
		VALUES ($1::uuid, $2, $3, $4, $5, $6, $7::uuid, $8)`,
		f.ID, f.Mode, string(f.State), f.CreatedAt, f.Deadline, nullableTime(f.EndedAt), nullableText(f.SelectID), nullableText(string(f.AbandonReason))); err != nil {
		return err
	}
	for i, p := range f.Parties {
		if _, err := t.q.Exec(t.ctx, `INSERT INTO matchmaking.found_parties (found_id, party_id, side, party_order) VALUES ($1::uuid, $2::uuid, $3, $4)`,
			f.ID, p.PartyID, string(p.Side), i); err != nil {
			return err
		}
	}
	for i, seat := range f.Seats {
		if _, err := t.q.Exec(t.ctx, `INSERT INTO matchmaking.found_seats (found_id, account_id, party_id, side, seat_order, decision)
			VALUES ($1::uuid, $2::uuid, $3::uuid, $4, $5, $6)`,
			f.ID, seat.AccountID, seat.PartyID, string(seat.Side), i, nullableText(string(seat.Decision))); err != nil {
			return err
		}
	}
	if f.State != matchmaking.Pending {
		return nil
	}
	for _, seat := range f.Seats {
		_, err := t.q.Exec(t.ctx, `INSERT INTO matchmaking.pending_seats (account_id, found_id) VALUES ($1::uuid, $2::uuid)`, seat.AccountID, f.ID)
		var pgErr *pgconn.PgError
		if errors.As(err, &pgErr) && pgErr.Code == uniqueViolation {
			return matchmaking.ErrAlreadyFound
		}
		if err != nil {
			return err
		}
	}
	return nil
}

func (t matchmakingTx) LockFound(id string) (matchmaking.Found, error) {
	return loadFound(t.ctx, t.q, id, true)
}

// SaveFound stores a proposed match's changes. Its parties and players never
// change, so only its state and the answers do.
func (t matchmakingTx) SaveFound(f matchmaking.Found) error {
	tag, err := t.q.Exec(t.ctx, `UPDATE matchmaking.found SET state = $2, ended_at = $3, select_id = $4::uuid, abandon_reason = $5 WHERE id = $1::uuid`,
		f.ID, string(f.State), nullableTime(f.EndedAt), nullableText(f.SelectID), nullableText(string(f.AbandonReason)))
	if err != nil {
		return err
	}
	if tag.RowsAffected() == 0 {
		return matchmaking.ErrFoundNotFound
	}
	for _, seat := range f.Seats {
		if _, err := t.q.Exec(t.ctx, `UPDATE matchmaking.found_seats SET decision = $3 WHERE found_id = $1::uuid AND account_id = $2::uuid`,
			f.ID, seat.AccountID, nullableText(string(seat.Decision))); err != nil {
			return err
		}
	}
	if f.State != matchmaking.Pending {
		_, err := t.q.Exec(t.ctx, `DELETE FROM matchmaking.pending_seats WHERE found_id = $1::uuid`, f.ID)
		return err
	}
	return nil
}

func (t matchmakingTx) PendingFoundOf(accountID string) (string, error) {
	return pendingFoundOf(t.ctx, t.q, accountID)
}

func pendingFoundOf(ctx context.Context, q querier, accountID string) (string, error) {
	if !uuidPattern.MatchString(accountID) {
		return "", matchmaking.ErrFoundNotFound
	}
	var id string
	err := q.QueryRow(ctx, `SELECT found_id::text FROM matchmaking.pending_seats WHERE account_id = $1::uuid`, accountID).Scan(&id)
	if errors.Is(err, pgx.ErrNoRows) {
		return "", matchmaking.ErrFoundNotFound
	}
	return id, err
}

func (s *MatchmakingStore) PendingFor(ctx context.Context, accountID string) (matchmaking.Found, error) {
	q := querierFor(ctx, s.pool)
	id, err := pendingFoundOf(ctx, q, accountID)
	if err != nil {
		return matchmaking.Found{}, err
	}
	return loadFound(ctx, q, id, false)
}

func (s *MatchmakingStore) Pending(ctx context.Context) ([]matchmaking.Found, error) {
	q := querierFor(ctx, s.pool)
	rows, err := q.Query(ctx, `SELECT id::text FROM matchmaking.found WHERE state = 'pending' ORDER BY created_at, id`)
	if err != nil {
		return nil, err
	}
	ids, err := pgx.CollectRows(rows, pgx.RowTo[string])
	if err != nil {
		return nil, err
	}
	out := make([]matchmaking.Found, 0, len(ids))
	for _, id := range ids {
		f, err := loadFound(ctx, q, id, false)
		if err != nil {
			return nil, err
		}
		out = append(out, f)
	}
	return out, nil
}

// FoundByID returns any proposed match, for tests and diagnostics.
func (s *MatchmakingStore) FoundByID(ctx context.Context, id string) (matchmaking.Found, error) {
	return loadFound(ctx, querierFor(ctx, s.pool), id, false)
}

func loadFound(ctx context.Context, q querier, id string, lock bool) (matchmaking.Found, error) {
	sql := `SELECT id::text, mode, state, created_at, deadline, ended_at, coalesce(select_id::text, ''), coalesce(abandon_reason, '')
		FROM matchmaking.found WHERE id = $1::uuid`
	if lock {
		sql += ` FOR UPDATE`
	}
	var f matchmaking.Found
	var state, reason string
	var endedAt *time.Time
	err := q.QueryRow(ctx, sql, id).Scan(&f.ID, &f.Mode, &state, &f.CreatedAt, &f.Deadline, &endedAt, &f.SelectID, &reason)
	if errors.Is(err, pgx.ErrNoRows) {
		return matchmaking.Found{}, matchmaking.ErrFoundNotFound
	}
	if err != nil {
		return matchmaking.Found{}, err
	}
	f.State, f.AbandonReason, f.EndedAt = matchmaking.State(state), matchmaking.AbandonReason(reason), timeOrZero(endedAt)

	rows, err := q.Query(ctx, `SELECT party_id::text, side FROM matchmaking.found_parties WHERE found_id = $1::uuid ORDER BY party_order`, id)
	if err != nil {
		return matchmaking.Found{}, err
	}
	f.Parties, err = pgx.CollectRows(rows, func(r pgx.CollectableRow) (matchmaking.FoundParty, error) {
		var p matchmaking.FoundParty
		var side string
		err := r.Scan(&p.PartyID, &side)
		p.Side = match.Side(side)
		return p, err
	})
	if err != nil {
		return matchmaking.Found{}, err
	}
	rows, err = q.Query(ctx, `SELECT account_id::text, party_id::text, side, coalesce(decision, '')
		FROM matchmaking.found_seats WHERE found_id = $1::uuid ORDER BY seat_order`, id)
	if err != nil {
		return matchmaking.Found{}, err
	}
	f.Seats, err = pgx.CollectRows(rows, func(r pgx.CollectableRow) (matchmaking.Seat, error) {
		var seat matchmaking.Seat
		var side, decision string
		err := r.Scan(&seat.AccountID, &seat.PartyID, &side, &decision)
		seat.Side, seat.Decision = match.Side(side), matchmaking.Decision(decision)
		return seat, err
	})
	if err != nil {
		return matchmaking.Found{}, err
	}
	return f, nil
}
