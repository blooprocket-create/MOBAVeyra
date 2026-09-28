package selection

import (
	"errors"
	"reflect"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// casual opens a Casual Select for acc-1 on side A and acc-2 on side B, each
// owning a starter.
func (f *fixture) casual(t *testing.T, starter1, starter2 string) Session {
	t.Helper()
	f.onboard(t, "acc-1", starter1)
	f.onboard(t, "acc-2", starter2)
	id, err := f.svc.OpenCasual(ctx, casualMode, []CasualSeat{{AccountID: "acc-1", Side: match.SideA}, {AccountID: "acc-2", Side: match.SideB}})
	if err != nil {
		t.Fatalf("OpenCasual: %v", err)
	}
	s, err := f.svc.ForParticipant(ctx, "acc-1", id)
	if err != nil {
		t.Fatalf("ForParticipant: %v", err)
	}
	return s
}

func TestCasualSelectSeatsTheMatchedPlayers(t *testing.T) {
	f := newFixture(t)
	s := f.casual(t, "cairn", "oriel")
	if s.Kind != KindCasual || s.State != Picking || s.Mode != casualMode || !s.Deadline.Equal(t0.Add(fixtureCasual.PickDuration)) || len(s.Seats) != 2 ||
		s.Seats[1].Side != match.SideB || s.Seats[1].DisplayName != "Player acc-2" {
		t.Fatalf("casual select: %+v", s)
	}
	if _, err := f.svc.OpenCasual(ctx, casualMode, []CasualSeat{{AccountID: "acc-2", Side: match.SideA}}); !errors.Is(err, ErrBusy) {
		t.Fatalf("a player already selecting: %v", err)
	}
}

func TestEveryoneLockingStartsTheMatchAndLetsThePartiesGo(t *testing.T) {
	f := newFixture(t)
	s := f.casual(t, "cairn", "oriel")
	if _, err := f.svc.Lock(ctx, "acc-1", "cairn"); err != nil {
		t.Fatalf("Lock acc-1: %v", err)
	}
	locked, err := f.svc.Lock(ctx, "acc-2", "oriel")
	if err != nil || locked.State != Started {
		t.Fatalf("Lock acc-2: %+v %v", locked, err)
	}
	m, _, _ := f.matches.BySelect(ctx, s.ID)
	if m.Rules != match.RulesStandard || m.Mode != casualMode || m.Participants[1].VanguardID != "oriel" || m.Participants[1].Side != match.SideB {
		t.Fatalf("the match: %+v", m)
	}
	if len(f.ends.calls) != 1 || !f.ends.calls[0].started || len(f.ends.calls[0].accounts) != 2 {
		t.Fatalf("matchmaking lets every party go: %+v", f.ends.calls)
	}
}

func TestPicksAreUniqueAcrossTeamsAndHoversReserveNothing(t *testing.T) {
	f := newFixture(t)
	s := f.casual(t, "cairn", "cairn")
	// Hovers are private and hold nothing: both may consider Cairn.
	if _, err := f.svc.Hover(ctx, "acc-2", "cairn"); err != nil {
		t.Fatalf("Hover acc-2: %v", err)
	}
	if _, err := f.svc.Lock(ctx, "acc-1", "cairn"); err != nil {
		t.Fatalf("the first lock wins: %v", err)
	}
	if _, err := f.svc.Hover(ctx, "acc-2", "cairn"); !errors.Is(err, ErrTaken) {
		t.Fatalf("a locked Vanguard is taken: %v", err)
	}
	if _, err := f.svc.Lock(ctx, "acc-2", "cairn"); !errors.Is(err, ErrTaken) {
		t.Fatalf("and cannot be locked again: %v", err)
	}
	// At the end of the timer acc-2's hover is taken, so it has nothing to lock.
	f.now = s.Deadline
	if err := f.svc.Tick(ctx); err != nil {
		t.Fatalf("Tick: %v", err)
	}
	ended, _ := f.svc.ForParticipant(ctx, "acc-1", s.ID)
	if ended.State != Cancelled || ended.CancelReason != CancelTimedOut {
		t.Fatalf("timed out: %+v", ended)
	}
	if len(f.ends.calls) != 1 || f.ends.calls[0].started || !reflect.DeepEqual(f.ends.calls[0].leaving, []string{"acc-2"}) {
		t.Fatalf("only acc-2's party leaves the queue: %+v", f.ends.calls)
	}
}

func TestLeavingCancelsTheSelectAsADodge(t *testing.T) {
	f := newFixture(t)
	s := f.casual(t, "cairn", "oriel")
	left, err := f.svc.Leave(ctx, "acc-2")
	if err != nil || left.State != Cancelled || left.CancelReason != CancelLeft || left.LeftBy != "acc-2" {
		t.Fatalf("Leave: %+v %v", left, err)
	}
	if len(f.ends.calls) != 1 || !reflect.DeepEqual(f.ends.calls[0].leaving, []string{"acc-2"}) {
		t.Fatalf("the leaver's party leaves the queue; the other returns to it: %+v", f.ends.calls)
	}
	if _, ok, _ := f.svc.Current(ctx, "acc-1"); ok {
		t.Fatalf("select %s is over for everyone", s.ID)
	}

	f.onboard(t, "acc-3", "qazharr")
	f.practice(t, "acc-3")
	if _, err := f.svc.Leave(ctx, "acc-3"); !errors.Is(err, ErrCannotLeave) {
		t.Fatalf("practice cannot be left: %v", err)
	}
}

func TestAPlayerWhoStopsPollingCancelsTheSelect(t *testing.T) {
	f := newFixture(t)
	s := f.casual(t, "cairn", "oriel")
	f.now = t0.Add(fixtureCasual.PresenceTimeout / 2)
	if _, ok, err := f.svc.Poll(ctx, "acc-1"); err != nil || !ok {
		t.Fatalf("Poll: %v", err)
	}
	f.now = t0.Add(fixtureCasual.PresenceTimeout + time.Second)
	if err := f.svc.Tick(ctx); err != nil {
		t.Fatalf("Tick: %v", err)
	}
	ended, _ := f.svc.ForParticipant(ctx, "acc-1", s.ID)
	if ended.State != Cancelled || ended.CancelReason != CancelPresenceLost {
		t.Fatalf("a disconnect cancels it: %+v", ended)
	}
	if len(f.ends.calls) != 1 || !reflect.DeepEqual(f.ends.calls[0].leaving, []string{"acc-2"}) {
		t.Fatalf("the silent player's party leaves the queue: %+v", f.ends.calls)
	}
}
