package selection

import (
	"context"
	"encoding/json"
	"errors"
	"io"
	"log/slog"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/catalog"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

var (
	ctx = context.Background()
	t0  = time.Date(2026, 9, 27, 12, 0, 0, 0, time.UTC)
)

// Fixture settings, independent of the committed config.
const (
	fixturePick     = 30 * time.Second
	fixtureStarting = time.Minute
)

var fixturePractice = PracticeSettings{Enabled: true, Mode: "custom_practice", HostSide: match.SideA, PickDuration: fixturePick}

var fixtureCasual = CasualSettings{PickDuration: 2 * fixturePick, PresenceTimeout: 10 * time.Second}

// fixtureCustomMode is the mode the fixture's custom matches record.
const fixtureCustomMode = "custom_game"

var fixtureCustom = CustomSettings{Mode: fixtureCustomMode, PickDuration: fixturePick}

// fixtureFluxSpells is the roster of Flux Spells the fixture offers.
var fixtureFluxSpells = []string{"blink", "mend", "scorch"}

// casualMode is a matchmade mode, one a side as in local play.
const casualMode = "casual_select"

type fixture struct {
	svc      *Service
	store    *MemStore
	matches  *match.Service
	accounts *account.Service
	alloc    *match.FakeAllocator
	queued   map[string]bool
	blocks   blockList
	ends     *selectEnds
	now      time.Time
}

// blockList holds blocks between accounts, either way round.
type blockList map[[2]string]bool

func (b blockList) BlockedAmong(_ context.Context, accounts []string) (bool, error) {
	for _, x := range accounts {
		for _, y := range accounts {
			if b[[2]string{x, y}] {
				return true, nil
			}
		}
	}
	return false, nil
}

// selectEnds records what matchmaking was told of each matchmade select's end.
type selectEnds struct{ calls []selectEnd }

type selectEnd struct {
	accounts, leaving []string
	started           bool
}

func (e *selectEnds) SelectEnded(_ context.Context, accounts, leaving []string, started bool) error {
	e.calls = append(e.calls, selectEnd{accounts: accounts, leaving: leaving, started: started})
	return nil
}

func newFixture(t *testing.T) *fixture {
	t.Helper()
	f := &fixture{store: NewMemStore(), alloc: match.NewFakeAllocator(), queued: map[string]bool{}, blocks: blockList{}, ends: &selectEnds{}, now: t0}
	clock := func() time.Time { return f.now }
	names := match.AccountsFunc(func(_ context.Context, ids []string) (map[string]string, error) {
		out := map[string]string{}
		for _, id := range ids {
			out[id] = "Player " + id
		}
		return out, nil
	})
	f.matches = match.NewService(match.NewMemStore(), names, f.alloc, match.Settings{
		Modes:             map[string]match.Mode{casualMode: {ID: casualMode, Enabled: true, HumanPlayersPerTeam: 1}},
		Maps:              match.FakeMaps,
		Practice:          match.PracticeSettings{Enabled: true, Mode: fixturePractice.Mode, HostSide: fixturePractice.HostSide},
		Custom:            match.CustomModeSettings{Enabled: true, Mode: fixtureCustomMode, PlayersPerSide: 5, StartingGoldMax: 20000},
		ReadyTimeout:      time.Minute,
		MaxDuration:       time.Hour,
		RemoveServerAfter: time.Minute,
		HostPortMin:       7780,
		HostPortMax:       7789,
		PublicHost:        "127.0.0.1",
		BackendURL:        "http://backend:8080",
	}, clock)
	vanguards := catalog.New(catalog.Settings{Released: []string{"cairn", "qazharr", "oriel", "bryn"}, Starters: []string{"cairn", "qazharr", "oriel"},
		RotationSlots: 12, StandIn: catalog.StandInNone})
	f.accounts = account.NewService(account.NewMemStore(), vanguards, clock)
	parties := PartiesFunc(func(_ context.Context, id string) (bool, error) { return f.queued[id], nil })
	f.svc = NewService(f.store, f.accounts, names, f.matches, parties, f.blocks,
		Settings{Practice: fixturePractice, Casual: fixtureCasual, Custom: fixtureCustom, StartingTimeout: fixtureStarting, FluxSpells: fixtureFluxSpells}, clock,
		slog.New(slog.NewTextHandler(io.Discard, nil)))
	f.svc.SetMatchmaking(f.ends)
	return f
}

// onboard finishes an account's tutorial with a starter, which it then owns.
func (f *fixture) onboard(t *testing.T, id, starter string) {
	t.Helper()
	if _, err := f.accounts.ChooseStarter(ctx, id, starter); err != nil {
		t.Fatal(err)
	}
}

func (f *fixture) practice(t *testing.T, id string) Session {
	t.Helper()
	s, err := f.svc.StartPractice(ctx, id)
	if err != nil {
		t.Fatalf("StartPractice: %v", err)
	}
	return s
}

func TestPracticeOpensASelectForItsHostAlone(t *testing.T) {
	f := newFixture(t)
	f.onboard(t, "acc-1", "cairn")
	s := f.practice(t, "acc-1")
	if s.State != Picking || s.Kind != KindPractice || s.HostAccountID != "acc-1" || len(s.Seats) != 1 || s.Seats[0].Side != match.SideA ||
		s.Seats[0].DisplayName != "Player acc-1" || !s.Deadline.Equal(t0.Add(fixturePick)) {
		t.Fatalf("practice select: %+v", s)
	}
	current, ok, err := f.svc.Current(ctx, "acc-1")
	if err != nil || !ok || current.ID != s.ID {
		t.Fatalf("Current: %+v %v %v", current, ok, err)
	}
}

func TestPracticeNeedsTheTutorialAndAFreePlayer(t *testing.T) {
	f := newFixture(t)
	if _, err := f.svc.StartPractice(ctx, "acc-1"); !errors.Is(err, ErrTutorialRequired) {
		t.Fatalf("before the tutorial: want ErrTutorialRequired, got %v", err)
	}
	f.onboard(t, "acc-1", "cairn")
	f.practice(t, "acc-1")
	if _, err := f.svc.StartPractice(ctx, "acc-1"); !errors.Is(err, ErrBusy) {
		t.Fatalf("already selecting: want ErrBusy, got %v", err)
	}

	f.onboard(t, "acc-2", "oriel")
	f.queued["acc-2"] = true
	if _, err := f.svc.StartPractice(ctx, "acc-2"); !errors.Is(err, ErrBusy) {
		t.Fatalf("a queued party: want ErrBusy, got %v", err)
	}

	f.onboard(t, "acc-3", "oriel")
	if _, err := f.matches.Create(ctx, match.Spec{Mode: fixturePractice.Mode, Rules: match.RulesPractice, HostAccountID: "acc-3",
		Seats: []match.Seat{{AccountID: "acc-3", Side: match.SideA, VanguardID: "oriel"}}}); err != nil {
		t.Fatal(err)
	}
	if _, err := f.svc.StartPractice(ctx, "acc-3"); !errors.Is(err, ErrBusy) {
		t.Fatalf("in a match: want ErrBusy, got %v", err)
	}

	f.svc.settings.Practice.Enabled = false
	if _, err := f.svc.StartPractice(ctx, "acc-4"); !errors.Is(err, ErrPracticeDisabled) {
		t.Fatalf("disabled: want ErrPracticeDisabled, got %v", err)
	}
}

func TestLockingCreatesThePracticeMatchOnce(t *testing.T) {
	f := newFixture(t)
	f.onboard(t, "acc-1", "oriel")
	s := f.practice(t, "acc-1")
	if _, err := f.svc.Hover(ctx, "acc-1", "bryn"); !errors.Is(err, ErrNotAvailable) {
		t.Fatalf("a Vanguard the player may not pick: want ErrNotAvailable, got %v", err)
	}
	if hovered, err := f.svc.Hover(ctx, "acc-1", "oriel"); err != nil || hovered.Seats[0].Hover != "oriel" {
		t.Fatalf("Hover: %+v %v", hovered, err)
	}
	locked, err := f.svc.Lock(ctx, "acc-1", "oriel")
	if err != nil || locked.State != Started || locked.MatchID == "" || locked.Seats[0].Locked != "oriel" {
		t.Fatalf("Lock: %+v %v", locked, err)
	}
	m, found, err := f.matches.BySelect(ctx, s.ID)
	if err != nil || !found || m.ID != locked.MatchID || m.Rules != match.RulesPractice || m.HostAccountID != "acc-1" ||
		m.Participants[0].VanguardID != "oriel" || m.Mode != fixturePractice.Mode {
		t.Fatalf("the practice match: %+v %v %v", m, found, err)
	}
	if _, selecting, _ := f.svc.Current(ctx, "acc-1"); selecting {
		t.Fatal("a started select no longer holds its player")
	}
	if final, err := f.svc.ForParticipant(ctx, "acc-1", s.ID); err != nil || final.State != Started {
		t.Fatalf("the final state: %+v %v", final, err)
	}
	if _, err := f.svc.ForParticipant(ctx, "acc-2", s.ID); !errors.Is(err, ErrSelectNotFound) {
		t.Fatalf("an outsider: want ErrSelectNotFound, got %v", err)
	}
	if _, err := f.svc.Lock(ctx, "acc-1", "oriel"); !errors.Is(err, ErrSelectNotFound) {
		t.Fatalf("locking again after the start: want ErrSelectNotFound, got %v", err)
	}
}

func TestStartingFluxSpellsAreFreeAndFollowTheSavedLoadout(t *testing.T) {
	f := newFixture(t)
	f.onboard(t, "acc-1", "oriel")
	f.practice(t, "acc-1")
	if hovered, err := f.svc.Hover(ctx, "acc-1", "oriel"); err != nil || hovered.Seats[0].FluxSpells != ([2]string{}) {
		t.Fatalf("no saved loadout leaves both slots empty: %+v %v", hovered, err)
	}
	for _, refused := range [][2]string{{"blink", "enfeeble"}, {"mend", "mend"}, {"Blink", ""}} {
		if _, err := f.svc.SetFluxSpells(ctx, "acc-1", refused); !errors.Is(err, match.ErrInvalidFluxSpells) {
			t.Fatalf("%v: want ErrInvalidFluxSpells, got %v", refused, err)
		}
	}
	chosen, err := f.svc.SetFluxSpells(ctx, "acc-1", [2]string{"", "scorch"})
	if err != nil || chosen.Seats[0].FluxSpells != [2]string{"", "scorch"} || !chosen.Deadline.Equal(t0.Add(fixturePick)) {
		t.Fatalf("an empty slot and a roster spell, and the timer untouched: %+v %v", chosen, err)
	}
	locked, err := f.svc.Lock(ctx, "acc-1", "oriel")
	if err != nil || locked.State != Started {
		t.Fatalf("Lock: %+v %v", locked, err)
	}
	m, _, err := f.matches.BySelect(ctx, locked.ID)
	if err != nil || m.Participants[0].FluxSpells != [2]string{"", "scorch"} {
		t.Fatalf("the match takes the spells: %+v %v", m.Participants, err)
	}

	// The match never readies and fails, freeing its player for another select. The spells never
	// went into a match, so nothing is saved (Pre-Game Client UX Bible 37).
	f.now = f.now.Add(2 * time.Minute)
	if err := f.matches.Reap(ctx); err != nil {
		t.Fatal(err)
	}
	f.practice(t, "acc-1")
	if hovered, err := f.svc.Hover(ctx, "acc-1", "oriel"); err != nil || hovered.Seats[0].FluxSpells != ([2]string{}) {
		t.Fatalf("a match that failed to start saves nothing: %+v %v", hovered, err)
	}

	// This time the server becomes ready: the player took the spells into a match, which later ends.
	// The next select prefills them until the player chooses.
	if _, err := f.svc.SetFluxSpells(ctx, "acc-1", [2]string{"", "scorch"}); err != nil {
		t.Fatal(err)
	}
	locked, err = f.svc.Lock(ctx, "acc-1", "oriel")
	if err != nil || locked.State != Started {
		t.Fatalf("Lock: %+v %v", locked, err)
	}
	spec, ok := f.alloc.Spec(locked.MatchID)
	var assignment match.Assignment
	if !ok || json.Unmarshal(spec.Assignment, &assignment) != nil {
		t.Fatalf("no assignment for %s", locked.MatchID)
	}
	if err := f.matches.ServerReady(ctx, assignment.ServerCredential, locked.MatchID); err != nil {
		t.Fatalf("ServerReady: %v", err)
	}
	f.now = f.now.Add(2 * time.Hour)
	if err := f.matches.Reap(ctx); err != nil {
		t.Fatal(err)
	}
	f.practice(t, "acc-1")
	if hovered, err := f.svc.Hover(ctx, "acc-1", "oriel"); err != nil || hovered.Seats[0].FluxSpells != [2]string{"", "scorch"} {
		t.Fatalf("the saved loadout: %+v %v", hovered, err)
	}
	if _, err := f.svc.SetFluxSpells(ctx, "acc-1", [2]string{"blink", "mend"}); err != nil {
		t.Fatal(err)
	}
	if again, err := f.svc.Hover(ctx, "acc-1", "oriel"); err != nil || again.Seats[0].FluxSpells != [2]string{"blink", "mend"} {
		t.Fatalf("a player's own choice stays: %+v %v", again, err)
	}
}

func TestTheTimerLocksAHoverOrCancels(t *testing.T) {
	f := newFixture(t)
	f.onboard(t, "acc-1", "cairn")
	f.onboard(t, "acc-2", "oriel")
	hovering := f.practice(t, "acc-1")
	idle := f.practice(t, "acc-2")
	if _, err := f.svc.Hover(ctx, "acc-1", "cairn"); err != nil {
		t.Fatal(err)
	}

	f.now = t0.Add(fixturePick - time.Second)
	_ = f.svc.Tick(ctx)
	if s, _ := f.svc.ForParticipant(ctx, "acc-1", hovering.ID); s.State != Picking {
		t.Fatalf("before the deadline nothing changes: %+v", s)
	}
	f.now = t0.Add(fixturePick)
	if err := f.svc.Tick(ctx); err != nil {
		t.Fatalf("Tick: %v", err)
	}
	if s, _ := f.svc.ForParticipant(ctx, "acc-1", hovering.ID); s.State != Started || s.Seats[0].Locked != "cairn" {
		t.Fatalf("the hover is locked and the match created: %+v", s)
	}
	if s, _ := f.svc.ForParticipant(ctx, "acc-2", idle.ID); s.State != Cancelled || s.CancelReason != CancelTimedOut {
		t.Fatalf("nothing to lock cancels: %+v", s)
	}
	if _, err := f.svc.Lock(ctx, "acc-2", "oriel"); !errors.Is(err, ErrSelectNotFound) {
		t.Fatalf("a cancelled select takes no lock: %v", err)
	}
}

func TestPicksAfterTheDeadlineAreRefused(t *testing.T) {
	f := newFixture(t)
	f.onboard(t, "acc-1", "cairn")
	f.practice(t, "acc-1")
	f.now = t0.Add(fixturePick)
	if _, err := f.svc.Lock(ctx, "acc-1", "cairn"); !errors.Is(err, ErrExpired) {
		t.Fatalf("want ErrExpired, got %v", err)
	}
}

func TestAFailedMatchCancelsTheSelectAndFreesThePlayer(t *testing.T) {
	f := newFixture(t)
	f.onboard(t, "acc-1", "cairn")
	f.practice(t, "acc-1")
	f.alloc.FailStarts(errors.New("docker is down"))
	s, err := f.svc.Lock(ctx, "acc-1", "cairn")
	if err != nil || s.State != Cancelled || s.CancelReason != CancelAllocationFailed {
		t.Fatalf("a failed allocation: %+v %v", s, err)
	}
	f.alloc.FailStarts(nil)
	if _, err := f.svc.StartPractice(ctx, "acc-1"); err != nil {
		t.Fatalf("the player is free again: %v", err)
	}
}

func TestAStuckStartIsSettled(t *testing.T) {
	f := newFixture(t)
	for _, id := range []string{"acc-1", "acc-2"} {
		f.onboard(t, id, "cairn")
	}
	stuck := func(id string) Session {
		s := f.practice(t, id)
		_ = f.store.InTx(ctx, func(_ context.Context, tx Tx) error {
			locked, _ := tx.LockSession(s.ID)
			_ = locked.Lock(id, "cairn", f.now)
			locked.BeginStarting(f.now)
			return tx.SaveSession(locked)
		})
		return s
	}
	lost := stuck("acc-1")
	made := stuck("acc-2")
	// The second's match was created before the crash.
	created, err := f.matches.Create(ctx, match.Spec{Mode: fixturePractice.Mode, Rules: match.RulesPractice, HostAccountID: "acc-2",
		Seats: []match.Seat{{AccountID: "acc-2", Side: match.SideA, VanguardID: "cairn"}}, SelectID: made.ID})
	if err != nil {
		t.Fatal(err)
	}
	f.now = t0.Add(fixtureStarting - time.Second)
	_ = f.svc.Tick(ctx)
	if s, _ := f.svc.ForParticipant(ctx, "acc-1", lost.ID); s.State != Starting {
		t.Fatalf("before the timeout it waits: %+v", s)
	}
	f.now = t0.Add(fixtureStarting)
	if err := f.svc.Tick(ctx); err != nil {
		t.Fatalf("Tick: %v", err)
	}
	if s, _ := f.svc.ForParticipant(ctx, "acc-1", lost.ID); s.State != Cancelled || s.CancelReason != CancelStartingTimedOut {
		t.Fatalf("no match: %+v", s)
	}
	if s, _ := f.svc.ForParticipant(ctx, "acc-2", made.ID); s.State != Started || s.MatchID != created.ID {
		t.Fatalf("its match was found: %+v", s)
	}
}

func TestSessionRules(t *testing.T) {
	s := Session{State: Picking, Deadline: t0.Add(time.Minute), Seats: []Seat{{AccountID: "a"}, {AccountID: "b"}}}
	if err := s.Hover("stranger", "cairn", t0); !errors.Is(err, ErrSelectNotFound) {
		t.Fatalf("a stranger: %v", err)
	}
	if err := s.Lock("a", "cairn", t0); err != nil || s.AllLocked() {
		t.Fatalf("one lock: %v %+v", err, s)
	}
	if err := s.Hover("a", "oriel", t0); !errors.Is(err, ErrAlreadyLocked) {
		t.Fatalf("a lock is permanent: %v", err)
	}
	if err := s.Lock("b", "bryn", t0.Add(time.Minute)); !errors.Is(err, ErrExpired) {
		t.Fatalf("at the deadline: %v", err)
	}
	if err := s.Lock("b", "bryn", t0); err != nil || !s.AllLocked() {
		t.Fatalf("both locked: %v %+v", err, s)
	}
	s.Cancel(CancelTimedOut, t0)
	if s.State != Cancelled || s.CancelReason != CancelTimedOut {
		t.Fatal("an active select cancels")
	}
	s.Cancel(CancelAllocationFailed, t0.Add(time.Second))
	if s.CancelReason != CancelTimedOut || !s.EndedAt.Equal(t0) {
		t.Fatal("a finished select does not cancel again")
	}
}
