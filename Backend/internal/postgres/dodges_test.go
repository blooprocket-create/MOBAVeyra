package postgres

import (
	"context"
	"testing"
	"time"
)

func TestQueueRestrictionsInPostgres(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	account, err := store.EnsureDevAccount(ctx, "DevTwo")
	if err != nil {
		t.Fatal(err)
	}
	dodges := store.Dodges()
	now := time.Now().UTC().Truncate(time.Second)
	if err := dodges.Restrict(ctx, account.ID, now.Add(5*time.Minute)); err != nil {
		t.Fatal(err)
	}
	ends, err := dodges.Ends(ctx, []string{account.ID, "not-a-uuid"}, now)
	if err != nil || len(ends) != 1 || !ends[account.ID].Equal(now.Add(5*time.Minute)) {
		t.Fatalf("restricted: %v %v", ends, err)
	}
	// Starting again replaces the end.
	if err := dodges.Restrict(ctx, account.ID, now.Add(time.Minute)); err != nil {
		t.Fatal(err)
	}
	if ends, _ := dodges.Ends(ctx, []string{account.ID}, now); !ends[account.ID].Equal(now.Add(time.Minute)) {
		t.Fatalf("replaced: %v", ends)
	}
	// Past its end, it no longer restricts.
	if ends, _ := dodges.Ends(ctx, []string{account.ID}, now.Add(2*time.Minute)); len(ends) != 0 {
		t.Fatalf("over: %v", ends)
	}
	_ = dodges.Restrict(ctx, account.ID, now.Add(-time.Hour))
}
