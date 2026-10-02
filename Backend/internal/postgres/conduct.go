package postgres

import (
	"context"
	"errors"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/conduct"
)

// ConductStore implements conduct.Store.
type ConductStore struct{ pool *pgxpool.Pool }

// Conduct returns the reports and commendations store.
func (s *Store) Conduct() *ConductStore { return &ConductStore{pool: s.pool} }

// InTx joins the transaction ctx carries, or opens one.
func (s *ConductStore) InTx(ctx context.Context, fn func(context.Context, conduct.Tx) error) error {
	return inTx(ctx, s.pool, func(ctx context.Context, tx pgx.Tx) error { return fn(ctx, conductTx{ctx: ctx, q: tx}) })
}

const reportColumns = `match_id::text, reporter_id::text, reported_id::text, reported_name, reason, details, client_id, created_at`

func scanReport(row pgx.Row) (conduct.Report, error) {
	var r conduct.Report
	err := row.Scan(&r.MatchID, &r.ReporterID, &r.ReportedID, &r.ReportedName, &r.Reason, &r.Details, &r.ClientID, &r.CreatedAt)
	r.CreatedAt = r.CreatedAt.UTC()
	return r, err
}

func collectReports(rows pgx.Rows, err error) ([]conduct.Report, error) {
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []conduct.Report
	for rows.Next() {
		r, err := scanReport(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, r)
	}
	return out, rows.Err()
}

func (s *ConductStore) ReportsBy(ctx context.Context, matchID, reporterID string) ([]conduct.Report, error) {
	if !uuidPattern.MatchString(matchID) || !uuidPattern.MatchString(reporterID) {
		return nil, nil
	}
	return collectReports(querierFor(ctx, s.pool).Query(ctx, `SELECT `+reportColumns+` FROM conduct.reports
		WHERE match_id = $1::uuid AND reporter_id = $2::uuid ORDER BY created_at, reported_name`, matchID, reporterID))
}

const commendationColumns = `match_id::text, commender_id::text, commended_id::text, commended_name, created_at`

func scanCommendation(row pgx.Row) (conduct.Commendation, error) {
	var c conduct.Commendation
	err := row.Scan(&c.MatchID, &c.CommenderID, &c.CommendedID, &c.CommendedName, &c.CreatedAt)
	if errors.Is(err, pgx.ErrNoRows) {
		return conduct.Commendation{}, conduct.ErrNoCommendation
	}
	c.CreatedAt = c.CreatedAt.UTC()
	return c, err
}

func (s *ConductStore) CommendationBy(ctx context.Context, matchID, commenderID string) (conduct.Commendation, error) {
	if !uuidPattern.MatchString(matchID) || !uuidPattern.MatchString(commenderID) {
		return conduct.Commendation{}, conduct.ErrNoCommendation
	}
	return scanCommendation(querierFor(ctx, s.pool).QueryRow(ctx, `SELECT `+commendationColumns+` FROM conduct.commendations
		WHERE match_id = $1::uuid AND commender_id = $2::uuid`, matchID, commenderID))
}

func (s *ConductStore) Case(ctx context.Context, matchID string) (conduct.Case, bool, error) {
	if !uuidPattern.MatchString(matchID) {
		return conduct.Case{}, false, nil
	}
	q := querierFor(ctx, s.pool)
	c := conduct.Case{MatchID: matchID}
	err := q.QueryRow(ctx, `SELECT opened_at FROM conduct.cases WHERE match_id = $1::uuid`, matchID).Scan(&c.OpenedAt)
	if errors.Is(err, pgx.ErrNoRows) {
		return conduct.Case{}, false, nil
	}
	if err != nil {
		return conduct.Case{}, false, err
	}
	c.OpenedAt = c.OpenedAt.UTC()
	c.Reports, err = collectReports(q.Query(ctx, `SELECT `+reportColumns+` FROM conduct.reports WHERE match_id = $1::uuid ORDER BY created_at`, matchID))
	return c, err == nil, err
}

func (s *ConductStore) Commendations(ctx context.Context, matchID string) ([]conduct.Commendation, error) {
	if !uuidPattern.MatchString(matchID) {
		return nil, nil
	}
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT `+commendationColumns+` FROM conduct.commendations WHERE match_id = $1::uuid ORDER BY created_at`, matchID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []conduct.Commendation
	for rows.Next() {
		c, err := scanCommendation(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, c)
	}
	return out, rows.Err()
}

type conductTx struct {
	ctx context.Context
	q   querier
}

func (t conductTx) LockCase(matchID string) error {
	_, err := t.q.Exec(t.ctx, `SELECT pg_advisory_xact_lock(hashtextextended('conduct:' || $1, 0))`, matchID)
	return err
}

func (t conductTx) Report(matchID, reporterID, reportedID string) (conduct.Report, error) {
	r, err := scanReport(t.q.QueryRow(t.ctx, `SELECT `+reportColumns+` FROM conduct.reports
		WHERE match_id = $1::uuid AND reporter_id = $2::uuid AND reported_id = $3::uuid`, matchID, reporterID, reportedID))
	if errors.Is(err, pgx.ErrNoRows) {
		return conduct.Report{}, conduct.ErrReportNotFound
	}
	return r, err
}

func (t conductTx) ReportByClientID(reporterID, clientID string) (conduct.Report, error) {
	r, err := scanReport(t.q.QueryRow(t.ctx, `SELECT `+reportColumns+` FROM conduct.reports WHERE reporter_id = $1::uuid AND client_id = $2`, reporterID, clientID))
	if errors.Is(err, pgx.ErrNoRows) {
		return conduct.Report{}, conduct.ErrReportNotFound
	}
	return r, err
}

func (t conductTx) OpenCase(matchID string, at time.Time) error {
	_, err := t.q.Exec(t.ctx, `INSERT INTO conduct.cases (match_id, opened_at) VALUES ($1::uuid, $2) ON CONFLICT (match_id) DO NOTHING`, matchID, at)
	return err
}

func (t conductTx) AddReport(r conduct.Report) error {
	_, err := t.q.Exec(t.ctx, `INSERT INTO conduct.reports (match_id, reporter_id, reported_id, reported_name, reason, details, client_id, created_at)
		VALUES ($1::uuid, $2::uuid, $3::uuid, $4, $5, $6, $7, $8)`, r.MatchID, r.ReporterID, r.ReportedID, r.ReportedName, r.Reason, r.Details, r.ClientID, r.CreatedAt)
	return err
}

func (t conductTx) Commendation(matchID, commenderID string) (conduct.Commendation, error) {
	return scanCommendation(t.q.QueryRow(t.ctx, `SELECT `+commendationColumns+` FROM conduct.commendations
		WHERE match_id = $1::uuid AND commender_id = $2::uuid`, matchID, commenderID))
}

func (t conductTx) AddCommendation(c conduct.Commendation) error {
	_, err := t.q.Exec(t.ctx, `INSERT INTO conduct.commendations (match_id, commender_id, commended_id, commended_name, created_at)
		VALUES ($1::uuid, $2::uuid, $3::uuid, $4, $5)`, c.MatchID, c.CommenderID, c.CommendedID, c.CommendedName, c.CreatedAt)
	return err
}
