package postgres

import (
	"context"
	"errors"
	"fmt"
	"slices"
	"strings"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/chat"
)

// ChatStore implements chat.Store.
type ChatStore struct{ pool *pgxpool.Pool }

// Chat returns the chat store.
func (s *Store) Chat() *ChatStore { return &ChatStore{pool: s.pool} }

// InTx joins the transaction ctx carries, or opens one.
func (s *ChatStore) InTx(ctx context.Context, fn func(context.Context, chat.Tx) error) error {
	return inTx(ctx, s.pool, func(ctx context.Context, tx pgx.Tx) error { return fn(ctx, chatTx{ctx: ctx, q: tx}) })
}

const messageColumns = `seq, kind, conversation, sender_id::text, sender_name, coalesce(recipient_id::text, ''), body, sent_at, client_id`

func scanMessage(row pgx.Row) (chat.Message, error) {
	var m chat.Message
	var kind string
	err := row.Scan(&m.Seq, &kind, &m.Key, &m.SenderID, &m.SenderName, &m.RecipientID, &m.Text, &m.SentAt, &m.ClientID)
	m.Kind = chat.Kind(kind)
	m.SentAt = m.SentAt.UTC()
	return m, err
}

func (s *ChatStore) Messages(ctx context.Context, q chat.Query) ([]chat.Message, error) {
	args := []any{q.After, q.Since}
	arg := func(v any) string {
		args = append(args, v)
		return fmt.Sprintf("$%d", len(args))
	}
	var clauses []string
	for _, r := range q.Rooms {
		clauses = append(clauses, fmt.Sprintf("(kind = %s AND conversation = %s AND seq > %s AND sent_at >= %s)",
			arg(string(r.Kind)), arg(r.Key), arg(r.After), arg(r.Since)))
	}
	if uuidPattern.MatchString(q.Direct) {
		d := arg(q.Direct)
		clauses = append(clauses, fmt.Sprintf("(kind = 'direct' AND (sender_id = %s::uuid OR recipient_id = %s::uuid))", d, d))
	}
	if len(clauses) == 0 || q.Limit <= 0 {
		return nil, nil
	}
	order := "ASC"
	if q.Newest {
		order = "DESC"
	}
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT `+messageColumns+` FROM chat.messages
		WHERE seq > $1 AND sent_at >= $2 AND (`+strings.Join(clauses, " OR ")+`)
		ORDER BY seq `+order+` LIMIT `+arg(q.Limit), args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []chat.Message
	for rows.Next() {
		m, err := scanMessage(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, m)
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	if q.Newest {
		slices.Reverse(out)
	}
	return out, nil
}

func (s *ChatStore) OpenPostMatch(ctx context.Context, accountID string) ([]chat.PostMatchMember, error) {
	if !uuidPattern.MatchString(accountID) {
		return nil, nil
	}
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT match_id::text, join_seq FROM chat.postmatch_members
		WHERE account_id = $1::uuid AND join_seq > 0 AND NOT left_chat ORDER BY join_seq`, accountID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []chat.PostMatchMember
	for rows.Next() {
		m := chat.PostMatchMember{AccountID: accountID}
		if err := rows.Scan(&m.MatchID, &m.JoinSeq); err != nil {
			return nil, err
		}
		out = append(out, m)
	}
	return out, rows.Err()
}

func (s *ChatStore) Mutes(ctx context.Context, matchID, muterID string) ([]string, error) {
	if !uuidPattern.MatchString(matchID) || !uuidPattern.MatchString(muterID) {
		return nil, nil
	}
	rows, err := querierFor(ctx, s.pool).Query(ctx, `SELECT muted_id::text FROM chat.postmatch_mutes
		WHERE match_id = $1::uuid AND muter_id = $2::uuid ORDER BY muted_id`, matchID, muterID)
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

func (s *ChatStore) LastSeq(ctx context.Context) (int64, error) {
	var seq int64
	err := querierFor(ctx, s.pool).QueryRow(ctx, `SELECT coalesce(max(seq), 0) FROM chat.messages`).Scan(&seq)
	return seq, err
}

type chatTx struct {
	ctx context.Context
	q   querier
}

func (t chatTx) LockSends() error {
	_, err := t.q.Exec(t.ctx, `SELECT pg_advisory_xact_lock(hashtextextended('chat:sends', 0))`)
	return err
}

func (t chatTx) ByClientID(senderID, clientID string) (chat.Message, error) {
	m, err := scanMessage(t.q.QueryRow(t.ctx, `SELECT `+messageColumns+` FROM chat.messages
		WHERE sender_id = $1::uuid AND client_id = $2`, senderID, clientID))
	if errors.Is(err, pgx.ErrNoRows) {
		return chat.Message{}, chat.ErrMessageNotFound
	}
	return m, err
}

func (t chatTx) CountSentSince(senderID string, since time.Time) (int, error) {
	var n int
	err := t.q.QueryRow(t.ctx, `SELECT count(*) FROM chat.messages WHERE sender_id = $1::uuid AND sent_at >= $2`, senderID, since).Scan(&n)
	return n, err
}

func (t chatTx) Add(m chat.Message) (chat.Message, error) {
	var recipient any
	if m.RecipientID != "" {
		recipient = m.RecipientID
	}
	err := t.q.QueryRow(t.ctx, `INSERT INTO chat.messages (kind, conversation, sender_id, sender_name, recipient_id, body, sent_at, client_id)
		VALUES ($1, $2, $3::uuid, $4, $5::uuid, $6, $7, $8) RETURNING seq`,
		string(m.Kind), m.Key, m.SenderID, m.SenderName, recipient, m.Text, m.SentAt, m.ClientID).Scan(&m.Seq)
	return m, err
}

func (t chatTx) Prune(before time.Time) error {
	_, err := t.q.Exec(t.ctx, `DELETE FROM chat.messages WHERE sent_at < $1`, before)
	return err
}

func (t chatTx) PostMatchMember(matchID, accountID string) (chat.PostMatchMember, error) {
	m := chat.PostMatchMember{MatchID: matchID, AccountID: accountID}
	err := t.q.QueryRow(t.ctx, `SELECT join_seq, left_chat FROM chat.postmatch_members WHERE match_id = $1::uuid AND account_id = $2::uuid`,
		matchID, accountID).Scan(&m.JoinSeq, &m.Left)
	if errors.Is(err, pgx.ErrNoRows) {
		return chat.PostMatchMember{}, chat.ErrNotJoined
	}
	return m, err
}

func (t chatTx) SavePostMatchMember(m chat.PostMatchMember) error {
	_, err := t.q.Exec(t.ctx, `INSERT INTO chat.postmatch_members (match_id, account_id, join_seq, left_chat) VALUES ($1::uuid, $2::uuid, $3, $4)
		ON CONFLICT (match_id, account_id) DO UPDATE SET join_seq = EXCLUDED.join_seq, left_chat = EXCLUDED.left_chat`,
		m.MatchID, m.AccountID, m.JoinSeq, m.Left)
	return err
}

func (t chatTx) SetMute(matchID, muterID, mutedID string, muted bool) error {
	if !muted {
		_, err := t.q.Exec(t.ctx, `DELETE FROM chat.postmatch_mutes WHERE match_id = $1::uuid AND muter_id = $2::uuid AND muted_id = $3::uuid`,
			matchID, muterID, mutedID)
		return err
	}
	_, err := t.q.Exec(t.ctx, `INSERT INTO chat.postmatch_mutes (match_id, muter_id, muted_id) VALUES ($1::uuid, $2::uuid, $3::uuid)
		ON CONFLICT DO NOTHING`, matchID, muterID, mutedID)
	return err
}
