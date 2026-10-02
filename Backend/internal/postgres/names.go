package postgres

import (
	"context"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
)

// Display-name changes (ADR-049). Every call joins the caller's unit of work.

func (s *Store) nameState(ctx context.Context, accountID string, lock bool) (identity.NameState, error) {
	if !uuidPattern.MatchString(accountID) {
		return identity.NameState{}, identity.ErrNotFound
	}
	sql := `SELECT free_rename_used, last_rename_at, rename_required, last_launcher_login_at FROM identity.accounts WHERE id = $1::uuid`
	if lock {
		sql += ` FOR UPDATE`
	}
	var st identity.NameState
	var last *time.Time
	err := querierFor(ctx, s.pool).QueryRow(ctx, sql, accountID).Scan(&st.FreeChangeUsed, &last, &st.RenameRequired, &st.LastLauncherLogin)
	if errors.Is(err, pgx.ErrNoRows) {
		return identity.NameState{}, identity.ErrNotFound
	}
	st.LastChangeAt = timeOrZero(last)
	return st, err
}

func (s *Store) NameState(ctx context.Context, accountID string) (identity.NameState, error) {
	return s.nameState(ctx, accountID, false)
}

func (s *Store) LockNameState(ctx context.Context, accountID string) (identity.NameState, error) {
	return s.nameState(ctx, accountID, true)
}

func (s *Store) HolderOfName(ctx context.Context, name string) (identity.Account, error) {
	var a identity.Account
	err := querierFor(ctx, s.pool).QueryRow(ctx, `SELECT id::text, display_name FROM identity.accounts WHERE lower(display_name) = lower($1) FOR UPDATE`,
		name).Scan(&a.ID, &a.DisplayName)
	return a, notFound(err)
}

// renameError maps a uniqueness violation to ErrDisplayNameTaken.
func renameError(err error) error {
	var pgErr *pgconn.PgError
	if errors.As(err, &pgErr) && pgErr.Code == uniqueViolation {
		return identity.ErrDisplayNameTaken
	}
	return err
}

func (s *Store) SetDisplayName(ctx context.Context, accountID, name string, voluntary bool, at time.Time) error {
	sql := `UPDATE identity.accounts SET display_name = $2, rename_required = false WHERE id = $1::uuid`
	args := []any{accountID, name}
	if voluntary {
		sql = `UPDATE identity.accounts SET display_name = $2, free_rename_used = true, last_rename_at = $3 WHERE id = $1::uuid`
		args = append(args, at)
	}
	tag, err := querierFor(ctx, s.pool).Exec(ctx, sql, args...)
	if err != nil {
		return renameError(err)
	}
	if tag.RowsAffected() == 0 {
		return identity.ErrNotFound
	}
	return nil
}

func (s *Store) RequireRename(ctx context.Context, accountID, placeholder string) error {
	_, err := querierFor(ctx, s.pool).Exec(ctx, `UPDATE identity.accounts SET display_name = $2, rename_required = true WHERE id = $1::uuid`,
		accountID, placeholder)
	return renameError(err)
}

func (s *Store) TouchLauncherLogin(ctx context.Context, accountID string, at time.Time) error {
	_, err := querierFor(ctx, s.pool).Exec(ctx, `UPDATE identity.accounts SET last_launcher_login_at = $2 WHERE id = $1::uuid`, accountID, at)
	return err
}
