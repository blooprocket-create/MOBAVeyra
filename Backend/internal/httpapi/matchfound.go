package httpapi

import (
	"net/http"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/matchmaking"
)

// routeMatchFound registers Match Found (ADR-010 §10; Parties & Social Bible §3).
func (s *Server) routeMatchFound(mux *http.ServeMux) {
	if s.Matchmaking == nil {
		return
	}
	mux.HandleFunc("GET /v1/me/match-found", s.authed(s.myMatchFound))
	mux.HandleFunc("POST /v1/me/match-found/accept", s.authed(s.acceptMatchFound))
	mux.HandleFunc("POST /v1/me/match-found/decline", s.authed(s.declineMatchFound))
}

type matchFoundJSON struct {
	ID    string `json:"id"`
	Mode  string `json:"mode"`
	State string `json:"state"`
	// Deadline is the server's; RemainingSeconds is what is left of it by the
	// server's clock, so a client counts down without trusting its own.
	Deadline         time.Time `json:"deadline"`
	RemainingSeconds float64   `json:"remainingSeconds"`
	// Accepted and Total count the players; nobody learns who declined.
	Accepted int `json:"accepted"`
	Total    int `json:"total"`
	// You is the player's own answer: pending, accepted or declined.
	You string `json:"you"`
	// SelectID is the champion select an accepted match opened.
	SelectID      *string `json:"selectId"`
	AbandonReason *string `json:"abandonReason"`
}

func (s *Server) toMatchFoundJSON(f matchmaking.Found, actor string) matchFoundJSON {
	accepted, total := f.Counts()
	out := matchFoundJSON{
		ID:               f.ID,
		Mode:             f.Mode,
		State:            string(f.State),
		Deadline:         f.Deadline,
		RemainingSeconds: s.Matchmaking.RemainingAccept(f).Seconds(),
		Accepted:         accepted,
		Total:            total,
		You:              "pending",
		SelectID:         textOrNil(f.SelectID),
		AbandonReason:    textOrNil(string(f.AbandonReason)),
	}
	for _, seat := range f.Seats {
		if seat.AccountID == actor && seat.Decision != matchmaking.Undecided {
			out.You = string(seat.Decision)
		}
	}
	return out
}

// myMatchFound answers "is a match waiting for my answer".
func (s *Server) myMatchFound(w http.ResponseWriter, r *http.Request, actor string) {
	f, ok, err := s.Matchmaking.Current(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	if !ok {
		writeJSON(w, http.StatusOK, map[string]any{"matchFound": nil})
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"matchFound": s.toMatchFoundJSON(f, actor)})
}

// acceptMatchFound accepts. When it is the last acceptance, champion select has
// opened by the time it answers, and the answer names it.
func (s *Server) acceptMatchFound(w http.ResponseWriter, r *http.Request, actor string) {
	f, err := s.Matchmaking.Accept(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"matchFound": s.toMatchFoundJSON(f, actor)})
}

// declineMatchFound declines, which ends the match for everyone: the decliner
// leaves the queue and the others return to it.
func (s *Server) declineMatchFound(w http.ResponseWriter, r *http.Request, actor string) {
	f, err := s.Matchmaking.Decline(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"matchFound": s.toMatchFoundJSON(f, actor)})
}
