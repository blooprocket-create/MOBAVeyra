package match

import (
	"errors"
	"fmt"
	"testing"
	"time"
)

// endedFor stores a match acc-1 played on side as vanguard, ended at the
// given minute after t0, won by winner ("" for none).
func (f *fixture) endedFor(n int, mode string, side Side, vanguard string, winner Side, minute int) string {
	id := fmt.Sprintf("00000000-0000-4000-8000-%012d", n)
	f.store.matches[id] = Match{ID: id, Mode: mode, Rules: RulesStandard, State: Ended, EndedAt: t0.Add(time.Duration(minute) * time.Minute),
		Participants: []Participant{{AccountID: "acc-1", DisplayName: "DevOne", Side: side, VanguardID: vanguard}},
		Result:       &Result{EndReason: EndDeveloperRequest, Winner: winner, DurationSeconds: float64(60 * minute)}}
	return id
}

func TestOutcomeFor(t *testing.T) {
	if OutcomeFor(SideA, SideA) != OutcomeWin || OutcomeFor(SideB, SideA) != OutcomeLoss || OutcomeFor(SideA, "") != OutcomeNoContest {
		t.Fatal("a player wins with their side, loses to the other, and a match nobody won is no contest")
	}
}

func TestHistoryIsNewestFirstInPages(t *testing.T) {
	f := newFixture(t)
	oldest := f.endedFor(1, "casual_select", SideA, "cairn", SideA, 10)
	middle := f.endedFor(2, "casual_select", SideB, "oriel", SideA, 20)
	newest := f.endedFor(3, "custom_practice", SideA, "cairn", "", 30)
	// Another player's match, an active one and a failed one are not in acc-1's history.
	f.store.matches["x"] = Match{ID: "x", State: Ended, EndedAt: t0, Participants: []Participant{{AccountID: "acc-2", Side: SideA}}, Result: &Result{}}
	f.store.matches["y"] = Match{ID: "y", State: Ready, Participants: []Participant{{AccountID: "acc-1", Side: SideA}}}
	f.store.matches["z"] = Match{ID: "z", State: Failed, EndedAt: t0, Participants: []Participant{{AccountID: "acc-1", Side: SideA}}}

	page, next, err := f.svc.History(ctx, "acc-1", HistoryFilter{}, "")
	if err != nil || len(page) != 2 || page[0].MatchID != newest || page[1].MatchID != middle || next == "" {
		t.Fatalf("the first page: %+v %q %v", page, next, err)
	}
	if page[0].Outcome != OutcomeNoContest || page[1].Outcome != OutcomeLoss || page[1].VanguardID != "oriel" || page[1].DurationSeconds != 1200 {
		t.Fatalf("each entry's own side, Vanguard and outcome: %+v", page)
	}
	page, next, err = f.svc.History(ctx, "acc-1", HistoryFilter{}, next)
	if err != nil || len(page) != 1 || page[0].MatchID != oldest || page[0].Outcome != OutcomeWin || next != "" {
		t.Fatalf("the last page: %+v %q %v", page, next, err)
	}
}

func TestHistoryFiltersApplyToEveryRecord(t *testing.T) {
	f := newFixture(t)
	for i := 1; i <= 5; i++ {
		f.endedFor(i, "casual_select", SideA, "cairn", SideB, i)
	}
	won := f.endedFor(6, "casual_select", SideA, "oriel", SideA, 6)
	practised := f.endedFor(7, "custom_practice", SideA, "cairn", "", 7)
	// Beyond the first page's reach, the filters still find them (UX-67).
	for _, c := range []struct {
		filter HistoryFilter
		want   []string
	}{
		{HistoryFilter{VanguardID: "oriel"}, []string{won}},
		{HistoryFilter{Outcome: OutcomeWin}, []string{won}},
		{HistoryFilter{Mode: "custom_practice"}, []string{practised}},
		{HistoryFilter{Mode: "casual_select", VanguardID: "cairn", Outcome: OutcomeWin}, nil},
	} {
		page, _, err := f.svc.History(ctx, "acc-1", c.filter, "")
		if err != nil || len(page) != len(c.want) {
			t.Fatalf("%+v: %+v %v", c.filter, page, err)
		}
		for i := range c.want {
			if page[i].MatchID != c.want[i] {
				t.Fatalf("%+v: %+v", c.filter, page)
			}
		}
	}
	losses, next, _ := f.svc.History(ctx, "acc-1", HistoryFilter{Outcome: OutcomeLoss}, "")
	more, _, _ := f.svc.History(ctx, "acc-1", HistoryFilter{Outcome: OutcomeLoss}, next)
	if len(losses) != 2 || len(more) != 2 || losses[0].EndedAt.Before(more[0].EndedAt) {
		t.Fatalf("a filtered history pages newest first too: %+v then %+v", losses, more)
	}
}

func TestHistoryRefusesBadFiltersAndCursors(t *testing.T) {
	f := newFixture(t)
	for _, filter := range []HistoryFilter{{VanguardID: "Cairn!"}, {Mode: "a b"}, {Outcome: "draw"}} {
		if _, _, err := f.svc.History(ctx, "acc-1", filter, ""); !errors.Is(err, ErrInvalidFilter) {
			t.Fatalf("%+v: want ErrInvalidFilter, got %v", filter, err)
		}
	}
	for _, cursor := range []string{"not base64!", "bm90IGEgY3Vyc29y", HistoryCursor{EndedAt: t0, MatchID: "m"}.Encode()} {
		if _, _, err := f.svc.History(ctx, "acc-1", HistoryFilter{}, cursor); !errors.Is(err, ErrInvalidCursor) {
			t.Fatalf("%q: want ErrInvalidCursor, got %v", cursor, err)
		}
	}
}

func TestHistoryCursorsRoundTrip(t *testing.T) {
	c := HistoryCursor{EndedAt: t0.Add(1234567 * time.Microsecond), MatchID: "00000000-0000-4000-8000-000000000001"}
	got, err := DecodeHistoryCursor(c.Encode())
	if err != nil || !got.EndedAt.Equal(c.EndedAt) || got.MatchID != c.MatchID {
		t.Fatalf("round trip: %+v %v", got, err)
	}
}
