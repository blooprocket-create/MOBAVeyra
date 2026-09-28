package match

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"strings"
	"testing"
	"time"
)

type fixture struct {
	svc   *Service
	store *MemStore
	alloc *FakeAllocator
	now   time.Time
}

// Fixture settings, independent of the committed config.
const (
	fixtureReadyTimeout      = 2 * time.Minute
	fixtureMaxDuration       = 4 * time.Hour
	fixtureRemoveServerAfter = 2 * time.Minute
	fixturePortMin           = 7780
	fixturePortMax           = 7781
)

var fixtureAccounts = map[string]string{"acc-1": "DevOne", "acc-2": "DevTwo", "acc-3": "DevThree", "acc-4": "DevFour"}

func newFixture(t *testing.T) *fixture {
	t.Helper()
	f := &fixture{store: NewMemStore(), alloc: NewFakeAllocator(), now: t0}
	accounts := AccountsFunc(func(_ context.Context, ids []string) (map[string]string, error) {
		out := map[string]string{}
		for _, id := range ids {
			if name, ok := fixtureAccounts[id]; ok {
				out[id] = name
			}
		}
		return out, nil
	})
	f.svc = NewService(f.store, accounts, f.alloc, Settings{
		Modes:             map[string]Mode{"casual_select": fiveAll, "ranked": {ID: "ranked", HumanPlayersPerTeam: 5}},
		Practice:          fixturePractice,
		ReadyTimeout:      fixtureReadyTimeout,
		MaxDuration:       fixtureMaxDuration,
		RemoveServerAfter: fixtureRemoveServerAfter,
		HostPortMin:       fixturePortMin,
		HostPortMax:       fixturePortMax,
		PublicHost:        "127.0.0.1",
		BackendURL:        "http://backend:8080",
	}, func() time.Time { return f.now })
	return f
}

var ctx = context.Background()

// fixturePractice is practice as the fixture configures it.
var fixturePractice = PracticeSettings{Enabled: true, Mode: "custom_practice", HostSide: SideA,
	Bots: []Bot{{Side: SideB, VanguardID: "cairn"}, {Side: SideB, VanguardID: "bryn"}}}

// standard asks for a casual match with these seats.
func standard(seats ...Seat) Spec {
	return Spec{Mode: "casual_select", Rules: RulesStandard, Seats: seats}
}

// practice asks for acc-1's practice match as a Vanguard.
func practice(vanguard string) Spec {
	return Spec{Mode: fixturePractice.Mode, Rules: RulesPractice, HostAccountID: "acc-1",
		Seats: []Seat{{AccountID: "acc-1", Side: fixturePractice.HostSide, VanguardID: vanguard}}}
}

func (f *fixture) create(t *testing.T, seats ...Seat) Match {
	t.Helper()
	m, err := f.svc.Create(ctx, standard(seats...))
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	return m
}

// assignment returns what the match's server was handed on its stdin.
func (f *fixture) assignment(t *testing.T, matchID string) Assignment {
	t.Helper()
	spec, ok := f.alloc.Spec(matchID)
	if !ok {
		t.Fatalf("no server was started for %s", matchID)
	}
	var a Assignment
	if err := json.Unmarshal(spec.Assignment, &a); err != nil {
		t.Fatalf("assignment: %v", err)
	}
	return a
}

func (f *fixture) ready(t *testing.T, m Match) Assignment {
	t.Helper()
	a := f.assignment(t, m.ID)
	if err := f.svc.ServerReady(ctx, a.ServerCredential, m.ID); err != nil {
		t.Fatalf("ServerReady: %v", err)
	}
	return a
}

var twoSeats = []Seat{{AccountID: "acc-1", Side: SideA, VanguardID: "cairn"}, {AccountID: "acc-2", Side: SideB, VanguardID: "cairn"}}

func TestCreateStartsAServerWithTheRoster(t *testing.T) {
	f := newFixture(t)
	m := f.create(t, twoSeats...)
	if m.State != Allocating || m.Server.HostPort != fixturePortMin || m.JoinKey != nil || m.ServerCredentialHash != nil {
		t.Fatalf("unexpected match or secrets returned: %+v", m)
	}
	spec, _ := f.alloc.Spec(m.ID)
	if spec.HostPort != fixturePortMin {
		t.Fatalf("server started on %d", spec.HostPort)
	}
	a := f.assignment(t, m.ID)
	if !strings.HasPrefix(a.ServerCredential, "vms_") || len(a.Participants) != 2 || a.Participants[0].DisplayName != "DevOne" {
		t.Fatalf("wrong assignment: %+v", a)
	}
	stored, _ := f.store.MatchByID(ctx, m.ID)
	if len(stored.JoinKey) != 32 || bytes.Contains(spec.Assignment, stored.JoinKey) {
		t.Fatal("the key must be stored and never sent")
	}
	if bytes.Equal(stored.ServerCredentialHash, []byte(a.ServerCredential)) {
		t.Fatal("the server credential must be stored only as a hash")
	}
}

func TestAPlayerGetsTheTicketOnlyOnceReady(t *testing.T) {
	f := newFixture(t)
	m := f.create(t, twoSeats...)
	pm, ok, err := f.svc.Current(ctx, "acc-2")
	if err != nil || !ok || pm.State != Allocating || pm.Ticket != "" || pm.ServerPort != 0 || pm.Side != SideB {
		t.Fatalf("before ready: %+v %v %v", pm, ok, err)
	}
	a := f.ready(t, m)
	pm, _, _ = f.svc.Current(ctx, "acc-2")
	if pm.State != Ready || pm.ServerHost != "127.0.0.1" || pm.ServerPort != fixturePortMin || !strings.HasPrefix(pm.Ticket, "vjt_") {
		t.Fatalf("after ready: %+v", pm)
	}
	if TicketHash(pm.Ticket) != a.Participants[1].TicketHash {
		t.Fatal("the ticket does not match the hash the server was given")
	}
	again, _, _ := f.svc.Current(ctx, "acc-2")
	if again.Ticket != pm.Ticket {
		t.Fatal("asking again after a crash must give the same ticket")
	}
	if _, ok, _ := f.svc.Current(ctx, "acc-3"); ok {
		t.Fatal("an account outside the match has no match")
	}
}

func TestCreateRefusesBadRequests(t *testing.T) {
	cases := map[string]struct {
		mode  string
		seats []Seat
		want  error
	}{
		"unknown mode":    {"nope", twoSeats, ErrUnknownMode},
		"disabled mode":   {"ranked", twoSeats, ErrUnknownMode},
		"unknown account": {"casual_select", []Seat{{AccountID: "ghost", Side: SideA, VanguardID: "cairn"}}, ErrAccountNotFound},
		"bad side":        {"casual_select", []Seat{{AccountID: "acc-1", Side: "C", VanguardID: "cairn"}}, ErrInvalidRoster},
		"duplicate":       {"casual_select", []Seat{{AccountID: "acc-1", Side: SideA, VanguardID: "cairn"}, {AccountID: "acc-1", Side: SideB, VanguardID: "cairn"}}, ErrInvalidRoster},
		"no Vanguard":     {"casual_select", []Seat{{AccountID: "acc-1", Side: SideA}}, ErrInvalidVanguard},
		"bad Vanguard":    {"casual_select", []Seat{{AccountID: "acc-1", Side: SideA, VanguardID: "Cairn"}}, ErrInvalidVanguard},
		"practice mode":   {fixturePractice.Mode, []Seat{{AccountID: "acc-1", Side: SideA, VanguardID: "cairn"}}, ErrUnknownMode},
	}
	for name, tc := range cases {
		t.Run(name, func(t *testing.T) {
			f := newFixture(t)
			if _, err := f.svc.Create(ctx, Spec{Mode: tc.mode, Rules: RulesStandard, Seats: tc.seats}); !errors.Is(err, tc.want) {
				t.Fatalf("want %v, got %v", tc.want, err)
			}
		})
	}
}

func TestTheAssignmentCarriesTheModeRulesAndVanguards(t *testing.T) {
	f := newFixture(t)
	seats := []Seat{{AccountID: "acc-1", Side: SideA, VanguardID: "oriel"}, {AccountID: "acc-2", Side: SideB, VanguardID: "bryn"}}
	m := f.create(t, seats...)
	a := f.assignment(t, m.ID)
	if a.SchemaVersion != 2 || a.Mode != "casual_select" || a.Rules != "Standard" || len(a.HostAccountID) != 0 {
		t.Fatalf("wrong assignment header: %+v", a)
	}
	if a.Bots == nil || len(a.Bots) != 0 {
		t.Fatalf("a standard match has no bots, written as an empty list: %+v", a.Bots)
	}
	if a.Participants[0].VanguardID != "oriel" || a.Participants[1].VanguardID != "bryn" {
		t.Fatalf("wrong Vanguards: %+v", a.Participants)
	}
	pm, _, _ := f.svc.Current(ctx, "acc-2")
	if pm.Mode != "casual_select" || pm.Rules != RulesStandard || pm.VanguardID != "bryn" {
		t.Fatalf("the player's view: %+v", pm)
	}
}

func TestAPracticeMatchIsItsHostAndTheConfiguredBots(t *testing.T) {
	f := newFixture(t)
	m, err := f.svc.Create(ctx, practice("qazharr"))
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	if m.Rules != RulesPractice || m.HostAccountID != "acc-1" || m.Mode != fixturePractice.Mode {
		t.Fatalf("practice match: %+v", m)
	}
	if len(m.Bots) != len(fixturePractice.Bots) || m.Bots[1] != fixturePractice.Bots[1] {
		t.Fatalf("practice bots: %+v", m.Bots)
	}
	a := f.assignment(t, m.ID)
	if a.Rules != "Practice" || len(a.HostAccountID) != 1 || a.HostAccountID[0] != "acc-1" || a.Participants[0].VanguardID != "qazharr" {
		t.Fatalf("practice assignment: %+v", a)
	}
	want := []AssignedBot{{Side: SideB, VanguardID: "cairn"}, {Side: SideB, VanguardID: "bryn"}}
	if len(a.Participants) != 1 || len(a.Bots) != len(want) || a.Bots[0] != want[0] || a.Bots[1] != want[1] {
		t.Fatalf("the bots are no participants, and go to the server in order: %+v %+v", a.Participants, a.Bots)
	}
}

func TestPracticeRefusesAnythingButItsHostAlone(t *testing.T) {
	cases := map[string]struct {
		change func(*Spec)
		want   error
	}{
		"another mode":  {func(s *Spec) { s.Mode = "casual_select" }, ErrUnknownMode},
		"no host":       {func(s *Spec) { s.HostAccountID = "" }, ErrInvalidRoster},
		"host not seat": {func(s *Spec) { s.HostAccountID = "acc-2" }, ErrInvalidRoster},
		"other side":    {func(s *Spec) { s.Seats[0].Side = SideB }, ErrInvalidRoster},
		"two seats":     {func(s *Spec) { s.Seats = append(s.Seats, Seat{AccountID: "acc-2", Side: SideB, VanguardID: "cairn"}) }, ErrInvalidRoster},
		"no Vanguard":   {func(s *Spec) { s.Seats[0].VanguardID = "" }, ErrInvalidVanguard},
		"unknown rules": {func(s *Spec) { s.Rules = "draft" }, ErrInvalidRules},
	}
	for name, tc := range cases {
		t.Run(name, func(t *testing.T) {
			f := newFixture(t)
			spec := practice("cairn")
			tc.change(&spec)
			if _, err := f.svc.Create(ctx, spec); !errors.Is(err, tc.want) {
				t.Fatalf("want %v, got %v", tc.want, err)
			}
		})
	}
	t.Run("disabled", func(t *testing.T) {
		f := newFixture(t)
		f.svc.settings.Practice.Enabled = false
		if _, err := f.svc.Create(ctx, practice("cairn")); !errors.Is(err, ErrUnknownMode) {
			t.Fatalf("want ErrUnknownMode, got %v", err)
		}
	})
	t.Run("a standard match has no host", func(t *testing.T) {
		f := newFixture(t)
		spec := standard(twoSeats...)
		spec.HostAccountID = "acc-1"
		if _, err := f.svc.Create(ctx, spec); !errors.Is(err, ErrInvalidRoster) {
			t.Fatalf("want ErrInvalidRoster, got %v", err)
		}
	})
}

func TestOnlyAPracticeMatchCanBeHostEnded(t *testing.T) {
	f := newFixture(t)
	casual := f.create(t, Seat{AccountID: "acc-2", Side: SideA, VanguardID: "cairn"})
	casualAssignment := f.ready(t, casual)
	stored, _ := f.store.MatchByID(ctx, casual.ID)
	hostEnded := resultFor(stored)
	hostEnded.EndReason = EndHostEnded
	if err := f.svc.ServerResult(ctx, casualAssignment.ServerCredential, casual.ID, hostEnded); !errors.Is(err, ErrInvalidResult) {
		t.Fatalf("a standard match: want ErrInvalidResult, got %v", err)
	}

	m, _ := f.svc.Create(ctx, practice("cairn"))
	a := f.ready(t, m)
	stored, _ = f.store.MatchByID(ctx, m.ID)
	hostEnded = resultFor(stored)
	hostEnded.EndReason = EndHostEnded
	if err := f.svc.ServerResult(ctx, a.ServerCredential, m.ID, hostEnded); err != nil {
		t.Fatalf("a practice match: %v", err)
	}
}

func TestASelectCreatesOneMatch(t *testing.T) {
	f := newFixture(t)
	spec := practice("cairn")
	spec.SelectID = "select-1"
	first, err := f.svc.Create(ctx, spec)
	if err != nil {
		t.Fatal(err)
	}
	spec.HostAccountID, spec.Seats[0].AccountID = "acc-2", "acc-2"
	if _, err := f.svc.Create(ctx, spec); !errors.Is(err, ErrSelectHasMatch) {
		t.Fatalf("a second match for the select: want ErrSelectHasMatch, got %v", err)
	}
	found, ok, err := f.svc.BySelect(ctx, "select-1")
	if err != nil || !ok || found.ID != first.ID || found.JoinKey != nil {
		t.Fatalf("BySelect: %+v %v %v", found, ok, err)
	}
	if _, ok, _ := f.svc.BySelect(ctx, "no-such-select"); ok {
		t.Fatal("an unknown select has no match")
	}
}

func TestOnlyParticipantsSeeAMatch(t *testing.T) {
	f := newFixture(t)
	m := f.create(t, twoSeats...)
	got, p, err := f.svc.ForParticipant(ctx, "acc-2", m.ID)
	if err != nil || got.ID != m.ID || p.Side != SideB || p.VanguardID != "cairn" || got.JoinKey != nil || got.ServerCredentialHash != nil {
		t.Fatalf("a participant's view: %+v %+v %v", got, p, err)
	}
	if _, _, err := f.svc.ForParticipant(ctx, "acc-3", m.ID); !errors.Is(err, ErrMatchNotFound) {
		t.Fatalf("an outsider: want ErrMatchNotFound, got %v", err)
	}
	if _, _, err := f.svc.ForParticipant(ctx, "acc-1", "no-such-match"); !errors.Is(err, ErrMatchNotFound) {
		t.Fatalf("an unknown match: want ErrMatchNotFound, got %v", err)
	}
}

func TestAnAccountHasOneActiveMatch(t *testing.T) {
	f := newFixture(t)
	f.create(t, twoSeats...)
	_, err := f.svc.Create(ctx, standard([]Seat{{AccountID: "acc-2", Side: SideA, VanguardID: "cairn"}, {AccountID: "acc-3", Side: SideB, VanguardID: "cairn"}}...))
	if !errors.Is(err, ErrAlreadyInMatch) {
		t.Fatalf("want ErrAlreadyInMatch, got %v", err)
	}
	// The refused match took no port.
	m := f.create(t, Seat{AccountID: "acc-3", Side: SideA, VanguardID: "cairn"})
	if m.Server.HostPort != fixturePortMin+1 {
		t.Fatalf("port %d, want the next free one", m.Server.HostPort)
	}
}

func TestPortsRunOut(t *testing.T) {
	f := newFixture(t)
	f.create(t, Seat{AccountID: "acc-1", Side: SideA, VanguardID: "cairn"})
	f.create(t, Seat{AccountID: "acc-2", Side: SideA, VanguardID: "cairn"})
	if _, err := f.svc.Create(ctx, standard([]Seat{{AccountID: "acc-3", Side: SideA, VanguardID: "cairn"}}...)); !errors.Is(err, ErrNoServerCapacity) {
		t.Fatalf("want ErrNoServerCapacity, got %v", err)
	}
}

func TestAFailedStartReleasesEverything(t *testing.T) {
	f := newFixture(t)
	f.alloc.FailStarts(errors.New("docker is down"))
	_, err := f.svc.Create(ctx, standard(twoSeats...))
	if !errors.Is(err, ErrAllocationFailed) {
		t.Fatalf("want ErrAllocationFailed, got %v", err)
	}
	if _, ok, _ := f.svc.Current(ctx, "acc-1"); ok {
		t.Fatal("the players must be free again")
	}
	f.alloc.FailStarts(nil)
	if m := f.create(t, twoSeats...); m.Server.HostPort != fixturePortMin {
		t.Fatalf("the failed match's port must be free again, got %d", m.Server.HostPort)
	}
}

func TestServerCredentialsAreScopedToTheirMatch(t *testing.T) {
	f := newFixture(t)
	m1 := f.create(t, Seat{AccountID: "acc-1", Side: SideA, VanguardID: "cairn"})
	m2 := f.create(t, Seat{AccountID: "acc-2", Side: SideA, VanguardID: "cairn"})
	a1 := f.assignment(t, m1.ID)
	for name, cred := range map[string]string{
		"no prefix":     strings.TrimPrefix(a1.ServerCredential, "vms_"),
		"unknown":       "vms_unknown",
		"another match": f.assignment(t, m2.ID).ServerCredential,
	} {
		if err := f.svc.ServerReady(ctx, cred, m1.ID); !errors.Is(err, ErrUnauthorized) {
			t.Fatalf("%s: want ErrUnauthorized, got %v", name, err)
		}
	}
	if err := f.svc.ServerReady(ctx, a1.ServerCredential, m1.ID); err != nil {
		t.Fatalf("ServerReady: %v", err)
	}
	if err := f.svc.ServerReady(ctx, a1.ServerCredential, m1.ID); err != nil {
		t.Fatalf("a repeated ready must succeed: %v", err)
	}
}

func TestResultEndsTheMatchAndKillsTickets(t *testing.T) {
	f := newFixture(t)
	m := f.create(t, twoSeats...)
	a := f.ready(t, m)
	stored, _ := f.store.MatchByID(ctx, m.ID)
	r := resultFor(stored)
	f.now = t0.Add(10 * time.Minute)
	if err := f.svc.ServerResult(ctx, a.ServerCredential, m.ID, r); err != nil {
		t.Fatalf("ServerResult: %v", err)
	}
	got, _ := f.svc.Get(ctx, m.ID)
	if got.State != Ended || got.Result == nil || got.Result.EndReason != EndDeveloperRequest || !got.EndedAt.Equal(f.now) {
		t.Fatalf("not ended: %+v", got)
	}
	stored, _ = f.store.MatchByID(ctx, m.ID)
	if stored.JoinKey != nil {
		t.Fatal("the join key must be erased")
	}
	if _, ok, _ := f.svc.Current(ctx, "acc-1"); ok {
		t.Fatal("an ended match is no longer the player's match")
	}
	if err := f.svc.ServerResult(ctx, a.ServerCredential, m.ID, r); err != nil {
		t.Fatalf("a replayed result must succeed: %v", err)
	}
	r.DurationSeconds++
	if err := f.svc.ServerResult(ctx, a.ServerCredential, m.ID, r); !errors.Is(err, ErrResultConflict) {
		t.Fatalf("want ErrResultConflict, got %v", err)
	}
}

func TestResultNeedsAReadyMatch(t *testing.T) {
	f := newFixture(t)
	m := f.create(t, twoSeats...)
	a := f.assignment(t, m.ID)
	stored, _ := f.store.MatchByID(ctx, m.ID)
	if err := f.svc.ServerResult(ctx, a.ServerCredential, m.ID, resultFor(stored)); !errors.Is(err, ErrInvalidState) {
		t.Fatalf("want ErrInvalidState, got %v", err)
	}
}

func TestReaperFailsAServerThatNeverGetsReady(t *testing.T) {
	f := newFixture(t)
	m := f.create(t, twoSeats...)
	f.now = t0.Add(fixtureReadyTimeout - time.Second)
	if err := f.svc.Reap(ctx); err != nil {
		t.Fatalf("Reap: %v", err)
	}
	if got, _ := f.svc.Get(ctx, m.ID); got.State != Allocating {
		t.Fatal("failed before the ready timeout")
	}
	f.now = t0.Add(fixtureReadyTimeout)
	_ = f.svc.Reap(ctx)
	if got, _ := f.svc.Get(ctx, m.ID); got.State != Failed || got.FailureReason != FailReadyTimeout {
		t.Fatalf("want ready_timeout, got %+v", got)
	}
}

func TestReaperFailsAMatchPastItsMaximum(t *testing.T) {
	f := newFixture(t)
	m := f.create(t, twoSeats...)
	f.ready(t, m)
	f.now = t0.Add(fixtureMaxDuration)
	_ = f.svc.Reap(ctx)
	if got, _ := f.svc.Get(ctx, m.ID); got.FailureReason != FailMaxDuration {
		t.Fatalf("want max_duration, got %+v", got)
	}
}

func TestReaperFailsAMatchWhoseServerStopped(t *testing.T) {
	f := newFixture(t)
	m := f.create(t, twoSeats...)
	f.ready(t, m)
	f.alloc.Exit(m.ID, 1)
	_ = f.svc.Reap(ctx)
	if got, _ := f.svc.Get(ctx, m.ID); got.FailureReason != FailServerExited {
		t.Fatalf("want server_exited, got %+v", got)
	}
}

func TestReaperWaitsForAServerThatIsStillBeingCreated(t *testing.T) {
	f := newFixture(t)
	m := f.create(t, twoSeats...)
	_ = f.alloc.Remove(ctx, m.ID) // as if the container did not exist yet
	_ = f.svc.Reap(ctx)
	if got, _ := f.svc.Get(ctx, m.ID); got.State != Allocating {
		t.Fatalf("a missing server while allocating must wait for the ready timeout: %+v", got)
	}
}

func TestReaperLeavesAnEndedMatchAndRemovesItsServerLater(t *testing.T) {
	f := newFixture(t)
	m := f.create(t, twoSeats...)
	a := f.ready(t, m)
	stored, _ := f.store.MatchByID(ctx, m.ID)
	if err := f.svc.ServerResult(ctx, a.ServerCredential, m.ID, resultFor(stored)); err != nil {
		t.Fatalf("ServerResult: %v", err)
	}
	f.alloc.Exit(m.ID, 0)
	f.now = t0.Add(fixtureRemoveServerAfter - time.Second)
	_ = f.svc.Reap(ctx)
	got, _ := f.svc.Get(ctx, m.ID)
	if got.State != Ended || !got.Server.RemovedAt.IsZero() {
		t.Fatalf("an ended match must stay ended and keep its server a while: %+v", got)
	}
	f.now = t0.Add(fixtureRemoveServerAfter)
	_ = f.svc.Reap(ctx)
	got, _ = f.svc.Get(ctx, m.ID)
	if got.Server.RemovedAt.IsZero() || len(f.alloc.Removed()) != 1 {
		t.Fatalf("the server should have been removed: %+v", got)
	}
	// Its port is free again.
	if next := f.create(t, twoSeats...); next.Server.HostPort != fixturePortMin {
		t.Fatalf("port %d, want the removed server's", next.Server.HostPort)
	}
}
