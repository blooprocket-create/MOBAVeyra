package party

import (
	"context"
	"errors"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

type clock struct{ t time.Time }

func (c *clock) now() time.Time { c.t = c.t.Add(time.Second); return c.t }

type fixture struct {
	parties *Service
	social  *social.Service
	clock   *clock
}

var accounts = []string{"a", "b", "c", "d", "e", "f", "g"}

func newFixture(t *testing.T) fixture {
	t.Helper()
	soc := social.NewService(social.NewMemStore(accounts...))
	clk := &clock{t: t0}
	svc := NewService(NewMemStore(), soc, Settings{
		Rules:          testRules,
		InviteLifetime: time.Minute,
		DefaultPrivacy: Private,
	}, clk.now)
	return fixture{parties: svc, social: soc, clock: clk}
}

func (f fixture) befriend(t *testing.T, a, b string) {
	t.Helper()
	ctx := context.Background()
	if _, err := f.social.SendFriendRequest(ctx, a, b); err != nil {
		t.Fatal(err)
	}
	if err := f.social.AcceptFriendRequest(ctx, b, a); err != nil {
		t.Fatal(err)
	}
}

func (f fixture) invite(t *testing.T, from, to string) Party {
	t.Helper()
	ctx := context.Background()
	inv, err := f.parties.Invite(ctx, from, to)
	if err != nil {
		t.Fatalf("invite %s->%s: %v", from, to, err)
	}
	p, err := f.parties.AcceptInvite(ctx, to, inv.ID)
	if err != nil {
		t.Fatalf("accept %s: %v", to, err)
	}
	return p
}

func TestInviteRequiresFriendshipAndCreatesModelessParty(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	if _, err := f.parties.Invite(ctx, "a", "b"); !errors.Is(err, ErrNotFriends) {
		t.Fatalf("want ErrNotFriends, got %v", err)
	}
	f.befriend(t, "a", "b")
	p := f.invite(t, "a", "b")
	if p.LeaderID != "a" || p.Mode != "" || len(p.Members) != 2 {
		t.Fatalf("unexpected party: %+v", p)
	}
}

func TestAnyMemberCanInvite(t *testing.T) {
	f := newFixture(t)
	f.befriend(t, "a", "b")
	f.befriend(t, "b", "c")
	f.invite(t, "a", "b")
	p := f.invite(t, "b", "c")
	if len(p.Members) != 3 || p.LeaderID != "a" {
		t.Fatalf("unexpected party: %+v", p)
	}
}

func TestBlocksPreventJoiningInBothDirections(t *testing.T) {
	for _, dir := range []struct{ blocker, blocked string }{{"a", "c"}, {"c", "a"}} {
		f := newFixture(t)
		ctx := context.Background()
		f.befriend(t, "a", "b")
		f.befriend(t, "b", "c")
		f.invite(t, "a", "b")
		inv, err := f.parties.Invite(ctx, "b", "c")
		if err != nil {
			t.Fatal(err)
		}
		if err := f.social.Block(ctx, dir.blocker, dir.blocked); err != nil {
			t.Fatal(err)
		}
		if _, err := f.parties.AcceptInvite(ctx, "c", inv.ID); !errors.Is(err, ErrBlocked) {
			t.Fatalf("%s blocks %s: want ErrBlocked, got %v", dir.blocker, dir.blocked, err)
		}
	}
}

func TestCapacityIsCheckedAtAcceptance(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	for _, id := range accounts[1:] {
		f.befriend(t, "a", id)
	}
	// Six invites can be sent to a party with room for four more.
	var invites []Invite
	for _, id := range accounts[1:] {
		inv, err := f.parties.Invite(ctx, "a", id)
		if err != nil {
			t.Fatal(err)
		}
		invites = append(invites, inv)
	}
	for i, inv := range invites {
		_, err := f.parties.AcceptInvite(ctx, inv.InviteeID, inv.ID)
		if i < 4 && err != nil {
			t.Fatalf("accept %d: %v", i, err)
		}
		if i >= 4 && !errors.Is(err, ErrPartyFull) {
			t.Fatalf("accept %d: want ErrPartyFull, got %v", i, err)
		}
	}
}

func TestQueueLockBlocksAcceptanceAndLeavingCancels(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	f.befriend(t, "a", "c")
	f.invite(t, "a", "b")
	pending, err := f.parties.Invite(ctx, "a", "c")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.SelectMode(ctx, "a", "casual"); err != nil {
		t.Fatal(err)
	}
	for _, id := range []string{"a", "b"} {
		if _, err := f.parties.SetReady(ctx, id, true); err != nil {
			t.Fatal(err)
		}
	}
	if _, err := f.parties.StartQueue(ctx, "a"); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.AcceptInvite(ctx, "c", pending.ID); !errors.Is(err, ErrPartyLocked) {
		t.Fatalf("accepting into a queued party: want ErrPartyLocked, got %v", err)
	}
	if err := f.parties.Leave(ctx, "b"); err != nil {
		t.Fatal(err)
	}
	p, err := f.parties.Get(ctx, "a")
	if err != nil {
		t.Fatal(err)
	}
	if p.Status != Idle || p.Members[0].Ready {
		t.Fatalf("leaving during queue must cancel it and reset Ready: %+v", p)
	}
	// The invitation survived the queue and can be used now.
	if _, err := f.parties.AcceptInvite(ctx, "c", pending.ID); err != nil {
		t.Fatalf("accept after queue ended: %v", err)
	}
}

func TestAcceptingMovesFromOldPartyUnlessItIsQueued(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	f.befriend(t, "c", "b")
	f.befriend(t, "c", "d")
	f.invite(t, "a", "b") // b is in a's party
	f.invite(t, "c", "d") // c leads another party

	inv, err := f.parties.Invite(ctx, "c", "b")
	if err != nil {
		t.Fatal(err)
	}
	p, err := f.parties.AcceptInvite(ctx, "b", inv.ID)
	if err != nil {
		t.Fatal(err)
	}
	if len(p.Members) != 3 {
		t.Fatalf("b should have joined c's party: %+v", p)
	}
	old, err := f.parties.Get(ctx, "a")
	if err != nil || len(old.Members) != 1 {
		t.Fatalf("b should have left a's party: %+v %v", old, err)
	}

	// A queued party cannot be left by accepting another invite.
	if _, err := f.parties.SelectMode(ctx, "c", "casual"); err != nil {
		t.Fatal(err)
	}
	for _, id := range []string{"b", "c", "d"} {
		if _, err := f.parties.SetReady(ctx, id, true); err != nil {
			t.Fatal(err)
		}
	}
	if _, err := f.parties.StartQueue(ctx, "c"); err != nil {
		t.Fatal(err)
	}
	back, err := f.parties.Invite(ctx, "a", "b")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.AcceptInvite(ctx, "b", back.ID); !errors.Is(err, ErrPartyLocked) {
		t.Fatalf("want ErrPartyLocked, got %v", err)
	}
}

func TestInvitesExpire(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	inv, err := f.parties.Invite(ctx, "a", "b")
	if err != nil {
		t.Fatal(err)
	}
	f.clock.t = f.clock.t.Add(time.Minute)
	if _, err := f.parties.AcceptInvite(ctx, "b", inv.ID); !errors.Is(err, ErrInviteNotFound) {
		t.Fatalf("want ErrInviteNotFound, got %v", err)
	}
	if list, _ := f.parties.Invites(ctx, "b"); len(list) != 0 {
		t.Fatalf("expired invites must not be listed: %+v", list)
	}
}

func TestOnlyInviteeCanUseAnInvite(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	inv, err := f.parties.Invite(ctx, "a", "b")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.AcceptInvite(ctx, "c", inv.ID); !errors.Is(err, ErrInviteNotFound) {
		t.Fatalf("want ErrInviteNotFound, got %v", err)
	}
	if err := f.parties.DeclineInvite(ctx, "b", inv.ID); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.AcceptInvite(ctx, "b", inv.ID); !errors.Is(err, ErrInviteNotFound) {
		t.Fatalf("declined invite: want ErrInviteNotFound, got %v", err)
	}
}

func TestPublicJoin(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	f.befriend(t, "b", "c")
	f.invite(t, "a", "b")
	p, _ := f.parties.Get(ctx, "a")

	if _, err := f.parties.JoinPublic(ctx, "c", p.ID); !errors.Is(err, ErrPartyNotJoinable) {
		t.Fatalf("private party: want ErrPartyNotJoinable, got %v", err)
	}
	if _, err := f.parties.SetPrivacy(ctx, "a", Public); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.JoinPublic(ctx, "d", p.ID); !errors.Is(err, ErrPartyNotJoinable) {
		t.Fatalf("stranger: want ErrPartyNotJoinable, got %v", err)
	}
	// c is b's friend, not the leader's.
	if _, err := f.parties.JoinPublic(ctx, "c", p.ID); err != nil {
		t.Fatalf("friend of a member: %v", err)
	}
}

func TestBlockRemovesBlockedMemberAndInvites(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	f.befriend(t, "a", "c")
	f.invite(t, "a", "b")
	inv, err := f.parties.Invite(ctx, "a", "c")
	if err != nil {
		t.Fatal(err)
	}

	for _, blocked := range []string{"b", "c"} {
		if err := f.social.Block(ctx, "a", blocked); err != nil {
			t.Fatal(err)
		}
		if err := f.parties.OnBlock(ctx, "a", blocked); err != nil {
			t.Fatal(err)
		}
	}
	p, err := f.parties.Get(ctx, "a")
	if err != nil || len(p.Members) != 1 {
		t.Fatalf("blocked member must leave the party: %+v %v", p, err)
	}
	if _, err := f.parties.AcceptInvite(ctx, "c", inv.ID); !errors.Is(err, ErrInviteNotFound) {
		t.Fatalf("invite between blocked accounts must be withdrawn: %v", err)
	}
}

func TestLastMemberLeavingDeletesParty(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	if _, err := f.parties.SelectMode(ctx, "a", "casual"); err != nil {
		t.Fatal(err)
	}
	if err := f.parties.Leave(ctx, "a"); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.Get(ctx, "a"); !errors.Is(err, ErrNotInParty) {
		t.Fatalf("want ErrNotInParty, got %v", err)
	}
}

func TestQueuedPartyCannotInvite(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	if _, err := f.parties.SelectMode(ctx, "a", "casual"); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.SetReady(ctx, "a", true); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.StartQueue(ctx, "a"); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.Invite(ctx, "a", "b"); !errors.Is(err, ErrPartyLocked) {
		t.Fatalf("want ErrPartyLocked, got %v", err)
	}
}

// A blocks B while C, A's party-mate, has invited B: that invite would put A
// and B in one party, so it must go too.
func TestBlockWithdrawsPartyMatesInvites(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "c")
	f.befriend(t, "c", "b")
	f.invite(t, "a", "c")
	inv, err := f.parties.Invite(ctx, "c", "b")
	if err != nil {
		t.Fatal(err)
	}
	if err := f.social.Block(ctx, "a", "b"); err != nil {
		t.Fatal(err)
	}
	if err := f.parties.OnBlock(ctx, "a", "b"); err != nil {
		t.Fatal(err)
	}
	if list, _ := f.parties.Invites(ctx, "b"); len(list) != 0 {
		t.Fatalf("invite into the blocker's party must be withdrawn: %+v", list)
	}
	if _, err := f.parties.AcceptInvite(ctx, "b", inv.ID); !errors.Is(err, ErrInviteNotFound) {
		t.Fatalf("want ErrInviteNotFound, got %v", err)
	}
}
