package postgres

import (
	"context"
	"errors"
	"slices"
	"testing"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/favorites"
)

func TestFavoriteVanguardsInPostgres(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	account, err := store.EnsureDevAccount(ctx, "DevTwo")
	if err != nil {
		t.Fatal(err)
	}
	marks := store.Favorites()
	for _, id := range []string{"oriel", "cairn"} {
		_ = marks.Remove(ctx, account.ID, id)
	}
	if got, err := marks.Favorites(ctx, account.ID); err != nil || len(got) != 0 {
		t.Fatalf("none yet: %v %v", got, err)
	}
	if got, err := marks.Favorites(ctx, "not-a-uuid"); err != nil || len(got) != 0 {
		t.Fatalf("a malformed account: %v %v", got, err)
	}
	if err := marks.Add(ctx, account.ID, "oriel", 2); err != nil {
		t.Fatal(err)
	}
	if err := marks.Add(ctx, account.ID, "cairn", 2); err != nil {
		t.Fatal(err)
	}
	// Marking again is no error; a third past the most is refused.
	if err := marks.Add(ctx, account.ID, "oriel", 2); err != nil {
		t.Fatalf("twice: %v", err)
	}
	if err := marks.Add(ctx, account.ID, "bryn", 2); !errors.Is(err, favorites.ErrFull) {
		t.Fatalf("past the most: %v", err)
	}
	got, err := marks.Favorites(ctx, account.ID)
	if err != nil || len(got) != 2 || !slices.Contains(got, "oriel") || !slices.Contains(got, "cairn") {
		t.Fatalf("marked: %v %v", got, err)
	}
	if err := marks.Remove(ctx, account.ID, "oriel"); err != nil {
		t.Fatal(err)
	}
	if got, err := marks.Favorites(ctx, account.ID); err != nil || !slices.Equal(got, []string{"cairn"}) {
		t.Fatalf("unmarked: %v %v", got, err)
	}
	_ = marks.Remove(ctx, account.ID, "cairn")
}
