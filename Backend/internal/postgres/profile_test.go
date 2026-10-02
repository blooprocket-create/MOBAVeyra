package postgres

import (
	"context"
	"errors"
	"testing"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/profile"
)

func TestProfileAppearancesInPostgres(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	account, err := store.EnsureDevAccount(ctx, "DevOne")
	if err != nil {
		t.Fatal(err)
	}
	profiles := store.Profile()
	if _, err := profiles.Appearance(ctx, account.ID); !errors.Is(err, profile.ErrNoAppearance) {
		t.Fatalf("an account that never chose: %v", err)
	}
	if _, err := profiles.Appearance(ctx, "not-a-uuid"); !errors.Is(err, profile.ErrNoAppearance) {
		t.Fatalf("a malformed account: %v", err)
	}
	chosen := profile.Appearance{Icon: "vanguard_cairn", Background: "default", FeaturedVanguard: "cairn", ShowMatchHistory: true}
	if err := profiles.SaveAppearance(ctx, account.ID, chosen); err != nil {
		t.Fatal(err)
	}
	if got, err := profiles.Appearance(ctx, account.ID); err != nil || got != chosen {
		t.Fatalf("saved: %+v %v", got, err)
	}
	// Saving again replaces the choice; no featured Vanguard is stored as none.
	cleared := profile.Appearance{Icon: "default", Background: "vanguard_oriel"}
	if err := profiles.SaveAppearance(ctx, account.ID, cleared); err != nil {
		t.Fatal(err)
	}
	if got, err := profiles.Appearance(ctx, account.ID); err != nil || got != cleared {
		t.Fatalf("replaced: %+v %v", got, err)
	}
	if err := profiles.DeleteAppearance(ctx, account.ID); err != nil {
		t.Fatal(err)
	}
	if _, err := profiles.Appearance(ctx, account.ID); !errors.Is(err, profile.ErrNoAppearance) {
		t.Fatalf("deleted: %v", err)
	}
}
