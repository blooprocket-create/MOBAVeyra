package httpapi

import (
	"context"
	"errors"
	"net/http"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
)

func (s *Server) routeParty(mux *http.ServeMux) {
	mux.HandleFunc("GET /v1/modes", s.authed(s.listModes))
	mux.HandleFunc("GET /v1/party", s.authed(s.getParty))
	mux.HandleFunc("PUT /v1/party/mode", s.authed(s.selectMode))
	mux.HandleFunc("PUT /v1/party/privacy", s.authed(s.setPrivacy))
	mux.HandleFunc("PUT /v1/party/ready", s.authed(s.setReady))
	mux.HandleFunc("PUT /v1/party/leader", s.authed(s.transferLeader))
	mux.HandleFunc("POST /v1/party/leave", s.authed(s.leaveParty))
	mux.HandleFunc("DELETE /v1/party/members/{accountId}", s.authed(s.kick))
	mux.HandleFunc("POST /v1/party/queue", s.authed(s.startQueue))
	mux.HandleFunc("DELETE /v1/party/queue", s.authed(s.cancelQueue))
	mux.HandleFunc("GET /v1/party/invites", s.authed(s.listInvites))
	mux.HandleFunc("POST /v1/party/invites", s.authed(s.invite))
	mux.HandleFunc("POST /v1/party/invites/{inviteId}/accept", s.authed(s.acceptInvite))
	mux.HandleFunc("POST /v1/party/invites/{inviteId}/decline", s.authed(s.declineInvite))
	mux.HandleFunc("POST /v1/parties/{partyId}/join", s.authed(s.joinParty))
}

type memberJSON struct {
	AccountID   string `json:"accountId"`
	DisplayName string `json:"displayName"`
	Ready       bool   `json:"ready"`
	Leader      bool   `json:"leader"`
}

type partyJSON struct {
	ID      string       `json:"id"`
	Mode    string       `json:"mode"`
	Privacy string       `json:"privacy"`
	Status  string       `json:"status"`
	Members []memberJSON `json:"members"`
}

type inviteJSON struct {
	ID        string      `json:"id"`
	PartyID   string      `json:"partyId"`
	Inviter   accountJSON `json:"inviter"`
	ExpiresAt time.Time   `json:"expiresAt"`
}

func (s *Server) partyJSON(ctx context.Context, p party.Party) (partyJSON, error) {
	ids := make([]string, len(p.Members))
	for i, m := range p.Members {
		ids[i] = m.AccountID
	}
	accounts, err := s.Identity.Accounts(ctx, ids)
	if err != nil {
		return partyJSON{}, err
	}
	out := partyJSON{ID: p.ID, Mode: p.Mode, Privacy: string(p.Privacy), Status: string(p.Status)}
	for _, m := range p.Members {
		out.Members = append(out.Members, memberJSON{
			AccountID:   m.AccountID,
			DisplayName: accounts[m.AccountID].DisplayName,
			Ready:       m.Ready,
			Leader:      m.AccountID == p.LeaderID,
		})
	}
	return out, nil
}

// respondParty writes {"party": ...}, or {"party": null} when the actor is
// no longer in a party.
func (s *Server) respondParty(w http.ResponseWriter, r *http.Request, p party.Party, err error) {
	if errors.Is(err, party.ErrNotInParty) {
		writeJSON(w, http.StatusOK, map[string]any{"party": nil})
		return
	}
	if err != nil {
		s.fail(w, err)
		return
	}
	body, err := s.partyJSON(r.Context(), p)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"party": body})
}

func (s *Server) listModes(w http.ResponseWriter, _ *http.Request, _ string) {
	writeJSON(w, http.StatusOK, map[string][]ModeInfo{"modes": s.Modes})
}

func (s *Server) getParty(w http.ResponseWriter, r *http.Request, actor string) {
	p, err := s.Party.Get(r.Context(), actor)
	s.respondParty(w, r, p, err)
}

func (s *Server) selectMode(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		Mode string `json:"mode"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	p, err := s.Party.SelectMode(r.Context(), actor, req.Mode)
	s.respondPartyOrFail(w, r, p, err)
}

func (s *Server) setPrivacy(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		Privacy string `json:"privacy"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	p, err := s.Party.SetPrivacy(r.Context(), actor, party.Privacy(req.Privacy))
	s.respondPartyOrFail(w, r, p, err)
}

func (s *Server) setReady(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		Ready *bool `json:"ready"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	if req.Ready == nil {
		writeError(w, http.StatusBadRequest, "malformed_request")
		return
	}
	p, err := s.Party.SetReady(r.Context(), actor, *req.Ready)
	s.respondPartyOrFail(w, r, p, err)
}

func (s *Server) transferLeader(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		AccountID string `json:"accountId"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	p, err := s.Party.TransferLeader(r.Context(), actor, req.AccountID)
	s.respondPartyOrFail(w, r, p, err)
}

func (s *Server) leaveParty(w http.ResponseWriter, r *http.Request, actor string) {
	s.done(w, s.Party.Leave(r.Context(), actor))
}

func (s *Server) kick(w http.ResponseWriter, r *http.Request, actor string) {
	p, err := s.Party.Kick(r.Context(), actor, r.PathValue("accountId"))
	s.respondPartyOrFail(w, r, p, err)
}

func (s *Server) startQueue(w http.ResponseWriter, r *http.Request, actor string) {
	p, err := s.Party.StartQueue(r.Context(), actor)
	s.respondPartyOrFail(w, r, p, err)
}

func (s *Server) cancelQueue(w http.ResponseWriter, r *http.Request, actor string) {
	p, err := s.Party.CancelQueue(r.Context(), actor)
	s.respondPartyOrFail(w, r, p, err)
}

func (s *Server) listInvites(w http.ResponseWriter, r *http.Request, actor string) {
	invites, err := s.Party.Invites(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	inviters := make([]string, len(invites))
	for i, inv := range invites {
		inviters[i] = inv.InviterID
	}
	accounts, err := s.Identity.Accounts(r.Context(), inviters)
	if err != nil {
		s.fail(w, err)
		return
	}
	out := make([]inviteJSON, 0, len(invites))
	for _, inv := range invites {
		out = append(out, inviteJSON{
			ID:        inv.ID,
			PartyID:   inv.PartyID,
			Inviter:   *toAccountJSON(accounts[inv.InviterID]),
			ExpiresAt: inv.ExpiresAt,
		})
	}
	writeJSON(w, http.StatusOK, map[string][]inviteJSON{"invites": out})
}

func (s *Server) invite(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		AccountID string `json:"accountId"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	inv, err := s.Party.Invite(r.Context(), actor, req.AccountID)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"id": inv.ID, "expiresAt": inv.ExpiresAt})
}

func (s *Server) acceptInvite(w http.ResponseWriter, r *http.Request, actor string) {
	p, err := s.Party.AcceptInvite(r.Context(), actor, r.PathValue("inviteId"))
	s.respondPartyOrFail(w, r, p, err)
}

func (s *Server) declineInvite(w http.ResponseWriter, r *http.Request, actor string) {
	s.done(w, s.Party.DeclineInvite(r.Context(), actor, r.PathValue("inviteId")))
}

func (s *Server) joinParty(w http.ResponseWriter, r *http.Request, actor string) {
	p, err := s.Party.JoinPublic(r.Context(), actor, r.PathValue("partyId"))
	s.respondPartyOrFail(w, r, p, err)
}

// respondPartyOrFail reports every error, including not_in_party, for
// actions that require a party.
func (s *Server) respondPartyOrFail(w http.ResponseWriter, r *http.Request, p party.Party, err error) {
	if err != nil {
		s.fail(w, err)
		return
	}
	s.respondParty(w, r, p, nil)
}
