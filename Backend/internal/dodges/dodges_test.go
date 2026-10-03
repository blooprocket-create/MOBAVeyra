package dodges

import (
	"context"
	"testing"
	"time"
)

func TestADodgeRestrictsForTheConfiguredTimeAndAnotherStartsItAgain(t *testing.T) {
	ctx := context.Background()
	now := time.Date(2026, 10, 2, 12, 0, 0, 0, time.UTC)
	s := NewService(NewMemStore(), 5*time.Minute, func() time.Time { return now })
	if left, err := s.Remaining(ctx, []string{"a", "b"}); err != nil || len(left) != 0 {
		t.Fatalf("no one yet: %v %v", left, err)
	}
	if err := s.Dodged(ctx, "a"); err != nil {
		t.Fatal(err)
	}
	now = now.Add(2 * time.Minute)
	left, err := s.Remaining(ctx, []string{"a", "b"})
	if err != nil || len(left) != 1 || left["a"] != 3*time.Minute {
		t.Fatalf("a alone, three minutes left: %v %v", left, err)
	}
	// Again while restricted: the full length from now, no more.
	if err := s.Dodged(ctx, "a"); err != nil {
		t.Fatal(err)
	}
	if left, _ := s.Remaining(ctx, []string{"a"}); left["a"] != 5*time.Minute {
		t.Fatalf("started again: %v", left)
	}
	now = now.Add(5 * time.Minute)
	if left, _ := s.Remaining(ctx, []string{"a"}); len(left) != 0 {
		t.Fatalf("over: %v", left)
	}
}
