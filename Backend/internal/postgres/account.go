package postgres

import (
	"context"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
)

// AccountStore implements account.Store.
type AccountStore struct{ pool *pgxpool.Pool }

// Account returns the onboarding and entitlement store.
func (s *Store) Account() *AccountStore { return &AccountStore{pool: s.pool} }

func (s *AccountStore) InTx(ctx context.Context, fn func(account.Tx) error) error {
	return inTx(ctx, s.pool, func(ctx context.Context, tx pgx.Tx) error { return fn(accountTx{ctx: ctx, q: tx}) })
}

type accountTx struct {
	ctx context.Context
	q   querier
}

func (t accountTx) CompleteOnboarding(p account.Profile) error {
	_, err := t.q.Exec(t.ctx, `INSERT INTO account.onboarding (account_id, starter_vanguard_id, completed_at) VALUES ($1::uuid, $2, $3)`,
		p.AccountID, p.StarterVanguardID, p.CompletedAt)
	var pgErr *pgconn.PgError
	if errors.As(err, &pgErr) && pgErr.Code == uniqueViolation {
		return account.ErrAlreadyChosen
	}
	return err
}

func (t accountTx) Grant(accountID string, e account.Entitlement) error {
	_, err := t.q.Exec(t.ctx, `INSERT INTO account.entitlements (account_id, vanguard_id, source, granted_at)
		VALUES ($1::uuid, $2, $3, $4) ON CONFLICT (account_id, vanguard_id) DO NOTHING`,
		accountID, e.VanguardID, string(e.Source), e.GrantedAt)
	return err
}

func (s *AccountStore) Profile(ctx context.Context, accountID string) (account.Profile, error) {
	p := account.Profile{AccountID: accountID}
	if !uuidPattern.MatchString(accountID) {
		return p, nil
	}
	var completedAt time.Time
	err := querierFor(ctx, s.pool).QueryRow(ctx, `SELECT starter_vanguard_id, completed_at FROM account.onboarding WHERE account_id = $1::uuid`,
		accountID).Scan(&p.StarterVanguardID, &completedAt)
	if errors.Is(err, pgx.ErrNoRows) {
		return p, nil
	}
	if err != nil {
		return account.Profile{}, err
	}
	p.TutorialCompleted, p.CompletedAt = true, completedAt
	return p, nil
}

func (s *AccountStore) Entitlements(ctx context.Context, accountID string) ([]account.Entitlement, error) {
	if !uuidPattern.MatchString(accountID) {
		return nil, nil
	}
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT vanguard_id, source, granted_at FROM account.entitlements
		WHERE account_id = $1::uuid ORDER BY granted_at, vanguard_id`, accountID)
	if err != nil {
		return nil, err
	}
	return pgx.CollectRows(rows, func(r pgx.CollectableRow) (account.Entitlement, error) {
		var e account.Entitlement
		var source string
		err := r.Scan(&e.VanguardID, &source, &e.GrantedAt)
		e.Source = account.Source(source)
		return e, err
	})
}

func (s *AccountStore) ResetOnboarding(ctx context.Context, accountID string) error {
	if !uuidPattern.MatchString(accountID) {
		return nil
	}
	return inTx(ctx, s.pool, func(ctx context.Context, tx pgx.Tx) error {
		if _, err := tx.Exec(ctx, `DELETE FROM account.onboarding WHERE account_id = $1::uuid`, accountID); err != nil {
			return err
		}
		_, err := tx.Exec(ctx, `DELETE FROM account.entitlements WHERE account_id = $1::uuid AND source = $2`, accountID, string(account.SourceStarter))
		return err
	})
}
