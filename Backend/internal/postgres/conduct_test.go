package postgres

import (
	"context"
	"errors"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/conduct"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// conductMatches answers one match the fixture created: DevOne and DevTwo on one side, DevThree on the other.
type conductMatches struct {
	id     string
	played conduct.PlayedMatch
}

func (m conductMatches) Played(_ context.Context, accountID, matchID string) (conduct.PlayedMatch, error) {
	if matchID != m.id {
		return conduct.PlayedMatch{}, conduct.ErrNotParticipant
	}
	for _, p := range m.played.Participants {
		if p.AccountID == accountID {
			return m.played, nil
		}
	}
	return conduct.PlayedMatch{}, conduct.ErrNotParticipant
}

func TestConductInPostgres(t *testing.T) {
	f := newMatchFixture(t, "DevOne", "DevTwo", "DevThree")
	ctx := context.Background()
	m, err := f.svc.Create(ctx, f.casual(f.seats(map[string]match.Side{"DevOne": match.SideA, "DevTwo": match.SideA, "DevThree": match.SideB})))
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	now := time.Now().UTC().Truncate(time.Microsecond)
	one, two, three := f.ids["DevOne"], f.ids["DevTwo"], f.ids["DevThree"]
	matches := conductMatches{id: m.ID, played: conduct.PlayedMatch{EndedAt: now.Add(-time.Minute), Participants: []conduct.Participant{
		{AccountID: one, DisplayName: "DevOne", Side: "A"}, {AccountID: two, DisplayName: "DevTwo", Side: "A"}, {AccountID: three, DisplayName: "DevThree", Side: "B"}}}}
	// Fixture tuning, independent of the committed config.
	svc := conduct.NewService(f.store.Conduct(), matches, conduct.Tuning{Reasons: []string{"afk", "other"}, DetailsMaxCharacters: 50,
		ReportWindow: time.Hour, CommendWindow: 10 * time.Minute}, func() time.Time { return now })

	first, err := svc.Report(ctx, one, m.ID, conduct.ReportRequest{ReportedName: "DevThree", Reason: "afk", Details: "stood still", ClientID: "pg-report-0001"})
	if err != nil || first.ReportedID != three {
		t.Fatalf("Report: %+v %v", first, err)
	}
	again, err := svc.Report(ctx, one, m.ID, conduct.ReportRequest{ReportedName: "devthree", Reason: "other", ClientID: "pg-report-0002"})
	if err != nil || again.ClientID != first.ClientID || again.Reason != "afk" {
		t.Fatalf("a repeat: %+v %v", again, err)
	}
	if _, err := svc.Report(ctx, three, m.ID, conduct.ReportRequest{ReportedName: "DevOne", Reason: "other", ClientID: "pg-report-0003"}); err != nil {
		t.Fatalf("another reporter: %v", err)
	}
	c, commendations, err := svc.DevMatch(ctx, m.ID)
	if err != nil || len(c.Reports) != 2 || !c.OpenedAt.Equal(now) || len(commendations) != 0 {
		t.Fatalf("the case: %+v %v", c, err)
	}

	if _, err := svc.Commend(ctx, one, m.ID, "DevThree"); !errors.Is(err, conduct.ErrNotTeammate) {
		t.Fatalf("an opponent: %v", err)
	}
	if _, err := svc.Commend(ctx, one, m.ID, "DevTwo"); err != nil {
		t.Fatalf("Commend: %v", err)
	}
	rec, err := svc.Record(ctx, one, m.ID)
	if err != nil || len(rec.Reported) != 1 || rec.Reported[0] != "DevThree" || rec.Commended != "DevTwo" {
		t.Fatalf("record: %+v %v", rec, err)
	}
	if _, commendations, _ := svc.DevMatch(ctx, m.ID); len(commendations) != 1 || commendations[0].CommendedID != two {
		t.Fatalf("commendations: %+v", commendations)
	}
}
