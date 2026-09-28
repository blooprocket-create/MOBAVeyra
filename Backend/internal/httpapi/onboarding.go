package httpapi

import (
	"net/http"
	"slices"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
)

// routeOnboarding registers a player's onboarding and Vanguard routes
// (ADR-010 §6), and the development routes the launcher and scripts use.
func (s *Server) routeOnboarding(mux *http.ServeMux) {
	if s.Account == nil {
		return
	}
	mux.HandleFunc("GET /v1/me/profile", s.authed(s.myProfile))
	mux.HandleFunc("GET /v1/me/vanguards", s.authed(s.myVanguards))
	mux.HandleFunc("POST /v1/me/starter", s.authed(s.chooseStarter))
	if s.DevLogin {
		mux.HandleFunc("GET /v1/dev/accounts", s.devAccounts)
		mux.HandleFunc("POST /v1/dev/accounts/{name}/reset-onboarding", s.resetDevOnboarding)
	}
}

type tutorialJSON struct {
	Completed         bool    `json:"completed"`
	StarterVanguardID *string `json:"starterVanguardId"`
}

func (s *Server) profileJSON(r *http.Request, actor string, p account.Profile) (map[string]any, error) {
	accounts, err := s.Identity.Accounts(r.Context(), []string{actor})
	if err != nil {
		return nil, err
	}
	acct := accounts[actor]
	return map[string]any{
		"account":  toAccountJSON(acct),
		"tutorial": tutorialJSON{Completed: p.TutorialCompleted, StarterVanguardID: textOrNil(p.StarterVanguardID)},
	}, nil
}

// myProfile answers "who am I, and have I finished onboarding" (ADR-010 §2):
// the client shows the starter choice until the tutorial is completed.
func (s *Server) myProfile(w http.ResponseWriter, r *http.Request, actor string) {
	p, err := s.Account.Profile(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	out, err := s.profileJSON(r, actor, p)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, out)
}

// myVanguards lists what the player owns, what the rotation offers, what
// they may therefore pick, and the starters a new player chooses from.
func (s *Server) myVanguards(w http.ResponseWriter, r *http.Request, actor string) {
	a, err := s.Account.Vanguards(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"owned": a.Owned, "rotation": a.Rotation, "available": a.Available, "starters": a.Starters})
}

// chooseStarter completes the stubbed tutorial with a starter the player then
// owns (ADR-010 §6). It works once.
func (s *Server) chooseStarter(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		VanguardID string `json:"vanguardId"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	p, err := s.Account.ChooseStarter(r.Context(), actor, req.VanguardID)
	if err != nil {
		s.fail(w, err)
		return
	}
	out, err := s.profileJSON(r, actor, p)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, out)
}

// devAccounts lists the seeded development accounts, for the launcher's
// account picker. Local only: it exists only with development login.
func (s *Server) devAccounts(w http.ResponseWriter, _ *http.Request) {
	type devAccountJSON struct {
		DisplayName string `json:"displayName"`
	}
	out := []devAccountJSON{}
	for _, name := range s.DevAccounts {
		out = append(out, devAccountJSON{DisplayName: name})
	}
	writeJSON(w, http.StatusOK, map[string]any{"accounts": out})
}

// resetDevOnboarding returns a development account to before its starter
// choice, so scripted runs can repeat the whole flow. Local only.
func (s *Server) resetDevOnboarding(w http.ResponseWriter, r *http.Request) {
	name := r.PathValue("name")
	if !slices.Contains(s.DevAccounts, name) {
		writeError(w, http.StatusNotFound, "account_not_found")
		return
	}
	acct, err := s.Identity.LookupAccount(r.Context(), name)
	if err != nil {
		s.fail(w, err)
		return
	}
	if err := s.Account.ResetOnboarding(r.Context(), acct.ID); err != nil {
		s.fail(w, err)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}
