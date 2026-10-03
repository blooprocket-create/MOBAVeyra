package postgres

import (
	"context"
	"time"

	"github.com/jackc/pgx/v5/pgxpool"
)

// DodgesStore implements dodges.Store.
type DodgesStore struct{ pool *pgxpool.Pool }

// Dodges returns the queue-restriction store.
func (s *Store) Dodges() *DodgesStore { return &DodgesStore{pool: s.pool} }

func (s *DodgesStore) Restrict(ctx context.Context, accountID string, until time.Time) error {
	_, err := querierFor(ctx, s.pool).Exec(ctx, `INSERT INTO account.queue_restrictions (account_id, restricted_until) VALUES ($1::uuid, $2)
		ON CONFLICT (account_id) DO UPDATE SET restricted_until = EXCLUDED.restricted_until`, accountID, until)
	return err
}

func (s *DodgesStore) Ends(ctx context.Context, accountIDs []string, now time.Time) (map[string]time.Time, error) {
	ids := make([]string, 0, len(accountIDs))
	for _, id := range accountIDs {
		if uuidPattern.MatchString(id) {
			ids = append(ids, id)
		}
	}
	out := map[string]time.Time{}
	if len(ids) == 0 {
		return out, nil
	}
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT account_id::text, restricted_until FROM account.queue_restrictions
		WHERE account_id = ANY($1::uuid[]) AND restricted_until > $2`, ids, now)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	for rows.Next() {
		var id string
		var until time.Time
		if err := rows.Scan(&id, &until); err != nil {
			return nil, err
		}
		out[id] = until
	}
	return out, rows.Err()
}
