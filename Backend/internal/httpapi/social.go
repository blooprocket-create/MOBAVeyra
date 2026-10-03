package httpapi

import (
	"context"
	"net/http"
	"sort"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/presence"
)

func (s *Server) routeAccounts(mux *http.ServeMux) {
	mux.HandleFunc("GET /v1/accounts", s.authed(s.lookupAccount))
}

func (s *Server) routeSocial(mux *http.ServeMux) {
	mux.HandleFunc("GET /v1/friends", s.authed(s.listFriends))
	mux.HandleFunc("DELETE /v1/friends/{accountId}", s.authed(s.removeFriend))
	mux.HandleFunc("POST /v1/friends/requests", s.authed(s.sendFriendRequest))
	mux.HandleFunc("POST /v1/friends/requests/{accountId}/accept", s.authed(s.acceptFriendRequest))
	mux.HandleFunc("POST /v1/friends/requests/{accountId}/decline", s.authed(s.declineFriendRequest))
	mux.HandleFunc("DELETE /v1/friends/requests/{accountId}", s.authed(s.cancelFriendRequest))
	mux.HandleFunc("GET /v1/blocks", s.authed(s.listBlocks))
	mux.HandleFunc("PUT /v1/blocks/{accountId}", s.authed(s.block))
	mux.HandleFunc("DELETE /v1/blocks/{accountId}", s.authed(s.unblock))
}

// lookupAccount finds an account by exact display name: ?displayName=X.
func (s *Server) lookupAccount(w http.ResponseWriter, r *http.Request, _ string) {
	name := r.URL.Query().Get("displayName")
	if name == "" {
		writeError(w, http.StatusBadRequest, "malformed_request")
		return
	}
	acct, err := s.Identity.LookupAccount(r.Context(), name)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, toAccountJSON(acct))
}

// accountList resolves IDs to accounts for display, in a stable order.
func (s *Server) accountList(ctx context.Context, ids []string) ([]accountJSON, error) {
	accounts, err := s.Identity.Accounts(ctx, ids)
	if err != nil {
		return nil, err
	}
	out := make([]accountJSON, 0, len(ids))
	for _, id := range ids {
		if a, ok := accounts[id]; ok {
			out = append(out, *toAccountJSON(a))
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].DisplayName < out[j].DisplayName })
	return out, nil
}

func (s *Server) listFriends(w http.ResponseWriter, r *http.Request, actor string) {
	ctx := r.Context()
	lists := []struct {
		key   string
		fetch func(context.Context, string) ([]string, error)
	}{
		{"friends", s.Social.Friends},
		{"incomingRequests", s.Social.IncomingRequests},
		{"outgoingRequests", s.Social.OutgoingRequests},
	}
	out := map[string]any{}
	var friends []string
	for _, l := range lists {
		ids, err := l.fetch(ctx, actor)
		if err != nil {
			s.fail(w, err)
			return
		}
		if l.key == "friends" {
			friends = ids
		}
		list, err := s.accountList(ctx, ids)
		if err != nil {
			s.fail(w, err)
			return
		}
		out[l.key] = list
	}
	// Each friend's status as the player sees it (ADR-061 §2–§3).
	status := map[string]string{}
	if s.Presence != nil {
		shown, err := s.Presence.StatusFor(ctx, actor, friends)
		if err != nil {
			s.fail(w, err)
			return
		}
		for id, st := range shown {
			status[id] = string(st)
		}
		out["presence"] = status
	}
	// The friends whose Public party the player may join directly, by account:
	// the party to ask to join (Parties & Social Bible §1). A friend who shows
	// offline offers none, which would show them online (ADR-061 §3).
	joinable := map[string]string{}
	if s.Party != nil {
		found, err := s.Party.JoinableParties(ctx, actor, friends)
		if err != nil {
			s.fail(w, err)
			return
		}
		for id, partyID := range found {
			if s.Presence == nil || status[id] != string(presence.Offline) {
				joinable[id] = partyID
			}
		}
	}
	out["joinableParties"] = joinable
	writeJSON(w, http.StatusOK, out)
}

func (s *Server) sendFriendRequest(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		AccountID string `json:"accountId"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	outcome, err := s.Social.SendFriendRequest(r.Context(), actor, req.AccountID)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]string{"outcome": string(outcome)})
}

func (s *Server) acceptFriendRequest(w http.ResponseWriter, r *http.Request, actor string) {
	s.done(w, s.Social.AcceptFriendRequest(r.Context(), actor, r.PathValue("accountId")))
}

func (s *Server) declineFriendRequest(w http.ResponseWriter, r *http.Request, actor string) {
	s.done(w, s.Social.DeclineFriendRequest(r.Context(), actor, r.PathValue("accountId")))
}

func (s *Server) cancelFriendRequest(w http.ResponseWriter, r *http.Request, actor string) {
	s.done(w, s.Social.CancelFriendRequest(r.Context(), actor, r.PathValue("accountId")))
}

func (s *Server) removeFriend(w http.ResponseWriter, r *http.Request, actor string) {
	s.done(w, s.Social.RemoveFriend(r.Context(), actor, r.PathValue("accountId")))
}

func (s *Server) listBlocks(w http.ResponseWriter, r *http.Request, actor string) {
	ids, err := s.Social.Blocks(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	list, err := s.accountList(r.Context(), ids)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string][]accountJSON{"blocked": list})
}

// block records the block and applies its party consequences in one unit of
// work, so a block can never be committed while the two accounts still share
// a party or a pending invite.
func (s *Server) block(w http.ResponseWriter, r *http.Request, actor string) {
	target := r.PathValue("accountId")
	s.done(w, s.Atomic(r.Context(), func(ctx context.Context) error {
		if err := s.Social.Block(ctx, actor, target); err != nil {
			return err
		}
		if s.Lobby != nil {
			if err := s.Lobby.OnBlock(ctx, actor, target); err != nil {
				return err
			}
		}
		return s.Party.OnBlock(ctx, actor, target)
	}))
}

func (s *Server) unblock(w http.ResponseWriter, r *http.Request, actor string) {
	s.done(w, s.Social.Unblock(r.Context(), actor, r.PathValue("accountId")))
}

// done writes 204 on success or the mapped error.
func (s *Server) done(w http.ResponseWriter, err error) {
	if err != nil {
		s.fail(w, err)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}
