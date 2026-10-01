package matchmaking

import (
	"context"
	"errors"
	"io"
	"log/slog"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
)

var ctx = context.Background()

// Fixture values.
const (
	oneVsOne    = "casual_select"
	twoVsTwo    = "draft_pick"
	unmatched   = "ranked"
	versusAI    = "coop_beginner"
	acceptFor   = 15 * time.Second
	partyLimit  = 5
	inviteLimit = time.Minute
	searchSteps = 1000
)

// socialGraph is everyone's friend, with the blocks the test sets.
type socialGraph struct{ blocks map[[2]string]bool }

func (g socialGraph) AreFriends(context.Context, string, string) (bool, error)    { return true, nil }
func (g socialGraph) FriendOfAny(context.Context, string, []string) (bool, error) { return true, nil }
func (g socialGraph) BlockedWithAny(_ context.Context, a string, others []string) (bool, error) {
	for _, b := range others {
		if g.blocks[[2]string{a, b}] || g.blocks[[2]string{b, a}] {
			return true, nil
		}
	}
	return false, nil
}

func (g socialGraph) BlockedAmong(ctx context.Context, accounts []string) (bool, error) {
	for _, a := range accounts {
		if blocked, _ := g.BlockedWithAny(ctx, a, accounts); blocked {
			return true, nil
		}
	}
	return false, nil
}

// playing holds the accounts in a match or champion select.
type playing map[string]bool

func (p playing) Busy(_ context.Context, accounts []string) (bool, error) {
	for _, a := range accounts {
		if p[a] {
			return true, nil
		}
	}
	return false, nil
}

// selects records the champion selects it opened, or fails as told.
type selects struct {
	opened [][]SelectSeat
	fail   error
}

func (s *selects) OpenCasual(_ context.Context, _ string, seats []SelectSeat) (string, error) {
	if s.fail != nil {
		return "", s.fail
	}
	s.opened = append(s.opened, seats)
	return "select-1", nil
}

type fixture struct {
	t       *testing.T
	now     time.Time
	parties *party.Service
	store   *MemStore
	selects *selects
	social  socialGraph
	playing playing
	svc     *Service
}

func newFixture(t *testing.T) *fixture {
	f := &fixture{t: t, now: time.Date(2026, 9, 27, 12, 0, 0, 0, time.UTC), store: NewMemStore(), selects: &selects{},
		social: socialGraph{blocks: map[[2]string]bool{}}, playing: playing{}}
	clock := func() time.Time { return f.now }
	rules := party.Rules{MaxSize: partyLimit, Modes: map[string]party.Mode{
		oneVsOne:  {ID: oneVsOne, Enabled: true, HumanPlayersPerTeam: 1, Matchmade: true},
		twoVsTwo:  {ID: twoVsTwo, Enabled: true, HumanPlayersPerTeam: 2, Matchmade: true},
		unmatched: {ID: unmatched, Enabled: true, HumanPlayersPerTeam: 5},
		versusAI:  {ID: versusAI, Enabled: true, HumanPlayersPerTeam: 2, Matchmade: true},
	}}
	f.parties = party.NewService(party.NewMemStore(), f.social, party.Settings{Rules: rules, InviteLifetime: inviteLimit, DefaultPrivacy: party.Private}, clock)
	f.parties.SetActivity(f.playing)
	f.svc = NewService(f.store, f.parties, f.social, f.playing, f.selects, Settings{
		Modes:          []Mode{{ID: oneVsOne, TeamSize: 1}, {ID: twoVsTwo, TeamSize: 2}, {ID: versusAI, TeamSize: 2, VersusAI: true}},
		AcceptDuration: acceptFor,
		SearchLimit:    searchSteps,
	}, clock, slog.New(slog.NewTextHandler(io.Discard, nil)))
	return f
}

// queue puts the leader's party, with the other members in it, in a mode's
// queue, a second after the last one.
func (f *fixture) queue(mode, leader string, others ...string) {
	f.t.Helper()
	f.now = f.now.Add(time.Second)
	if _, err := f.parties.SelectMode(ctx, leader, mode); err != nil {
		f.t.Fatalf("SelectMode: %v", err)
	}
	for _, other := range others {
		invite, err := f.parties.Invite(ctx, leader, other)
		if err != nil {
			f.t.Fatalf("Invite: %v", err)
		}
		if _, err := f.parties.AcceptInvite(ctx, other, invite.ID); err != nil {
			f.t.Fatalf("AcceptInvite: %v", err)
		}
	}
	for _, member := range append([]string{leader}, others...) {
		if _, err := f.parties.SetReady(ctx, member, true); err != nil {
			f.t.Fatalf("SetReady: %v", err)
		}
	}
	if _, err := f.parties.StartQueue(ctx, leader); err != nil {
		f.t.Fatalf("StartQueue: %v", err)
	}
}

func (f *fixture) party(account string) party.Party {
	f.t.Helper()
	p, err := f.parties.Get(ctx, account)
	if err != nil {
		f.t.Fatalf("party of %s: %v", account, err)
	}
	return p
}

func (f *fixture) match() Found {
	f.t.Helper()
	if err := f.svc.MatchOnce(ctx); err != nil {
		f.t.Fatalf("MatchOnce: %v", err)
	}
	found, ok, err := f.svc.Current(ctx, "a")
	if err != nil || !ok {
		f.t.Fatalf("a has no match found: %v", err)
	}
	return found
}

func TestTwoQueuedPlayersAreFoundAMatch(t *testing.T) {
	f := newFixture(t)
	f.queue(oneVsOne, "a")
	f.queue(oneVsOne, "b")
	found := f.match()
	if found.State != Pending || found.Mode != oneVsOne || !found.Deadline.Equal(f.now.Add(acceptFor)) || len(found.Seats) != 2 {
		t.Fatalf("match found: %+v", found)
	}
	if found.Seats[0].Side != match.SideA || found.Seats[1].Side != match.SideB {
		t.Fatalf("one a side: %+v", found.Seats)
	}
	if f.party("a").Status != party.Found || f.party("b").Status != party.Found {
		t.Fatal("both parties are in the match found")
	}
	if err := f.svc.MatchOnce(ctx); err != nil {
		t.Fatalf("MatchOnce: %v", err)
	}
	if pending, _ := f.store.Pending(ctx); len(pending) != 1 {
		t.Fatalf("a found party is not matched again: %d pending", len(pending))
	}
}

func TestModesAndPartiesMatchSeparately(t *testing.T) {
	f := newFixture(t)
	f.queue(oneVsOne, "a")
	f.queue(twoVsTwo, "b")
	if err := f.svc.MatchOnce(ctx); err != nil {
		t.Fatalf("MatchOnce: %v", err)
	}
	if pending, _ := f.store.Pending(ctx); len(pending) != 0 {
		t.Fatal("one player in each mode makes no match")
	}
	// Two duos make a 2v2, each duo on one side.
	f.queue(twoVsTwo, "c", "d")
	if err := f.svc.MatchOnce(ctx); err != nil {
		t.Fatalf("MatchOnce: %v", err)
	}
	if pending, _ := f.store.Pending(ctx); len(pending) != 0 {
		t.Fatal("a solo and a duo are not four")
	}
	f.queue(twoVsTwo, "e", "g")
	if err := f.svc.MatchOnce(ctx); err != nil {
		t.Fatalf("MatchOnce: %v", err)
	}
	found, ok, _ := f.svc.Current(ctx, "c")
	if !ok || len(found.Parties) != 2 || found.Parties[0].Side == found.Parties[1].Side {
		t.Fatalf("the duos face each other: %+v", found)
	}
	if f.party("b").Status != party.Queued {
		t.Fatal("the solo waits")
	}
}

func TestBlockedPlayersAreNeverMatched(t *testing.T) {
	f := newFixture(t)
	f.social.blocks[[2]string{"b", "a"}] = true
	f.queue(oneVsOne, "a")
	f.queue(oneVsOne, "b")
	if err := f.svc.MatchOnce(ctx); err != nil {
		t.Fatalf("MatchOnce: %v", err)
	}
	if pending, _ := f.store.Pending(ctx); len(pending) != 0 {
		t.Fatal("a block holds either way, even at the cost of the wait")
	}
}

func TestABlockPlacedDuringMatchFoundStopsItForEveryone(t *testing.T) {
	f := newFixture(t)
	f.queue(oneVsOne, "a")
	queuedAt := f.party("a").QueuedAt
	f.queue(oneVsOne, "b")
	f.match()
	if _, err := f.svc.Accept(ctx, "a"); err != nil {
		t.Fatalf("Accept: %v", err)
	}
	// b blocks a after the match was found: the two may not share it (§6).
	f.social.blocks[[2]string{"b", "a"}] = true
	found, err := f.svc.Accept(ctx, "b")
	if err != nil || found.State != Abandoned || found.AbandonReason != AbandonNoLongerMatched {
		t.Fatalf("the last acceptance: %+v %v", found, err)
	}
	if len(f.selects.opened) != 0 {
		t.Fatalf("a select was opened for two players who block each other: %+v", f.selects.opened)
	}
	// Nobody is at fault: both return to the queue, in their places.
	if a := f.party("a"); a.Status != party.Queued || !a.QueuedAt.Equal(queuedAt) || f.party("b").Status != party.Queued {
		t.Fatalf("after the block: %+v, b %s", a, f.party("b").Status)
	}
}

func TestAPartyWithAPlayerInAMatchLeavesTheQueue(t *testing.T) {
	f := newFixture(t)
	f.queue(oneVsOne, "a")
	f.queue(oneVsOne, "b")
	f.queue(oneVsOne, "c")
	// a's match started after a queued, in a race the queue's own check missed.
	f.playing["a"] = true
	if err := f.svc.MatchOnce(ctx); err != nil {
		t.Fatalf("MatchOnce: %v", err)
	}
	if p := f.party("a"); p.Status != party.Idle || p.Members[0].Ready {
		t.Fatalf("a's party leaves the queue, Not Ready: %+v", p)
	}
	found, ok, _ := f.svc.Current(ctx, "b")
	if !ok || found.Seats[0].AccountID != "b" || found.Seats[1].AccountID != "c" {
		t.Fatalf("b and c are matched without a: %+v", found)
	}
}

func TestAPlayerInAMatchCannotQueue(t *testing.T) {
	f := newFixture(t)
	f.playing["a"] = true
	if _, err := f.parties.SelectMode(ctx, "a", oneVsOne); err != nil {
		t.Fatalf("SelectMode: %v", err)
	}
	if _, err := f.parties.SetReady(ctx, "a", true); err != nil {
		t.Fatalf("SetReady: %v", err)
	}
	if _, err := f.parties.StartQueue(ctx, "a"); !errors.Is(err, party.ErrMemberBusy) {
		t.Fatalf("queueing from a match: %v", err)
	}
	if f.party("a").Status != party.Idle {
		t.Fatal("the party stays idle")
	}
}

func TestWhenEveryoneAcceptsTheSelectOpens(t *testing.T) {
	f := newFixture(t)
	f.queue(oneVsOne, "a")
	f.queue(oneVsOne, "b")
	f.match()
	found, err := f.svc.Accept(ctx, "a")
	if err != nil || found.State != Pending {
		t.Fatalf("one accepted: %+v %v", found, err)
	}
	if accepted, total := found.Counts(); accepted != 1 || total != 2 {
		t.Fatalf("counts %d/%d", accepted, total)
	}
	if again, err := f.svc.Accept(ctx, "a"); err != nil || again.State != Pending {
		t.Fatalf("accepting twice is harmless: %v", err)
	}
	found, err = f.svc.Accept(ctx, "b")
	if err != nil || found.State != Accepted || found.SelectID != "select-1" {
		t.Fatalf("all accepted: %+v %v", found, err)
	}
	if len(f.selects.opened) != 1 || f.selects.opened[0][0] != (SelectSeat{AccountID: "a", Side: match.SideA}) {
		t.Fatalf("select seats: %+v", f.selects.opened)
	}
	if f.party("a").Status != party.Selecting || f.party("b").Status != party.Selecting {
		t.Fatal("both parties are in champion select")
	}
	if _, ok, _ := f.svc.Current(ctx, "a"); ok {
		t.Fatal("an accepted match found is over")
	}
	if _, err := f.svc.Decline(ctx, "a"); !errors.Is(err, ErrFoundNotFound) {
		t.Fatalf("nothing is left to decline: %v", err)
	}
}

func TestADeclineReturnsTheOthersToTheQueueInTheirPlaces(t *testing.T) {
	f := newFixture(t)
	f.queue(twoVsTwo, "a", "b")
	queuedAt := f.party("a").QueuedAt
	f.queue(twoVsTwo, "c")
	f.queue(twoVsTwo, "d")
	f.match()
	if _, err := f.svc.Accept(ctx, "a"); err != nil {
		t.Fatalf("Accept: %v", err)
	}
	found, err := f.svc.Decline(ctx, "c")
	if err != nil || found.State != Abandoned || found.AbandonReason != AbandonDeclined {
		t.Fatalf("declined: %+v %v", found, err)
	}
	if _, err := f.svc.Accept(ctx, "b"); !errors.Is(err, ErrFoundNotFound) {
		t.Fatalf("an abandoned match takes no answers: %v", err)
	}
	decliner := f.party("c")
	if decliner.Status != party.Idle || decliner.Members[0].Ready {
		t.Fatalf("the decliner leaves the queue, Not Ready: %+v", decliner)
	}
	for _, other := range []string{"a", "d"} {
		p := f.party(other)
		if p.Status != party.Queued || !p.Members[0].Ready {
			t.Fatalf("%s returns to the queue: %+v", other, p)
		}
	}
	if !f.party("a").QueuedAt.Equal(queuedAt) {
		t.Fatal("and keeps its place")
	}
	if _, err := f.svc.Accept(ctx, "c"); !errors.Is(err, ErrFoundNotFound) {
		t.Fatalf("Accept after the end: %v", err)
	}
}

func TestTheTimerSendsHomeOnlyThoseWhoDidNotAccept(t *testing.T) {
	f := newFixture(t)
	f.queue(oneVsOne, "a")
	f.queue(oneVsOne, "b")
	found := f.match()
	if _, err := f.svc.Accept(ctx, "a"); err != nil {
		t.Fatalf("Accept: %v", err)
	}
	f.now = found.Deadline
	if _, err := f.svc.Accept(ctx, "b"); !errors.Is(err, ErrExpired) {
		t.Fatalf("too late: %v", err)
	}
	if err := f.svc.Tick(ctx); err != nil {
		t.Fatalf("Tick: %v", err)
	}
	ended, _ := f.store.ByID(found.ID)
	if ended.State != Abandoned || ended.AbandonReason != AbandonTimedOut {
		t.Fatalf("timed out: %+v", ended)
	}
	if f.party("a").Status != party.Queued || f.party("b").Status != party.Idle {
		t.Fatalf("a requeues, b goes home: %s %s", f.party("a").Status, f.party("b").Status)
	}
}

func TestAPartyThatBreaksUpAbandonsItsMatch(t *testing.T) {
	f := newFixture(t)
	f.queue(twoVsTwo, "a", "b")
	f.queue(twoVsTwo, "c", "d")
	f.match()
	if err := f.parties.Leave(ctx, "b"); err != nil {
		t.Fatalf("Leave: %v", err)
	}
	if err := f.svc.Tick(ctx); err != nil {
		t.Fatalf("Tick: %v", err)
	}
	if f.party("a").Status != party.Idle || f.party("c").Status != party.Queued {
		t.Fatalf("the broken party is out, the other requeues: %s %s", f.party("a").Status, f.party("c").Status)
	}
	if _, ok, _ := f.svc.Current(ctx, "c"); ok {
		t.Fatal("the match found is over")
	}
}

func TestASelectThatCannotOpenSendsEveryoneHome(t *testing.T) {
	f := newFixture(t)
	f.selects.fail = errors.New("no select")
	f.queue(oneVsOne, "a")
	f.queue(oneVsOne, "b")
	f.match()
	f.svc.Accept(ctx, "a") //nolint:errcheck // checked below
	found, err := f.svc.Accept(ctx, "b")
	if err != nil || found.State != Abandoned || found.AbandonReason != AbandonSelectFailed {
		t.Fatalf("select failed: %+v %v", found, err)
	}
	if f.party("a").Status != party.Idle || f.party("b").Status != party.Idle {
		t.Fatal("both parties are idle")
	}
}

func TestAnEndedSelectSettlesItsParties(t *testing.T) {
	f := newFixture(t)
	f.queue(oneVsOne, "a")
	queuedAt := f.party("a").QueuedAt
	f.queue(oneVsOne, "b")
	f.match()
	f.svc.Accept(ctx, "a") //nolint:errcheck // checked by the states below
	f.svc.Accept(ctx, "b") //nolint:errcheck
	// b dodged: b's party leaves the queue, a's returns to it in its place.
	if err := f.svc.SelectEnded(ctx, []string{"a", "b"}, []string{"b"}, false); err != nil {
		t.Fatalf("SelectEnded: %v", err)
	}
	if a := f.party("a"); a.Status != party.Queued || !a.QueuedAt.Equal(queuedAt) || f.party("b").Status != party.Idle {
		t.Fatalf("after a dodge: %+v, b %s", a, f.party("b").Status)
	}

	f.queue(oneVsOne, "b")
	found := f.match()
	f.svc.Accept(ctx, "a") //nolint:errcheck
	f.svc.Accept(ctx, "b") //nolint:errcheck
	// The match started: everyone is free again, Not Ready (UX-15).
	if err := f.svc.SelectEnded(ctx, []string{"a", "b"}, nil, true); err != nil {
		t.Fatalf("SelectEnded: %v", err)
	}
	for _, account := range []string{"a", "b"} {
		if p := f.party(account); p.Status != party.Idle || p.Members[0].Ready {
			t.Fatalf("%s after match %s started: %+v", account, found.ID, p)
		}
	}
}

func TestOnlyMatchmadeModesQueue(t *testing.T) {
	f := newFixture(t)
	if _, err := f.parties.SelectMode(ctx, "a", unmatched); err != nil {
		t.Fatalf("SelectMode: %v", err)
	}
	if _, err := f.parties.SetReady(ctx, "a", true); err != nil {
		t.Fatalf("SetReady: %v", err)
	}
	if _, err := f.parties.StartQueue(ctx, "a"); !errors.Is(err, party.ErrModeUnavailable) {
		t.Fatalf("a mode without a matchmaker cannot queue: %v", err)
	}
}

func TestAgainstAITheHumansFillOneSideAndOnlyTheyAccept(t *testing.T) {
	f := newFixture(t)
	f.queue(versusAI, "a")
	if err := f.svc.MatchOnce(ctx); err != nil {
		t.Fatalf("MatchOnce: %v", err)
	}
	if _, ok, _ := f.svc.Current(ctx, "a"); ok {
		t.Fatal("one human of two waits: matchmaking never fills a human side with AI")
	}
	f.queue(versusAI, "b")
	found := f.match()
	if len(found.Seats) != 2 || found.Seats[0].Side != match.SideA || found.Seats[1].Side != match.SideA {
		t.Fatalf("both humans on side A: %+v", found.Seats)
	}
	if _, err := f.svc.Accept(ctx, "a"); err != nil {
		t.Fatalf("Accept a: %v", err)
	}
	found, err := f.svc.Accept(ctx, "b")
	if err != nil || found.State != Accepted || len(f.selects.opened) != 1 || len(f.selects.opened[0]) != 2 {
		t.Fatalf("both humans accepted, and the select opened for them alone: %+v %v %+v", found, err, f.selects.opened)
	}
}
