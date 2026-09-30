package lobby

import (
	"errors"
	"testing"
	"time"
)

var testLimits = Limits{
	PlayersPerSide:  5,
	Released:        map[string]bool{"cairn": true, "oriel": true, "bryn": true, "raska": true},
	StartingGoldMin: 0,
	StartingGoldMax: 20000,
}

var t0 = time.Date(2026, 9, 29, 12, 0, 0, 0, time.UTC)

func mustErr(t *testing.T, err, want error) {
	t.Helper()
	if !errors.Is(err, want) {
		t.Fatalf("want %v, got %v", want, err)
	}
}

func mustOK(t *testing.T, err error) {
	t.Helper()
	if err != nil {
		t.Fatal(err)
	}
}

func seat(side string, i int) Seat { return Seat{Side: side, Index: i} }

func gold(g float64) *float64 { return &g }

func TestANewLobbySeatsItsHostAndPlaysOpenEnded(t *testing.T) {
	l := New("l", "host", t0)
	if l.HostID != "host" || l.Status != Open || len(l.Members) != 1 || l.Members[0].Seat != seat(SideA, 0) {
		t.Fatalf("new lobby: %+v", l)
	}
	if l.Settings.VictoryEnabled {
		t.Fatal("a lobby with one side empty is open-ended practice (§1)")
	}
}

func TestJoinSeatsTheSideWithFewerHumans(t *testing.T) {
	l := New("l", "host", t0)
	mustOK(t, l.Join("b", testLimits, t0))
	if m, _ := l.Occupant(seat(SideB, 0)); m == nil || m.AccountID != "b" {
		t.Fatalf("the second human joins side B: %+v", l.Members)
	}
	mustOK(t, l.Join("c", testLimits, t0))
	if m, _ := l.Occupant(seat(SideA, 1)); m == nil || m.AccountID != "c" {
		t.Fatalf("a tie seats side A: %+v", l.Members)
	}
	mustErr(t, l.Join("c", testLimits, t0), ErrAlreadyInLobby)
}

func TestJoinFillsTheOtherSideAndThenRefuses(t *testing.T) {
	small := testLimits
	small.PlayersPerSide = 1
	l := New("l", "host", t0)
	mustOK(t, l.SetBot("host", Bot{Seat: seat(SideB, 0), VanguardID: "cairn", Difficulty: Beginner}, small))
	mustErr(t, l.Join("b", small, t0), ErrLobbyFull)
	mustOK(t, l.RemoveBot("host", seat(SideB, 0)))
	mustOK(t, l.Join("b", small, t0))
}

func TestTheHostPassesToTheLongestPresentAndTheLastOneOutCloses(t *testing.T) {
	l := New("l", "host", t0)
	mustOK(t, l.Join("b", testLimits, t0.Add(time.Minute)))
	mustOK(t, l.Join("c", testLimits, t0.Add(2*time.Minute)))
	empty, err := l.Remove("host")
	mustOK(t, err)
	if empty || l.HostID != "b" {
		t.Fatalf("host after the host leaves: %q", l.HostID)
	}
	mustOK(t, l.Kick("b", "c"))
	empty, err = l.Remove("b")
	mustOK(t, err)
	if !empty {
		t.Fatal("the last human out closes the lobby")
	}
}

func TestOnlyTheHostArrangesTheLobby(t *testing.T) {
	l := New("l", "host", t0)
	mustOK(t, l.Join("b", testLimits, t0))
	bot := Bot{Seat: seat(SideA, 1), VanguardID: "oriel", Difficulty: Intermediate}
	mustErr(t, l.SetBot("b", bot, testLimits), ErrNotHost)
	mustErr(t, l.Move("b", "b", seat(SideA, 2), testLimits), ErrNotHost)
	mustErr(t, l.Kick("b", "host"), ErrNotHost)
	mustErr(t, l.SetSettings("b", false, nil, testLimits), ErrNotHost)
	mustErr(t, l.Kick("host", "host"), ErrSelf)
}

func TestTheHostMovesHumansToEmptySeatsOnly(t *testing.T) {
	l := New("l", "host", t0)
	mustOK(t, l.Join("b", testLimits, t0))
	mustOK(t, l.Move("host", "b", seat(SideA, 3), testLimits))
	if m, _ := l.Occupant(seat(SideA, 3)); m == nil || m.AccountID != "b" {
		t.Fatalf("moved: %+v", l.Members)
	}
	mustErr(t, l.Move("host", "b", seat(SideA, 0), testLimits), ErrSlotTaken)
	mustErr(t, l.Move("host", "b", seat(SideB, 5), testLimits), ErrNoSuchSlot)
	mustErr(t, l.Move("host", "b", seat("C", 0), testLimits), ErrNoSuchSlot)
	mustErr(t, l.Move("host", "z", seat(SideB, 1), testLimits), ErrNotMember)
}

func TestBotsPlayAnyReleasedVanguardOncePerSide(t *testing.T) {
	l := New("l", "host", t0)
	mustOK(t, l.SetBot("host", Bot{Seat: seat(SideB, 0), VanguardID: "cairn", Difficulty: Beginner}, testLimits))
	// A mirror across sides is allowed; twice on one side is not (ADR-021 §8).
	mustOK(t, l.SetBot("host", Bot{Seat: seat(SideA, 1), VanguardID: "cairn", Difficulty: Intermediate}, testLimits))
	mustErr(t, l.SetBot("host", Bot{Seat: seat(SideB, 1), VanguardID: "cairn", Difficulty: Beginner}, testLimits), ErrDuplicateVanguard)
	// Changing a bot in its own seat is not a duplicate of itself.
	mustOK(t, l.SetBot("host", Bot{Seat: seat(SideB, 0), VanguardID: "cairn", Difficulty: Intermediate}, testLimits))
	mustErr(t, l.SetBot("host", Bot{Seat: seat(SideB, 1), VanguardID: "zed", Difficulty: Beginner}, testLimits), ErrUnknownVanguard)
	mustErr(t, l.SetBot("host", Bot{Seat: seat(SideB, 1), VanguardID: "oriel", Difficulty: "expert"}, testLimits), ErrUnknownDifficulty)
	mustErr(t, l.SetBot("host", Bot{Seat: seat(SideA, 0), VanguardID: "oriel", Difficulty: Beginner}, testLimits), ErrSlotTaken)
	mustErr(t, l.RemoveBot("host", seat(SideB, 4)), ErrNotABot)
	if len(l.Bots) != 2 || l.Bots[1].Difficulty != Intermediate {
		t.Fatalf("bots: %+v", l.Bots)
	}
}

func TestVictoryFollowsTheSidesUntilTheHostChooses(t *testing.T) {
	l := New("l", "host", t0)
	mustErr(t, l.SetSettings("host", true, nil, testLimits), ErrVictoryNeedsSides)
	mustOK(t, l.SetBot("host", Bot{Seat: seat(SideB, 0), VanguardID: "bryn", Difficulty: Beginner}, testLimits))
	if !l.Settings.VictoryEnabled {
		t.Fatal("an opposing side turns victory on while the host has not chosen")
	}
	mustOK(t, l.SetSettings("host", false, nil, testLimits))
	mustOK(t, l.SetBot("host", Bot{Seat: seat(SideB, 1), VanguardID: "raska", Difficulty: Beginner}, testLimits))
	if l.Settings.VictoryEnabled {
		t.Fatal("the host's choice stands")
	}
	mustOK(t, l.SetSettings("host", true, nil, testLimits))
	mustOK(t, l.RemoveBot("host", seat(SideB, 0)))
	mustOK(t, l.RemoveBot("host", seat(SideB, 1)))
	if l.Settings.VictoryEnabled {
		t.Fatal("an empty side can never lose, so victory goes off")
	}
}

func TestStartingGoldStaysInItsRange(t *testing.T) {
	l := New("l", "host", t0)
	mustErr(t, l.SetSettings("host", false, gold(-1), testLimits), ErrGoldOutOfRange)
	mustErr(t, l.SetSettings("host", false, gold(testLimits.StartingGoldMax+1), testLimits), ErrGoldOutOfRange)
	mustOK(t, l.SetSettings("host", false, gold(1500), testLimits))
	if l.Settings.StartingGold == nil || *l.Settings.StartingGold != 1500 {
		t.Fatalf("starting Gold: %v", l.Settings.StartingGold)
	}
	mustOK(t, l.SetSettings("host", false, nil, testLimits))
	if l.Settings.StartingGold != nil {
		t.Fatal("none plays the game's own starting Gold")
	}
}

func TestASelectingLobbyIsFixed(t *testing.T) {
	l := New("l", "host", t0)
	mustOK(t, l.CheckLaunch("host"))
	l.BeginSelecting()
	mustErr(t, l.CheckLaunch("host"), ErrLobbyLocked)
	mustErr(t, l.Join("b", testLimits, t0), ErrLobbyLocked)
	mustErr(t, l.SetBot("host", Bot{Seat: seat(SideB, 0), VanguardID: "cairn", Difficulty: Beginner}, testLimits), ErrLobbyLocked)
	l.Reopen()
	mustOK(t, l.Join("b", testLimits, t0))
}
