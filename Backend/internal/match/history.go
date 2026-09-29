package match

import (
	"context"
	"encoding/base64"
	"errors"
	"fmt"
	"regexp"
	"strconv"
	"strings"
	"time"
)

// Match History (Pre-Game Client UX Bible 51, 64, 67; ADR-017 §6): a player's
// completed matches, newest first, filtered by Vanguard, mode and personal
// outcome, in pages that continue from a cursor.

var (
	ErrInvalidFilter = errors.New("invalid match history filter")
	ErrInvalidCursor = errors.New("invalid match history cursor")
)

// Outcome is how a completed match ended for one player.
type Outcome string

const (
	OutcomeWin  Outcome = "win"
	OutcomeLoss Outcome = "loss"
	// OutcomeNoContest is a match no side won: ended by its host, a
	// developer or abandonment.
	OutcomeNoContest Outcome = "no_contest"
)

// Valid reports whether o is an outcome history knows.
func (o Outcome) Valid() bool {
	return o == OutcomeWin || o == OutcomeLoss || o == OutcomeNoContest
}

// OutcomeFor is the personal outcome of a player on side for a match won by
// winner, or by no one when winner is empty. There is no personal loss
// override yet (UX-51): a player's outcome is their team's.
func OutcomeFor(side, winner Side) Outcome {
	switch {
	case winner == "":
		return OutcomeNoContest
	case winner == side:
		return OutcomeWin
	default:
		return OutcomeLoss
	}
}

// HistoryFilter narrows a player's history. Each empty field matches every
// match; the filters combine.
type HistoryFilter struct {
	VanguardID string
	Mode       string
	Outcome    Outcome
}

// Validate reports ErrInvalidFilter unless each set field has its format.
func (f HistoryFilter) Validate() error {
	if (f.VanguardID != "" && !IsContentID(f.VanguardID)) || (f.Mode != "" && !IsContentID(f.Mode)) || (f.Outcome != "" && !f.Outcome.Valid()) {
		return ErrInvalidFilter
	}
	return nil
}

// matchIDPattern is the format newID gives a match's ID.
var matchIDPattern = regexp.MustCompile(`^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$`)

// HistoryCursor is where a page of history ends: the next page holds matches
// that ended before it, or at the same moment with a smaller ID.
type HistoryCursor struct {
	EndedAt time.Time
	MatchID string
}

// Encode is the cursor as the client passes it back: opaque to the client.
func (c HistoryCursor) Encode() string {
	return base64.RawURLEncoding.EncodeToString([]byte(fmt.Sprintf("%d.%s", c.EndedAt.UnixMicro(), c.MatchID)))
}

// DecodeHistoryCursor reads a cursor Encode made.
func DecodeHistoryCursor(s string) (HistoryCursor, error) {
	raw, err := base64.RawURLEncoding.DecodeString(s)
	if err != nil {
		return HistoryCursor{}, ErrInvalidCursor
	}
	micros, id, ok := strings.Cut(string(raw), ".")
	at, err := strconv.ParseInt(micros, 10, 64)
	if !ok || err != nil || !matchIDPattern.MatchString(id) {
		return HistoryCursor{}, ErrInvalidCursor
	}
	return HistoryCursor{EndedAt: time.UnixMicro(at).UTC(), MatchID: id}, nil
}

// HistoryEntry is one completed match as a player's history lists it.
type HistoryEntry struct {
	MatchID         string
	Mode            string
	Rules           Rules
	EndedAt         time.Time
	DurationSeconds float64
	Side            Side
	VanguardID      string
	Outcome         Outcome
}

// History lists accountID's completed matches newest first: a page of the
// configured size after cursor (empty for the first page), and the cursor of
// the next page, empty when there is none. The filters apply across every
// record, not a loaded subset (UX-67).
func (s *Service) History(ctx context.Context, accountID string, filter HistoryFilter, cursor string) ([]HistoryEntry, string, error) {
	if err := filter.Validate(); err != nil {
		return nil, "", err
	}
	var after *HistoryCursor
	if cursor != "" {
		c, err := DecodeHistoryCursor(cursor)
		if err != nil {
			return nil, "", err
		}
		after = &c
	}
	size := s.settings.HistoryPageSize
	// One more than a page says whether another follows.
	entries, err := s.store.MatchHistory(ctx, accountID, filter, after, size+1)
	if err != nil {
		return nil, "", err
	}
	if len(entries) <= size {
		return entries, "", nil
	}
	entries = entries[:size]
	last := entries[len(entries)-1]
	return entries, HistoryCursor{EndedAt: last.EndedAt, MatchID: last.MatchID}.Encode(), nil
}

// HistoryModes lists every mode accountID has a completed match in, sorted:
// the history's mode filter offers each, whichever pages are loaded (UX-67).
func (s *Service) HistoryModes(ctx context.Context, accountID string) ([]string, error) {
	return s.store.HistoryModes(ctx, accountID)
}

// historyEntryFor is m as accountID's history lists it, if it belongs there:
// completed with a result, with the account on its roster, and matching filter.
func historyEntryFor(m Match, accountID string, filter HistoryFilter) (HistoryEntry, bool) {
	p, ok := m.Participant(accountID)
	if !ok || m.State != Ended || m.Result == nil {
		return HistoryEntry{}, false
	}
	e := HistoryEntry{MatchID: m.ID, Mode: m.Mode, Rules: m.Rules, EndedAt: m.EndedAt, DurationSeconds: m.Result.DurationSeconds, Side: p.Side,
		VanguardID: p.VanguardID, Outcome: OutcomeFor(p.Side, m.Result.Winner)}
	if (filter.VanguardID != "" && e.VanguardID != filter.VanguardID) || (filter.Mode != "" && e.Mode != filter.Mode) || (filter.Outcome != "" && e.Outcome != filter.Outcome) {
		return HistoryEntry{}, false
	}
	return e, true
}

// before reports whether e comes after cursor in newest-first order.
func (c HistoryCursor) before(e HistoryEntry) bool {
	return e.EndedAt.Before(c.EndedAt) || (e.EndedAt.Equal(c.EndedAt) && e.MatchID < c.MatchID)
}
