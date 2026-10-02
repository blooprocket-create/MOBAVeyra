package httpapi

import (
	"context"
	"net/http"
	"slices"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/conduct"
)

// conductMatches stands in for the match domain: the test accounts played match-1, the first two on side A.
type conductMatches struct{ played conduct.PlayedMatch }

func (m *conductMatches) Played(_ context.Context, accountID, matchID string) (conduct.PlayedMatch, error) {
	if matchID != "match-1" || !slices.ContainsFunc(m.played.Participants, func(p conduct.Participant) bool { return p.AccountID == accountID }) {
		return conduct.PlayedMatch{}, conduct.ErrNotParticipant
	}
	return m.played, nil
}

func TestConductOverHTTP(t *testing.T) {
	d := newTestDeps(t, true)
	d.DevMatches = true
	matches := &conductMatches{}
	// Fixture tuning, independent of the committed config.
	d.Conduct = conduct.NewService(conduct.NewMemStore(), matches, conduct.Tuning{Reasons: []string{"afk", "other"}, DetailsMaxCharacters: 20,
		ReportWindow: time.Hour, CommendWindow: time.Hour}, time.Now)
	srv := serve(t, d)
	one, oneID := gameSession(t, srv, testAccounts[0])
	_, twoID := gameSession(t, srv, testAccounts[1])
	stranger, _ := gameSession(t, srv, testAccounts[2])
	matches.played = conduct.PlayedMatch{EndedAt: time.Now(), Participants: []conduct.Participant{
		{AccountID: oneID, DisplayName: testAccounts[0], Side: "A"}, {AccountID: twoID, DisplayName: testAccounts[1], Side: "A"}}}

	status, got := call(t, srv, "POST", "/v1/me/matches/match-1/reports", one, map[string]string{"reportedName": testAccounts[1], "reason": "afk", "details": "idle", "clientId": "http-report-0001"})
	if status != http.StatusOK || got["report"].(map[string]any)["status"] != "received" {
		t.Fatalf("report: %d %v", status, got)
	}
	for _, c := range []struct {
		token string
		body  map[string]string
		code  string
	}{
		{one, map[string]string{"reportedName": testAccounts[1], "reason": "rudeness", "clientId": "http-report-0002"}, "invalid_reason"},
		{one, map[string]string{"reportedName": "Nobody", "reason": "afk", "clientId": "http-report-0003"}, "unknown_player"},
		{one, map[string]string{"reportedName": testAccounts[1], "reason": "afk", "details": "this is far too long to send", "clientId": "http-report-0004"}, "details_too_long"},
		{one, map[string]string{"reportedName": testAccounts[1], "reason": "afk", "clientId": "bad"}, "invalid_report"},
		{stranger, map[string]string{"reportedName": testAccounts[0], "reason": "afk", "clientId": "http-report-0005"}, "not_participant"},
	} {
		if status, got := call(t, srv, "POST", "/v1/me/matches/match-1/reports", c.token, c.body); got["error"] != c.code {
			t.Errorf("%v: %d %v, want %s", c.body, status, got, c.code)
		}
	}
	if status, got = call(t, srv, "POST", "/v1/me/matches/match-1/commendation", one, map[string]string{"name": testAccounts[1]}); status != http.StatusOK {
		t.Fatalf("commend: %d %v", status, got)
	}
	status, got = call(t, srv, "GET", "/v1/me/matches/match-1/conduct", one, nil)
	record := got["conduct"].(map[string]any)
	if status != http.StatusOK || record["commended"] != testAccounts[1] || len(record["reported"].([]any)) != 1 {
		t.Fatalf("record: %d %v", status, got)
	}
	status, got = call(t, srv, "GET", "/v1/dev/matches/match-1/conduct", "", nil)
	reports := got["case"].(map[string]any)["reports"].([]any)
	if status != http.StatusOK || len(reports) != 1 || reports[0].(map[string]any)["reportedId"] != twoID || len(got["commendations"].([]any)) != 1 {
		t.Fatalf("the dev view: %d %v", status, got)
	}
}
