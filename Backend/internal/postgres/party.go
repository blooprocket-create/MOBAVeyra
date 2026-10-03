package postgres

import (
	"context"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
)

// uniqueViolation is Postgres SQLSTATE 23505.
const uniqueViolation = "23505"

// PartyStore implements party.Store.
type PartyStore struct{ pool *pgxpool.Pool }

// Party returns the party store.
func (s *Store) Party() *PartyStore { return &PartyStore{pool: s.pool} }

func (s *PartyStore) InTx(ctx context.Context, fn func(context.Context, party.Tx) error) error {
	return inTx(ctx, s.pool, func(ctx context.Context, tx pgx.Tx) error { return fn(ctx, partyTx{ctx: ctx, q: tx}) })
}

// querier is satisfied by both the pool and a transaction.
type querier interface {
	Exec(ctx context.Context, sql string, args ...any) (pgconn.CommandTag, error)
	Query(ctx context.Context, sql string, args ...any) (pgx.Rows, error)
	QueryRow(ctx context.Context, sql string, args ...any) pgx.Row
}

type partyTx struct {
	ctx context.Context
	q   querier
}

func (t partyTx) PartyIDOf(accountID string) (string, error) {
	var id string
	err := t.q.QueryRow(t.ctx, `SELECT party_id::text FROM party.members WHERE account_id = $1::uuid`, accountID).Scan(&id)
	if errors.Is(err, pgx.ErrNoRows) {
		return "", party.ErrNotInParty
	}
	return id, err
}

func (t partyTx) LockParty(id string) (party.Party, error) {
	return loadParty(t.ctx, t.q, id, true)
}

func loadParty(ctx context.Context, q querier, id string, lock bool) (party.Party, error) {
	sql := `SELECT id::text, leader_id::text, coalesce(mode, ''), privacy, status, queued_at FROM party.parties WHERE id = $1::uuid`
	if lock {
		sql += ` FOR UPDATE`
	}
	var p party.Party
	var privacy, status string
	var queuedAt *time.Time
	err := q.QueryRow(ctx, sql, id).Scan(&p.ID, &p.LeaderID, &p.Mode, &privacy, &status, &queuedAt)
	if errors.Is(err, pgx.ErrNoRows) {
		return party.Party{}, party.ErrPartyNotFound
	}
	if err != nil {
		return party.Party{}, err
	}
	p.Privacy, p.Status, p.QueuedAt = party.Privacy(privacy), party.Status(status), timeOrZero(queuedAt)

	rows, err := q.Query(ctx, `SELECT account_id::text, ready, joined_at FROM party.members
		WHERE party_id = $1::uuid ORDER BY joined_at, account_id`, id)
	if err != nil {
		return party.Party{}, err
	}
	p.Members, err = pgx.CollectRows(rows, func(r pgx.CollectableRow) (party.Member, error) {
		var m party.Member
		err := r.Scan(&m.AccountID, &m.Ready, &m.JoinedAt)
		return m, err
	})
	return p, err
}

func nullableMode(mode string) *string {
	if mode == "" {
		return nil
	}
	return &mode
}

func (t partyTx) CreateParty(p party.Party) error {
	if _, err := t.q.Exec(t.ctx, `INSERT INTO party.parties (id, leader_id, mode, privacy, status, queued_at)
		VALUES ($1::uuid, $2::uuid, $3, $4, $5, $6)`,
		p.ID, p.LeaderID, nullableMode(p.Mode), string(p.Privacy), string(p.Status), nullableTime(p.QueuedAt)); err != nil {
		return err
	}
	return t.insertMembers(p)
}

func (t partyTx) SaveParty(p party.Party) error {
	if _, err := t.q.Exec(t.ctx, `UPDATE party.parties SET leader_id = $2::uuid, mode = $3, privacy = $4, status = $5, queued_at = $6
		WHERE id = $1::uuid`, p.ID, p.LeaderID, nullableMode(p.Mode), string(p.Privacy), string(p.Status), nullableTime(p.QueuedAt)); err != nil {
		return err
	}
	if _, err := t.q.Exec(t.ctx, `DELETE FROM party.members WHERE party_id = $1::uuid`, p.ID); err != nil {
		return err
	}
	return t.insertMembers(p)
}

func (t partyTx) insertMembers(p party.Party) error {
	for _, m := range p.Members {
		_, err := t.q.Exec(t.ctx, `INSERT INTO party.members (party_id, account_id, ready, joined_at)
			VALUES ($1::uuid, $2::uuid, $3, $4)`, p.ID, m.AccountID, m.Ready, m.JoinedAt)
		var pgErr *pgconn.PgError
		if errors.As(err, &pgErr) && pgErr.Code == uniqueViolation {
			return party.ErrAlreadyInParty
		}
		if err != nil {
			return err
		}
	}
	return nil
}

func (t partyTx) LockQueued(mode string) ([]party.Party, error) {
	// SKIP LOCKED: a party another transaction holds, such as another
	// matchmaker pass's, is left to it.
	rows, err := t.q.Query(t.ctx, `SELECT id::text FROM party.parties WHERE status = 'queued' AND mode = $1
		ORDER BY queued_at, id FOR UPDATE SKIP LOCKED`, mode)
	if err != nil {
		return nil, err
	}
	ids, err := pgx.CollectRows(rows, pgx.RowTo[string])
	if err != nil {
		return nil, err
	}
	out := make([]party.Party, 0, len(ids))
	for _, id := range ids {
		p, err := loadParty(t.ctx, t.q, id, false)
		if err != nil {
			return nil, err
		}
		out = append(out, p)
	}
	return out, nil
}

func (t partyTx) DeleteParty(id string) error {
	_, err := t.q.Exec(t.ctx, `DELETE FROM party.parties WHERE id = $1::uuid`, id)
	return err
}

func (t partyTx) PutInvite(inv party.Invite) error {
	_, err := t.q.Exec(t.ctx, `
		INSERT INTO party.invites (id, party_id, inviter_id, invitee_id, created_at, expires_at)
		VALUES ($1::uuid, $2::uuid, $3::uuid, $4::uuid, $5, $6)
		ON CONFLICT (party_id, invitee_id) DO UPDATE
		SET id = EXCLUDED.id, inviter_id = EXCLUDED.inviter_id, created_at = EXCLUDED.created_at, expires_at = EXCLUDED.expires_at`,
		inv.ID, inv.PartyID, inv.InviterID, inv.InviteeID, inv.CreatedAt, inv.ExpiresAt)
	return err
}

const inviteColumns = `id::text, party_id::text, inviter_id::text, invitee_id::text, created_at, expires_at`

func scanInvite(r pgx.Row) (party.Invite, error) {
	var inv party.Invite
	err := r.Scan(&inv.ID, &inv.PartyID, &inv.InviterID, &inv.InviteeID, &inv.CreatedAt, &inv.ExpiresAt)
	return inv, err
}

func (t partyTx) Invite(id string, now time.Time) (party.Invite, error) {
	if !uuidPattern.MatchString(id) {
		return party.Invite{}, party.ErrInviteNotFound
	}
	inv, err := scanInvite(t.q.QueryRow(t.ctx, `SELECT `+inviteColumns+` FROM party.invites
		WHERE id = $1::uuid AND expires_at > $2`, id, now))
	if errors.Is(err, pgx.ErrNoRows) {
		return party.Invite{}, party.ErrInviteNotFound
	}
	return inv, err
}

func (t partyTx) DeleteInvite(id string) error {
	_, err := t.q.Exec(t.ctx, `DELETE FROM party.invites WHERE id = $1::uuid`, id)
	return err
}

func (t partyTx) DeleteInvitesBetween(a, b string) error {
	_, err := t.q.Exec(t.ctx, `DELETE FROM party.invites
		WHERE (inviter_id = $1::uuid AND invitee_id = $2::uuid) OR (inviter_id = $2::uuid AND invitee_id = $1::uuid)`, a, b)
	return err
}

func (t partyTx) DeleteInvitesInto(partyID, invitee string) error {
	_, err := t.q.Exec(t.ctx, `DELETE FROM party.invites WHERE party_id = $1::uuid AND invitee_id = $2::uuid`, partyID, invitee)
	return err
}

func (s *PartyStore) PartyOf(ctx context.Context, accountID string) (party.Party, error) {
	q := querierFor(ctx, s.pool)
	id, err := partyTx{ctx: ctx, q: q}.PartyIDOf(accountID)
	if err != nil {
		return party.Party{}, err
	}
	p, err := loadParty(ctx, q, id, false)
	if errors.Is(err, party.ErrPartyNotFound) {
		return party.Party{}, party.ErrNotInParty
	}
	return p, err
}

// PartiesOf reads the accounts' parties and all their members in two
// statements, however many accounts are asked about.
func (s *PartyStore) PartiesOf(ctx context.Context, accountIDs []string) (map[string]party.Party, error) {
	var ids []string
	for _, id := range accountIDs {
		if uuidPattern.MatchString(id) {
			ids = append(ids, id)
		}
	}
	out := map[string]party.Party{}
	if len(ids) == 0 {
		return out, nil
	}
	q := querierFor(ctx, s.pool)
	rows, err := q.Query(ctx, `SELECT id::text, leader_id::text, coalesce(mode, ''), privacy, status, queued_at FROM party.parties
		WHERE id IN (SELECT party_id FROM party.members WHERE account_id = ANY($1::uuid[]))`, ids)
	if err != nil {
		return nil, err
	}
	parties, err := pgx.CollectRows(rows, func(r pgx.CollectableRow) (party.Party, error) {
		var p party.Party
		var privacy, status string
		var queuedAt *time.Time
		err := r.Scan(&p.ID, &p.LeaderID, &p.Mode, &privacy, &status, &queuedAt)
		p.Privacy, p.Status, p.QueuedAt = party.Privacy(privacy), party.Status(status), timeOrZero(queuedAt)
		return p, err
	})
	if err != nil || len(parties) == 0 {
		return out, err
	}
	partyIDs := make([]string, len(parties))
	byID := map[string]*party.Party{}
	for i := range parties {
		partyIDs[i] = parties[i].ID
		byID[parties[i].ID] = &parties[i]
	}
	rows, err = q.Query(ctx, `SELECT party_id::text, account_id::text, ready, joined_at FROM party.members
		WHERE party_id = ANY($1::uuid[]) ORDER BY party_id, joined_at, account_id`, partyIDs)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	for rows.Next() {
		var partyID string
		var m party.Member
		if err := rows.Scan(&partyID, &m.AccountID, &m.Ready, &m.JoinedAt); err != nil {
			return nil, err
		}
		if p := byID[partyID]; p != nil {
			p.Members = append(p.Members, m)
		}
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	asked := map[string]bool{}
	for _, id := range ids {
		asked[id] = true
	}
	for _, p := range parties {
		for _, m := range p.Members {
			if asked[m.AccountID] {
				out[m.AccountID] = p
			}
		}
	}
	return out, nil
}

func (s *PartyStore) MembersIn(ctx context.Context, statuses []party.Status) ([]string, error) {
	names := make([]string, len(statuses))
	for i, status := range statuses {
		names[i] = string(status)
	}
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT m.account_id::text FROM party.members m
		JOIN party.parties p ON p.id = m.party_id
		WHERE p.status = ANY($1::text[]) ORDER BY m.account_id`, names)
	if err != nil {
		return nil, err
	}
	return pgx.CollectRows(rows, pgx.RowTo[string])
}

func (s *PartyStore) InvitesFor(ctx context.Context, accountID string, now time.Time) ([]party.Invite, error) {
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT `+inviteColumns+` FROM party.invites
		WHERE invitee_id = $1::uuid AND expires_at > $2 ORDER BY created_at`, accountID, now)
	if err != nil {
		return nil, err
	}
	return pgx.CollectRows(rows, func(r pgx.CollectableRow) (party.Invite, error) { return scanInvite(r) })
}
