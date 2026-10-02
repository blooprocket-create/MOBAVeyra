package postgres

import (
	"context"
	"errors"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/progression"
)

// ProgressionStore implements progression.Store.
type ProgressionStore struct{ pool *pgxpool.Pool }

// Progression returns the account progression store.
func (s *Store) Progression() *ProgressionStore { return &ProgressionStore{pool: s.pool} }

// InTx joins the transaction ctx carries, as the match result's does, or
// opens one.
func (s *ProgressionStore) InTx(ctx context.Context, fn func(context.Context, progression.Tx) error) error {
	return inTx(ctx, s.pool, func(ctx context.Context, tx pgx.Tx) error { return fn(ctx, progressionTx{ctx: ctx, q: tx}) })
}

type progressionTx struct {
	ctx context.Context
	q   querier
}

func scanAccount(row pgx.Row, accountID string) (progression.Account, error) {
	a := progression.Account{AccountID: accountID, Level: 1}
	err := row.Scan(&a.Level, &a.LevelXP, &a.LifetimeXP, &a.Flux, &a.RefinedFlux)
	if errors.Is(err, pgx.ErrNoRows) {
		return progression.Account{AccountID: accountID, Level: 1}, nil
	}
	return a, err
}

func (t progressionTx) LockAccount(accountID string) (progression.Account, error) {
	// A first lock creates the account's row, so concurrent locks queue on it.
	if _, err := t.q.Exec(t.ctx, `INSERT INTO progression.accounts (account_id, level, level_xp, lifetime_xp, flux, refined_flux, updated_at)
		VALUES ($1::uuid, 1, 0, 0, 0, 0, now()) ON CONFLICT (account_id) DO NOTHING`, accountID); err != nil {
		return progression.Account{}, err
	}
	return scanAccount(t.q.QueryRow(t.ctx, `SELECT level, level_xp, lifetime_xp, flux, refined_flux FROM progression.accounts
		WHERE account_id = $1::uuid FOR UPDATE`, accountID), accountID)
}

func (t progressionTx) SaveAccount(a progression.Account) error {
	_, err := t.q.Exec(t.ctx, `UPDATE progression.accounts SET level = $2, level_xp = $3, lifetime_xp = $4, flux = $5, refined_flux = $6, updated_at = now()
		WHERE account_id = $1::uuid`, a.AccountID, a.Level, a.LevelXP, a.LifetimeXP, a.Flux, a.RefinedFlux)
	return err
}

func (t progressionTx) LockMastery(accountID, vanguardID string) (progression.Mastery, error) {
	if _, err := t.q.Exec(t.ctx, `INSERT INTO progression.mastery (account_id, vanguard_id, level, level_points, lifetime_points, updated_at)
		VALUES ($1::uuid, $2, 1, 0, 0, now()) ON CONFLICT (account_id, vanguard_id) DO NOTHING`, accountID, vanguardID); err != nil {
		return progression.Mastery{}, err
	}
	m := progression.Mastery{VanguardID: vanguardID}
	err := t.q.QueryRow(t.ctx, `SELECT level, level_points, lifetime_points FROM progression.mastery
		WHERE account_id = $1::uuid AND vanguard_id = $2 FOR UPDATE`, accountID, vanguardID).Scan(&m.Level, &m.LevelPoints, &m.LifetimePoints)
	return m, err
}

func (t progressionTx) SaveMastery(accountID string, m progression.Mastery) error {
	_, err := t.q.Exec(t.ctx, `UPDATE progression.mastery SET level = $3, level_points = $4, lifetime_points = $5, updated_at = now()
		WHERE account_id = $1::uuid AND vanguard_id = $2`, accountID, m.VanguardID, m.Level, m.LevelPoints, m.LifetimePoints)
	return err
}

func (t progressionTx) AddGrant(g progression.Grant) error {
	tag, err := t.q.Exec(t.ctx, `INSERT INTO progression.grants (match_id, account_id, reason, account_xp, level_before, level_after, flux, refined_flux,
		vanguard_id, mastery_points, mastery_before, mastery_after, granted_at)
		VALUES ($1::uuid, $2::uuid, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13) ON CONFLICT (match_id, account_id) DO NOTHING`,
		g.MatchID, g.AccountID, string(g.Reason), g.AccountXP, g.LevelBefore, g.LevelAfter, g.Flux, g.RefinedFlux,
		g.VanguardID, g.MasteryPoints, g.MasteryBefore, g.MasteryAfter, g.GrantedAt)
	if err != nil {
		return err
	}
	if tag.RowsAffected() == 0 {
		return progression.ErrAlreadyGranted
	}
	return nil
}

func (t progressionTx) Purchase(purchaseID string) (progression.Purchase, error) {
	p := progression.Purchase{PurchaseID: purchaseID}
	var currency string
	err := t.q.QueryRow(t.ctx, `SELECT account_id::text, vanguard_id, currency, price, purchased_at FROM progression.purchases WHERE purchase_id = $1`,
		purchaseID).Scan(&p.AccountID, &p.VanguardID, &currency, &p.Price, &p.PurchasedAt)
	if errors.Is(err, pgx.ErrNoRows) {
		return progression.Purchase{}, progression.ErrPurchaseNotFound
	}
	p.Currency = progression.Currency(currency)
	return p, err
}

func (t progressionTx) AddPurchase(p progression.Purchase) error {
	_, err := t.q.Exec(t.ctx, `INSERT INTO progression.purchases (purchase_id, account_id, vanguard_id, currency, price, purchased_at)
		VALUES ($1, $2::uuid, $3, $4, $5, $6)`, p.PurchaseID, p.AccountID, p.VanguardID, string(p.Currency), p.Price, p.PurchasedAt)
	var pgErr *pgconn.PgError
	if errors.As(err, &pgErr) && pgErr.Code == uniqueViolation {
		// Another account's purchase holds the ID.
		return progression.ErrPurchaseConflict
	}
	return err
}

func (t progressionTx) AddDevAdjustment(a progression.DevAdjustment) error {
	_, err := t.q.Exec(t.ctx, `INSERT INTO progression.dev_adjustments (account_id, flux, refined_flux, granted_at) VALUES ($1::uuid, $2, $3, $4)`,
		a.AccountID, a.Flux, a.RefinedFlux, a.GrantedAt)
	return err
}

func (s *ProgressionStore) Account(ctx context.Context, accountID string) (progression.Account, error) {
	if !uuidPattern.MatchString(accountID) {
		return progression.Account{AccountID: accountID, Level: 1}, nil
	}
	return scanAccount(querierFor(ctx, s.pool).QueryRow(ctx, `SELECT level, level_xp, lifetime_xp, flux, refined_flux FROM progression.accounts
		WHERE account_id = $1::uuid`, accountID), accountID)
}

func (s *ProgressionStore) Masteries(ctx context.Context, accountID string) ([]progression.Mastery, error) {
	if !uuidPattern.MatchString(accountID) {
		return nil, nil
	}
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT vanguard_id, level, level_points, lifetime_points FROM progression.mastery
		WHERE account_id = $1::uuid ORDER BY vanguard_id`, accountID)
	if err != nil {
		return nil, err
	}
	return pgx.CollectRows(rows, func(r pgx.CollectableRow) (progression.Mastery, error) {
		var m progression.Mastery
		err := r.Scan(&m.VanguardID, &m.Level, &m.LevelPoints, &m.LifetimePoints)
		return m, err
	})
}

func (s *ProgressionStore) Grant(ctx context.Context, matchID, accountID string) (progression.Grant, error) {
	if !uuidPattern.MatchString(matchID) || !uuidPattern.MatchString(accountID) {
		return progression.Grant{}, progression.ErrNoGrant
	}
	g := progression.Grant{MatchID: matchID, AccountID: accountID}
	var reason string
	err := querierFor(ctx, s.pool).QueryRow(ctx, `SELECT reason, account_xp, level_before, level_after, flux, refined_flux, vanguard_id,
		mastery_points, mastery_before, mastery_after, granted_at FROM progression.grants WHERE match_id = $1::uuid AND account_id = $2::uuid`,
		matchID, accountID).Scan(&reason, &g.AccountXP, &g.LevelBefore, &g.LevelAfter, &g.Flux, &g.RefinedFlux, &g.VanguardID,
		&g.MasteryPoints, &g.MasteryBefore, &g.MasteryAfter, &g.GrantedAt)
	if errors.Is(err, pgx.ErrNoRows) {
		return progression.Grant{}, progression.ErrNoGrant
	}
	g.Reason = progression.Reason(reason)
	return g, err
}
