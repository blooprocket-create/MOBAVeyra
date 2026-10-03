package postgres

import (
	"context"
	"testing"
	"time"
)

func TestPresenceInPostgres(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	account, err := store.EnsureDevAccount(ctx, "DevThree")
	if err != nil {
		t.Fatal(err)
	}
	presence := store.Presence()
	now := time.Now().UTC().Truncate(time.Second)
	if err := presence.Touch(ctx, account.ID, now); err != nil {
		t.Fatal(err)
	}
	// An older write never moves it back.
	if err := presence.Touch(ctx, account.ID, now.Add(-time.Minute)); err != nil {
		t.Fatal(err)
	}
	seen, err := presence.Seen(ctx, []string{account.ID, "not-a-uuid"})
	if err != nil || len(seen) != 1 || !seen[account.ID].Equal(now) {
		t.Fatalf("seen: %v %v", seen, err)
	}
	if err := presence.SetAppearOffline(ctx, account.ID, true); err != nil {
		t.Fatal(err)
	}
	hidden, err := presence.AppearingOffline(ctx, []string{account.ID})
	if err != nil || !hidden[account.ID] {
		t.Fatalf("appearing offline: %v %v", hidden, err)
	}
	// Seen again, it still appears offline: the setting lasts until changed.
	_ = presence.Touch(ctx, account.ID, now.Add(time.Minute))
	if hidden, _ := presence.AppearingOffline(ctx, []string{account.ID}); !hidden[account.ID] {
		t.Fatal("a touch turned Appear Offline off")
	}
	_ = presence.SetAppearOffline(ctx, account.ID, false)
	if hidden, _ := presence.AppearingOffline(ctx, []string{account.ID}); len(hidden) != 0 {
		t.Fatalf("turned off: %v", hidden)
	}
}
