// Package postgres implements backend storage on Postgres, the database of
// record under ADR-005, and applies embedded schema migrations.
package postgres

import (
	"context"
	"embed"
	"errors"
	"fmt"
	"io/fs"
	"sort"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
)

//go:embed migrations/*.sql
var migrationFiles embed.FS

// migrationLockKey is an arbitrary fixed key for pg_advisory_xact_lock so
// concurrently starting backends apply migrations one at a time.
const migrationLockKey = 0x5665797261 // "Veyra"

// Store is the Postgres-backed store.
type Store struct {
	pool *pgxpool.Pool
}

// Open connects to Postgres and applies pending migrations.
func Open(ctx context.Context, databaseURL string) (*Store, error) {
	pool, err := pgxpool.New(ctx, databaseURL)
	if err != nil {
		return nil, fmt.Errorf("connect: %w", err)
	}
	if err := migrate(ctx, pool); err != nil {
		pool.Close()
		return nil, err
	}
	return &Store{pool: pool}, nil
}

// Close releases the connection pool.
func (s *Store) Close() { s.pool.Close() }

// Ping reports whether the database is reachable.
func (s *Store) Ping(ctx context.Context) error { return s.pool.Ping(ctx) }

func migrate(ctx context.Context, pool *pgxpool.Pool) error {
	names, err := fs.Glob(migrationFiles, "migrations/*.sql")
	if err != nil {
		return err
	}
	sort.Strings(names)

	tx, err := pool.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(ctx) //nolint:errcheck // no-op after commit

	if _, err := tx.Exec(ctx, `SELECT pg_advisory_xact_lock($1)`, migrationLockKey); err != nil {
		return fmt.Errorf("migration lock: %w", err)
	}
	if _, err := tx.Exec(ctx, `CREATE TABLE IF NOT EXISTS public.schema_migrations (
		name text PRIMARY KEY, applied_at timestamptz NOT NULL DEFAULT now())`); err != nil {
		return fmt.Errorf("create schema_migrations: %w", err)
	}
	for _, name := range names {
		var applied bool
		if err := tx.QueryRow(ctx, `SELECT EXISTS (SELECT 1 FROM public.schema_migrations WHERE name = $1)`, name).Scan(&applied); err != nil {
			return err
		}
		if applied {
			continue
		}
		body, err := migrationFiles.ReadFile(name)
		if err != nil {
			return err
		}
		if _, err := tx.Exec(ctx, string(body)); err != nil {
			return fmt.Errorf("apply %s: %w", name, err)
		}
		if _, err := tx.Exec(ctx, `INSERT INTO public.schema_migrations (name) VALUES ($1)`, name); err != nil {
			return err
		}
	}
	return tx.Commit(ctx)
}

func (s *Store) EnsureDevAccount(ctx context.Context, displayName string) (identity.Account, error) {
	var a identity.Account
	err := s.pool.QueryRow(ctx, `
		INSERT INTO identity.accounts (display_name, dev_seeded) VALUES ($1, true)
		ON CONFLICT (display_name) DO UPDATE SET display_name = EXCLUDED.display_name
		RETURNING id::text, display_name`, displayName).Scan(&a.ID, &a.DisplayName)
	return a, err
}

func (s *Store) AccountByDisplayName(ctx context.Context, displayName string) (identity.Account, error) {
	var a identity.Account
	err := s.pool.QueryRow(ctx, `SELECT id::text, display_name FROM identity.accounts WHERE display_name = $1`,
		displayName).Scan(&a.ID, &a.DisplayName)
	return a, notFound(err)
}

func (s *Store) AccountByID(ctx context.Context, id string) (identity.Account, error) {
	var a identity.Account
	err := s.pool.QueryRow(ctx, `SELECT id::text, display_name FROM identity.accounts WHERE id = $1::uuid`,
		id).Scan(&a.ID, &a.DisplayName)
	return a, notFound(err)
}

func (s *Store) CreateSession(ctx context.Context, sess identity.Session) error {
	var build *string
	if sess.BuildVersion != "" {
		build = &sess.BuildVersion
	}
	_, err := s.pool.Exec(ctx, `
		INSERT INTO identity.sessions (token_hash, account_id, kind, build_version, created_at, expires_at)
		VALUES ($1, $2::uuid, $3, $4, $5, $6)`,
		sess.TokenHash, sess.AccountID, string(sess.Kind), build, sess.CreatedAt, sess.ExpiresAt)
	return err
}

func (s *Store) ActiveSession(ctx context.Context, tokenHash []byte, now time.Time) (identity.Session, error) {
	var sess identity.Session
	var kind string
	var build *string
	err := s.pool.QueryRow(ctx, `
		SELECT token_hash, account_id::text, kind, build_version, created_at, expires_at
		FROM identity.sessions
		WHERE token_hash = $1 AND revoked_at IS NULL AND expires_at > $2`,
		tokenHash, now).Scan(&sess.TokenHash, &sess.AccountID, &kind, &build, &sess.CreatedAt, &sess.ExpiresAt)
	if err != nil {
		return identity.Session{}, notFound(err)
	}
	sess.Kind = identity.SessionKind(kind)
	if build != nil {
		sess.BuildVersion = *build
	}
	return sess, nil
}

func (s *Store) CreateLaunchCode(ctx context.Context, c identity.LaunchCode) error {
	_, err := s.pool.Exec(ctx, `
		INSERT INTO identity.launch_codes (code_hash, account_id, build_version, created_at, expires_at)
		VALUES ($1, $2::uuid, $3, $4, $5)`,
		c.CodeHash, c.AccountID, c.BuildVersion, c.CreatedAt, c.ExpiresAt)
	return err
}

func (s *Store) ConsumeLaunchCode(ctx context.Context, codeHash []byte, now time.Time) (identity.LaunchCode, error) {
	var c identity.LaunchCode
	err := s.pool.QueryRow(ctx, `
		UPDATE identity.launch_codes SET consumed_at = $2
		WHERE code_hash = $1 AND consumed_at IS NULL AND expires_at > $2
		RETURNING code_hash, account_id::text, build_version, created_at, expires_at`,
		codeHash, now).Scan(&c.CodeHash, &c.AccountID, &c.BuildVersion, &c.CreatedAt, &c.ExpiresAt)
	return c, notFound(err)
}

func notFound(err error) error {
	if errors.Is(err, pgx.ErrNoRows) {
		return identity.ErrNotFound
	}
	return err
}
