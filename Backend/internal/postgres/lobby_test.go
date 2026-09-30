package postgres

import (
	"context"
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/lobby"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

var testLobbyLimits = lobby.Limits{
	PlayersPerSide:  5,
	Released:        map[string]bool{"cairn": true, "oriel": true},
	StartingGoldMin: 0,
	StartingGoldMax: 20000,
}

type lobbyFixture struct {
	social  *social.Service
	lobbies *lobby.Service
	ids     map[string]string
}

func newLobbyFixture(t *testing.T, limits lobby.Limits, names ...string) lobbyFixture {
	t.Helper()
	store := openTestStore(t)
	ctx := context.Background()
	ids := map[string]string{}
	for _, n := range names {
		a, err := store.EnsureDevAccount(ctx, n)
		if err != nil {
			t.Fatal(err)
		}
		ids[n] = a.ID
	}
	soc := social.NewService(store.Social())
	return lobbyFixture{social: soc, lobbies: lobby.NewService(store.Lobby(), soc, nil, limits, time.Minute, time.Now), ids: ids}
}

func (f lobbyFixture) befriend(t *testing.T, a, b string) {
	t.Helper()
	ctx := context.Background()
	if _, err := f.social.SendFriendRequest(ctx, f.ids[a], f.ids[b]); err != nil {
		t.Fatal(err)
	}
	if err := f.social.AcceptFriendRequest(ctx, f.ids[b], f.ids[a]); err != nil {
		t.Fatal(err)
	}
}

func TestLobbyLifecycleInPostgres(t *testing.T) {
	f := newLobbyFixture(t, testLobbyLimits, "A", "B")
	ctx := context.Background()
	a, b := f.ids["A"], f.ids["B"]
	f.befriend(t, "A", "B")
	if _, err := f.lobbies.Create(ctx, a); err != nil {
		t.Fatal(err)
	}
	inv, err := f.lobbies.Invite(ctx, a, b)
	if err != nil {
		t.Fatal(err)
	}
	if list, _ := f.lobbies.Invites(ctx, b); len(list) != 1 || list[0].ID != inv.ID {
		t.Fatalf("B's invites: %+v", list)
	}
	if _, err := f.lobbies.AcceptInvite(ctx, b, inv.ID); err != nil {
		t.Fatal(err)
	}
	bot := lobby.Bot{Seat: lobby.Seat{Side: lobby.SideB, Index: 1}, VanguardID: "oriel", Difficulty: lobby.Intermediate}
	if _, err := f.lobbies.SetBot(ctx, a, bot); err != nil {
		t.Fatal(err)
	}
	gold := 2500.0
	if _, err := f.lobbies.SetSettings(ctx, a, false, &gold); err != nil {
		t.Fatal(err)
	}

	l, err := f.lobbies.Get(ctx, b)
	if err != nil {
		t.Fatal(err)
	}
	if l.HostID != a || l.Status != lobby.Open || len(l.Members) != 2 || len(l.Bots) != 1 || l.Bots[0] != bot {
		t.Fatalf("stored lobby: %+v", l)
	}
	if l.Settings.VictoryEnabled || !l.Settings.VictoryChosen || l.Settings.StartingGold == nil || *l.Settings.StartingGold != gold {
		t.Fatalf("stored settings: %+v", l.Settings)
	}
	if m, _ := l.Occupant(lobby.Seat{Side: lobby.SideB, Index: 0}); m == nil || m.AccountID != b {
		t.Fatalf("B's seat: %+v", l.Members)
	}

	if err := f.lobbies.Leave(ctx, a); err != nil {
		t.Fatal(err)
	}
	if l, err = f.lobbies.Get(ctx, b); err != nil || l.HostID != b {
		t.Fatalf("host after A leaves: %+v %v", l, err)
	}
	if err := f.lobbies.Leave(ctx, b); err != nil {
		t.Fatal(err)
	}
	if _, err := f.lobbies.Get(ctx, b); !errors.Is(err, lobby.ErrNotInLobby) {
		t.Fatalf("the last one out must close the lobby: %v", err)
	}
}

// Two friends racing for the last seat: exactly one gets it.
func TestConcurrentAcceptsRespectLobbySeats(t *testing.T) {
	oneSeat := testLobbyLimits
	oneSeat.PlayersPerSide = 1
	f := newLobbyFixture(t, oneSeat, "A", "B", "C")
	ctx := context.Background()
	a := f.ids["A"]
	f.befriend(t, "A", "B")
	f.befriend(t, "A", "C")
	if _, err := f.lobbies.Create(ctx, a); err != nil {
		t.Fatal(err)
	}
	invites := map[string]string{}
	for _, n := range []string{"B", "C"} {
		inv, err := f.lobbies.Invite(ctx, a, f.ids[n])
		if err != nil {
			t.Fatal(err)
		}
		invites[n] = inv.ID
	}
	var wg sync.WaitGroup
	errs := make(chan error, 2)
	for _, n := range []string{"B", "C"} {
		wg.Add(1)
		go func(name string) {
			defer wg.Done()
			_, err := f.lobbies.AcceptInvite(ctx, f.ids[name], invites[name])
			errs <- err
		}(n)
	}
	wg.Wait()
	close(errs)
	var joined, full int
	for err := range errs {
		switch {
		case err == nil:
			joined++
		case errors.Is(err, lobby.ErrLobbyFull):
			full++
		default:
			t.Fatalf("accept: %v", err)
		}
	}
	if joined != 1 || full != 1 {
		t.Fatalf("joined %d, refused %d", joined, full)
	}
}
