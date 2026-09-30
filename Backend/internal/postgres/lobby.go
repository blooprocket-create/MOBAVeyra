package postgres

import (
	"context"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/lobby"
)

// LobbyStore implements lobby.Store.
type LobbyStore struct{ pool *pgxpool.Pool }

// Lobby returns the custom-lobby store.
func (s *Store) Lobby() *LobbyStore { return &LobbyStore{pool: s.pool} }

func (s *LobbyStore) InTx(ctx context.Context, fn func(context.Context, lobby.Tx) error) error {
	return inTx(ctx, s.pool, func(ctx context.Context, tx pgx.Tx) error { return fn(ctx, lobbyTx{ctx: ctx, q: tx}) })
}

type lobbyTx struct {
	ctx context.Context
	q   querier
}

func (t lobbyTx) LobbyIDOf(accountID string) (string, error) {
	var id string
	err := t.q.QueryRow(t.ctx, `SELECT lobby_id::text FROM lobby.members WHERE account_id = $1::uuid`, accountID).Scan(&id)
	if errors.Is(err, pgx.ErrNoRows) {
		return "", lobby.ErrNotInLobby
	}
	return id, err
}

func (t lobbyTx) LockLobby(id string) (lobby.Lobby, error) {
	return loadLobby(t.ctx, t.q, id, true)
}

func loadLobby(ctx context.Context, q querier, id string, lock bool) (lobby.Lobby, error) {
	sql := `SELECT id::text, host_id::text, status, victory_enabled, victory_chosen, starting_gold FROM lobby.lobbies WHERE id = $1::uuid`
	if lock {
		sql += ` FOR UPDATE`
	}
	var l lobby.Lobby
	var status string
	err := q.QueryRow(ctx, sql, id).Scan(&l.ID, &l.HostID, &status, &l.Settings.VictoryEnabled, &l.Settings.VictoryChosen, &l.Settings.StartingGold)
	if errors.Is(err, pgx.ErrNoRows) {
		return lobby.Lobby{}, lobby.ErrLobbyNotFound
	}
	if err != nil {
		return lobby.Lobby{}, err
	}
	l.Status = lobby.Status(status)

	rows, err := q.Query(ctx, `SELECT account_id::text, joined_at, side, seat FROM lobby.members
		WHERE lobby_id = $1::uuid ORDER BY joined_at, account_id`, id)
	if err != nil {
		return lobby.Lobby{}, err
	}
	l.Members, err = pgx.CollectRows(rows, func(r pgx.CollectableRow) (lobby.Member, error) {
		var m lobby.Member
		err := r.Scan(&m.AccountID, &m.JoinedAt, &m.Seat.Side, &m.Seat.Index)
		return m, err
	})
	if err != nil {
		return lobby.Lobby{}, err
	}
	rows, err = q.Query(ctx, `SELECT side, seat, vanguard_id, difficulty FROM lobby.bots
		WHERE lobby_id = $1::uuid ORDER BY side, seat`, id)
	if err != nil {
		return lobby.Lobby{}, err
	}
	l.Bots, err = pgx.CollectRows(rows, func(r pgx.CollectableRow) (lobby.Bot, error) {
		var b lobby.Bot
		err := r.Scan(&b.Seat.Side, &b.Seat.Index, &b.VanguardID, &b.Difficulty)
		return b, err
	})
	return l, err
}

func (t lobbyTx) CreateLobby(l lobby.Lobby) error {
	if _, err := t.q.Exec(t.ctx, `INSERT INTO lobby.lobbies (id, host_id, status, victory_enabled, victory_chosen, starting_gold)
		VALUES ($1::uuid, $2::uuid, $3, $4, $5, $6)`,
		l.ID, l.HostID, string(l.Status), l.Settings.VictoryEnabled, l.Settings.VictoryChosen, l.Settings.StartingGold); err != nil {
		return err
	}
	return t.insertSeats(l)
}

func (t lobbyTx) SaveLobby(l lobby.Lobby) error {
	tag, err := t.q.Exec(t.ctx, `UPDATE lobby.lobbies SET host_id = $2::uuid, status = $3, victory_enabled = $4, victory_chosen = $5, starting_gold = $6
		WHERE id = $1::uuid`, l.ID, l.HostID, string(l.Status), l.Settings.VictoryEnabled, l.Settings.VictoryChosen, l.Settings.StartingGold)
	if err != nil {
		return err
	}
	if tag.RowsAffected() == 0 {
		return lobby.ErrLobbyNotFound
	}
	for _, table := range []string{"lobby.members", "lobby.bots"} {
		if _, err := t.q.Exec(t.ctx, `DELETE FROM `+table+` WHERE lobby_id = $1::uuid`, l.ID); err != nil {
			return err
		}
	}
	return t.insertSeats(l)
}

func (t lobbyTx) insertSeats(l lobby.Lobby) error {
	for _, m := range l.Members {
		_, err := t.q.Exec(t.ctx, `INSERT INTO lobby.members (lobby_id, account_id, joined_at, side, seat)
			VALUES ($1::uuid, $2::uuid, $3, $4, $5)`, l.ID, m.AccountID, m.JoinedAt, m.Seat.Side, m.Seat.Index)
		var pgErr *pgconn.PgError
		if errors.As(err, &pgErr) && pgErr.Code == uniqueViolation {
			return lobby.ErrAlreadyInLobby
		}
		if err != nil {
			return err
		}
	}
	for _, b := range l.Bots {
		if _, err := t.q.Exec(t.ctx, `INSERT INTO lobby.bots (lobby_id, side, seat, vanguard_id, difficulty)
			VALUES ($1::uuid, $2, $3, $4, $5)`, l.ID, b.Seat.Side, b.Seat.Index, b.VanguardID, b.Difficulty); err != nil {
			return err
		}
	}
	return nil
}

func (t lobbyTx) DeleteLobby(id string) error {
	_, err := t.q.Exec(t.ctx, `DELETE FROM lobby.lobbies WHERE id = $1::uuid`, id)
	return err
}

func (t lobbyTx) PutInvite(inv lobby.Invite) error {
	_, err := t.q.Exec(t.ctx, `
		INSERT INTO lobby.invites (id, lobby_id, inviter_id, invitee_id, created_at, expires_at)
		VALUES ($1::uuid, $2::uuid, $3::uuid, $4::uuid, $5, $6)
		ON CONFLICT (lobby_id, invitee_id) DO UPDATE
		SET id = EXCLUDED.id, inviter_id = EXCLUDED.inviter_id, created_at = EXCLUDED.created_at, expires_at = EXCLUDED.expires_at`,
		inv.ID, inv.LobbyID, inv.InviterID, inv.InviteeID, inv.CreatedAt, inv.ExpiresAt)
	return err
}

const lobbyInviteColumns = `id::text, lobby_id::text, inviter_id::text, invitee_id::text, created_at, expires_at`

func scanLobbyInvite(r pgx.Row) (lobby.Invite, error) {
	var inv lobby.Invite
	err := r.Scan(&inv.ID, &inv.LobbyID, &inv.InviterID, &inv.InviteeID, &inv.CreatedAt, &inv.ExpiresAt)
	return inv, err
}

func (t lobbyTx) Invite(id string, now time.Time) (lobby.Invite, error) {
	if !uuidPattern.MatchString(id) {
		return lobby.Invite{}, lobby.ErrInviteNotFound
	}
	inv, err := scanLobbyInvite(t.q.QueryRow(t.ctx, `SELECT `+lobbyInviteColumns+` FROM lobby.invites
		WHERE id = $1::uuid AND expires_at > $2`, id, now))
	if errors.Is(err, pgx.ErrNoRows) {
		return lobby.Invite{}, lobby.ErrInviteNotFound
	}
	return inv, err
}

func (t lobbyTx) DeleteInvite(id string) error {
	_, err := t.q.Exec(t.ctx, `DELETE FROM lobby.invites WHERE id = $1::uuid`, id)
	return err
}

func (t lobbyTx) DeleteInvitesBetween(a, b string) error {
	_, err := t.q.Exec(t.ctx, `DELETE FROM lobby.invites
		WHERE (inviter_id = $1::uuid AND invitee_id = $2::uuid) OR (inviter_id = $2::uuid AND invitee_id = $1::uuid)`, a, b)
	return err
}

func (t lobbyTx) DeleteInvitesInto(lobbyID, invitee string) error {
	_, err := t.q.Exec(t.ctx, `DELETE FROM lobby.invites WHERE lobby_id = $1::uuid AND invitee_id = $2::uuid`, lobbyID, invitee)
	return err
}

func (s *LobbyStore) LobbyOf(ctx context.Context, accountID string) (lobby.Lobby, error) {
	q := querierFor(ctx, s.pool)
	id, err := lobbyTx{ctx: ctx, q: q}.LobbyIDOf(accountID)
	if err != nil {
		return lobby.Lobby{}, err
	}
	l, err := loadLobby(ctx, q, id, false)
	if errors.Is(err, lobby.ErrLobbyNotFound) {
		return lobby.Lobby{}, lobby.ErrNotInLobby
	}
	return l, err
}

func (s *LobbyStore) InvitesFor(ctx context.Context, accountID string, now time.Time) ([]lobby.Invite, error) {
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT `+lobbyInviteColumns+` FROM lobby.invites
		WHERE invitee_id = $1::uuid AND expires_at > $2 ORDER BY created_at`, accountID, now)
	if err != nil {
		return nil, err
	}
	return pgx.CollectRows(rows, func(r pgx.CollectableRow) (lobby.Invite, error) { return scanLobbyInvite(r) })
}
