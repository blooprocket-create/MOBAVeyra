package postgres

import (
	"context"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// matchSelectConstraint is the unique constraint that lets a champion select
// create at most one match (migration 0006).
const matchSelectConstraint = "matches_select_id_key"

// matchPortLockKey is a fixed key for pg_advisory_xact_lock, so concurrent
// match creations pick host ports one at a time. It differs from
// migrationLockKey.
const matchPortLockKey = 0x56657972614d5054 // "VeyraMPT"

// MatchStore implements match.Store.
type MatchStore struct{ pool *pgxpool.Pool }

// Match returns the match store.
func (s *Store) Match() *MatchStore { return &MatchStore{pool: s.pool} }

func (s *MatchStore) InTx(ctx context.Context, fn func(context.Context, match.Tx) error) error {
	return inTx(ctx, s.pool, func(ctx context.Context, tx pgx.Tx) error { return fn(ctx, matchTx{ctx: ctx, q: tx}) })
}

type matchTx struct {
	ctx context.Context
	q   querier
}

func (t matchTx) FreePort(lo, hi int) (int, error) {
	if _, err := t.q.Exec(t.ctx, `SELECT pg_advisory_xact_lock($1)`, int64(matchPortLockKey)); err != nil {
		return 0, err
	}
	rows, err := t.q.Query(t.ctx, `SELECT host_port FROM match.matches
		WHERE server_removed_at IS NULL AND host_port BETWEEN $1 AND $2`, lo, hi)
	if err != nil {
		return 0, err
	}
	held, err := pgx.CollectRows(rows, pgx.RowTo[int32])
	if err != nil {
		return 0, err
	}
	taken := map[int]bool{}
	for _, p := range held {
		taken[int(p)] = true
	}
	for port := lo; port <= hi; port++ {
		if !taken[port] {
			return port, nil
		}
	}
	return 0, match.ErrNoServerCapacity
}

func (t matchTx) CreateMatch(m match.Match) error {
	_, err := t.q.Exec(t.ctx, `
		INSERT INTO match.matches (id, mode, rules, host_account_id, select_id, state, created_at, ready_at, ended_at, join_key,
			server_credential_hash, host_port, server_removed_at, failure_reason)
		VALUES ($1::uuid, $2, $3, $4::uuid, $5::uuid, $6, $7, $8, $9, $10, $11, $12, $13, $14)`,
		m.ID, m.Mode, string(m.Rules), nullableText(m.HostAccountID), nullableText(m.SelectID), string(m.State), m.CreatedAt,
		nullableTime(m.ReadyAt), nullableTime(m.EndedAt), m.JoinKey, m.ServerCredentialHash, m.Server.HostPort,
		nullableTime(m.Server.RemovedAt), nullableText(string(m.FailureReason)))
	var pgErr *pgconn.PgError
	if errors.As(err, &pgErr) && pgErr.Code == uniqueViolation && pgErr.ConstraintName == matchSelectConstraint {
		return match.ErrSelectHasMatch
	}
	if err != nil {
		return err
	}
	for i, p := range m.Participants {
		if _, err := t.q.Exec(t.ctx, `INSERT INTO match.participants (match_id, account_id, display_name, side, roster_order, vanguard_id, flux_spells)
			VALUES ($1::uuid, $2::uuid, $3, $4, $5, $6, $7)`, m.ID, p.AccountID, p.DisplayName, string(p.Side), i, nullableText(p.VanguardID),
			p.FluxSpells[:]); err != nil {
			return err
		}
	}
	for i, b := range m.Bots {
		if _, err := t.q.Exec(t.ctx, `INSERT INTO match.bots (match_id, bot_order, side, vanguard_id, difficulty) VALUES ($1::uuid, $2, $3, $4, $5)`,
			m.ID, i, string(b.Side), b.VanguardID, string(b.Difficulty)); err != nil {
			return err
		}
	}
	if !m.State.Active() {
		return nil
	}
	for _, p := range m.Participants {
		_, err := t.q.Exec(t.ctx, `INSERT INTO match.active_assignments (account_id, match_id) VALUES ($1::uuid, $2::uuid)`,
			p.AccountID, m.ID)
		if errors.As(err, &pgErr) && pgErr.Code == uniqueViolation {
			return match.ErrAlreadyInMatch
		}
		if err != nil {
			return err
		}
	}
	return nil
}

func (t matchTx) LockMatch(id string) (match.Match, error) {
	return loadMatch(t.ctx, t.q, id, true)
}

// SaveMatch stores a match's changes. Participants never change after the
// match is created, and a result, once stored, never changes (match.End
// accepts only an identical replay).
func (t matchTx) SaveMatch(m match.Match) error {
	tag, err := t.q.Exec(t.ctx, `
		UPDATE match.matches SET state = $2, ready_at = $3, ended_at = $4, join_key = $5,
			server_removed_at = $6, failure_reason = $7
		WHERE id = $1::uuid`,
		m.ID, string(m.State), nullableTime(m.ReadyAt), nullableTime(m.EndedAt), m.JoinKey,
		nullableTime(m.Server.RemovedAt), nullableText(string(m.FailureReason)))
	if err != nil {
		return err
	}
	if tag.RowsAffected() == 0 {
		return match.ErrMatchNotFound
	}
	if !m.State.Active() {
		if _, err := t.q.Exec(t.ctx, `DELETE FROM match.active_assignments WHERE match_id = $1::uuid`, m.ID); err != nil {
			return err
		}
	}
	if m.Result == nil {
		return nil
	}
	r := m.Result
	tag, err = t.q.Exec(t.ctx, `INSERT INTO match.results (match_id, end_reason, winner, duration_seconds)
		VALUES ($1::uuid, $2, $3, $4) ON CONFLICT (match_id) DO NOTHING`,
		m.ID, string(r.EndReason), nullableText(string(r.Winner)), r.DurationSeconds)
	if err != nil || tag.RowsAffected() == 0 {
		return err
	}
	for _, p := range r.Participants {
		if _, err := t.q.Exec(t.ctx, `INSERT INTO match.result_participants (match_id, account_id, joined, connected_at_end)
			VALUES ($1::uuid, $2::uuid, $3, $4)`, m.ID, p.AccountID, p.Joined, p.ConnectedAtEnd); err != nil {
			return err
		}
	}
	return nil
}

func (s *MatchStore) ActiveMatchFor(ctx context.Context, accountID string) (match.Match, error) {
	if !uuidPattern.MatchString(accountID) {
		return match.Match{}, match.ErrMatchNotFound
	}
	q := querierFor(ctx, s.pool)
	var id string
	err := q.QueryRow(ctx, `SELECT match_id::text FROM match.active_assignments WHERE account_id = $1::uuid`, accountID).Scan(&id)
	if errors.Is(err, pgx.ErrNoRows) {
		return match.Match{}, match.ErrMatchNotFound
	}
	if err != nil {
		return match.Match{}, err
	}
	return loadMatch(ctx, q, id, false)
}

func (s *MatchStore) MatchByID(ctx context.Context, id string) (match.Match, error) {
	if !uuidPattern.MatchString(id) {
		return match.Match{}, match.ErrMatchNotFound
	}
	return loadMatch(ctx, querierFor(ctx, s.pool), id, false)
}

func (s *MatchStore) MatchByServerCredential(ctx context.Context, hash []byte) (match.Match, error) {
	q := querierFor(ctx, s.pool)
	var id string
	err := q.QueryRow(ctx, `SELECT id::text FROM match.matches WHERE server_credential_hash = $1`, hash).Scan(&id)
	if errors.Is(err, pgx.ErrNoRows) {
		return match.Match{}, match.ErrMatchNotFound
	}
	if err != nil {
		return match.Match{}, err
	}
	return loadMatch(ctx, q, id, false)
}

func (s *MatchStore) MatchBySelectID(ctx context.Context, selectID string) (match.Match, error) {
	if !uuidPattern.MatchString(selectID) {
		return match.Match{}, match.ErrMatchNotFound
	}
	q := querierFor(ctx, s.pool)
	var id string
	err := q.QueryRow(ctx, `SELECT id::text FROM match.matches WHERE select_id = $1::uuid`, selectID).Scan(&id)
	if errors.Is(err, pgx.ErrNoRows) {
		return match.Match{}, match.ErrMatchNotFound
	}
	if err != nil {
		return match.Match{}, err
	}
	return loadMatch(ctx, q, id, false)
}

func (s *MatchStore) LastFluxSpells(ctx context.Context, accountID, vanguardID string) ([2]string, error) {
	var out [2]string
	if !uuidPattern.MatchString(accountID) {
		return out, nil
	}
	var spells []string
	err := querierFor(ctx, s.pool).QueryRow(ctx, `SELECT p.flux_spells FROM match.participants p
		JOIN match.matches m ON m.id = p.match_id
		WHERE p.account_id = $1::uuid AND p.vanguard_id = $2 AND m.ready_at IS NOT NULL
		ORDER BY m.created_at DESC, m.id DESC LIMIT 1`, accountID, vanguardID).Scan(&spells)
	if errors.Is(err, pgx.ErrNoRows) {
		return out, nil
	}
	if err != nil {
		return out, err
	}
	copy(out[:], spells)
	return out, nil
}

func (s *MatchStore) MatchesNeedingAttention(ctx context.Context) ([]match.Match, error) {
	q := querierFor(ctx, s.pool)
	rows, err := q.Query(ctx, `SELECT id::text FROM match.matches WHERE server_removed_at IS NULL ORDER BY created_at, id`)
	if err != nil {
		return nil, err
	}
	ids, err := pgx.CollectRows(rows, pgx.RowTo[string])
	if err != nil {
		return nil, err
	}
	out := make([]match.Match, 0, len(ids))
	for _, id := range ids {
		m, err := loadMatch(ctx, q, id, false)
		if err != nil {
			return nil, err
		}
		out = append(out, m)
	}
	return out, nil
}

func loadMatch(ctx context.Context, q querier, id string, lock bool) (match.Match, error) {
	sql := `SELECT id::text, mode, rules, coalesce(host_account_id::text, ''), coalesce(select_id::text, ''), state, created_at,
		ready_at, ended_at, join_key, server_credential_hash, host_port, server_removed_at, coalesce(failure_reason, '')
		FROM match.matches WHERE id = $1::uuid`
	if lock {
		sql += ` FOR UPDATE`
	}
	var m match.Match
	var rules, state, failure string
	var readyAt, endedAt, removedAt *time.Time
	var port int32
	err := q.QueryRow(ctx, sql, id).Scan(&m.ID, &m.Mode, &rules, &m.HostAccountID, &m.SelectID, &state, &m.CreatedAt, &readyAt,
		&endedAt, &m.JoinKey, &m.ServerCredentialHash, &port, &removedAt, &failure)
	if errors.Is(err, pgx.ErrNoRows) {
		return match.Match{}, match.ErrMatchNotFound
	}
	if err != nil {
		return match.Match{}, err
	}
	m.Rules = match.Rules(rules)
	m.State = match.State(state)
	m.FailureReason = match.FailureReason(failure)
	m.ReadyAt, m.EndedAt = timeOrZero(readyAt), timeOrZero(endedAt)
	m.Server = match.Server{HostPort: int(port), RemovedAt: timeOrZero(removedAt)}

	rows, err := q.Query(ctx, `SELECT account_id::text, display_name, side, coalesce(vanguard_id, ''), flux_spells FROM match.participants
		WHERE match_id = $1::uuid ORDER BY roster_order`, id)
	if err != nil {
		return match.Match{}, err
	}
	m.Participants, err = pgx.CollectRows(rows, func(r pgx.CollectableRow) (match.Participant, error) {
		var p match.Participant
		var side string
		var spells []string
		err := r.Scan(&p.AccountID, &p.DisplayName, &side, &p.VanguardID, &spells)
		p.Side = match.Side(side)
		copy(p.FluxSpells[:], spells)
		return p, err
	})
	if err != nil {
		return match.Match{}, err
	}
	rows, err = q.Query(ctx, `SELECT side, vanguard_id, difficulty FROM match.bots WHERE match_id = $1::uuid ORDER BY bot_order`, id)
	if err != nil {
		return match.Match{}, err
	}
	m.Bots, err = pgx.CollectRows(rows, func(r pgx.CollectableRow) (match.Bot, error) {
		var b match.Bot
		var side, difficulty string
		err := r.Scan(&side, &b.VanguardID, &difficulty)
		b.Side = match.Side(side)
		b.Difficulty = match.BotDifficulty(difficulty)
		return b, err
	})
	if err != nil {
		return match.Match{}, err
	}

	var r match.Result
	var reason string
	var winner *string
	err = q.QueryRow(ctx, `SELECT end_reason, winner, duration_seconds FROM match.results WHERE match_id = $1::uuid`, id).
		Scan(&reason, &winner, &r.DurationSeconds)
	if errors.Is(err, pgx.ErrNoRows) {
		return m, nil
	}
	if err != nil {
		return match.Match{}, err
	}
	r.EndReason = match.EndReason(reason)
	if winner != nil {
		r.Winner = match.Side(*winner)
	}
	rows, err = q.Query(ctx, `SELECT account_id::text, joined, connected_at_end FROM match.result_participants
		WHERE match_id = $1::uuid ORDER BY account_id`, id)
	if err != nil {
		return match.Match{}, err
	}
	r.Participants, err = pgx.CollectRows(rows, func(row pgx.CollectableRow) (match.ParticipantResult, error) {
		var p match.ParticipantResult
		err := row.Scan(&p.AccountID, &p.Joined, &p.ConnectedAtEnd)
		return p, err
	})
	if err != nil {
		return match.Match{}, err
	}
	m.Result = &r
	return m, nil
}

func nullableTime(t time.Time) *time.Time {
	if t.IsZero() {
		return nil
	}
	return &t
}

func timeOrZero(t *time.Time) time.Time {
	if t == nil {
		return time.Time{}
	}
	return *t
}

func nullableText(s string) *string {
	if s == "" {
		return nil
	}
	return &s
}
