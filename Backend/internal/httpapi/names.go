package httpapi

import (
	"net/http"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
)

// routeNames registers the player's display name and its change (ADR-049).
func (s *Server) routeNames(mux *http.ServeMux) {
	mux.HandleFunc("GET /v1/me/display-name", s.authed(s.myDisplayName))
	mux.HandleFunc("PUT /v1/me/display-name", s.authed(s.changeDisplayName))
}

func (s *Server) writeNameStatus(w http.ResponseWriter, st identity.NameStatus) {
	var next *time.Time
	if !st.NextChangeAt.IsZero() {
		at := st.NextChangeAt.UTC()
		next = &at
	}
	writeJSON(w, http.StatusOK, map[string]any{"displayName": map[string]any{
		"name": st.Account.DisplayName, "freeChangeAvailable": st.FreeChangeAvailable, "nextChangeAt": next, "renameRequired": st.RenameRequired,
		"price": map[string]int64{"flux": st.PriceFlux, "refinedFlux": st.PriceRefinedFlux},
	}})
}

// myDisplayName returns the player's name, whether their free change is
// left, when they may next change it, and the price.
func (s *Server) myDisplayName(w http.ResponseWriter, r *http.Request, actor string) {
	st, err := s.Identity.NameStatus(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	s.writeNameStatus(w, st)
}

// changeDisplayName changes the player's name, claiming it from an inactive
// holder where the rules allow, and charging for it after the free change.
func (s *Server) changeDisplayName(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		Name     string `json:"name"`
		Currency string `json:"currency"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	if _, err := s.Identity.ChangeDisplayName(r.Context(), actor, req.Name, req.Currency); err != nil {
		s.fail(w, err)
		return
	}
	s.myDisplayName(w, r, actor)
}

// named refuses a request that would show a claimed account's placeholder to
// other players until it chooses a new name (ADR-049 §4).
func (s *Server) named(h func(w http.ResponseWriter, r *http.Request, actor string)) func(w http.ResponseWriter, r *http.Request, actor string) {
	return func(w http.ResponseWriter, r *http.Request, actor string) {
		required, err := s.Identity.RenameRequired(r.Context(), actor)
		if err != nil {
			s.fail(w, err)
			return
		}
		if required {
			s.fail(w, identity.ErrRenameRequired)
			return
		}
		h(w, r, actor)
	}
}
