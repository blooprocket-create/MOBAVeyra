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
	sql := `SELECT id::text, leader_id::text, coalesce(mode, ''), privacy, status FROM party.parties WHERE id = $1::uuid`
	if lock {
		sql += ` FOR UPDATE`
	}
	var p party.Party
	var privacy, status string
	err := q.QueryRow(ctx, sql, id).Scan(&p.ID, &p.LeaderID, &p.Mode, &privacy, &status)
	if errors.Is(err, pgx.ErrNoRows) {
		return party.Party{}, party.ErrPartyNotFound
	}
	if err != nil {
		return party.Party{}, err
	}
	p.Privacy, p.Status = party.Privacy(privacy), party.Status(status)

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
	if _, err := t.q.Exec(t.ctx, `INSERT INTO party.parties (id, leader_id, mode, privacy, status)
		VALUES ($1::uuid, $2::uuid, $3, $4, $5)`,
		p.ID, p.LeaderID, nullableMode(p.Mode), string(p.Privacy), string(p.Status)); err != nil {
		return err
	}
	return t.insertMembers(p)
}

func (t partyTx) SaveParty(p party.Party) error {
	if _, err := t.q.Exec(t.ctx, `UPDATE party.parties SET leader_id = $2::uuid, mode = $3, privacy = $4, status = $5
		WHERE id = $1::uuid`, p.ID, p.LeaderID, nullableMode(p.Mode), string(p.Privacy), string(p.Status)); err != nil {
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

func (s *PartyStore) InvitesFor(ctx context.Context, accountID string, now time.Time) ([]party.Invite, error) {
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT `+inviteColumns+` FROM party.invites
		WHERE invitee_id = $1::uuid AND expires_at > $2 ORDER BY created_at`, accountID, now)
	if err != nil {
		return nil, err
	}
	return pgx.CollectRows(rows, func(r pgx.CollectableRow) (party.Invite, error) { return scanInvite(r) })
}
