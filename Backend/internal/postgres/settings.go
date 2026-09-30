package postgres

import (
	"context"
	"encoding/json"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/settings"
)

// SettingsStore implements settings.Store.
type SettingsStore struct{ pool *pgxpool.Pool }

// Settings returns the account settings store.
func (s *Store) Settings() *SettingsStore { return &SettingsStore{pool: s.pool} }

func (s *SettingsStore) Get(ctx context.Context, accountID string) (settings.Document, error) {
	d := settings.Document{Values: map[string]string{}}
	if !uuidPattern.MatchString(accountID) {
		return d, nil
	}
	var doc []byte
	err := querierFor(ctx, s.pool).QueryRow(ctx, `SELECT revision, doc FROM account.settings WHERE account_id = $1::uuid`, accountID).Scan(&d.Revision, &doc)
	if errors.Is(err, pgx.ErrNoRows) {
		return d, nil
	}
	if err != nil {
		return settings.Document{}, err
	}
	if err := json.Unmarshal(doc, &d.Values); err != nil {
		return settings.Document{}, err
	}
	return d, nil
}

func (s *SettingsStore) Put(ctx context.Context, accountID string, base int64, values map[string]string) (settings.Document, error) {
	doc, err := json.Marshal(values)
	if err != nil {
		return settings.Document{}, err
	}
	now := time.Now().UTC()
	q := querierFor(ctx, s.pool)
	var revision int64
	// The first write inserts, and any later one updates the revision it was
	// based on; either way a stale base changes nothing.
	if base == 0 {
		err = q.QueryRow(ctx, `INSERT INTO account.settings (account_id, revision, doc, updated_at) VALUES ($1::uuid, 1, $2, $3)
			ON CONFLICT (account_id) DO NOTHING RETURNING revision`, accountID, doc, now).Scan(&revision)
	} else {
		err = q.QueryRow(ctx, `UPDATE account.settings SET revision = revision + 1, doc = $3, updated_at = $4
			WHERE account_id = $1::uuid AND revision = $2 RETURNING revision`, accountID, base, doc, now).Scan(&revision)
	}
	if errors.Is(err, pgx.ErrNoRows) {
		return settings.Document{}, settings.ErrConflict
	}
	if err != nil {
		return settings.Document{}, err
	}
	return settings.Document{Revision: revision, Values: values}, nil
}
