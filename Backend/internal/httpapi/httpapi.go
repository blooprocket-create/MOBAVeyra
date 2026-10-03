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
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/chat"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/conduct"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/dodges"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/favorites"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/lobby"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/matchmaking"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/profile"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/progression"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/settings"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

// Pinger reports whether a dependency is reachable.
type Pinger interface {
	Ping(ctx context.Context) error
}

// ModeInfo describes a matchmade mode to clients.
type ModeInfo struct {
	ID      string `json:"id"`
	Enabled bool   `json:"enabled"`
	// Category is the Play page's group: ranked, casual or ai.
	Category            string `json:"category"`
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
	// Lobby is optional; without it no custom-lobby routes are registered.
	Lobby *lobby.Service
	// Match is optional; without it no match routes are registered.
	Match *match.Service
	// Account is optional; without it no onboarding routes are registered.
	Account *account.Service
	// Progression is optional; without it no progression, Collection or purchase
	// routes are registered, and match results carry no rewards (ADR-045 §7).
	Progression *progression.Service
	// Selection is optional; without it no practice or champion-select routes
	// are registered.
	Selection *selection.Service
	// Matchmaking is optional; without it no Match Found routes are registered.
	Matchmaking *matchmaking.Service
	// Settings is optional; without it no account settings routes are
	// registered (ADR-024 §1).
	Settings *settings.Service
	// Chat is optional; without it no chat routes are registered (ADR-046).
	Chat *chat.Service
	// Conduct is optional; without it no report or commendation routes are
	// registered (ADR-047).
	Conduct *conduct.Service
	// Profile is optional; without it no profile routes are registered
	// (ADR-048).
	Profile *profile.Service
	// Favorites is optional; without it no favorite routes are registered
	// (ADR-058 §5).
	Favorites *favorites.Service
	// Dodges answers how long the player cannot queue after leaving a
	// matchmade champion select (ADR-060 §3).
	Dodges *dodges.Service
	Modes  []ModeInfo
	Ready  Pinger
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
	// Player sign-in and registration through the identity provider (ADR-038).
	mux.HandleFunc("POST /v1/login", s.playerLogin)
	mux.HandleFunc("POST /v1/register", s.register)
	mux.HandleFunc("POST /v1/launch-codes", s.issueLaunchCode)
	mux.HandleFunc("POST /v1/game-sessions", s.redeemLaunchCode)
	mux.HandleFunc("GET /v1/me", s.me)
	s.routeAccounts(mux)
	s.routeSocial(mux)
	s.routeParty(mux)
	if d.Lobby != nil {
		s.routeLobby(mux)
	}
	s.routeMatch(mux)
	s.routeOnboarding(mux)
	s.routeProgression(mux)
	s.routeSelection(mux)
	s.routeMatchFound(mux)
	s.routeSettings(mux)
	s.routeChat(mux)
	s.routeConduct(mux)
	s.routeProfile(mux)
	s.routeNames(mux)
	s.routeFavorites(mux)
	s.routeRestriction(mux)
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

// playerLogin exchanges an identity-provider token for a launcher session.
func (s *Server) playerLogin(w http.ResponseWriter, r *http.Request) {
	var req struct {
		ProviderToken string `json:"providerToken"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	tok, acct, err := s.Identity.PlayerLogin(r.Context(), req.ProviderToken)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, tokenJSON{Token: tok.Token, ExpiresAt: tok.ExpiresAt, Account: toAccountJSON(acct)})
}

// register creates the Veyra account for an identity-provider token, with the
// display name the player chose, and returns a launcher session for it.
func (s *Server) register(w http.ResponseWriter, r *http.Request) {
	var req struct {
		ProviderToken string `json:"providerToken"`
		DisplayName   string `json:"displayName"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	tok, acct, err := s.Identity.Register(r.Context(), req.ProviderToken, req.DisplayName)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusCreated, tokenJSON{Token: tok.Token, ExpiresAt: tok.ExpiresAt, Account: toAccountJSON(acct)})
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
	{identity.ErrPlayerLoginDisabled, http.StatusNotFound, "not_found"},
	{identity.ErrNotRegistered, http.StatusNotFound, "not_registered"},
	{identity.ErrAlreadyRegistered, http.StatusConflict, "already_registered"},
	{identity.ErrDisplayNameTaken, http.StatusConflict, "display_name_taken"},
	{identity.ErrInvalidDisplayName, http.StatusBadRequest, "invalid_display_name"},
	{identity.ErrRenameCooldown, http.StatusConflict, "rename_cooldown"},
	{identity.ErrSameDisplayName, http.StatusConflict, "same_display_name"},
	{identity.ErrInvalidCurrency, http.StatusBadRequest, "invalid_currency"},
	{identity.ErrRenameRequired, http.StatusConflict, "rename_required"},
	{identity.ErrNamesDisabled, http.StatusNotFound, "not_found"},

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
	{lobby.ErrNotInLobby, http.StatusConflict, "not_in_lobby"},
	{settings.ErrTooLarge, http.StatusRequestEntityTooLarge, "settings_too_large"},
	{settings.ErrInvalid, http.StatusBadRequest, "bad_settings"},
	{lobby.ErrAlreadyInLobby, http.StatusConflict, "already_in_lobby"},
	{lobby.ErrLobbyNotFound, http.StatusNotFound, "lobby_not_found"},
	{lobby.ErrNotHost, http.StatusForbidden, "not_host"},
	{lobby.ErrNotMember, http.StatusNotFound, "not_a_member"},
	{lobby.ErrLobbyFull, http.StatusConflict, "lobby_full"},
	{lobby.ErrLobbyLocked, http.StatusConflict, "lobby_locked"},
	{lobby.ErrNoSuchSlot, http.StatusBadRequest, "no_such_seat"},
	{lobby.ErrSlotTaken, http.StatusConflict, "seat_taken"},
	{lobby.ErrNotABot, http.StatusNotFound, "not_a_bot"},
	{lobby.ErrUnknownVanguard, http.StatusBadRequest, "invalid_vanguard"},
	{lobby.ErrUnknownDifficulty, http.StatusBadRequest, "invalid_difficulty"},
	{lobby.ErrDuplicateVanguard, http.StatusConflict, "vanguard_taken"},
	{lobby.ErrGoldOutOfRange, http.StatusBadRequest, "starting_gold_out_of_range"},
	{lobby.ErrVictoryNeedsSides, http.StatusConflict, "victory_needs_both_sides"},
	{lobby.ErrSelf, http.StatusBadRequest, "cannot_target_self"},
	{lobby.ErrNotFriends, http.StatusForbidden, "not_friends"},
	{lobby.ErrBlocked, http.StatusForbidden, "blocked"},
	{lobby.ErrInviteNotFound, http.StatusNotFound, "invite_not_found"},
	{lobby.ErrBusy, http.StatusConflict, "member_busy"},
	{lobby.ErrNoHuman, http.StatusConflict, "no_human"},
	{lobby.ErrLaunchUnavailable, http.StatusConflict, "launch_unavailable"},

	{matchmaking.ErrFoundNotFound, http.StatusNotFound, "match_found_not_found"},
	{matchmaking.ErrAlreadyDecided, http.StatusConflict, "already_answered"},
	{matchmaking.ErrFoundOver, http.StatusConflict, "match_found_over"},
	{matchmaking.ErrExpired, http.StatusConflict, "expired"},

	{account.ErrNotAStarter, http.StatusBadRequest, "not_a_starter"},
	{account.ErrAlreadyChosen, http.StatusConflict, "already_completed"},
	{progression.ErrNotForSale, http.StatusBadRequest, "not_for_sale"},
	{progression.ErrAlreadyOwned, http.StatusConflict, "already_owned"},
	{progression.ErrInsufficient, http.StatusConflict, "insufficient_balance"},
	{progression.ErrInvalidPurchase, http.StatusBadRequest, "invalid_purchase"},
	{progression.ErrPurchaseConflict, http.StatusConflict, "purchase_conflict"},

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
	{selection.ErrNotYourTurn, http.StatusConflict, "not_your_turn"},
	{selection.ErrCannotTrade, http.StatusConflict, "cannot_trade"},

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
	{match.ErrInvalidFilter, http.StatusBadRequest, "invalid_filter"},
	{match.ErrInvalidCursor, http.StatusBadRequest, "invalid_cursor"},

	{chat.ErrNotInParty, http.StatusConflict, "not_in_party"},
	{chat.ErrNotFriends, http.StatusForbidden, "not_friends"},
	{chat.ErrBlocked, http.StatusForbidden, "blocked"},
	{chat.ErrNoSelect, http.StatusConflict, "no_select"},
	{chat.ErrNotParticipant, http.StatusNotFound, "not_participant"},
	{chat.ErrPostMatchClosed, http.StatusConflict, "postmatch_closed"},
	{chat.ErrAllChatOff, http.StatusConflict, "all_chat_off"},
	{chat.ErrEmptyMessage, http.StatusBadRequest, "empty_message"},
	{chat.ErrMessageTooLong, http.StatusBadRequest, "message_too_long"},
	{chat.ErrRateLimited, http.StatusTooManyRequests, "rate_limited"},
	{chat.ErrInvalidMessage, http.StatusBadRequest, "invalid_message"},
	{chat.ErrClientIDConflict, http.StatusConflict, "client_id_conflict"},
	{chat.ErrConversationChanged, http.StatusConflict, "conversation_changed"},

	{conduct.ErrNotParticipant, http.StatusNotFound, "not_participant"},
	{conduct.ErrUnknownPlayer, http.StatusNotFound, "unknown_player"},
	{conduct.ErrInvalidReason, http.StatusBadRequest, "invalid_reason"},
	{conduct.ErrDetailsTooLong, http.StatusBadRequest, "details_too_long"},
	{conduct.ErrReportClosed, http.StatusConflict, "report_closed"},
	{conduct.ErrInvalidRequest, http.StatusBadRequest, "invalid_report"},
	{conduct.ErrNotTeammate, http.StatusConflict, "not_teammate"},
	{conduct.ErrCommendClosed, http.StatusConflict, "commend_closed"},
	{conduct.ErrAlreadyCommended, http.StatusConflict, "already_commended"},

	{profile.ErrUnavailable, http.StatusNotFound, "profile_unavailable"},
	{profile.ErrHistoryPrivate, http.StatusForbidden, "history_private"},
	{profile.ErrInvalidIcon, http.StatusBadRequest, "invalid_icon"},
	{profile.ErrInvalidBackground, http.StatusBadRequest, "invalid_background"},
	{profile.ErrNotOwned, http.StatusConflict, "not_owned"},

	{favorites.ErrUnknownVanguard, http.StatusBadRequest, "unknown_vanguard"},
	{favorites.ErrPlaying, http.StatusConflict, "playing"},
	{party.ErrQueueRestricted, http.StatusConflict, "queue_restricted"},
	{party.ErrInviteeInMatch, http.StatusConflict, "invitee_in_match"},
	{favorites.ErrFull, http.StatusConflict, "favorites_full"},
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
