package presence

import (
	"context"
	"slices"
	"testing"
	"time"
)

var settings = Settings{OfflineAfter: 30 * time.Second, TouchEvery: 5 * time.Second, PostMatchGrace: 2 * time.Minute}

// fakeActivity says who is busy, as the match, selection and party domains would.
type fakeActivity map[string]Status

func (f fakeActivity) Activity(_ context.Context, ids []string) (map[string]Status, error) {
	out := map[string]Status{}
	for _, id := range ids {
		if status, ok := f[id]; ok {
			out[id] = status
		}
	}
	return out, nil
}

// fakeParties holds parties as member lists, the sweepable ones apart.
type fakeParties struct {
	parties   [][]string
	sweepable map[string]bool
	removed   []string
}

func (f *fakeParties) PartyMembers(_ context.Context, id string) ([]string, error) {
	for _, members := range f.parties {
		if slices.Contains(members, id) {
			return members, nil
		}
	}
	return nil, nil
}

func (f *fakeParties) SweepableMembers(context.Context) ([]string, error) {
	var out []string
	for _, members := range f.parties {
		for _, id := range members {
			if f.sweepable[id] {
				out = append(out, id)
			}
		}
	}
	return out, nil
}

func (f *fakeParties) RemoveOffline(_ context.Context, id string) (bool, error) {
	for i, members := range f.parties {
		if j := slices.Index(members, id); j >= 0 {
			f.parties[i] = slices.Delete(members, j, j+1)
			f.removed = append(f.removed, id)
			return true, nil
		}
	}
	return false, nil
}

type fakeMatches map[string]time.Time

func (f fakeMatches) LastEnded(_ context.Context, ids []string) (map[string]time.Time, error) {
	out := map[string]time.Time{}
	for _, id := range ids {
		if at, ok := f[id]; ok {
			out[id] = at
		}
	}
	return out, nil
}

func TestASeenAccountIsOnlineUntilItGoesQuietAndBusyOnesSayWhy(t *testing.T) {
	ctx := context.Background()
	now := time.Date(2026, 10, 2, 12, 0, 0, 0, time.UTC)
	s := NewService(NewMemStore(), settings, func() time.Time { return now })
	s.SetActivity(fakeActivity{"m": InMatch, "s": InSelect, "q": InQueue})
	// A restarted backend gives everyone its offline threshold to come back.
	now = now.Add(settings.OfflineAfter)
	for _, id := range []string{"a", "m", "s", "q"} {
		if err := s.Touch(ctx, id); err != nil {
			t.Fatal(err)
		}
	}
	status, err := s.StatusFor(ctx, "viewer", []string{"a", "m", "s", "q", "never"})
	if err != nil {
		t.Fatal(err)
	}
	want := map[string]Status{"a": Online, "m": InMatch, "s": InSelect, "q": InQueue, "never": Offline}
	for id, w := range want {
		if status[id] != w {
			t.Errorf("%s shows %s, want %s", id, status[id], w)
		}
	}
	now = now.Add(settings.OfflineAfter)
	status, _ = s.StatusFor(ctx, "viewer", []string{"a", "m"})
	if status["a"] != Offline || status["m"] != InMatch {
		t.Fatalf("quiet: a %s (want offline), m %s (a match counts unseen)", status["a"], status["m"])
	}
}

func TestTouchesAreWrittenAtMostOnceEachInterval(t *testing.T) {
	ctx := context.Background()
	now := time.Date(2026, 10, 2, 12, 0, 0, 0, time.UTC)
	store := NewMemStore()
	s := NewService(store, settings, func() time.Time { return now })
	for range 10 {
		_ = s.Touch(ctx, "a")
		now = now.Add(time.Second)
	}
	// At 0 s and 5 s.
	if store.Touches() != 2 {
		t.Fatalf("%d writes in ten seconds, want 2", store.Touches())
	}
}

func TestAppearOfflineHidesFromFriendsOutsideThePartyOnly(t *testing.T) {
	ctx := context.Background()
	now := time.Date(2026, 10, 2, 12, 0, 0, 0, time.UTC)
	s := NewService(NewMemStore(), settings, func() time.Time { return now })
	s.SetParties(&fakeParties{parties: [][]string{{"hidden", "partner"}}})
	_ = s.Touch(ctx, "hidden")
	if err := s.SetAppearOffline(ctx, "hidden", true); err != nil {
		t.Fatal(err)
	}
	if offline, _ := s.ShowsOffline(ctx, "outsider", "hidden"); !offline {
		t.Error("an outside friend sees them online")
	}
	if offline, _ := s.ShowsOffline(ctx, "partner", "hidden"); offline {
		t.Error("their party sees them offline")
	}
	self, err := s.Self(ctx, "hidden")
	if err != nil || self.Status != Online || !self.AppearOffline {
		t.Fatalf("they see themselves online, appearing offline: %+v %v", self, err)
	}
	_ = s.SetAppearOffline(ctx, "hidden", false)
	if offline, _ := s.ShowsOffline(ctx, "outsider", "hidden"); offline {
		t.Error("turned off, still hidden")
	}
}

func TestTheSweepRemovesOfflineMembersButNotPlayersOrThoseJustBackFromAMatch(t *testing.T) {
	ctx := context.Background()
	start := time.Date(2026, 10, 2, 12, 0, 0, 0, time.UTC)
	now := start
	s := NewService(NewMemStore(), settings, func() time.Time { return now })
	parties := &fakeParties{
		parties:   [][]string{{"leader", "quiet", "playing", "back", "hidden"}, {"selecting"}},
		sweepable: map[string]bool{"leader": true, "quiet": true, "playing": true, "back": true, "hidden": true},
	}
	s.SetParties(parties)
	s.SetActivity(fakeActivity{"playing": InMatch, "quiet": InQueue})
	_ = s.SetAppearOffline(ctx, "hidden", true)
	now = start.Add(time.Minute)
	s.SetMatches(fakeMatches{"back": now.Add(-time.Minute)})
	_ = s.Touch(ctx, "leader")
	_ = s.Touch(ctx, "hidden")
	removed, err := s.Sweep(ctx)
	if err != nil {
		t.Fatal(err)
	}
	// "quiet" is unseen, and its queue does not keep it; "selecting"'s party is not sweepable;
	// "back" is in its grace; Appear Offline never removes.
	if removed != 1 || !slices.Equal(parties.removed, []string{"quiet"}) {
		t.Fatalf("removed %d: %v, want quiet alone", removed, parties.removed)
	}
	now = now.Add(settings.PostMatchGrace)
	_ = s.Touch(ctx, "leader")
	_ = s.Touch(ctx, "hidden")
	if _, err := s.Sweep(ctx); err != nil {
		t.Fatal(err)
	}
	if !slices.Equal(parties.removed, []string{"quiet", "back"}) {
		t.Fatalf("after the grace: %v, want back removed too", parties.removed)
	}
}
