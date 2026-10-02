package postgres

import (
	"context"
	"errors"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/profile"
)

// ProfileStore implements profile.Store.
type ProfileStore struct{ pool *pgxpool.Pool }

// Profile returns the profile store.
func (s *Store) Profile() *ProfileStore { return &ProfileStore{pool: s.pool} }

func (s *ProfileStore) Appearance(ctx context.Context, accountID string) (profile.Appearance, error) {
	if !uuidPattern.MatchString(accountID) {
		return profile.Appearance{}, profile.ErrNoAppearance
	}
	var a profile.Appearance
	var featured *string
	err := querierFor(ctx, s.pool).QueryRow(ctx, `SELECT icon, background, featured_vanguard, show_match_history FROM profile.appearances
		WHERE account_id = $1::uuid`, accountID).Scan(&a.Icon, &a.Background, &featured, &a.ShowMatchHistory)
	if errors.Is(err, pgx.ErrNoRows) {
		return profile.Appearance{}, profile.ErrNoAppearance
	}
	if err != nil {
		return profile.Appearance{}, err
	}
	if featured != nil {
		a.FeaturedVanguard = *featured
	}
	return a, nil
}

func (s *ProfileStore) DeleteAppearance(ctx context.Context, accountID string) error {
	if !uuidPattern.MatchString(accountID) {
		return nil
	}
	_, err := querierFor(ctx, s.pool).Exec(ctx, `DELETE FROM profile.appearances WHERE account_id = $1::uuid`, accountID)
	return err
}

func (s *ProfileStore) SaveAppearance(ctx context.Context, accountID string, a profile.Appearance) error {
	var featured *string
	if a.FeaturedVanguard != "" {
		featured = &a.FeaturedVanguard
	}
	_, err := querierFor(ctx, s.pool).Exec(ctx, `INSERT INTO profile.appearances (account_id, icon, background, featured_vanguard, show_match_history, updated_at)
		VALUES ($1::uuid, $2, $3, $4, $5, now())
		ON CONFLICT (account_id) DO UPDATE SET icon = EXCLUDED.icon, background = EXCLUDED.background,
			featured_vanguard = EXCLUDED.featured_vanguard, show_match_history = EXCLUDED.show_match_history, updated_at = now()`,
		accountID, a.Icon, a.Background, featured, a.ShowMatchHistory)
	return err
}
