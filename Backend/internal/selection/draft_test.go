package selection

import (
	"context"
	"errors"
	"fmt"
	"reflect"
	"slices"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// draft opens a Draft Pick select for acc-1 on side A and acc-2 on side B,
// each owning a starter; the fixture's rotation holds every released Vanguard.
func (f *fixture) draft(t *testing.T) Session {
	t.Helper()
	f.onboard(t, "acc-1", "cairn")
	f.onboard(t, "acc-2", "oriel")
	id, err := f.svc.OpenDraft(ctx, draftMode, []CasualSeat{{AccountID: "acc-1", Side: match.SideA}, {AccountID: "acc-2", Side: match.SideB}})
	if err != nil {
		t.Fatalf("OpenDraft: %v", err)
	}
	s, err := f.svc.ForParticipant(ctx, "acc-1", id)
	if err != nil {
		t.Fatalf("ForParticipant: %v", err)
	}
	return s
}

// tickAt moves the clock, has the players poll as their clients do, and ticks
// the selects.
func (f *fixture) tickAt(t *testing.T, when time.Time, polling ...string) {
	t.Helper()
	f.now = when
	for _, id := range polling {
		if _, _, err := f.svc.Poll(ctx, id); err != nil {
			t.Fatalf("Poll %s: %v", id, err)
		}
	}
	if err := f.svc.Tick(ctx); err != nil {
		t.Fatalf("Tick: %v", err)
	}
}

// mayPlay lets accounts pick Vanguards beyond the starter they own, as the
// week's rotation would; the fixture's rotation is empty.
type mayPlay struct {
	Accounts
	extra map[string][]string
}

func (m mayPlay) MayPick(ctx context.Context, accountID, vanguardID string) (bool, error) {
	if slices.Contains(m.extra[accountID], vanguardID) {
		return true, nil
	}
	return m.Accounts.MayPick(ctx, accountID, vanguardID)
}

// mayAlsoPlay grants each account the Vanguards listed for it.
func (f *fixture) mayAlsoPlay(extra map[string][]string) {
	f.svc.accounts = mayPlay{Accounts: f.accounts, extra: extra}
}

// savedLoadouts gives accounts saved Flux Spell loadouts for Vanguards, as the
// matches they played would (Pre-Game Client UX Bible 37).
type savedLoadouts struct {
	Matches
	saved map[[2]string][2]string
}

func (s savedLoadouts) LastFluxSpells(ctx context.Context, accountID, vanguardID string) ([2]string, error) {
	if spells, ok := s.saved[[2]string{accountID, vanguardID}]; ok {
		return spells, nil
	}
	return s.Matches.LastFluxSpells(ctx, accountID, vanguardID)
}

func (f *fixture) selectOf(t *testing.T, accountID, id string) Session {
	t.Helper()
	s, err := f.svc.ForParticipant(ctx, accountID, id)
	if err != nil {
		t.Fatalf("ForParticipant: %v", err)
	}
	return s
}

func TestADraftBansThenPicksInTurns(t *testing.T) {
	f := newFixture(t)
	f.mayAlsoPlay(map[string][]string{"acc-1": {"qazharr"}, "acc-2": {"qazharr"}})
	s := f.draft(t)
	if s.Kind != KindDraft || s.Phase != PhaseBanning || s.Turn != 0 || !reflect.DeepEqual(s.Acting(), []string{"acc-1"}) ||
		!s.Deadline.Equal(t0.Add(fixtureDraft.Timing.Ban)) || s.PhaseLength() != fixtureDraft.Timing.Ban {
		t.Fatalf("the draft opens on side A's ban: %+v", s)
	}

	// Out of turn nobody bans or locks, though a pick may be hovered as intent.
	if _, err := f.svc.Ban(ctx, "acc-2", "bryn"); !errors.Is(err, ErrNotYourTurn) {
		t.Fatalf("side B bans out of turn: %v", err)
	}
	if _, err := f.svc.HoverBan(ctx, "acc-2", "bryn"); !errors.Is(err, ErrNotYourTurn) {
		t.Fatalf("side B hovers a ban out of turn: %v", err)
	}
	if _, err := f.svc.Lock(ctx, "acc-1", "cairn"); !errors.Is(err, ErrNotYourTurn) {
		t.Fatalf("a pick in a ban turn: %v", err)
	}
	if _, err := f.svc.Hover(ctx, "acc-2", "oriel"); err != nil {
		t.Fatalf("a pick intent: %v", err)
	}
	if _, err := f.svc.Ban(ctx, "acc-1", "nobody"); !errors.Is(err, ErrNotAvailable) {
		t.Fatalf("an unreleased Vanguard: %v", err)
	}

	// A ban may name any released Vanguard, owned or not.
	if hovered, err := f.svc.HoverBan(ctx, "acc-1", "bryn"); err != nil || hovered.Seats[0].BanHover != "bryn" {
		t.Fatalf("HoverBan: %+v %v", hovered, err)
	}
	banned, err := f.svc.Ban(ctx, "acc-1", "bryn")
	if err != nil || !reflect.DeepEqual(banned.Bans, []Ban{{Side: match.SideA, AccountID: "acc-1", VanguardID: "bryn"}}) ||
		banned.Seats[0].BanHover != "" || banned.Turn != 1 || !reflect.DeepEqual(banned.Acting(), []string{"acc-2"}) {
		t.Fatalf("side A's ban: %+v %v", banned, err)
	}
	if _, err := f.svc.Ban(ctx, "acc-2", "bryn"); !errors.Is(err, ErrTaken) {
		t.Fatalf("a Vanguard banned twice: %v", err)
	}
	f.now = t0.Add(5 * time.Second)
	picking, err := f.svc.Ban(ctx, "acc-2", "cairn")
	if err != nil || picking.Phase != PhasePicking || picking.Turn != 2 || !picking.Deadline.Equal(f.now.Add(fixturePick)) ||
		picking.PhaseLength() != fixturePick || !reflect.DeepEqual(picking.Acting(), []string{"acc-1"}) {
		t.Fatalf("the picks begin: %+v %v", picking, err)
	}

	// A ban takes a Vanguard from both teams; a pick waits for its turn.
	if _, err := f.svc.Lock(ctx, "acc-1", "cairn"); !errors.Is(err, ErrTaken) {
		t.Fatalf("a banned Vanguard: %v", err)
	}
	if _, err := f.svc.Lock(ctx, "acc-2", "oriel"); !errors.Is(err, ErrNotYourTurn) {
		t.Fatalf("a pick out of turn: %v", err)
	}
	if _, err := f.svc.Lock(ctx, "acc-1", "qazharr"); err != nil {
		t.Fatalf("side A's pick: %v", err)
	}
	if _, err := f.svc.Lock(ctx, "acc-2", "qazharr"); !errors.Is(err, ErrTaken) {
		t.Fatalf("a Vanguard picked across the table: %v", err)
	}
	final, err := f.svc.Lock(ctx, "acc-2", "oriel")
	if err != nil || final.State != Picking || final.Phase != PhaseFinal || !final.Deadline.Equal(f.now.Add(fixtureDraft.Timing.Final)) ||
		final.Acting() != nil {
		t.Fatalf("the final window: %+v %v", final, err)
	}

	// The match starts when the final window ends.
	f.tickAt(t, final.Deadline.Add(-time.Second), "acc-1", "acc-2")
	if waiting := f.selectOf(t, "acc-1", s.ID); waiting.State != Picking {
		t.Fatalf("before the window ends: %+v", waiting)
	}
	f.tickAt(t, final.Deadline, "acc-1", "acc-2")
	m, _, _ := f.matches.BySelect(ctx, s.ID)
	if started := f.selectOf(t, "acc-1", s.ID); started.State != Started || m.Mode != draftMode || m.Rules != match.RulesStandard ||
		m.Participants[0].VanguardID != "qazharr" || m.Participants[1].VanguardID != "oriel" {
		t.Fatalf("the match: %+v %+v", started, m)
	}
	if len(f.ends.calls) != 1 || !f.ends.calls[0].started {
		t.Fatalf("matchmaking lets every party go: %+v", f.ends.calls)
	}
}

func TestADraftTurnEndsWhenItsTimeRunsOut(t *testing.T) {
	f := newFixture(t)
	s := f.draft(t)
	// A banner who banned nothing bans their hover...
	if _, err := f.svc.HoverBan(ctx, "acc-1", "bryn"); err != nil {
		t.Fatalf("HoverBan: %v", err)
	}
	f.tickAt(t, s.Deadline, "acc-1", "acc-2")
	b := f.selectOf(t, "acc-1", s.ID)
	if len(b.Bans) != 1 || b.Bans[0].VanguardID != "bryn" || b.Turn != 1 || b.Phase != PhaseBanning || !b.Deadline.Equal(s.Deadline.Add(fixtureDraft.Timing.Ban)) {
		t.Fatalf("the hover is banned: %+v", b)
	}
	// ...or nothing.
	f.tickAt(t, b.Deadline, "acc-1", "acc-2")
	p := f.selectOf(t, "acc-1", s.ID)
	if len(p.Bans) != 1 || p.Phase != PhasePicking || p.Turn != 2 {
		t.Fatalf("no ban: %+v", p)
	}
	// A picker who picked nothing locks their hover...
	if _, err := f.svc.Hover(ctx, "acc-1", "cairn"); err != nil {
		t.Fatalf("Hover: %v", err)
	}
	f.tickAt(t, p.Deadline, "acc-1", "acc-2")
	q := f.selectOf(t, "acc-1", s.ID)
	if q.Seats[0].Locked != "cairn" || q.Turn != 3 || !reflect.DeepEqual(q.Acting(), []string{"acc-2"}) {
		t.Fatalf("the hover is locked: %+v", q)
	}
	// ...or the select is cancelled, and only they leave the queue.
	f.tickAt(t, q.Deadline, "acc-1", "acc-2")
	if ended := f.selectOf(t, "acc-1", s.ID); ended.State != Cancelled || ended.CancelReason != CancelTimedOut {
		t.Fatalf("timed out: %+v", ended)
	}
	if len(f.ends.calls) != 1 || f.ends.calls[0].started || !reflect.DeepEqual(f.ends.calls[0].leaving, []string{"acc-2"}) {
		t.Fatalf("only the player who owed a pick leaves the queue: %+v", f.ends.calls)
	}
}

func TestATickWorkingFromAnOldReadLeavesANewTurnAlone(t *testing.T) {
	f := newFixture(t)
	s := f.draft(t)
	// The ticker lists the select while side A's ban turn is running out...
	listed, err := f.store.Active(ctx)
	if err != nil || len(listed) != 1 {
		t.Fatalf("Active: %+v %v", listed, err)
	}
	// ...side A bans just before its deadline, which begins side B's turn...
	f.now = s.Deadline.Add(-time.Millisecond)
	next, err := f.svc.Ban(ctx, "acc-1", "bryn")
	if err != nil || next.Turn != 1 {
		t.Fatalf("Ban: %+v %v", next, err)
	}
	// ...and the tick, working from its old read once the old deadline passed, ends nothing.
	f.now = s.Deadline
	if err := f.svc.tickOne(ctx, listed[0]); err != nil {
		t.Fatalf("tickOne: %v", err)
	}
	after := f.selectOf(t, "acc-2", s.ID)
	if after.State != Picking || after.Turn != 1 || len(after.Bans) != 1 || !after.Deadline.Equal(next.Deadline) {
		t.Fatalf("side B's turn keeps its whole timer: %+v", after)
	}
}

func TestADraftPlayerWhoStopsPollingCancelsIt(t *testing.T) {
	f := newFixture(t)
	s := f.draft(t)
	f.tickAt(t, t0.Add(fixtureDraft.PresenceTimeout+time.Second), "acc-1")
	if ended := f.selectOf(t, "acc-1", s.ID); ended.State != Cancelled || ended.CancelReason != CancelPresenceLost {
		t.Fatalf("a disconnect cancels the draft: %+v", ended)
	}
	if len(f.ends.calls) != 1 || !reflect.DeepEqual(f.ends.calls[0].leaving, []string{"acc-2"}) {
		t.Fatalf("the silent player's party leaves the queue: %+v", f.ends.calls)
	}
}

// A side with fewer seats than a turn's count acts again in seat order, and a
// pick turn with nobody left to lock is skipped (ADR-041 §1), so the bible's
// turns run a draft of any size.
func TestDraftTurnsGoRoundTheSidesSeats(t *testing.T) {
	bible := Timing{Turns: []Turn{
		{Ban: true, Side: match.SideA, Count: 1}, {Ban: true, Side: match.SideB, Count: 2}, {Ban: true, Side: match.SideA, Count: 2}, {Ban: true, Side: match.SideB, Count: 1},
		{Side: match.SideA, Count: 1}, {Side: match.SideB, Count: 2}, {Side: match.SideA, Count: 2}, {Side: match.SideB, Count: 2}, {Side: match.SideA, Count: 2},
		{Side: match.SideB, Count: 1},
	}, Ban: time.Second, Pick: time.Second}
	s := Session{Kind: KindDraft, State: Picking, Timing: bible,
		Seats: []Seat{{AccountID: "a1", Side: match.SideA}, {AccountID: "a2", Side: match.SideA}, {AccountID: "b1", Side: match.SideB}}}
	s.beginTurn(t0)
	for i, acting := range [][]string{{"a1"}, {"b1", "b1"}, {"a2", "a1"}, {"b1"}} {
		if s.Phase != PhaseBanning || !reflect.DeepEqual(s.Acting(), acting) {
			t.Fatalf("ban turn %d: %s %v, want %v", i, s.Phase, s.Acting(), acting)
		}
		for j, id := range acting {
			if err := s.LockBan(id, fmt.Sprintf("ban_%d_%d", i, j), t0); err != nil {
				t.Fatalf("ban turn %d, %s: %v", i, id, err)
			}
		}
	}
	for i, acting := range [][]string{{"a1"}, {"b1"}, {"a2"}} {
		if s.Phase != PhasePicking || !reflect.DeepEqual(s.Acting(), acting) {
			t.Fatalf("pick %d: %s %v, want %v", i, s.Phase, s.Acting(), acting)
		}
		if err := s.Lock(acting[0], fmt.Sprintf("pick_%d", i), t0); err != nil {
			t.Fatalf("pick %d: %v", i, err)
		}
	}
	if s.Phase != PhaseFinal || s.Acting() != nil || len(s.Bans) != 6 || !s.AllLocked() {
		t.Fatalf("every seat picked: %+v", s)
	}
}

// teammates opens a Casual Select of acc-1, acc-3 and acc-4 on side A, seats 0
// to 2, against acc-2 on side B, seat 3.
func (f *fixture) teammates(t *testing.T) Session {
	t.Helper()
	for _, id := range []string{"acc-1", "acc-2", "acc-3", "acc-4"} {
		f.onboard(t, id, "cairn")
	}
	id, err := f.svc.OpenCasual(ctx, teamMode, []CasualSeat{{AccountID: "acc-1", Side: match.SideA}, {AccountID: "acc-3", Side: match.SideA},
		{AccountID: "acc-4", Side: match.SideA}, {AccountID: "acc-2", Side: match.SideB}})
	if err != nil {
		t.Fatalf("OpenCasual: %v", err)
	}
	return f.selectOf(t, "acc-1", id)
}

func TestLockedTeammatesTradeTheirVanguards(t *testing.T) {
	f := newFixture(t)
	f.mayAlsoPlay(map[string][]string{"acc-1": {"qazharr"}, "acc-3": {"qazharr"}, "acc-4": {"bryn"}, "acc-2": {"oriel"}})
	// acc-3 once took Scorch into a match with Cairn; acc-1 never played Qazharr.
	f.svc.matches = savedLoadouts{Matches: f.svc.matches, saved: map[[2]string][2]string{{"acc-3", "cairn"}: {"scorch", ""}}}
	s := f.teammates(t)
	if _, err := f.svc.OfferTrade(ctx, "acc-1", 1); !errors.Is(err, ErrCannotTrade) {
		t.Fatalf("before either locks: %v", err)
	}
	for _, lock := range [][2]string{{"acc-1", "cairn"}, {"acc-3", "qazharr"}, {"acc-4", "bryn"}} {
		if _, err := f.svc.Lock(ctx, lock[0], lock[1]); err != nil {
			t.Fatalf("Lock %s: %v", lock[0], err)
		}
	}
	if _, err := f.svc.SetFluxSpells(ctx, "acc-1", [2]string{"blink", "mend"}); err != nil {
		t.Fatalf("SetFluxSpells: %v", err)
	}
	for _, seat := range []int{0, 3, 9, -1} {
		if _, err := f.svc.OfferTrade(ctx, "acc-1", seat); !errors.Is(err, ErrCannotTrade) {
			t.Fatalf("an offer to seat %d: %v", seat, err)
		}
	}

	// One offer at a time: a new one replaces the last.
	if _, err := f.svc.OfferTrade(ctx, "acc-1", 1); err != nil {
		t.Fatalf("OfferTrade: %v", err)
	}
	offered, err := f.svc.OfferTrade(ctx, "acc-1", 2)
	if err != nil || !reflect.DeepEqual(offered.Trades, []Trade{{From: "acc-1", To: "acc-4"}}) {
		t.Fatalf("a second offer: %+v %v", offered.Trades, err)
	}
	if declined, err := f.svc.DeclineTrade(ctx, "acc-4", 0); err != nil || len(declined.Trades) != 0 {
		t.Fatalf("DeclineTrade: %+v %v", declined.Trades, err)
	}
	if _, err := f.svc.AcceptTrade(ctx, "acc-4", 0); !errors.Is(err, ErrCannotTrade) {
		t.Fatalf("a declined offer: %v", err)
	}

	// A trade swaps the two Vanguards, each player taking their new Vanguard's
	// saved loadout (UX-37), and every other offer to or from either player lapses.
	if _, err := f.svc.OfferTrade(ctx, "acc-1", 1); err != nil {
		t.Fatalf("OfferTrade: %v", err)
	}
	if _, err := f.svc.OfferTrade(ctx, "acc-4", 0); err != nil {
		t.Fatalf("OfferTrade: %v", err)
	}
	traded, err := f.svc.AcceptTrade(ctx, "acc-3", 0)
	if err != nil || traded.Seats[0].Locked != "qazharr" || traded.Seats[1].Locked != "cairn" || traded.Seats[0].Hover != "qazharr" ||
		traded.Seats[0].FluxSpells != ([2]string{}) || traded.Seats[1].FluxSpells != [2]string{"scorch", ""} || traded.Seats[0].FluxSpellsEdited ||
		len(traded.Trades) != 0 {
		t.Fatalf("the trade: %+v %v", traded, err)
	}
	// The player may still choose again for the Vanguard they received.
	if _, err := f.svc.SetFluxSpells(ctx, "acc-1", [2]string{"blink", "mend"}); err != nil {
		t.Fatalf("SetFluxSpells: %v", err)
	}

	// Each player must be allowed the Vanguard they receive; a refused trade
	// changes nothing.
	if _, err := f.svc.OfferTrade(ctx, "acc-4", 0); err != nil {
		t.Fatalf("OfferTrade: %v", err)
	}
	if _, err := f.svc.AcceptTrade(ctx, "acc-1", 2); !errors.Is(err, ErrNotAvailable) {
		t.Fatalf("a Vanguard acc-1 may not play: %v", err)
	}
	if kept := f.selectOf(t, "acc-1", s.ID); kept.Seats[0].Locked != "qazharr" || kept.Seats[2].Locked != "bryn" || len(kept.Trades) != 1 {
		t.Fatalf("nothing changed: %+v", kept)
	}

	// The match takes the traded Vanguards.
	started, err := f.svc.Lock(ctx, "acc-2", "oriel")
	m, _, _ := f.matches.BySelect(ctx, s.ID)
	if err != nil || started.State != Started || m.Participants[0].VanguardID != "qazharr" || m.Participants[1].VanguardID != "cairn" ||
		m.Participants[0].FluxSpells != [2]string{"blink", "mend"} {
		t.Fatalf("the match: %+v %+v %v", started, m, err)
	}
}

// After the last lock a select waits out its final window, in which locked
// teammates may still trade (ADR-041 §2).
func TestTheFinalWindowLeavesTimeToTrade(t *testing.T) {
	f := newFixture(t)
	f.svc.settings.Casual.FinalDuration = 10 * time.Second
	f.mayAlsoPlay(map[string][]string{"acc-1": {"bryn"}, "acc-3": {"qazharr"}, "acc-4": {"bryn"}, "acc-2": {"oriel"}})
	s := f.teammates(t)
	for _, lock := range [][2]string{{"acc-1", "cairn"}, {"acc-3", "qazharr"}, {"acc-4", "bryn"}, {"acc-2", "oriel"}} {
		if _, err := f.svc.Lock(ctx, lock[0], lock[1]); err != nil {
			t.Fatalf("Lock %s: %v", lock[0], err)
		}
	}
	final := f.selectOf(t, "acc-1", s.ID)
	if final.State != Picking || final.Phase != PhaseFinal || !final.Deadline.Equal(t0.Add(10*time.Second)) || final.PhaseLength() != 10*time.Second {
		t.Fatalf("the final window: %+v", final)
	}
	f.now = t0.Add(5 * time.Second)
	if _, err := f.svc.OfferTrade(ctx, "acc-1", 2); err != nil {
		t.Fatalf("OfferTrade: %v", err)
	}
	if _, err := f.svc.AcceptTrade(ctx, "acc-4", 0); err != nil {
		t.Fatalf("AcceptTrade: %v", err)
	}
	f.tickAt(t, final.Deadline, "acc-1", "acc-2", "acc-3", "acc-4")
	m, _, _ := f.matches.BySelect(ctx, s.ID)
	if started := f.selectOf(t, "acc-1", s.ID); started.State != Started || m.Participants[0].VanguardID != "bryn" || m.Participants[2].VanguardID != "cairn" {
		t.Fatalf("the match after the window: %+v %+v", started, m)
	}
}
