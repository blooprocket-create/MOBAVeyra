package postgres

import (
	"context"

	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/favorites"
)

// FavoritesStore implements favorites.Store.
type FavoritesStore struct{ pool *pgxpool.Pool }

// Favorites returns the favorites store.
func (s *Store) Favorites() *FavoritesStore { return &FavoritesStore{pool: s.pool} }

func (s *FavoritesStore) Favorites(ctx context.Context, accountID string) ([]string, error) {
	if !uuidPattern.MatchString(accountID) {
		return nil, nil
	}
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT vanguard_id FROM account.favorite_vanguards WHERE account_id = $1::uuid
		ORDER BY marked_at, vanguard_id`, accountID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []string
	for rows.Next() {
		var id string
		if err := rows.Scan(&id); err != nil {
			return nil, err
		}
		out = append(out, id)
	}
	return out, rows.Err()
}

func (s *FavoritesStore) Add(ctx context.Context, accountID, vanguardID string, most int) error {
	q := querierFor(ctx, s.pool)
	// One statement keeps the count and the insert together.
	tag, err := q.Exec(ctx, `INSERT INTO account.favorite_vanguards (account_id, vanguard_id, marked_at)
		SELECT $1::uuid, $2, now()
		WHERE (SELECT count(*) FROM account.favorite_vanguards WHERE account_id = $1::uuid) < $3
		ON CONFLICT (account_id, vanguard_id) DO NOTHING`, accountID, vanguardID, most)
	if err != nil {
		return err
	}
	if tag.RowsAffected() == 1 {
		return nil
	}
	// Nothing inserted: already a favorite, which is no error, or the account keeps its most.
	var marked bool
	if err := q.QueryRow(ctx, `SELECT EXISTS (SELECT 1 FROM account.favorite_vanguards WHERE account_id = $1::uuid AND vanguard_id = $2)`,
		accountID, vanguardID).Scan(&marked); err != nil {
		return err
	}
	if marked {
		return nil
	}
	return favorites.ErrFull
}

func (s *FavoritesStore) Remove(ctx context.Context, accountID, vanguardID string) error {
	if !uuidPattern.MatchString(accountID) {
		return nil
	}
	_, err := querierFor(ctx, s.pool).Exec(ctx, `DELETE FROM account.favorite_vanguards WHERE account_id = $1::uuid AND vanguard_id = $2`, accountID, vanguardID)
	return err
}
