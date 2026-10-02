package favorites

import (
	"context"
	"errors"
	"slices"
	"testing"
)

type released []string

func (r released) Released() []string { return r }

type playing map[string]bool

func (s playing) Playing(_ context.Context, accountID string) (bool, error) {
	return s[accountID], nil
}

func newTestService(most int, busy playing) (*Service, *MemStore) {
	store := NewMemStore()
	return NewService(store, released{"cairn", "oriel", "bryn"}, busy, most), store
}

func TestFavoritesAreMarkedAndUnmarkedInOrder(t *testing.T) {
	ctx := context.Background()
	s, _ := newTestService(3, playing{})
	if got, err := s.Set(ctx, "a1", "oriel", true); err != nil || !slices.Equal(got, []string{"oriel"}) {
		t.Fatalf("Set oriel = %v, %v", got, err)
	}
	if got, err := s.Set(ctx, "a1", "cairn", true); err != nil || !slices.Equal(got, []string{"oriel", "cairn"}) {
		t.Fatalf("Set cairn = %v, %v", got, err)
	}
	// Marking twice keeps one; unmarking one never marked is no error.
	if got, _ := s.Set(ctx, "a1", "cairn", true); !slices.Equal(got, []string{"oriel", "cairn"}) {
		t.Fatalf("twice = %v", got)
	}
	if got, err := s.Set(ctx, "a1", "bryn", false); err != nil || !slices.Equal(got, []string{"oriel", "cairn"}) {
		t.Fatalf("unmark unmarked = %v, %v", got, err)
	}
	if got, _ := s.Set(ctx, "a1", "oriel", false); !slices.Equal(got, []string{"cairn"}) {
		t.Fatalf("unmark = %v", got)
	}
	// Another account's are its own.
	if got, _ := s.Favorites(ctx, "a2"); len(got) != 0 {
		t.Fatalf("a2 = %v", got)
	}
}

func TestOnlyReleasedVanguardsOutsideChampionSelectUpToTheMost(t *testing.T) {
	ctx := context.Background()
	s, store := newTestService(1, playing{"busy": true})
	if _, err := s.Set(ctx, "a1", "no_such_vanguard", true); !errors.Is(err, ErrUnknownVanguard) {
		t.Fatalf("unknown = %v", err)
	}
	if _, err := s.Set(ctx, "busy", "cairn", true); !errors.Is(err, ErrPlaying) {
		t.Fatalf("in select = %v", err)
	}
	if _, err := s.Set(ctx, "a1", "cairn", true); err != nil {
		t.Fatal(err)
	}
	if _, err := s.Set(ctx, "a1", "oriel", true); !errors.Is(err, ErrFull) {
		t.Fatalf("past the most = %v", err)
	}
	// A favorite no longer released drops out of what shows.
	_ = store.Add(ctx, "a3", "retired", 5)
	_ = store.Add(ctx, "a3", "bryn", 5)
	if got, _ := s.Favorites(ctx, "a3"); !slices.Equal(got, []string{"bryn"}) {
		t.Fatalf("released only = %v", got)
	}
}
