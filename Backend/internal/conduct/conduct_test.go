package conduct

import (
	"context"
	"errors"
	"fmt"
	"slices"
	"strings"
	"testing"
	"time"
)

var (
	ctx = context.Background()
	t0  = time.Date(2026, 10, 2, 12, 0, 0, 0, time.UTC)
)

// fixtureTuning holds fixture values, independent of the committed config.
var fixtureTuning = Tuning{Reasons: []string{"afk", "other"}, DetailsMaxCharacters: 10, ReportWindow: 24 * time.Hour, CommendWindow: 10 * time.Minute}

// fakeMatches holds one ended match: DevOne and DevTwo on side A, DevThree on side B.
type fakeMatches struct{ match PlayedMatch }

func (m *fakeMatches) Played(_ context.Context, accountID, matchID string) (PlayedMatch, error) {
	if matchID != "m1" || !slices.ContainsFunc(m.match.Participants, func(p Participant) bool { return p.AccountID == accountID }) {
		return PlayedMatch{}, ErrNotParticipant
	}
	return m.match, nil
}

type fixture struct {
	svc     *Service
	store   *MemStore
	matches *fakeMatches
	now     time.Time
	ids     int
}

func newFixture(t *testing.T) *fixture {
	t.Helper()
	f := &fixture{store: NewMemStore(), now: t0.Add(time.Minute), matches: &fakeMatches{match: PlayedMatch{EndedAt: t0, Participants: []Participant{
		{AccountID: "acc-1", DisplayName: "DevOne", Side: "A"}, {AccountID: "acc-2", DisplayName: "DevTwo", Side: "A"},
		{AccountID: "acc-3", DisplayName: "DevThree", Side: "B"}}}}}
	f.svc = NewService(f.store, f.matches, fixtureTuning, func() time.Time { return f.now })
	return f
}

func (f *fixture) report(actor, name, reason, details string) (Report, error) {
	f.ids++
	return f.svc.Report(ctx, actor, "m1", ReportRequest{ReportedName: name, Reason: reason, Details: details, ClientID: fmt.Sprintf("report-%04d", f.ids)})
}

func TestAParticipantReportsAnotherHumanIntoTheMatchsCase(t *testing.T) {
	f := newFixture(t)
	r, err := f.report("acc-1", "devthree", "afk", "  left\nat 5  ")
	if err != nil || r.ReportedID != "acc-3" || r.ReportedName != "DevThree" || r.Details != "left at 5" {
		t.Fatalf("report: %+v %v", r, err)
	}
	if _, err := f.report("acc-1", "DevTwo", "other", ""); err != nil {
		t.Fatalf("a second player: %v", err)
	}
	if _, err := f.report("acc-3", "DevOne", "other", ""); err != nil {
		t.Fatalf("another reporter: %v", err)
	}
	c, commendations, err := f.svc.DevMatch(ctx, "m1")
	if err != nil || c.MatchID != "m1" || len(c.Reports) != 3 || !c.OpenedAt.Equal(f.now) || len(commendations) != 0 {
		t.Fatalf("case: %+v %v", c, err)
	}
	rec, err := f.svc.Record(ctx, "acc-1", "m1")
	if err != nil || !slices.Equal(rec.Reported, []string{"DevThree", "DevTwo"}) || rec.Commended != "" {
		t.Fatalf("record: %+v %v", rec, err)
	}
	// The record carries what a report may give, so the client's form offers what is accepted.
	if !slices.Equal(rec.Reasons, fixtureTuning.Reasons) || rec.DetailsMaxCharacters != fixtureTuning.DetailsMaxCharacters {
		t.Fatalf("record's reasons and details limit: %+v", rec)
	}
}

func TestARepeatedReportReturnsTheFirst(t *testing.T) {
	f := newFixture(t)
	first, _ := f.report("acc-1", "DevThree", "afk", "")
	again, err := f.report("acc-1", "DevThree", "other", "changed")
	if err != nil || again.Reason != "afk" || again.ClientID != first.ClientID {
		t.Fatalf("a repeat: %+v %v", again, err)
	}
	resent, err := f.svc.Report(ctx, "acc-1", "m1", ReportRequest{ReportedName: "DevThree", Reason: "afk", ClientID: first.ClientID})
	if err != nil || resent.ClientID != first.ClientID {
		t.Fatalf("a resend: %+v %v", resent, err)
	}
	if c, _, _ := f.svc.DevMatch(ctx, "m1"); len(c.Reports) != 1 {
		t.Fatalf("the case holds %d reports", len(c.Reports))
	}
}

func TestReportsAreRefusedOutsideTheRules(t *testing.T) {
	f := newFixture(t)
	for _, c := range []struct {
		actor, name, reason, details string
		want                         error
	}{
		{"acc-9", "DevThree", "afk", "", ErrNotParticipant},
		{"acc-1", "DevOne", "afk", "", ErrUnknownPlayer},
		{"acc-1", "Nobody", "afk", "", ErrUnknownPlayer},
		{"acc-1", "DevThree", "rudeness", "", ErrInvalidReason},
		{"acc-1", "DevThree", "afk", strings.Repeat("x", fixtureTuning.DetailsMaxCharacters+1), ErrDetailsTooLong},
	} {
		if _, err := f.report(c.actor, c.name, c.reason, c.details); !errors.Is(err, c.want) {
			t.Errorf("%+v: %v", c, err)
		}
	}
	if _, err := f.svc.Report(ctx, "acc-1", "m1", ReportRequest{ReportedName: "DevThree", Reason: "afk", ClientID: "bad id"}); !errors.Is(err, ErrInvalidRequest) {
		t.Errorf("a bad client ID: %v", err)
	}
	f.now = t0.Add(fixtureTuning.ReportWindow)
	if _, err := f.report("acc-1", "DevThree", "afk", ""); !errors.Is(err, ErrReportClosed) {
		t.Errorf("after the window: %v", err)
	}
	f.matches.match.EndedAt = time.Time{}
	f.now = t0
	if _, err := f.report("acc-1", "DevThree", "afk", ""); !errors.Is(err, ErrReportClosed) {
		t.Errorf("a match without a result: %v", err)
	}
	if c, ok, _ := f.store.Case(ctx, "m1"); ok {
		t.Fatalf("a refused report opened a case: %+v", c)
	}
}

func TestATeammateIsCommendedOnceFromTheImmediateResults(t *testing.T) {
	f := newFixture(t)
	if _, err := f.svc.Commend(ctx, "acc-1", "m1", "DevThree"); !errors.Is(err, ErrNotTeammate) {
		t.Fatalf("an opponent: %v", err)
	}
	if _, err := f.svc.Commend(ctx, "acc-1", "m1", "DevOne"); !errors.Is(err, ErrUnknownPlayer) {
		t.Fatalf("oneself: %v", err)
	}
	c, err := f.svc.Commend(ctx, "acc-1", "m1", "devtwo")
	if err != nil || c.CommendedID != "acc-2" || c.CommendedName != "DevTwo" {
		t.Fatalf("Commend: %+v %v", c, err)
	}
	if again, err := f.svc.Commend(ctx, "acc-1", "m1", "DevTwo"); err != nil || !again.CreatedAt.Equal(c.CreatedAt) {
		t.Fatalf("a resend: %+v %v", again, err)
	}
	f.matches.match.Participants = append(f.matches.match.Participants, Participant{AccountID: "acc-4", DisplayName: "DevFour", Side: "A"})
	if _, err := f.svc.Commend(ctx, "acc-1", "m1", "DevFour"); !errors.Is(err, ErrAlreadyCommended) {
		t.Fatalf("a second teammate: %v", err)
	}
	if rec, _ := f.svc.Record(ctx, "acc-1", "m1"); rec.Commended != "DevTwo" {
		t.Fatalf("record: %+v", rec)
	}
	f.now = t0.Add(fixtureTuning.CommendWindow)
	if _, err := f.svc.Commend(ctx, "acc-2", "m1", "DevOne"); !errors.Is(err, ErrCommendClosed) {
		t.Fatalf("after the immediate results: %v", err)
	}
	if _, commendations, _ := f.svc.DevMatch(ctx, "m1"); len(commendations) != 1 {
		t.Fatalf("commendations %+v", commendations)
	}
}

func TestOnlyAParticipantReadsItsRecord(t *testing.T) {
	f := newFixture(t)
	if _, err := f.svc.Record(ctx, "acc-9", "m1"); !errors.Is(err, ErrNotParticipant) {
		t.Fatalf("a stranger: %v", err)
	}
	rec, err := f.svc.Record(ctx, "acc-3", "m1")
	if err != nil || len(rec.Reported) != 0 || rec.Commended != "" {
		t.Fatalf("an empty record: %+v %v", rec, err)
	}
}
