// Package httpapi exposes the backend over HTTP/JSON. It only translates
// HTTP to domain calls; every rule lives in the domain packages. Routes are
// listed in Backend/README.md.
package httpapi

import (
	"context"
	"encoding/json"
	"errors"
	"log/slog"
	"net/http"
	"strings"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/matchmaking"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

// Pinger reports whether a dependency is reachable.
type Pinger interface {
	Ping(ctx context.Context) error
}

// ModeInfo describes a matchmade mode to clients.
type ModeInfo struct {
	ID                  string `json:"id"`
	Enabled             bool   `json:"enabled"`
	HumanPlayersPerTeam int    `json:"humanPlayersPerTeam"`
	// Matchmaking is casualSelect, or notImplemented for a mode that cannot be
	// queued yet, which clients show as not yet available.
	Matchmaking string `json:"matchmaking"`
}

// Deps are the handler dependencies.
type Deps struct {
	Identity *identity.Service
	Social   *social.Service
	Party    *party.Service
	// Match is optional; without it no match routes are registered.
	Match *match.Service
	// Account is optional; without it no onboarding routes are registered.
	Account *account.Service
	// Selection is optional; without it no practice or champion-select routes
	// are registered.
	Selection *selection.Service
	// Matchmaking is optional; without it no Match Found routes are registered.
	Matchmaking *matchmaking.Service
	Modes       []ModeInfo
	Ready       Pinger
	// Atomic runs fn as one unit of work across domains: store calls made
	// with the ctx it receives share one transaction.
	Atomic         func(ctx context.Context, fn func(context.Context) error) error
	BodyLimitBytes int64
	// DevLogin registers the passwordless dev-login route and the development
	// account routes (local only).
	DevLogin bool
	// DevAccounts are the seeded development accounts' display names.
	DevAccounts []string
	// DevMatches registers the dev match-creation routes (local only).
	DevMatches bool
	Log        *slog.Logger
}

// Server holds the handler dependencies.
type Server struct {
	Deps
}

// New builds the HTTP handler.
func New(d Deps) http.Handler {
	s := &Server{Deps: d}
	mux := http.NewServeMux()
	mux.HandleFunc("GET /healthz", s.healthz)
	mux.HandleFunc("GET /readyz", s.readyz)
	if d.DevLogin {
		mux.HandleFunc("POST /v1/dev/login", s.devLoginHandler)
	}
	mux.HandleFunc("POST /v1/launch-codes", s.issueLaunchCode)
	mux.HandleFunc("POST /v1/game-sessions", s.redeemLaunchCode)
	mux.HandleFunc("GET /v1/me", s.me)
	s.routeAccounts(mux)
	s.routeSocial(mux)
	s.routeParty(mux)
	s.routeMatch(mux)
	s.routeOnboarding(mux)
	s.routeSelection(mux)
	s.routeMatchFound(mux)
	return mux
}

// authed wraps a handler that requires a game session.
func (s *Server) authed(h func(w http.ResponseWriter, r *http.Request, actor string)) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		token, ok := bearer(r)
		if !ok {
			writeError(w, http.StatusUnauthorized, "invalid_credentials")
			return
		}
		acct, err := s.Identity.AuthenticateGame(r.Context(), token)
		if err != nil {
			s.fail(w, err)
			return
		}
		h(w, r, acct.ID)
	}
}

type accountJSON struct {
	ID          string `json:"id"`
	DisplayName string `json:"displayName"`
}

type tokenJSON struct {
	Token     string       `json:"token"`
	ExpiresAt time.Time    `json:"expiresAt"`
	Account   *accountJSON `json:"account,omitempty"`
}

func (s *Server) healthz(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, map[string]string{"status": "ok"})
}

func (s *Server) readyz(w http.ResponseWriter, r *http.Request) {
	if err := s.Ready.Ping(r.Context()); err != nil {
		s.Log.Warn("readiness check failed", "err", err)
		writeError(w, http.StatusServiceUnavailable, "not_ready")
		return
	}
	writeJSON(w, http.StatusOK, map[string]string{"status": "ready"})
}

func (s *Server) devLoginHandler(w http.ResponseWriter, r *http.Request) {
	var req struct {
		AccountName string `json:"accountName"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	tok, acct, err := s.Identity.DevLogin(r.Context(), req.AccountName)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, tokenJSON{Token: tok.Token, ExpiresAt: tok.ExpiresAt, Account: toAccountJSON(acct)})
}

func (s *Server) issueLaunchCode(w http.ResponseWriter, r *http.Request) {
	token, ok := bearer(r)
	if !ok {
		writeError(w, http.StatusUnauthorized, "invalid_credentials")
		return
	}
	var req struct {
		BuildVersion string `json:"buildVersion"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	code, err := s.Identity.IssueLaunchCode(r.Context(), token, req.BuildVersion)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, tokenJSON{Token: code.Token, ExpiresAt: code.ExpiresAt})
}

func (s *Server) redeemLaunchCode(w http.ResponseWriter, r *http.Request) {
	var req struct {
		LaunchCode   string `json:"launchCode"`
		BuildVersion string `json:"buildVersion"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	tok, acct, err := s.Identity.RedeemLaunchCode(r.Context(), req.LaunchCode, req.BuildVersion)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, tokenJSON{Token: tok.Token, ExpiresAt: tok.ExpiresAt, Account: toAccountJSON(acct)})
}

func (s *Server) me(w http.ResponseWriter, r *http.Request) {
	token, ok := bearer(r)
	if !ok {
		writeError(w, http.StatusUnauthorized, "invalid_credentials")
		return
	}
	acct, err := s.Identity.AuthenticateGame(r.Context(), token)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, toAccountJSON(acct))
}

// decode reads a size-limited JSON body with no unknown fields.
func (s *Server) decode(w http.ResponseWriter, r *http.Request, dst any) bool {
	r.Body = http.MaxBytesReader(w, r.Body, s.BodyLimitBytes)
	dec := json.NewDecoder(r.Body)
	dec.DisallowUnknownFields()
	if err := dec.Decode(dst); err != nil {
		var tooLarge *http.MaxBytesError
		if errors.As(err, &tooLarge) {
			writeError(w, http.StatusRequestEntityTooLarge, "body_too_large")
			return false
		}
		writeError(w, http.StatusBadRequest, "malformed_request")
		return false
	}
	return true
}

// errorStatus maps domain errors to HTTP status and a stable error code.
var errorStatus = []struct {
	err    error
	status int
	code   string
}{
	{identity.ErrInvalidCredentials, http.StatusUnauthorized, "invalid_credentials"},
	{identity.ErrInvalidBuildVersion, http.StatusBadRequest, "invalid_build_version"},
	{identity.ErrDevLoginDisabled, http.StatusNotFound, "not_found"},
	{identity.ErrNotFound, http.StatusNotFound, "account_not_found"},

	{social.ErrSelf, http.StatusBadRequest, "cannot_target_self"},
	{social.ErrAccountNotFound, http.StatusNotFound, "account_not_found"},
	{social.ErrBlocked, http.StatusForbidden, "blocked"},
	{social.ErrAlreadyFriends, http.StatusConflict, "already_friends"},
	{social.ErrRequestNotFound, http.StatusNotFound, "friend_request_not_found"},
	{social.ErrNotFriends, http.StatusConflict, "not_friends"},

	{party.ErrNotInParty, http.StatusConflict, "not_in_party"},
	{party.ErrAlreadyInParty, http.StatusConflict, "already_in_party"},
	{party.ErrNotLeader, http.StatusForbidden, "not_leader"},
	{party.ErrNotMember, http.StatusNotFound, "not_a_member"},
	{party.ErrPartyFull, http.StatusConflict, "party_full"},
	{party.ErrPartyLocked, http.StatusConflict, "party_locked"},
	{party.ErrNotAllReady, http.StatusConflict, "not_all_ready"},
	{party.ErrNoMode, http.StatusConflict, "no_mode_selected"},
	{party.ErrUnknownMode, http.StatusBadRequest, "unknown_mode"},
	{party.ErrTooManyForMode, http.StatusConflict, "party_too_large_for_mode"},
	{party.ErrInvalidPrivacy, http.StatusBadRequest, "invalid_privacy"},
	{party.ErrSelf, http.StatusBadRequest, "cannot_target_self"},
	{party.ErrNotFriends, http.StatusForbidden, "not_friends"},
	{party.ErrBlocked, http.StatusForbidden, "blocked"},
	{party.ErrInviteNotFound, http.StatusNotFound, "invite_not_found"},
	{party.ErrPartyNotJoinable, http.StatusForbidden, "party_not_joinable"},
	{party.ErrPartyNotFound, http.StatusNotFound, "party_not_found"},
	{party.ErrModeUnavailable, http.StatusConflict, "mode_not_available"},
	{party.ErrMemberBusy, http.StatusConflict, "member_busy"},

	{matchmaking.ErrFoundNotFound, http.StatusNotFound, "match_found_not_found"},
	{matchmaking.ErrAlreadyDecided, http.StatusConflict, "already_answered"},
	{matchmaking.ErrFoundOver, http.StatusConflict, "match_found_over"},
	{matchmaking.ErrExpired, http.StatusConflict, "expired"},

	{account.ErrNotAStarter, http.StatusBadRequest, "not_a_starter"},
	{account.ErrAlreadyChosen, http.StatusConflict, "already_completed"},

	{selection.ErrTutorialRequired, http.StatusConflict, "tutorial_required"},
	{selection.ErrBusy, http.StatusConflict, "busy"},
	{selection.ErrPracticeDisabled, http.StatusConflict, "practice_disabled"},
	{selection.ErrSelectNotFound, http.StatusNotFound, "select_not_found"},
	{selection.ErrNotAvailable, http.StatusBadRequest, "not_available"},
	{selection.ErrAlreadyLocked, http.StatusConflict, "already_locked"},
	{selection.ErrExpired, http.StatusConflict, "expired"},
	{selection.ErrInvalidState, http.StatusConflict, "invalid_state"},
	{selection.ErrTaken, http.StatusConflict, "taken"},
	{selection.ErrCannotLeave, http.StatusConflict, "cannot_leave"},

	{match.ErrUnknownMode, http.StatusBadRequest, "unknown_mode"},
	{match.ErrInvalidRules, http.StatusBadRequest, "invalid_rules"},
	{match.ErrInvalidMap, http.StatusBadRequest, "invalid_map"},
	{match.ErrInvalidRoster, http.StatusBadRequest, "invalid_roster"},
	{match.ErrInvalidVanguard, http.StatusBadRequest, "invalid_vanguard"},
	{match.ErrInvalidFluxSpells, http.StatusBadRequest, "invalid_flux_spells"},
	{match.ErrAccountNotFound, http.StatusNotFound, "account_not_found"},
	{match.ErrAlreadyInMatch, http.StatusConflict, "already_in_match"},
	{match.ErrNoServerCapacity, http.StatusServiceUnavailable, "no_server_capacity"},
	{match.ErrAllocationFailed, http.StatusBadGateway, "allocation_failed"},
	{match.ErrMatchNotFound, http.StatusNotFound, "match_not_found"},
	{match.ErrUnauthorized, http.StatusUnauthorized, "invalid_credentials"},
	{match.ErrInvalidState, http.StatusConflict, "invalid_state"},
	{match.ErrInvalidResult, http.StatusBadRequest, "invalid_result"},
	{match.ErrResultConflict, http.StatusConflict, "result_conflict"},
}

func (s *Server) fail(w http.ResponseWriter, err error) {
	for _, e := range errorStatus {
		if errors.Is(err, e.err) {
			writeError(w, e.status, e.code)
			return
		}
	}
	s.Log.Error("request failed", "err", err)
	writeError(w, http.StatusInternalServerError, "internal_error")
}

func bearer(r *http.Request) (string, bool) {
	const scheme = "Bearer "
	h := r.Header.Get("Authorization")
	if !strings.HasPrefix(h, scheme) || len(h) == len(scheme) {
		return "", false
	}
	return h[len(scheme):], true
}

func toAccountJSON(a identity.Account) *accountJSON {
	return &accountJSON{ID: a.ID, DisplayName: a.DisplayName}
}

func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Cache-Control", "no-store")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(v)
}

func writeError(w http.ResponseWriter, status int, code string) {
	writeJSON(w, status, map[string]string{"error": code})
}
