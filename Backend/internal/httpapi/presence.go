package httpapi

import (
	"net/http"
)

// routePresence registers the player's own presence and Appear Offline
// (ADR-061 §3).
func (s *Server) routePresence(mux *http.ServeMux) {
	if s.Presence == nil {
		return
	}
	mux.HandleFunc("GET /v1/me/presence", s.authed(s.myPresence))
	mux.HandleFunc("PUT /v1/me/presence", s.authed(s.setMyPresence))
}

type presenceJSON struct {
	// Status is the player's own: in_match, in_select, in_queue or online.
	Status        string `json:"status"`
	AppearOffline bool   `json:"appearOffline"`
}

// myPresence returns the player's status and Appear Offline. The client also
// reads it now and then on screens that ask nothing else, which keeps the
// player seen.
func (s *Server) myPresence(w http.ResponseWriter, r *http.Request, actor string) {
	self, err := s.Presence.Self(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, presenceJSON{Status: string(self.Status), AppearOffline: self.AppearOffline})
}

// setMyPresence turns Appear Offline on or off: {"appearOffline": true}.
func (s *Server) setMyPresence(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		AppearOffline *bool `json:"appearOffline"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	if req.AppearOffline == nil {
		writeError(w, http.StatusBadRequest, "malformed_request")
		return
	}
	if err := s.Presence.SetAppearOffline(r.Context(), actor, *req.AppearOffline); err != nil {
		s.fail(w, err)
		return
	}
	s.myPresence(w, r, actor)
}
