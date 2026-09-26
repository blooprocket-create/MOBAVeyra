package httpapi

import (
	"errors"
	"net/http"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// routeMatch registers the match routes (ADR-007). The development-only
// match-creation routes stand in for Match Found and exist only when enabled.
func (s *Server) routeMatch(mux *http.ServeMux) {
	if s.Match == nil {
		return
	}
	if s.DevMatches {
		mux.HandleFunc("POST /v1/dev/matches", s.createDevMatch)
		mux.HandleFunc("GET /v1/dev/matches/{matchId}", s.getDevMatch)
	}
	mux.HandleFunc("GET /v1/me/match", s.authed(s.myMatch))
	mux.HandleFunc("POST /v1/server/matches/{matchId}/ready", s.serverReady)
	mux.HandleFunc("POST /v1/server/matches/{matchId}/result", s.serverResult)
}

type participantJSON struct {
	AccountID   string `json:"accountId"`
	DisplayName string `json:"displayName"`
	Side        string `json:"side"`
}

type participantResultJSON struct {
	AccountID      string `json:"accountId"`
	Joined         bool   `json:"joined"`
	ConnectedAtEnd bool   `json:"connectedAtEnd"`
}

type resultJSON struct {
	EndReason       string                  `json:"endReason"`
	Winner          *string                 `json:"winner"`
	DurationSeconds float64                 `json:"durationSeconds"`
	Participants    []participantResultJSON `json:"participants"`
}

// devMatchJSON is a development view of a match. It carries no secrets.
type devMatchJSON struct {
	ID            string            `json:"id"`
	Mode          string            `json:"mode"`
	State         string            `json:"state"`
	Participants  []participantJSON `json:"participants"`
	HostPort      int               `json:"hostPort"`
	CreatedAt     time.Time         `json:"createdAt"`
	ReadyAt       *time.Time        `json:"readyAt"`
	EndedAt       *time.Time        `json:"endedAt"`
	ServerRemoved bool              `json:"serverRemoved"`
	FailureReason *string           `json:"failureReason"`
	Result        *resultJSON       `json:"result"`
}

func toDevMatchJSON(m match.Match) devMatchJSON {
	out := devMatchJSON{
		ID:            m.ID,
		Mode:          m.Mode,
		State:         string(m.State),
		HostPort:      m.Server.HostPort,
		CreatedAt:     m.CreatedAt,
		ReadyAt:       timeOrNil(m.ReadyAt),
		EndedAt:       timeOrNil(m.EndedAt),
		ServerRemoved: !m.Server.RemovedAt.IsZero(),
		FailureReason: textOrNil(string(m.FailureReason)),
		Participants:  []participantJSON{},
	}
	for _, p := range m.Participants {
		out.Participants = append(out.Participants, participantJSON{AccountID: p.AccountID, DisplayName: p.DisplayName, Side: string(p.Side)})
	}
	if r := m.Result; r != nil {
		out.Result = &resultJSON{EndReason: string(r.EndReason), Winner: textOrNil(string(r.Winner)), DurationSeconds: r.DurationSeconds,
			Participants: []participantResultJSON{}}
		for _, p := range r.Participants {
			out.Result.Participants = append(out.Result.Participants, participantResultJSON(p))
		}
	}
	return out
}

func (s *Server) createDevMatch(w http.ResponseWriter, r *http.Request) {
	var req struct {
		Mode         string `json:"mode"`
		Participants []struct {
			AccountID string `json:"accountId"`
			Side      string `json:"side"`
		} `json:"participants"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	seats := make([]match.Seat, len(req.Participants))
	for i, p := range req.Participants {
		seats[i] = match.Seat{AccountID: p.AccountID, Side: match.Side(p.Side)}
	}
	m, err := s.Match.CreateDevMatch(r.Context(), req.Mode, seats)
	if err != nil {
		if errors.Is(err, match.ErrAllocationFailed) {
			s.Log.Error("match server did not start", "err", err)
		}
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusCreated, map[string]any{"match": toDevMatchJSON(m)})
}

func (s *Server) getDevMatch(w http.ResponseWriter, r *http.Request) {
	m, err := s.Match.Get(r.Context(), r.PathValue("matchId"))
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"match": toDevMatchJSON(m)})
}

// myMatch answers a player's "what is my match" (ADR-007 §10). The server
// address and the join ticket appear only once the match is ready.
func (s *Server) myMatch(w http.ResponseWriter, r *http.Request, actor string) {
	pm, ok, err := s.Match.Current(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	if !ok {
		writeJSON(w, http.StatusOK, map[string]any{"match": nil})
		return
	}
	type serverJSON struct {
		Host string `json:"host"`
		Port int    `json:"port"`
	}
	out := struct {
		ID     string      `json:"id"`
		State  string      `json:"state"`
		Side   string      `json:"side"`
		Server *serverJSON `json:"server"`
		Ticket *string     `json:"ticket"`
	}{ID: pm.MatchID, State: string(pm.State), Side: string(pm.Side)}
	if pm.Ticket != "" {
		out.Server = &serverJSON{Host: pm.ServerHost, Port: pm.ServerPort}
		out.Ticket = &pm.Ticket
	}
	writeJSON(w, http.StatusOK, map[string]any{"match": out})
}

func (s *Server) serverReady(w http.ResponseWriter, r *http.Request) {
	credential, ok := bearer(r)
	if !ok {
		writeError(w, http.StatusUnauthorized, "invalid_credentials")
		return
	}
	var req struct{}
	if !s.decode(w, r, &req) {
		return
	}
	if err := s.Match.ServerReady(r.Context(), credential, r.PathValue("matchId")); err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]string{"status": "ready"})
}

func (s *Server) serverResult(w http.ResponseWriter, r *http.Request) {
	credential, ok := bearer(r)
	if !ok {
		writeError(w, http.StatusUnauthorized, "invalid_credentials")
		return
	}
	var req resultJSON
	if !s.decode(w, r, &req) {
		return
	}
	result := match.Result{EndReason: match.EndReason(req.EndReason), DurationSeconds: req.DurationSeconds}
	if req.Winner != nil {
		if *req.Winner == "" {
			writeError(w, http.StatusBadRequest, "invalid_result")
			return
		}
		result.Winner = match.Side(*req.Winner)
	}
	for _, p := range req.Participants {
		result.Participants = append(result.Participants, match.ParticipantResult(p))
	}
	if err := s.Match.ServerResult(r.Context(), credential, r.PathValue("matchId"), result); err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]string{"status": "recorded"})
}

func timeOrNil(t time.Time) *time.Time {
	if t.IsZero() {
		return nil
	}
	return &t
}

func textOrNil(s string) *string {
	if s == "" {
		return nil
	}
	return &s
}
