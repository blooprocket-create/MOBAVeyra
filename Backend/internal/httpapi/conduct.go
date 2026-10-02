package httpapi

import (
	"net/http"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/conduct"
)

// routeConduct registers reports, commendation and the player's own conduct
// record (ADR-047), and the development route that shows a match's case.
func (s *Server) routeConduct(mux *http.ServeMux) {
	if s.Conduct == nil {
		return
	}
	mux.HandleFunc("POST /v1/me/matches/{matchId}/reports", s.authed(s.reportPlayer))
	mux.HandleFunc("POST /v1/me/matches/{matchId}/commendation", s.authed(s.commendTeammate))
	mux.HandleFunc("GET /v1/me/matches/{matchId}/conduct", s.authed(s.myConduct))
	if s.DevMatches {
		mux.HandleFunc("GET /v1/dev/matches/{matchId}/conduct", s.devConduct)
	}
}

// reportPlayer files the player's report of another participant. The answer
// says only that it was received (ADR-047 §2).
func (s *Server) reportPlayer(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		ReportedName string `json:"reportedName"`
		Reason       string `json:"reason"`
		Details      string `json:"details"`
		ClientID     string `json:"clientId"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	if _, err := s.Conduct.Report(r.Context(), actor, r.PathValue("matchId"),
		conduct.ReportRequest{ReportedName: req.ReportedName, Reason: req.Reason, Details: req.Details, ClientID: req.ClientID}); err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"report": map[string]string{"reportedName": req.ReportedName, "status": "received"}})
}

// commendTeammate records the player's one commendation in a match (ADR-047 §3).
func (s *Server) commendTeammate(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		Name string `json:"name"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	c, err := s.Conduct.Commend(r.Context(), actor, r.PathValue("matchId"), req.Name)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"commendation": map[string]string{"name": c.CommendedName}})
}

// myConduct lists whom the player reported and commended in a match.
func (s *Server) myConduct(w http.ResponseWriter, r *http.Request, actor string) {
	rec, err := s.Conduct.Record(r.Context(), actor, r.PathValue("matchId"))
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"conduct": map[string]any{"reported": rec.Reported, "commended": textOrNil(rec.Commended)}})
}

// devConduct shows a match's case and commendations, for scripted runs.
// Local only.
func (s *Server) devConduct(w http.ResponseWriter, r *http.Request) {
	c, commendations, err := s.Conduct.DevMatch(r.Context(), r.PathValue("matchId"))
	if err != nil {
		s.fail(w, err)
		return
	}
	type reportJSON struct {
		Reporter  string `json:"reporterId"`
		Reported  string `json:"reportedId"`
		Name      string `json:"reportedName"`
		Reason    string `json:"reason"`
		Details   string `json:"details"`
		CreatedAt string `json:"createdAt"`
	}
	type commendationJSON struct {
		Commender string `json:"commenderId"`
		Commended string `json:"commendedId"`
		Name      string `json:"commendedName"`
	}
	reports := make([]reportJSON, 0, len(c.Reports))
	for _, rep := range c.Reports {
		reports = append(reports, reportJSON{Reporter: rep.ReporterID, Reported: rep.ReportedID, Name: rep.ReportedName, Reason: rep.Reason, Details: rep.Details,
			CreatedAt: rep.CreatedAt.UTC().Format(time.RFC3339Nano)})
	}
	commended := make([]commendationJSON, 0, len(commendations))
	for _, cm := range commendations {
		commended = append(commended, commendationJSON{Commender: cm.CommenderID, Commended: cm.CommendedID, Name: cm.CommendedName})
	}
	writeJSON(w, http.StatusOK, map[string]any{"case": map[string]any{"open": len(reports) > 0, "reports": reports}, "commendations": commended})
}
