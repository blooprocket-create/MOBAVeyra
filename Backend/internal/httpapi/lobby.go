package httpapi

import (
	"context"
	"errors"
	"net/http"
	"strconv"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/lobby"
)

func (s *Server) routeLobby(mux *http.ServeMux) {
	mux.HandleFunc("GET /v1/lobby", s.authed(s.getLobby))
	mux.HandleFunc("POST /v1/lobby", s.authed(s.createLobby))
	mux.HandleFunc("POST /v1/lobby/leave", s.authed(s.leaveLobby))
	mux.HandleFunc("DELETE /v1/lobby/members/{accountId}", s.authed(s.kickFromLobby))
	mux.HandleFunc("PUT /v1/lobby/members/{accountId}/seat", s.authed(s.moveInLobby))
	mux.HandleFunc("PUT /v1/lobby/seats/{side}/{index}/bot", s.authed(s.setLobbyBot))
	mux.HandleFunc("DELETE /v1/lobby/seats/{side}/{index}/bot", s.authed(s.removeLobbyBot))
	mux.HandleFunc("PUT /v1/lobby/settings", s.authed(s.setLobbySettings))
	mux.HandleFunc("GET /v1/lobby/invites", s.authed(s.listLobbyInvites))
	mux.HandleFunc("POST /v1/lobby/invites", s.authed(s.inviteToLobby))
	mux.HandleFunc("POST /v1/lobby/invites/{inviteId}/accept", s.authed(s.acceptLobbyInvite))
	mux.HandleFunc("POST /v1/lobby/invites/{inviteId}/decline", s.authed(s.declineLobbyInvite))
}

// lobbySeatJSON is one seat. Kind is "empty", "human" or "bot"; the fields
// that do not apply to it are empty.
type lobbySeatJSON struct {
	Side        string `json:"side"`
	Index       int    `json:"index"`
	Kind        string `json:"kind"`
	AccountID   string `json:"accountId"`
	DisplayName string `json:"displayName"`
	Host        bool   `json:"host"`
	VanguardID  string `json:"vanguardId"`
	Difficulty  string `json:"difficulty"`
}

type lobbySettingsJSON struct {
	VictoryEnabled bool `json:"victoryEnabled"`
	// StartingGold is null for the game's own starting Gold.
	StartingGold *float64 `json:"startingGold"`
}

type goldRangeJSON struct {
	Min float64 `json:"min"`
	Max float64 `json:"max"`
}

type lobbyJSON struct {
	ID            string `json:"id"`
	HostAccountID string `json:"hostAccountId"`
	// Status is open, or selecting while the champion select it launched runs.
	Status            string            `json:"status"`
	PlayersPerSide    int               `json:"playersPerSide"`
	Settings          lobbySettingsJSON `json:"settings"`
	StartingGoldRange goldRangeJSON     `json:"startingGoldRange"`
	// Seats are side A's, then side B's, each in order.
	Seats []lobbySeatJSON `json:"seats"`
}

type lobbyInviteJSON struct {
	ID        string      `json:"id"`
	LobbyID   string      `json:"lobbyId"`
	Inviter   accountJSON `json:"inviter"`
	ExpiresAt time.Time   `json:"expiresAt"`
}

func (s *Server) lobbyJSON(ctx context.Context, l lobby.Lobby) (lobbyJSON, error) {
	accounts, err := s.Identity.Accounts(ctx, l.MemberIDs())
	if err != nil {
		return lobbyJSON{}, err
	}
	limits := s.Lobby.Limits()
	out := lobbyJSON{
		ID:                l.ID,
		HostAccountID:     l.HostID,
		Status:            string(l.Status),
		PlayersPerSide:    limits.PlayersPerSide,
		Settings:          lobbySettingsJSON{VictoryEnabled: l.Settings.VictoryEnabled, StartingGold: l.Settings.StartingGold},
		StartingGoldRange: goldRangeJSON{Min: limits.StartingGoldMin, Max: limits.StartingGoldMax},
		Seats:             []lobbySeatJSON{},
	}
	for _, side := range lobby.Sides {
		for i := 0; i < limits.PlayersPerSide; i++ {
			seat := lobbySeatJSON{Side: side, Index: i, Kind: "empty"}
			member, bot := l.Occupant(lobby.Seat{Side: side, Index: i})
			switch {
			case member != nil:
				seat.Kind, seat.AccountID, seat.DisplayName, seat.Host = "human", member.AccountID, accounts[member.AccountID].DisplayName, member.AccountID == l.HostID
			case bot != nil:
				seat.Kind, seat.VanguardID, seat.Difficulty = "bot", bot.VanguardID, bot.Difficulty
			}
			out.Seats = append(out.Seats, seat)
		}
	}
	return out, nil
}

// respondLobby writes {"lobby": ...}, or {"lobby": null} when the actor is in
// no lobby.
func (s *Server) respondLobby(w http.ResponseWriter, r *http.Request, l lobby.Lobby, err error) {
	if errors.Is(err, lobby.ErrNotInLobby) {
		writeJSON(w, http.StatusOK, map[string]any{"lobby": nil})
		return
	}
	if err != nil {
		s.fail(w, err)
		return
	}
	body, err := s.lobbyJSON(r.Context(), l)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"lobby": body})
}

// respondLobbyOrFail reports every error, including not_in_lobby, for actions
// that need a lobby.
func (s *Server) respondLobbyOrFail(w http.ResponseWriter, r *http.Request, l lobby.Lobby, err error) {
	if err != nil {
		s.fail(w, err)
		return
	}
	s.respondLobby(w, r, l, nil)
}

func seatFromPath(r *http.Request) lobby.Seat {
	index, err := strconv.Atoi(r.PathValue("index"))
	if err != nil {
		index = -1
	}
	return lobby.Seat{Side: r.PathValue("side"), Index: index}
}

func (s *Server) getLobby(w http.ResponseWriter, r *http.Request, actor string) {
	l, err := s.Lobby.Get(r.Context(), actor)
	s.respondLobby(w, r, l, err)
}

func (s *Server) createLobby(w http.ResponseWriter, r *http.Request, actor string) {
	l, err := s.Lobby.Create(r.Context(), actor)
	s.respondLobbyOrFail(w, r, l, err)
}

func (s *Server) leaveLobby(w http.ResponseWriter, r *http.Request, actor string) {
	s.done(w, s.Lobby.Leave(r.Context(), actor))
}

func (s *Server) kickFromLobby(w http.ResponseWriter, r *http.Request, actor string) {
	l, err := s.Lobby.Kick(r.Context(), actor, r.PathValue("accountId"))
	s.respondLobbyOrFail(w, r, l, err)
}

func (s *Server) moveInLobby(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		Side  string `json:"side"`
		Index *int   `json:"index"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	if req.Index == nil {
		writeError(w, http.StatusBadRequest, "malformed_request")
		return
	}
	l, err := s.Lobby.Move(r.Context(), actor, r.PathValue("accountId"), lobby.Seat{Side: req.Side, Index: *req.Index})
	s.respondLobbyOrFail(w, r, l, err)
}

func (s *Server) setLobbyBot(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		VanguardID string `json:"vanguardId"`
		Difficulty string `json:"difficulty"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	l, err := s.Lobby.SetBot(r.Context(), actor, lobby.Bot{Seat: seatFromPath(r), VanguardID: req.VanguardID, Difficulty: req.Difficulty})
	s.respondLobbyOrFail(w, r, l, err)
}

func (s *Server) removeLobbyBot(w http.ResponseWriter, r *http.Request, actor string) {
	l, err := s.Lobby.RemoveBot(r.Context(), actor, seatFromPath(r))
	s.respondLobbyOrFail(w, r, l, err)
}

func (s *Server) setLobbySettings(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		VictoryEnabled *bool    `json:"victoryEnabled"`
		StartingGold   *float64 `json:"startingGold"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	if req.VictoryEnabled == nil {
		writeError(w, http.StatusBadRequest, "malformed_request")
		return
	}
	l, err := s.Lobby.SetSettings(r.Context(), actor, *req.VictoryEnabled, req.StartingGold)
	s.respondLobbyOrFail(w, r, l, err)
}

func (s *Server) listLobbyInvites(w http.ResponseWriter, r *http.Request, actor string) {
	invites, err := s.Lobby.Invites(r.Context(), actor)
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
	out := make([]lobbyInviteJSON, 0, len(invites))
	for _, inv := range invites {
		out = append(out, lobbyInviteJSON{ID: inv.ID, LobbyID: inv.LobbyID, Inviter: *toAccountJSON(accounts[inv.InviterID]), ExpiresAt: inv.ExpiresAt})
	}
	writeJSON(w, http.StatusOK, map[string][]lobbyInviteJSON{"invites": out})
}

func (s *Server) inviteToLobby(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		AccountID string `json:"accountId"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	inv, err := s.Lobby.Invite(r.Context(), actor, req.AccountID)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"id": inv.ID, "expiresAt": inv.ExpiresAt})
}

func (s *Server) acceptLobbyInvite(w http.ResponseWriter, r *http.Request, actor string) {
	l, err := s.Lobby.AcceptInvite(r.Context(), actor, r.PathValue("inviteId"))
	s.respondLobbyOrFail(w, r, l, err)
}

func (s *Server) declineLobbyInvite(w http.ResponseWriter, r *http.Request, actor string) {
	s.done(w, s.Lobby.DeclineInvite(r.Context(), actor, r.PathValue("inviteId")))
}
