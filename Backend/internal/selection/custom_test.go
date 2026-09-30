package selection

import (
	"context"
	"errors"
	"testing"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// lobbyEnds records what the lobbies were told of each custom select's end.
type lobbyEnds struct{ calls []lobbyEnd }

type lobbyEnd struct {
	lobbyID string
	started bool
}

func (e *lobbyEnds) SelectEnded(_ context.Context, lobbyID string, started bool) error {
	e.calls = append(e.calls, lobbyEnd{lobbyID: lobbyID, started: started})
	return nil
}

// custom opens a custom select for acc-1 (the host) on side A and acc-2 on
// side B, each owning a starter, beside bots.
func (f *fixture) custom(t *testing.T, starter1, starter2 string, bots ...match.Bot) (Session, *lobbyEnds) {
	t.Helper()
	ends := &lobbyEnds{}
	f.svc.SetLobbies(ends)
	f.onboard(t, "acc-1", starter1)
	f.onboard(t, "acc-2", starter2)
	s, err := f.svc.OpenCustom(ctx, CustomLaunch{
		LobbyID:       "lobby-1",
		HostAccountID: "acc-1",
		Seats:         []CasualSeat{{AccountID: "acc-1", Side: match.SideA}, {AccountID: "acc-2", Side: match.SideB}},
		Bots:          bots,
		Settings:      match.CustomSettings{VictoryEnabled: true},
	})
	if err != nil {
		t.Fatalf("OpenCustom: %v", err)
	}
	return s, ends
}

func TestACustomSelectSeatsTheLobbysHumansBesideItsBots(t *testing.T) {
	f := newFixture(t)
	bot := match.Bot{Side: match.SideB, VanguardID: "bryn", Difficulty: match.BotIntermediate}
	s, _ := f.custom(t, "cairn", "qazharr", bot)
	if s.Kind != KindCustom || s.HostAccountID != "acc-1" || s.LobbyID != "lobby-1" || len(s.Seats) != 2 || len(s.Bots) != 1 || s.Bots[0] != bot {
		t.Fatalf("custom select: %+v", s)
	}
	if s.Custom == nil || !s.Custom.VictoryEnabled || s.Mode != fixtureCustomMode {
		t.Fatalf("custom rules: %+v", s)
	}
}

func TestACustomSelectNeedsEveryHumanFree(t *testing.T) {
	f := newFixture(t)
	f.onboard(t, "acc-1", "cairn")
	f.onboard(t, "acc-2", "qazharr")
	f.queued["acc-2"] = true
	_, err := f.svc.OpenCustom(ctx, CustomLaunch{LobbyID: "lobby-1", HostAccountID: "acc-1",
		Seats: []CasualSeat{{AccountID: "acc-1", Side: match.SideA}, {AccountID: "acc-2", Side: match.SideB}}})
	if !errors.Is(err, ErrBusy) {
		t.Fatalf("a queued player: want ErrBusy, got %v", err)
	}
	_, err = f.svc.OpenCustom(ctx, CustomLaunch{LobbyID: "lobby-1", HostAccountID: "acc-3", Seats: []CasualSeat{{AccountID: "acc-3", Side: match.SideA}}})
	if !errors.Is(err, ErrTutorialRequired) {
		t.Fatalf("before the tutorial: want ErrTutorialRequired, got %v", err)
	}
}

func TestInACustomSelectAVanguardIsTakenOnlyOnItsSideBotsIncluded(t *testing.T) {
	f := newFixture(t)
	f.custom(t, "cairn", "cairn", match.Bot{Side: match.SideA, VanguardID: "cairn", Difficulty: match.BotBeginner})
	if _, err := f.svc.Lock(ctx, "acc-1", "cairn"); !errors.Is(err, ErrTaken) {
		t.Fatalf("a bot on the side plays it: want ErrTaken, got %v", err)
	}
	if _, err := f.svc.Hover(ctx, "acc-2", "cairn"); err != nil {
		t.Fatalf("the other side may mirror it: %v", err)
	}
}

func TestACustomSelectStartsItsMatchWithTheLobbysBotsAndRules(t *testing.T) {
	f := newFixture(t)
	bots := []match.Bot{{Side: match.SideB, VanguardID: "bryn", Difficulty: match.BotIntermediate}}
	_, ends := f.custom(t, "cairn", "cairn", bots...)
	if _, err := f.svc.Lock(ctx, "acc-1", "cairn"); err != nil {
		t.Fatal(err)
	}
	s, err := f.svc.Lock(ctx, "acc-2", "cairn")
	if err != nil || s.State != Started {
		t.Fatalf("mirrors across sides start the match: %+v %v", s, err)
	}
	m, _, err := f.matches.BySelect(ctx, s.ID)
	if err != nil {
		t.Fatal(err)
	}
	if m.Rules != match.RulesCustom || m.HostAccountID != "acc-1" || len(m.Bots) != 1 || m.Bots[0] != bots[0] || m.Custom == nil || !m.Custom.VictoryEnabled {
		t.Fatalf("custom match: %+v", m)
	}
	if len(ends.calls) != 1 || ends.calls[0] != (lobbyEnd{lobbyID: "lobby-1", started: true}) {
		t.Fatalf("the lobby learns its match started: %+v", ends.calls)
	}
}

func TestLeavingACustomSelectReturnsEveryoneToTheLobby(t *testing.T) {
	f := newFixture(t)
	_, ends := f.custom(t, "cairn", "qazharr")
	s, err := f.svc.Leave(ctx, "acc-2")
	if err != nil || s.State != Cancelled || s.CancelReason != CancelLeft {
		t.Fatalf("leave: %+v %v", s, err)
	}
	if len(ends.calls) != 1 || ends.calls[0] != (lobbyEnd{lobbyID: "lobby-1", started: false}) {
		t.Fatalf("the lobby reopens: %+v", ends.calls)
	}
	if len(f.ends.calls) != 0 {
		t.Fatalf("matchmaking hears nothing of a custom select: %+v", f.ends.calls)
	}
}

func TestACustomSelectWhoseTimerLapsesReturnsToTheLobby(t *testing.T) {
	f := newFixture(t)
	_, ends := f.custom(t, "cairn", "qazharr")
	if _, err := f.svc.Lock(ctx, "acc-1", "cairn"); err != nil {
		t.Fatal(err)
	}
	f.now = f.now.Add(fixturePick)
	if err := f.svc.Tick(ctx); err != nil {
		t.Fatal(err)
	}
	if len(ends.calls) != 1 || ends.calls[0].started {
		t.Fatalf("the lobby reopens: %+v", ends.calls)
	}
}
