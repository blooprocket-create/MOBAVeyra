package lobby

import (
	"context"
	"errors"
	"testing"
)

// launches records what a lobby handed champion select, or refuses with err.
type launches struct {
	got []Launch
	err error
}

func (l *launches) OpenCustom(_ context.Context, launch Launch) error {
	if l.err != nil {
		return l.err
	}
	l.got = append(l.got, launch)
	return nil
}

func TestTheHostLaunchesTheHumansAndBotsInSeatOrder(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	launcher := &launches{}
	f.lobbies.SetLauncher(launcher)
	f.befriend(t, "a", "b")
	_, _ = f.lobbies.Create(ctx, "a")
	inv, _ := f.lobbies.Invite(ctx, "a", "b")
	if _, err := f.lobbies.AcceptInvite(ctx, "b", inv.ID); err != nil {
		t.Fatal(err)
	}
	for _, bot := range []Bot{
		{Seat: seat(SideB, 3), VanguardID: "raska", Difficulty: Beginner},
		{Seat: seat(SideB, 1), VanguardID: "oriel", Difficulty: Intermediate},
	} {
		if _, err := f.lobbies.SetBot(ctx, "a", bot); err != nil {
			t.Fatal(err)
		}
	}
	_, err := f.lobbies.Launch(ctx, "b")
	mustErr(t, err, ErrNotHost)
	l, err := f.lobbies.Launch(ctx, "a")
	mustOK(t, err)
	if l.Status != Selecting || len(launcher.got) != 1 {
		t.Fatalf("launched: %+v %+v", l, launcher.got)
	}
	got := launcher.got[0]
	if got.HostID != "a" || len(got.Members) != 2 || got.Members[0].AccountID != "a" || got.Members[1].AccountID != "b" {
		t.Fatalf("the humans, side A first: %+v", got.Members)
	}
	if len(got.Bots) != 2 || got.Bots[0].VanguardID != "oriel" || got.Bots[1].VanguardID != "raska" {
		t.Fatalf("the bots in seat order, which sets their lanes: %+v", got.Bots)
	}
	if !got.Settings.VictoryEnabled {
		t.Fatal("both sides have Vanguards, so victory is on")
	}
	_, err = f.lobbies.SetBot(ctx, "a", Bot{Seat: seat(SideA, 2), VanguardID: "cairn", Difficulty: Beginner})
	mustErr(t, err, ErrLobbyLocked)
	mustErr(t, f.lobbies.Leave(ctx, "b"), ErrLobbyLocked)
}

func TestTheBusyCannotBeLaunched(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.lobbies.SetLauncher(&launches{})
	_, _ = f.lobbies.Create(ctx, "a")
	f.busy["a"] = true
	_, err := f.lobbies.Launch(ctx, "a")
	mustErr(t, err, ErrBusy)
	l, _ := f.lobbies.Get(ctx, "a")
	if l.Status != Open {
		t.Fatal("a refused launch leaves the lobby open")
	}
}

func TestARefusedSelectLeavesTheLobbyOpen(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	refusal := errors.New("no select")
	f.lobbies.SetLauncher(&launches{err: refusal})
	_, _ = f.lobbies.Create(ctx, "a")
	_, err := f.lobbies.Launch(ctx, "a")
	mustErr(t, err, refusal)
	l, _ := f.lobbies.Get(ctx, "a")
	if l.Status != Open {
		t.Fatal("the lobby stays open")
	}
}

func TestTheSelectsEndSettlesTheLobby(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	f.lobbies.SetLauncher(&launches{})
	l, _ := f.lobbies.Create(ctx, "a")
	if _, err := f.lobbies.Launch(ctx, "a"); err != nil {
		t.Fatal(err)
	}
	mustOK(t, f.lobbies.SelectEnded(ctx, l.ID, false))
	if got, _ := f.lobbies.Get(ctx, "a"); got.Status != Open {
		t.Fatal("a select without a match returns everyone to the lobby")
	}
	if _, err := f.lobbies.Launch(ctx, "a"); err != nil {
		t.Fatal(err)
	}
	mustOK(t, f.lobbies.SelectEnded(ctx, l.ID, true))
	_, err := f.lobbies.Get(ctx, "a")
	mustErr(t, err, ErrNotInLobby)
	mustOK(t, f.lobbies.SelectEnded(ctx, l.ID, true))
}

func TestWithoutChampionSelectNothingLaunches(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	_, _ = f.lobbies.Create(ctx, "a")
	_, err := f.lobbies.Launch(ctx, "a")
	mustErr(t, err, ErrLaunchUnavailable)
}
