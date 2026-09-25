package social

import (
	"context"
	"errors"
	"testing"
)

func TestFriendRequestLifecycle(t *testing.T) {
	ctx := context.Background()
	s := NewService(NewMemStore("a", "b", "c"))

	if out, err := s.SendFriendRequest(ctx, "a", "b"); err != nil || out != Requested {
		t.Fatalf("send: %v %v", out, err)
	}
	if in, _ := s.IncomingRequests(ctx, "b"); len(in) != 1 || in[0] != "a" {
		t.Fatalf("incoming: %v", in)
	}
	if err := s.AcceptFriendRequest(ctx, "b", "a"); err != nil {
		t.Fatal(err)
	}
	if ok, _ := s.AreFriends(ctx, "b", "a"); !ok {
		t.Fatal("should be friends")
	}
	if _, err := s.SendFriendRequest(ctx, "a", "b"); !errors.Is(err, ErrAlreadyFriends) {
		t.Fatalf("want ErrAlreadyFriends, got %v", err)
	}
	if err := s.RemoveFriend(ctx, "a", "b"); err != nil {
		t.Fatal(err)
	}
	if err := s.AcceptFriendRequest(ctx, "b", "a"); !errors.Is(err, ErrRequestNotFound) {
		t.Fatalf("want ErrRequestNotFound, got %v", err)
	}
}

func TestCrossedRequestsBecomeFriends(t *testing.T) {
	ctx := context.Background()
	s := NewService(NewMemStore("a", "b"))
	if _, err := s.SendFriendRequest(ctx, "a", "b"); err != nil {
		t.Fatal(err)
	}
	if out, err := s.SendFriendRequest(ctx, "b", "a"); err != nil || out != BecameFriends {
		t.Fatalf("want BecameFriends, got %v %v", out, err)
	}
	if out, _ := s.OutgoingRequests(ctx, "a"); len(out) != 0 {
		t.Fatalf("request should be consumed: %v", out)
	}
}

func TestBlockingEndsFriendshipAndStopsRequestsBothWays(t *testing.T) {
	ctx := context.Background()
	s := NewService(NewMemStore("a", "b"))
	if _, err := s.SendFriendRequest(ctx, "a", "b"); err != nil {
		t.Fatal(err)
	}
	if err := s.AcceptFriendRequest(ctx, "b", "a"); err != nil {
		t.Fatal(err)
	}
	if err := s.Block(ctx, "b", "a"); err != nil {
		t.Fatal(err)
	}
	if ok, _ := s.AreFriends(ctx, "a", "b"); ok {
		t.Fatal("block must end the friendship")
	}
	for _, dir := range [][2]string{{"a", "b"}, {"b", "a"}} {
		if _, err := s.SendFriendRequest(ctx, dir[0], dir[1]); !errors.Is(err, ErrBlocked) {
			t.Fatalf("%s->%s: want ErrBlocked, got %v", dir[0], dir[1], err)
		}
	}
	if blocked, _ := s.BlockedWithAny(ctx, "a", []string{"x", "b"}); !blocked {
		t.Fatal("BlockedWithAny must see the block from the other side")
	}
	// Unblocking only removes the actor's own block.
	if err := s.Unblock(ctx, "a", "b"); err != nil {
		t.Fatal(err)
	}
	if blocked, _ := s.BlockedWithAny(ctx, "a", []string{"b"}); !blocked {
		t.Fatal("b's block must survive a's unblock")
	}
	if err := s.Unblock(ctx, "b", "a"); err != nil {
		t.Fatal(err)
	}
	if _, err := s.SendFriendRequest(ctx, "a", "b"); err != nil {
		t.Fatalf("after unblock: %v", err)
	}
}

func TestRejectsSelfAndUnknownAccounts(t *testing.T) {
	ctx := context.Background()
	s := NewService(NewMemStore("a"))
	if _, err := s.SendFriendRequest(ctx, "a", "a"); !errors.Is(err, ErrSelf) {
		t.Fatalf("want ErrSelf, got %v", err)
	}
	if _, err := s.SendFriendRequest(ctx, "a", "ghost"); !errors.Is(err, ErrAccountNotFound) {
		t.Fatalf("want ErrAccountNotFound, got %v", err)
	}
	if err := s.Block(ctx, "a", "ghost"); !errors.Is(err, ErrAccountNotFound) {
		t.Fatalf("want ErrAccountNotFound, got %v", err)
	}
}
