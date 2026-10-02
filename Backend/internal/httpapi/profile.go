package httpapi

import (
	"net/http"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/profile"
)

// routeProfile registers public profiles, their shared Match History and the
// player's own profile settings (ADR-048).
func (s *Server) routeProfile(mux *http.ServeMux) {
	if s.Profile == nil {
		return
	}
	mux.HandleFunc("GET /v1/profiles/{name}", s.authed(s.viewProfile))
	mux.HandleFunc("GET /v1/profiles/{name}/matches", s.authed(s.profileMatchHistory))
	mux.HandleFunc("GET /v1/profiles/{name}/matches/{matchId}", s.authed(s.profileMatchResult))
	mux.HandleFunc("GET /v1/me/profile-settings", s.authed(s.myProfileSettings))
	mux.HandleFunc("PUT /v1/me/profile-settings", s.authed(s.saveProfileSettings))
}

type featuredJSON struct {
	VanguardID   string `json:"vanguardId"`
	MasteryLevel int    `json:"masteryLevel"`
}

// viewProfile returns a player's public profile, or profile_unavailable for
// an unknown name and a block either way alike (ADR-048 §3).
func (s *Server) viewProfile(w http.ResponseWriter, r *http.Request, actor string) {
	p, err := s.Profile.View(r.Context(), actor, r.PathValue("name"))
	if err != nil {
		s.fail(w, err)
		return
	}
	var featured *featuredJSON
	if p.Featured != nil {
		featured = &featuredJSON{VanguardID: p.Featured.VanguardID, MasteryLevel: p.Featured.MasteryLevel}
	}
	writeJSON(w, http.StatusOK, map[string]any{"profile": map[string]any{"name": p.Name, "icon": p.Icon, "background": p.Background, "level": p.Level,
		"featured": featured, "sharesMatchHistory": p.SharesMatchHistory}})
}

// profileMatchHistory pages a profile owner's Match History while they share
// it, exactly as the owner's own Match History pages.
func (s *Server) profileMatchHistory(w http.ResponseWriter, r *http.Request, actor string) {
	owner, err := s.Profile.HistoryOwner(r.Context(), actor, r.PathValue("name"))
	if err != nil {
		s.fail(w, err)
		return
	}
	s.writeMatchHistory(w, r, owner)
}

// profileMatchResult opens one of a sharing owner's matches as the owner sees
// it, without the owner's rewards.
func (s *Server) profileMatchResult(w http.ResponseWriter, r *http.Request, actor string) {
	owner, err := s.Profile.HistoryOwner(r.Context(), actor, r.PathValue("name"))
	if err != nil {
		s.fail(w, err)
		return
	}
	s.writeMatchResult(w, r, owner, false)
}

type profileSettingsJSON struct {
	Icon               string  `json:"icon"`
	Background         string  `json:"background"`
	FeaturedVanguardID *string `json:"featuredVanguardId"`
	ShowMatchHistory   bool    `json:"showMatchHistory"`
}

func (s *Server) writeProfileSettings(w http.ResponseWriter, r *http.Request, actor string, a profile.Appearance) {
	// The featured Vanguard's choices are the ones the player permanently owns.
	choices, err := s.Profile.FeaturedChoices(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	if choices == nil {
		choices = []string{}
	}
	c := s.Profile.Catalog()
	writeJSON(w, http.StatusOK, map[string]any{
		"settings": profileSettingsJSON{Icon: a.Icon, Background: a.Background, FeaturedVanguardID: textOrNil(a.FeaturedVanguard), ShowMatchHistory: a.ShowMatchHistory},
		"catalog": map[string]any{"icons": c.Icons, "backgrounds": c.Backgrounds, "defaultIcon": c.DefaultIcon, "defaultBackground": c.DefaultBackground,
			"featuredChoices": choices},
	})
}

// myProfileSettings returns the player's profile choices and the catalog.
func (s *Server) myProfileSettings(w http.ResponseWriter, r *http.Request, actor string) {
	a, err := s.Profile.Settings(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	s.writeProfileSettings(w, r, actor, a)
}

// saveProfileSettings saves the player's choices: catalog entries and a
// permanently owned featured Vanguard, or none (ADR-048 §4).
func (s *Server) saveProfileSettings(w http.ResponseWriter, r *http.Request, actor string) {
	var req profileSettingsJSON
	if !s.decode(w, r, &req) {
		return
	}
	a := profile.Appearance{Icon: req.Icon, Background: req.Background, ShowMatchHistory: req.ShowMatchHistory}
	if req.FeaturedVanguardID != nil {
		a.FeaturedVanguard = *req.FeaturedVanguardID
	}
	saved, err := s.Profile.SaveSettings(r.Context(), actor, a)
	if err != nil {
		s.fail(w, err)
		return
	}
	s.writeProfileSettings(w, r, actor, saved)
}
