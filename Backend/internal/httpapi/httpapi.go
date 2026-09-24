// Package httpapi exposes the backend over HTTP/JSON.
//
//	GET  /healthz              liveness
//	GET  /readyz               readiness (database reachable)
//	POST /v1/dev/login         {accountName} -> launcher session (local only)
//	POST /v1/launch-codes      Bearer launcher session, {buildVersion} -> launch code
//	POST /v1/game-sessions     {launchCode, buildVersion} -> game session
//	GET  /v1/me                Bearer game session -> account
package httpapi

import (
	"context"
	"encoding/json"
	"errors"
	"log/slog"
	"net/http"
	"strings"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
)

// Pinger reports whether a dependency is reachable.
type Pinger interface {
	Ping(ctx context.Context) error
}

// Server holds the handler dependencies.
type Server struct {
	identity       *identity.Service
	ready          Pinger
	bodyLimitBytes int64
	devLogin       bool
	log            *slog.Logger
}

// New builds the HTTP handler. The dev-login route exists only when devLogin is true.
func New(svc *identity.Service, ready Pinger, bodyLimitBytes int64, devLogin bool, log *slog.Logger) http.Handler {
	s := &Server{identity: svc, ready: ready, bodyLimitBytes: bodyLimitBytes, devLogin: devLogin, log: log}
	mux := http.NewServeMux()
	mux.HandleFunc("GET /healthz", s.healthz)
	mux.HandleFunc("GET /readyz", s.readyz)
	if devLogin {
		mux.HandleFunc("POST /v1/dev/login", s.devLoginHandler)
	}
	mux.HandleFunc("POST /v1/launch-codes", s.issueLaunchCode)
	mux.HandleFunc("POST /v1/game-sessions", s.redeemLaunchCode)
	mux.HandleFunc("GET /v1/me", s.me)
	return mux
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
	if err := s.ready.Ping(r.Context()); err != nil {
		s.log.Warn("readiness check failed", "err", err)
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
	tok, acct, err := s.identity.DevLogin(r.Context(), req.AccountName)
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
	code, err := s.identity.IssueLaunchCode(r.Context(), token, req.BuildVersion)
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
	tok, acct, err := s.identity.RedeemLaunchCode(r.Context(), req.LaunchCode, req.BuildVersion)
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
	acct, err := s.identity.AuthenticateGame(r.Context(), token)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, toAccountJSON(acct))
}

// decode reads a size-limited JSON body with no unknown fields.
func (s *Server) decode(w http.ResponseWriter, r *http.Request, dst any) bool {
	r.Body = http.MaxBytesReader(w, r.Body, s.bodyLimitBytes)
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

func (s *Server) fail(w http.ResponseWriter, err error) {
	switch {
	case errors.Is(err, identity.ErrInvalidCredentials):
		writeError(w, http.StatusUnauthorized, "invalid_credentials")
	case errors.Is(err, identity.ErrInvalidBuildVersion):
		writeError(w, http.StatusBadRequest, "invalid_build_version")
	case errors.Is(err, identity.ErrDevLoginDisabled):
		writeError(w, http.StatusNotFound, "not_found")
	default:
		s.log.Error("request failed", "err", err)
		writeError(w, http.StatusInternalServerError, "internal_error")
	}
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
