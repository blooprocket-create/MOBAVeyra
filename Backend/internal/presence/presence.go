// Package presence keeps who is online (Parties, Social & Matchmaking Bible
// §4–§5; ADR-061): each account's last-seen time, touched by its signed-in
// requests, and its Appear Offline setting. It says how each account shows to
// another, and removes members who went offline from their pre-game party.
// What an account is doing is the match, selection and party domains' to say,
// and who may join or invite is party's; this domain only says who is here.
package presence

import (
	"context"
	"log/slog"
	"sync"
	"time"
)

// Status is how an account shows to a friend (ADR-061 §2).
type Status string

const (
	Offline  Status = "offline"
	Online   Status = "online"
	InQueue  Status = "in_queue"
	InSelect Status = "in_select"
	InMatch  Status = "in_match"
)

// Store persists last-seen times and Appear Offline.
type Store interface {
	// Touch records that the account was seen at at.
	Touch(ctx context.Context, accountID string, at time.Time) error
	// Seen returns when each of accounts was last seen; an account never seen
	// is absent.
	Seen(ctx context.Context, accountIDs []string) (map[string]time.Time, error)
	// AppearingOffline returns those of accounts that appear offline.
	AppearingOffline(ctx context.Context, accountIDs []string) (map[string]bool, error)
	// SetAppearOffline turns the account's Appear Offline on or off.
	SetAppearOffline(ctx context.Context, accountID string, on bool) error
}

// Activity says what accounts are busy with: a live match (Reconnect-only
// included), a champion select or a queue. The main package joins the match,
// selection and party domains to implement it.
type Activity interface {
	// Activity returns InMatch, InSelect or InQueue for each of accounts that
	// is busy so; the others are absent.
	Activity(ctx context.Context, accountIDs []string) (map[string]Status, error)
}

// Parties says who shares the viewer's party, whom Appear Offline does not
// hide from (Bible §5), and removes members who went offline.
type Parties interface {
	// PartyMembers returns the members of the account's party, nil when it
	// has none.
	PartyMembers(ctx context.Context, accountID string) ([]string, error)
	// SweepableMembers lists the members of Idle and Queued parties.
	SweepableMembers(ctx context.Context) ([]string, error)
	// RemoveOffline takes a member out of their party, as leaving does, unless
	// the party moved on meanwhile; it reports whether it removed them.
	RemoveOffline(ctx context.Context, accountID string) (bool, error)
}

// Matches says when each account's last match ended, for the grace a party
// gives a player returning from one (Bible §4).
type Matches interface {
	LastEnded(ctx context.Context, accountIDs []string) (map[string]time.Time, error)
}

// Settings are the validated presence settings (ADR-061 §7).
type Settings struct {
	// OfflineAfter: an account not seen for this long is offline.
	OfflineAfter time.Duration
	// TouchEvery: an account's last-seen time is written at most this often.
	TouchEvery time.Duration
	// PostMatchGrace: how long after a match ends an offline member keeps
	// their party place.
	PostMatchGrace time.Duration
}

// Self is the account's own presence, as it reads it.
type Self struct {
	Status        Status
	AppearOffline bool
}

// Service applies the presence rules.
type Service struct {
	store    Store
	settings Settings
	now      func() time.Time
	// started is a floor for every last-seen time: a restarted backend gives
	// everyone OfflineAfter to come back before it counts them offline.
	started  time.Time
	activity Activity
	parties  Parties
	matches  Matches

	mu      sync.Mutex
	touched map[string]time.Time
}

// NewService builds a Service. now is injectable for tests.
func NewService(store Store, settings Settings, now func() time.Time) *Service {
	return &Service{store: store, settings: settings, now: now, started: now(), touched: map[string]time.Time{}}
}

// SetActivity connects what says who is in a match, a select or a queue.
// Until it is set, a present account is Online and an absent one Offline.
func (s *Service) SetActivity(a Activity) { s.activity = a }

// SetParties connects the party domain, which sees through Appear Offline and
// whose offline members Sweep removes. Until it is set, Appear Offline hides
// from everyone and Sweep removes no one.
func (s *Service) SetParties(p Parties) { s.parties = p }

// SetMatches connects the match domain, whose ended matches give their
// players the post-match grace. Until it is set, there is no grace.
func (s *Service) SetMatches(m Matches) { s.matches = m }

// Touch marks the account seen now, writing at most once every TouchEvery
// from this instance (ADR-061 §1).
func (s *Service) Touch(ctx context.Context, accountID string) error {
	now := s.now()
	s.mu.Lock()
	if last, ok := s.touched[accountID]; ok && now.Sub(last) < s.settings.TouchEvery {
		s.mu.Unlock()
		return nil
	}
	s.touched[accountID] = now
	s.mu.Unlock()
	return s.store.Touch(ctx, accountID, now)
}

// present returns those of accounts seen within OfflineAfter, whatever their
// Appear Offline.
func (s *Service) present(ctx context.Context, accountIDs []string) (map[string]bool, error) {
	seen, err := s.store.Seen(ctx, accountIDs)
	if err != nil {
		return nil, err
	}
	now := s.now()
	out := make(map[string]bool, len(accountIDs))
	for _, id := range accountIDs {
		last := seen[id]
		if last.Before(s.started) {
			last = s.started
		}
		out[id] = now.Sub(last) < s.settings.OfflineAfter
	}
	return out, nil
}

// actual returns each of accounts' status, ignoring Appear Offline: a live
// match, a select or a queue counts whether or not the client is seen.
func (s *Service) actual(ctx context.Context, accountIDs []string) (map[string]Status, error) {
	present, err := s.present(ctx, accountIDs)
	if err != nil {
		return nil, err
	}
	busy := map[string]Status{}
	if s.activity != nil {
		if busy, err = s.activity.Activity(ctx, accountIDs); err != nil {
			return nil, err
		}
	}
	out := make(map[string]Status, len(accountIDs))
	for _, id := range accountIDs {
		switch status, ok := busy[id]; {
		case ok:
			out[id] = status
		case present[id]:
			out[id] = Online
		default:
			out[id] = Offline
		}
	}
	return out, nil
}

// StatusFor returns how each of accounts shows to viewer: its status, or
// Offline when it appears offline and is not in the viewer's party
// (ADR-061 §2–§3).
func (s *Service) StatusFor(ctx context.Context, viewer string, accountIDs []string) (map[string]Status, error) {
	out, err := s.actual(ctx, accountIDs)
	if err != nil {
		return nil, err
	}
	hidden, err := s.store.AppearingOffline(ctx, accountIDs)
	if err != nil || len(hidden) == 0 {
		return out, err
	}
	party := map[string]bool{}
	if s.parties != nil {
		members, err := s.parties.PartyMembers(ctx, viewer)
		if err != nil {
			return nil, err
		}
		for _, id := range members {
			party[id] = true
		}
	}
	for id := range hidden {
		if id != viewer && !party[id] {
			out[id] = Offline
		}
	}
	return out, nil
}

// ShowsOffline reports whether the account shows offline to viewer, whom no
// party invitation may then reach (ADR-061 §5).
func (s *Service) ShowsOffline(ctx context.Context, viewer, accountID string) (bool, error) {
	status, err := s.StatusFor(ctx, viewer, []string{accountID})
	return status[accountID] == Offline, err
}

// Self returns the account's own status and Appear Offline setting.
func (s *Service) Self(ctx context.Context, accountID string) (Self, error) {
	status, err := s.actual(ctx, []string{accountID})
	if err != nil {
		return Self{}, err
	}
	hidden, err := s.store.AppearingOffline(ctx, []string{accountID})
	if err != nil {
		return Self{}, err
	}
	return Self{Status: status[accountID], AppearOffline: hidden[accountID]}, nil
}

// SetAppearOffline turns the account's Appear Offline on or off. It never
// changes their party (Bible §5).
func (s *Service) SetAppearOffline(ctx context.Context, accountID string, on bool) error {
	return s.store.SetAppearOffline(ctx, accountID, on)
}

// Sweep removes from their party each member of an Idle or Queued party who
// is offline, plays no live match and is past the grace that follows their
// last match (ADR-061 §4). It returns how many it removed.
func (s *Service) Sweep(ctx context.Context) (int, error) {
	if s.parties == nil {
		return 0, nil
	}
	members, err := s.parties.SweepableMembers(ctx)
	if err != nil || len(members) == 0 {
		return 0, err
	}
	// Only being seen counts here: a queue shows a member busy whether or not
	// their client is there, and the queue is what their absence must cancel.
	present, err := s.present(ctx, members)
	if err != nil {
		return 0, err
	}
	busy := map[string]Status{}
	if s.activity != nil {
		if busy, err = s.activity.Activity(ctx, members); err != nil {
			return 0, err
		}
	}
	var gone []string
	for _, id := range members {
		if !present[id] && busy[id] != InMatch {
			gone = append(gone, id)
		}
	}
	if len(gone) == 0 {
		return 0, nil
	}
	ended := map[string]time.Time{}
	if s.matches != nil {
		if ended, err = s.matches.LastEnded(ctx, gone); err != nil {
			return 0, err
		}
	}
	now := s.now()
	removed := 0
	for _, id := range gone {
		if last, ok := ended[id]; ok && now.Sub(last) < s.settings.PostMatchGrace {
			continue
		}
		ok, err := s.parties.RemoveOffline(ctx, id)
		if err != nil {
			return removed, err
		}
		if ok {
			removed++
		}
	}
	s.forgetStale(now)
	return removed, nil
}

// RunSweeper sweeps every interval until ctx ends.
func (s *Service) RunSweeper(ctx context.Context, interval time.Duration, log *slog.Logger) {
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			removed, err := s.Sweep(ctx)
			if err != nil && ctx.Err() == nil {
				log.Error("presence sweep failed", "err", err)
			}
			if removed > 0 {
				log.Info("removed offline party members", "count", removed)
			}
		}
	}
}

// forgetStale drops the write throttle's entries that no longer throttle
// anything, so it does not grow with every account ever seen.
func (s *Service) forgetStale(now time.Time) {
	s.mu.Lock()
	defer s.mu.Unlock()
	for id, at := range s.touched {
		if now.Sub(at) >= s.settings.TouchEvery {
			delete(s.touched, id)
		}
	}
}
