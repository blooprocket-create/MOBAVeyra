package chat

import (
	"cmp"
	"context"
	"maps"
	"slices"
	"sync"
	"time"
)

// MemStore is an in-memory Store for tests. A transaction works on a copy
// that replaces the store's state only when its callback succeeds; one lock
// serializes transactions, which is what LockSends promises.
type MemStore struct {
	mu    sync.Mutex
	state memState
}

type memState struct {
	messages []Message
	lastSeq  int64
	members  map[[2]string]PostMatchMember
	// mutes holds match, muter and muted.
	mutes map[[3]string]bool
}

func (s memState) clone() memState {
	return memState{messages: slices.Clone(s.messages), lastSeq: s.lastSeq, members: maps.Clone(s.members), mutes: maps.Clone(s.mutes)}
}

// NewMemStore returns an empty MemStore.
func NewMemStore() *MemStore {
	return &MemStore{state: memState{members: map[[2]string]PostMatchMember{}, mutes: map[[3]string]bool{}}}
}

func (s *MemStore) InTx(ctx context.Context, fn func(context.Context, Tx) error) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	tx := &memTx{state: s.state.clone()}
	if err := fn(ctx, tx); err != nil {
		return err
	}
	s.state = tx.state
	return nil
}

func (s *MemStore) Messages(_ context.Context, q Query) ([]Message, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var out []Message
	for _, m := range s.state.messages {
		if m.Seq > q.After && !m.SentAt.Before(q.Since) && InQuery(q, m) {
			out = append(out, m)
		}
	}
	if q.Newest && len(out) > q.Limit {
		out = out[len(out)-q.Limit:]
	}
	if !q.Newest && len(out) > q.Limit {
		out = out[:q.Limit]
	}
	return out, nil
}

// InQuery reports whether a message belongs to one of a query's rooms or is
// a direct message of its account. Stores that filter in memory share it.
func InQuery(q Query, m Message) bool {
	if m.Kind == KindDirect {
		return q.Direct != "" && (m.SenderID == q.Direct || m.RecipientID == q.Direct)
	}
	for _, r := range q.Rooms {
		if r.Kind == m.Kind && r.Key == m.Key && m.Seq > r.After && !m.SentAt.Before(r.Since) {
			return true
		}
	}
	return false
}

func (s *MemStore) OpenPostMatch(_ context.Context, accountID string) ([]PostMatchMember, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var out []PostMatchMember
	for _, m := range s.state.members {
		if m.AccountID == accountID && m.JoinSeq > 0 && !m.Left {
			out = append(out, m)
		}
	}
	slices.SortFunc(out, func(a, b PostMatchMember) int { return cmp.Compare(a.JoinSeq, b.JoinSeq) })
	return out, nil
}

func (s *MemStore) Mutes(_ context.Context, matchID, muterID string) ([]string, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	var out []string
	for key, on := range s.state.mutes {
		if on && key[0] == matchID && key[1] == muterID {
			out = append(out, key[2])
		}
	}
	slices.Sort(out)
	return out, nil
}

func (s *MemStore) LastSeq(context.Context) (int64, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.state.lastSeq, nil
}

type memTx struct{ state memState }

func (t *memTx) LockSends() error { return nil }

func (t *memTx) ByClientID(senderID, clientID string) (Message, error) {
	for _, m := range t.state.messages {
		if m.SenderID == senderID && m.ClientID == clientID {
			return m, nil
		}
	}
	return Message{}, ErrMessageNotFound
}

func (t *memTx) CountSentSince(senderID string, since time.Time) (int, error) {
	n := 0
	for _, m := range t.state.messages {
		if m.SenderID == senderID && !m.SentAt.Before(since) {
			n++
		}
	}
	return n, nil
}

func (t *memTx) Add(m Message) (Message, error) {
	t.state.lastSeq++
	m.Seq = t.state.lastSeq
	t.state.messages = append(t.state.messages, m)
	return m, nil
}

func (t *memTx) Prune(before time.Time) error {
	t.state.messages = slices.DeleteFunc(t.state.messages, func(m Message) bool { return m.SentAt.Before(before) })
	return nil
}

func (t *memTx) PostMatchMember(matchID, accountID string) (PostMatchMember, error) {
	if m, ok := t.state.members[[2]string{matchID, accountID}]; ok {
		return m, nil
	}
	return PostMatchMember{}, ErrNotJoined
}

func (t *memTx) SavePostMatchMember(m PostMatchMember) error {
	t.state.members[[2]string{m.MatchID, m.AccountID}] = m
	return nil
}

func (t *memTx) SetMute(matchID, muterID, mutedID string, muted bool) error {
	key := [3]string{matchID, muterID, mutedID}
	if muted {
		t.state.mutes[key] = true
	} else {
		delete(t.state.mutes, key)
	}
	return nil
}
