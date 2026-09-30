package lobby

import (
	"context"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

type clock struct{ t time.Time }

func (c *clock) now() time.Time { c.t = c.t.Add(time.Second); return c.t }

// busySet is an Activity where the listed accounts are busy.
type busySet map[string]bool

func (b busySet) Busy(_ context.Context, accounts []string) (bool, error) {
	for _, a := range accounts {
		if b[a] {
			return true, nil
		}
	}
	return false, nil
}

type fixture struct {
	lobbies *Service
	social  *social.Service
	busy    busySet
	clock   *clock
}

var accounts = []string{"a", "b", "c", "d"}

func newFixture(t *testing.T) fixture {
	t.Helper()
	soc := social.NewService(social.NewMemStore(accounts...))
	clk := &clock{t: t0}
	busy := busySet{}
	return fixture{lobbies: NewService(NewMemStore(), soc, busy, testLimits, time.Minute, clk.now), social: soc, busy: busy, clock: clk}
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

func TestAFriendIsInvitedAndSeated(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	if _, err := f.lobbies.Create(ctx, "a"); err != nil {
		t.Fatal(err)
	}
	_, err := f.lobbies.Invite(ctx, "a", "b")
	mustErr(t, err, ErrNotFriends)
	f.befriend(t, "a", "b")
	inv, err := f.lobbies.Invite(ctx, "a", "b")
	mustOK(t, err)
	invites, err := f.lobbies.Invites(ctx, "b")
	mustOK(t, err)
	if len(invites) != 1 || invites[0].ID != inv.ID {
		t.Fatalf("b's invites: %+v", invites)
	}
	l, err := f.lobbies.AcceptInvite(ctx, "b", inv.ID)
	mustOK(t, err)
	if m, _ := l.Occupant(seat(SideB, 0)); m == nil || m.AccountID != "b" {
		t.Fatalf("b sits on side B: %+v", l.Members)
	}
	_, err = f.lobbies.AcceptInvite(ctx, "b", inv.ID)
	mustErr(t, err, ErrInviteNotFound)
}

func TestOnlyTheHostInvites(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	f.befriend(t, "b", "c")
	_, _ = f.lobbies.Create(ctx, "a")
	inv, _ := f.lobbies.Invite(ctx, "a", "b")
	if _, err := f.lobbies.AcceptInvite(ctx, "b", inv.ID); err != nil {
		t.Fatal(err)
	}
	_, err := f.lobbies.Invite(ctx, "b", "c")
	mustErr(t, err, ErrNotHost)
}

func TestTheBusyCannotCreateOrJoin(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	f.busy["a"] = true
	_, err := f.lobbies.Create(ctx, "a")
	mustErr(t, err, ErrBusy)
	delete(f.busy, "a")
	_, err = f.lobbies.Create(ctx, "a")
	mustOK(t, err)
	_, err = f.lobbies.Create(ctx, "a")
	mustErr(t, err, ErrAlreadyInLobby)
	inv, err := f.lobbies.Invite(ctx, "a", "b")
	mustOK(t, err)
	f.busy["b"] = true
	_, err = f.lobbies.AcceptInvite(ctx, "b", inv.ID)
	mustErr(t, err, ErrBusy)
}

func TestAnInviteExpires(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	_, _ = f.lobbies.Create(ctx, "a")
	inv, err := f.lobbies.Invite(ctx, "a", "b")
	mustOK(t, err)
	f.clock.t = f.clock.t.Add(2 * time.Minute)
	_, err = f.lobbies.AcceptInvite(ctx, "b", inv.ID)
	mustErr(t, err, ErrInviteNotFound)
}

func TestABlockSeparatesTheTwo(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	f.befriend(t, "a", "c")
	_, _ = f.lobbies.Create(ctx, "a")
	inv, _ := f.lobbies.Invite(ctx, "a", "b")
	if _, err := f.lobbies.AcceptInvite(ctx, "b", inv.ID); err != nil {
		t.Fatal(err)
	}
	pending, err := f.lobbies.Invite(ctx, "a", "c")
	mustOK(t, err)
	mustOK(t, f.lobbies.OnBlock(ctx, "a", "b"))
	mustOK(t, f.lobbies.OnBlock(ctx, "c", "a"))
	l, err := f.lobbies.Get(ctx, "a")
	mustOK(t, err)
	if l.IsMember("b") {
		t.Fatal("the blocked member leaves")
	}
	_, err = f.lobbies.AcceptInvite(ctx, "c", pending.ID)
	mustErr(t, err, ErrInviteNotFound)
}

func TestLeavingHandsTheLobbyOnAndTheLastCloses(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.befriend(t, "a", "b")
	_, _ = f.lobbies.Create(ctx, "a")
	inv, _ := f.lobbies.Invite(ctx, "a", "b")
	if _, err := f.lobbies.AcceptInvite(ctx, "b", inv.ID); err != nil {
		t.Fatal(err)
	}
	mustOK(t, f.lobbies.Leave(ctx, "a"))
	l, err := f.lobbies.Get(ctx, "b")
	mustOK(t, err)
	if l.HostID != "b" {
		t.Fatalf("host: %q", l.HostID)
	}
	mustOK(t, f.lobbies.Leave(ctx, "b"))
	_, err = f.lobbies.Get(ctx, "b")
	mustErr(t, err, ErrNotInLobby)
}

func TestTheHostSetsBotsAndRules(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	_, _ = f.lobbies.Create(ctx, "a")
	l, err := f.lobbies.SetBot(ctx, "a", Bot{Seat: seat(SideB, 0), VanguardID: "oriel", Difficulty: Intermediate})
	mustOK(t, err)
	if !l.Settings.VictoryEnabled {
		t.Fatal("with an opponent, victory is on until the host chooses")
	}
	l, err = f.lobbies.SetSettings(ctx, "a", true, gold(3000))
	mustOK(t, err)
	if *l.Settings.StartingGold != 3000 {
		t.Fatalf("settings: %+v", l.Settings)
	}
	l, err = f.lobbies.RemoveBot(ctx, "a", seat(SideB, 0))
	mustOK(t, err)
	if l.Settings.VictoryEnabled || len(l.Bots) != 0 {
		t.Fatalf("after removing the only opponent: %+v", l)
	}
}
