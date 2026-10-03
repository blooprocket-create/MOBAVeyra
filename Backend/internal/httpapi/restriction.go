package httpapi

import (
	"math"
	"net/http"
)

// routeRestriction registers the player's own queue-dodge restriction
// (ADR-060 §3).
func (s *Server) routeRestriction(mux *http.ServeMux) {
	if s.Dodges == nil {
		return
	}
	mux.HandleFunc("GET /v1/me/restriction", s.authed(s.myRestriction))
}

// myRestriction returns how long the player cannot queue yet, in whole
// seconds; 0 when free.
func (s *Server) myRestriction(w http.ResponseWriter, r *http.Request, actor string) {
	left, err := s.Dodges.Remaining(r.Context(), []string{actor})
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"restrictedSeconds": math.Ceil(left[actor].Seconds())})
}
