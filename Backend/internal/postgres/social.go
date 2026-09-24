package postgres

import (
	"context"
	"regexp"
	"sort"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

var uuidPattern = regexp.MustCompile(`^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$`)

// SocialStore implements social.Store.
type SocialStore struct{ pool *pgxpool.Pool }

// Social returns the social-graph store.
func (s *Store) Social() *SocialStore { return &SocialStore{pool: s.pool} }

// inTx runs fn in a transaction, committing on success.
func inTx(ctx context.Context, pool *pgxpool.Pool, fn func(pgx.Tx) error) error {
	tx, err := pool.Begin(ctx)
	if err != nil {
		return err
	}
	defer tx.Rollback(ctx) //nolint:errcheck // no-op after commit
	if err := fn(tx); err != nil {
		return err
	}
	return tx.Commit(ctx)
}

func (s *SocialStore) InTx(ctx context.Context, fn func(social.Tx) error) error {
	return inTx(ctx, s.pool, func(tx pgx.Tx) error { return fn(socialTx{ctx: ctx, tx: tx}) })
}

type socialTx struct {
	ctx context.Context
	tx  pgx.Tx
}

func orderedPair(a, b string) (string, string) {
	if a < b {
		return a, b
	}
	return b, a
}

func (t socialTx) exists(sql string, args ...any) (bool, error) {
	var ok bool
	err := t.tx.QueryRow(t.ctx, `SELECT EXISTS (`+sql+`)`, args...).Scan(&ok)
	return ok, err
}

func (t socialTx) exec(sql string, args ...any) error {
	_, err := t.tx.Exec(t.ctx, sql, args...)
	return err
}

func (t socialTx) LockPair(a, b string) error {
	lo, hi := orderedPair(a, b)
	return t.exec(`SELECT pg_advisory_xact_lock(hashtextextended($1, 0))`, "social:"+lo+":"+hi)
}

func (t socialTx) AccountExists(id string) (bool, error) {
	if !uuidPattern.MatchString(id) {
		return false, nil
	}
	return t.exists(`SELECT 1 FROM identity.accounts WHERE id = $1::uuid`, id)
}

func (t socialTx) BlockedEither(a, b string) (bool, error) {
	return t.exists(`SELECT 1 FROM social.blocks
		WHERE (blocker_id = $1::uuid AND blocked_id = $2::uuid) OR (blocker_id = $2::uuid AND blocked_id = $1::uuid)`, a, b)
}

func (t socialTx) AreFriends(a, b string) (bool, error) {
	lo, hi := orderedPair(a, b)
	return t.exists(`SELECT 1 FROM social.friendships WHERE account_a = $1::uuid AND account_b = $2::uuid`, lo, hi)
}

func (t socialTx) RequestExists(from, to string) (bool, error) {
	return t.exists(`SELECT 1 FROM social.friend_requests WHERE from_id = $1::uuid AND to_id = $2::uuid`, from, to)
}

func (t socialTx) PutRequest(from, to string) error {
	return t.exec(`INSERT INTO social.friend_requests (from_id, to_id) VALUES ($1::uuid, $2::uuid)
		ON CONFLICT DO NOTHING`, from, to)
}

func (t socialTx) DeleteRequest(from, to string) error {
	return t.exec(`DELETE FROM social.friend_requests WHERE from_id = $1::uuid AND to_id = $2::uuid`, from, to)
}

func (t socialTx) PutFriendship(a, b string) error {
	lo, hi := orderedPair(a, b)
	return t.exec(`INSERT INTO social.friendships (account_a, account_b) VALUES ($1::uuid, $2::uuid)
		ON CONFLICT DO NOTHING`, lo, hi)
}

func (t socialTx) DeleteFriendship(a, b string) error {
	lo, hi := orderedPair(a, b)
	return t.exec(`DELETE FROM social.friendships WHERE account_a = $1::uuid AND account_b = $2::uuid`, lo, hi)
}

func (t socialTx) PutBlock(blocker, blocked string) error {
	return t.exec(`INSERT INTO social.blocks (blocker_id, blocked_id) VALUES ($1::uuid, $2::uuid)
		ON CONFLICT DO NOTHING`, blocker, blocked)
}

func (t socialTx) DeleteBlock(blocker, blocked string) error {
	return t.exec(`DELETE FROM social.blocks WHERE blocker_id = $1::uuid AND blocked_id = $2::uuid`, blocker, blocked)
}

func (s *SocialStore) ids(ctx context.Context, sql string, args ...any) ([]string, error) {
	rows, err := s.pool.Query(ctx, sql, args...)
	if err != nil {
		return nil, err
	}
	out, err := pgx.CollectRows(rows, pgx.RowTo[string])
	sort.Strings(out)
	return out, err
}

func (s *SocialStore) Friends(ctx context.Context, account string) ([]string, error) {
	return s.ids(ctx, `
		SELECT account_b::text FROM social.friendships WHERE account_a = $1::uuid
		UNION ALL
		SELECT account_a::text FROM social.friendships WHERE account_b = $1::uuid`, account)
}

func (s *SocialStore) IncomingRequests(ctx context.Context, account string) ([]string, error) {
	return s.ids(ctx, `SELECT from_id::text FROM social.friend_requests WHERE to_id = $1::uuid`, account)
}

func (s *SocialStore) OutgoingRequests(ctx context.Context, account string) ([]string, error) {
	return s.ids(ctx, `SELECT to_id::text FROM social.friend_requests WHERE from_id = $1::uuid`, account)
}

func (s *SocialStore) BlocksBy(ctx context.Context, account string) ([]string, error) {
	return s.ids(ctx, `SELECT blocked_id::text FROM social.blocks WHERE blocker_id = $1::uuid`, account)
}

func (s *SocialStore) exists(ctx context.Context, sql string, args ...any) (bool, error) {
	var ok bool
	err := s.pool.QueryRow(ctx, `SELECT EXISTS (`+sql+`)`, args...).Scan(&ok)
	return ok, err
}

func (s *SocialStore) AreFriends(ctx context.Context, a, b string) (bool, error) {
	lo, hi := orderedPair(a, b)
	return s.exists(ctx, `SELECT 1 FROM social.friendships WHERE account_a = $1::uuid AND account_b = $2::uuid`, lo, hi)
}

func (s *SocialStore) FriendOfAny(ctx context.Context, account string, others []string) (bool, error) {
	return s.exists(ctx, `SELECT 1 FROM social.friendships
		WHERE (account_a = $1::uuid AND account_b = ANY($2::uuid[]))
		   OR (account_b = $1::uuid AND account_a = ANY($2::uuid[]))`, account, others)
}

func (s *SocialStore) BlockedWithAny(ctx context.Context, account string, others []string) (bool, error) {
	return s.exists(ctx, `SELECT 1 FROM social.blocks
		WHERE (blocker_id = $1::uuid AND blocked_id = ANY($2::uuid[]))
		   OR (blocked_id = $1::uuid AND blocker_id = ANY($2::uuid[]))`, account, others)
}
