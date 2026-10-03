package httpapi

import "net/http"

// routeFavorites registers the player's favorite Vanguards (ADR-058 §5).
func (s *Server) routeFavorites(mux *http.ServeMux) {
	if s.Favorites == nil {
		return
	}
	mux.HandleFunc("GET /v1/me/favorites", s.authed(s.myFavorites))
	mux.HandleFunc("PUT /v1/me/favorites/{vanguardId}", s.authed(s.markFavorite))
	mux.HandleFunc("DELETE /v1/me/favorites/{vanguardId}", s.authed(s.unmarkFavorite))
}

func writeFavorites(w http.ResponseWriter, ids []string) {
	if ids == nil {
		ids = []string{}
	}
	writeJSON(w, http.StatusOK, map[string]any{"favorites": ids})
}

// myFavorites returns the player's favorite Vanguards in the order marked.
func (s *Server) myFavorites(w http.ResponseWriter, r *http.Request, actor string) {
	ids, err := s.Favorites.Favorites(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeFavorites(w, ids)
}

// markFavorite marks a released Vanguard a favorite, outside champion select.
func (s *Server) markFavorite(w http.ResponseWriter, r *http.Request, actor string) {
	s.setFavorite(w, r, actor, true)
}

// unmarkFavorite unmarks one, outside champion select.
func (s *Server) unmarkFavorite(w http.ResponseWriter, r *http.Request, actor string) {
	s.setFavorite(w, r, actor, false)
}

func (s *Server) setFavorite(w http.ResponseWriter, r *http.Request, actor string, favorite bool) {
	ids, err := s.Favorites.Set(r.Context(), actor, r.PathValue("vanguardId"), favorite)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeFavorites(w, ids)
}
