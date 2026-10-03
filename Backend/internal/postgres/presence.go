package postgres

import (
	"context"
	"time"

	"github.com/jackc/pgx/v5/pgxpool"
)

// PresenceStore implements presence.Store.
type PresenceStore struct{ pool *pgxpool.Pool }

// Presence returns the presence store.
func (s *Store) Presence() *PresenceStore { return &PresenceStore{pool: s.pool} }

func (s *PresenceStore) Touch(ctx context.Context, accountID string, at time.Time) error {
	if !uuidPattern.MatchString(accountID) {
		return nil
	}
	// Never back in time: two instances may write out of order.
	_, err := querierFor(ctx, s.pool).Exec(ctx, `INSERT INTO account.presence (account_id, seen_at) VALUES ($1::uuid, $2)
		ON CONFLICT (account_id) DO UPDATE SET seen_at = GREATEST(account.presence.seen_at, EXCLUDED.seen_at)`, accountID, at)
	return err
}

func (s *PresenceStore) Seen(ctx context.Context, accountIDs []string) (map[string]time.Time, error) {
	ids := validIDs(accountIDs)
	out := map[string]time.Time{}
	if len(ids) == 0 {
		return out, nil
	}
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT account_id::text, seen_at FROM account.presence
		WHERE account_id = ANY($1::uuid[]) AND seen_at IS NOT NULL`, ids)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	for rows.Next() {
		var id string
		var at time.Time
		if err := rows.Scan(&id, &at); err != nil {
			return nil, err
		}
		out[id] = at
	}
	return out, rows.Err()
}

func (s *PresenceStore) AppearingOffline(ctx context.Context, accountIDs []string) (map[string]bool, error) {
	ids := validIDs(accountIDs)
	out := map[string]bool{}
	if len(ids) == 0 {
		return out, nil
	}
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT account_id::text FROM account.presence
		WHERE account_id = ANY($1::uuid[]) AND appear_offline`, ids)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	for rows.Next() {
		var id string
		if err := rows.Scan(&id); err != nil {
			return nil, err
		}
		out[id] = true
	}
	return out, rows.Err()
}

func (s *PresenceStore) SetAppearOffline(ctx context.Context, accountID string, on bool) error {
	_, err := querierFor(ctx, s.pool).Exec(ctx, `INSERT INTO account.presence (account_id, appear_offline) VALUES ($1::uuid, $2)
		ON CONFLICT (account_id) DO UPDATE SET appear_offline = EXCLUDED.appear_offline`, accountID, on)
	return err
}

// validIDs keeps the account IDs a uuid column can hold.
func validIDs(accountIDs []string) []string {
	ids := make([]string, 0, len(accountIDs))
	for _, id := range accountIDs {
		if uuidPattern.MatchString(id) {
			ids = append(ids, id)
		}
	}
	return ids
}
