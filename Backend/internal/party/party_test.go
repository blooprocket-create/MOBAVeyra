package party

import (
	"errors"
	"testing"
	"time"
)

var testRules = Rules{
	MaxSize: 5,
	Modes: map[string]Mode{
		"casual": {ID: "casual", Enabled: true, HumanPlayersPerTeam: 5},
		"tiny":   {ID: "tiny", Enabled: true, HumanPlayersPerTeam: 2},
		"ranked": {ID: "ranked", Enabled: false, HumanPlayersPerTeam: 5},
	},
}

var t0 = time.Date(2026, 9, 24, 12, 0, 0, 0, time.UTC)

func newParty(ids ...string) *Party {
	p := &Party{ID: "p", LeaderID: ids[0], Privacy: Private, Status: Idle}
	for i, id := range ids {
		p.Members = append(p.Members, Member{AccountID: id, JoinedAt: t0.Add(time.Duration(i) * time.Minute)})
	}
	return p
}

func readyAll(t *testing.T, p *Party) {
	t.Helper()
	for _, m := range p.Members {
		if err := p.SetReady(m.AccountID, true); err != nil {
			t.Fatal(err)
		}
	}
}

func mustErr(t *testing.T, err, want error) {
	t.Helper()
	if !errors.Is(err, want) {
		t.Fatalf("want %v, got %v", want, err)
	}
}

func TestAddResetsReadyAndEnforcesCapacity(t *testing.T) {
	p := newParty("a", "b", "c", "d")
	readyAll(t, p)
	if err := p.Add("e", testRules, t0); err != nil {
		t.Fatal(err)
	}
	for _, m := range p.Members {
		if m.Ready {
			t.Fatalf("adding a member must reset everyone's Ready: %+v", m)
		}
	}
	mustErr(t, p.Add("f", testRules, t0), ErrPartyFull)
}

func TestModeChangeResetsReadyAndRejectsDisabledModes(t *testing.T) {
	p := newParty("a", "b")
	mustErr(t, p.SetMode("b", "casual", testRules), ErrNotLeader)
	if err := p.SetMode("a", "casual", testRules); err != nil {
		t.Fatal(err)
	}
	readyAll(t, p)
	if err := p.SetMode("a", "tiny", testRules); err != nil {
		t.Fatal(err)
	}
	if p.Members[0].Ready || p.Members[1].Ready {
		t.Fatal("changing mode must reset Ready")
	}
	mustErr(t, p.SetMode("a", "ranked", testRules), ErrUnknownMode)
	mustErr(t, p.SetMode("a", "nope", testRules), ErrUnknownMode)
}

func TestFindMatchRequiresLeaderModeReadyAndSize(t *testing.T) {
	p := newParty("a", "b", "c")
	mustErr(t, p.StartQueue("a", testRules), ErrNoMode)
	if err := p.SetMode("a", "tiny", testRules); err != nil {
		t.Fatal(err)
	}
	mustErr(t, p.StartQueue("a", testRules), ErrTooManyForMode)
	if err := p.SetMode("a", "casual", testRules); err != nil {
		t.Fatal(err)
	}
	mustErr(t, p.StartQueue("a", testRules), ErrNotAllReady)
	readyAll(t, p)
	mustErr(t, p.StartQueue("b", testRules), ErrNotLeader)
	if err := p.StartQueue("a", testRules); err != nil {
		t.Fatal(err)
	}
	if p.Status != Queued {
		t.Fatal("party should be queued")
	}
}

func TestQueueLocksComposition(t *testing.T) {
	p := newParty("a", "b")
	_ = p.SetMode("a", "casual", testRules)
	readyAll(t, p)
	if err := p.StartQueue("a", testRules); err != nil {
		t.Fatal(err)
	}
	mustErr(t, p.Add("c", testRules, t0), ErrPartyLocked)
	mustErr(t, p.SetReady("b", false), ErrPartyLocked)
	mustErr(t, p.SetMode("a", "tiny", testRules), ErrPartyLocked)
	mustErr(t, p.TransferLeader("a", "b"), ErrPartyLocked)
}

func TestDepartureDuringQueueCancelsAndResetsReady(t *testing.T) {
	p := newParty("a", "b", "c")
	_ = p.SetMode("a", "casual", testRules)
	readyAll(t, p)
	_ = p.StartQueue("a", testRules)
	if _, err := p.Remove("c"); err != nil {
		t.Fatal(err)
	}
	if p.Status != Idle {
		t.Fatal("a departure during queue must cancel the whole party's queue")
	}
	for _, m := range p.Members {
		if m.Ready {
			t.Fatal("remaining members must Ready again")
		}
	}
}

func TestLeaderDepartureTransfersToLongestStanding(t *testing.T) {
	p := newParty("a", "b", "c")
	// c joined after b, so b has stood longest once a leaves.
	if _, err := p.Remove("a"); err != nil {
		t.Fatal(err)
	}
	if p.LeaderID != "b" {
		t.Fatalf("want leader b, got %s", p.LeaderID)
	}
	empty, err := p.Remove("b")
	if err != nil || empty {
		t.Fatalf("remove b: empty=%v err=%v", empty, err)
	}
	empty, _ = p.Remove("c")
	if !empty {
		t.Fatal("removing the last member must report an empty party")
	}
}

func TestKickAndTransfer(t *testing.T) {
	p := newParty("a", "b", "c")
	mustErr(t, p.Kick("b", "c"), ErrNotLeader)
	mustErr(t, p.Kick("a", "a"), ErrSelf)
	if err := p.Kick("a", "c"); err != nil {
		t.Fatal(err)
	}
	mustErr(t, p.TransferLeader("a", "zzz"), ErrNotMember)
	readyAll(t, p)
	if err := p.TransferLeader("a", "b"); err != nil {
		t.Fatal(err)
	}
	if p.LeaderID != "b" || !p.Members[0].Ready {
		t.Fatal("transfer must change leader and preserve Ready")
	}
}

func TestLeaderCancelResetsReady(t *testing.T) {
	p := newParty("a", "b")
	_ = p.SetMode("a", "casual", testRules)
	readyAll(t, p)
	_ = p.StartQueue("a", testRules)
	mustErr(t, p.CancelQueue("b"), ErrNotLeader)
	if err := p.CancelQueue("a"); err != nil {
		t.Fatal(err)
	}
	if p.Status != Idle || p.Members[0].Ready {
		t.Fatal("cancel must unlock and reset Ready")
	}
}

func TestPrivacy(t *testing.T) {
	p := newParty("a", "b")
	mustErr(t, p.SetPrivacy("b", Public), ErrNotLeader)
	mustErr(t, p.SetPrivacy("a", "open"), ErrInvalidPrivacy)
	if err := p.SetPrivacy("a", Public); err != nil || p.Privacy != Public {
		t.Fatalf("SetPrivacy: %v", err)
	}
}
