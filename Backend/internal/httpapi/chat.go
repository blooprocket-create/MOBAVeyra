package httpapi

import (
	"net/http"
	"strconv"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/chat"
)

// routeChat registers Party Chat, friend messages, champion-select chat and
// post-match chat (ADR-046).
func (s *Server) routeChat(mux *http.ServeMux) {
	if s.Chat == nil {
		return
	}
	mux.HandleFunc("GET /v1/me/chat", s.authed(s.pollChat))
	mux.HandleFunc("POST /v1/me/chat/party", s.authed(s.sendChat(chat.KindParty, "")))
	mux.HandleFunc("POST /v1/me/chat/direct/{accountId}", s.authed(s.sendChat(chat.KindDirect, "accountId")))
	mux.HandleFunc("POST /v1/me/chat/select", s.authed(s.sendChat(chat.KindSelect, "")))
	mux.HandleFunc("POST /v1/me/chat/matches/{matchId}", s.authed(s.sendChat(chat.KindPostMatch, "matchId")))
	mux.HandleFunc("DELETE /v1/me/chat/matches/{matchId}", s.authed(s.leavePostMatchChat))
	mux.HandleFunc("PUT /v1/me/chat/matches/{matchId}/mutes/{accountId}", s.authed(s.mutePostMatch(true)))
	mux.HandleFunc("DELETE /v1/me/chat/matches/{matchId}/mutes/{accountId}", s.authed(s.mutePostMatch(false)))
}

type chatMessageJSON struct {
	Seq          int64       `json:"seq"`
	Kind         string      `json:"kind"`
	Conversation string      `json:"conversation"`
	Sender       accountJSON `json:"sender"`
	// RecipientID is a direct message's other account; null otherwise.
	RecipientID *string `json:"recipientId"`
	Text        string  `json:"text"`
	SentAt      string  `json:"sentAt"`
	ClientID    string  `json:"clientId"`
}

func toChatMessageJSON(m chat.Message) chatMessageJSON {
	return chatMessageJSON{Seq: m.Seq, Kind: string(m.Kind), Conversation: m.Key, Sender: accountJSON{ID: m.SenderID, DisplayName: m.SenderName},
		RecipientID: textOrNil(m.RecipientID), Text: m.Text, SentAt: m.SentAt.UTC().Format(time.RFC3339Nano), ClientID: m.ClientID}
}

// pollChat returns what the player may read after ?after=, or its newest
// messages without it (ADR-046 §4).
func (s *Server) pollChat(w http.ResponseWriter, r *http.Request, actor string) {
	var after int64
	raw, hasCursor := r.URL.Query()["after"]
	if hasCursor {
		v, err := strconv.ParseInt(raw[0], 10, 64)
		if err != nil || v < 0 || len(raw) != 1 {
			writeError(w, http.StatusBadRequest, "invalid_cursor")
			return
		}
		after = v
	}
	page, err := s.Chat.Poll(r.Context(), actor, after, hasCursor)
	if err != nil {
		s.fail(w, err)
		return
	}
	out := make([]chatMessageJSON, 0, len(page.Messages))
	for _, m := range page.Messages {
		out = append(out, toChatMessageJSON(m))
	}
	writeJSON(w, http.StatusOK, map[string]any{"messages": out, "next": page.Next, "more": page.More})
}

// sendChat sends one message of a kind; target names the path value holding
// a direct message's friend or a post-match message's match. The client
// generates clientId, so a retry after a lost answer returns the first
// message (ADR-046 §3).
func (s *Server) sendChat(kind chat.Kind, target string) func(http.ResponseWriter, *http.Request, string) {
	return func(w http.ResponseWriter, r *http.Request, actor string) {
		var req struct {
			ClientID string `json:"clientId"`
			Text     string `json:"text"`
		}
		if !s.decode(w, r, &req) {
			return
		}
		send := chat.Send{Kind: kind, ClientID: req.ClientID, Text: req.Text}
		if target != "" {
			send.Target = r.PathValue(target)
		}
		m, err := s.Chat.Send(r.Context(), actor, send)
		if err != nil {
			s.fail(w, err)
			return
		}
		writeJSON(w, http.StatusOK, map[string]any{"message": toChatMessageJSON(m)})
	}
}

// leavePostMatchChat ends the player's part in a match's post-match chat as it
// leaves the results screen (UX-60).
func (s *Server) leavePostMatchChat(w http.ResponseWriter, r *http.Request, actor string) {
	if err := s.Chat.LeavePostMatch(r.Context(), actor, r.PathValue("matchId")); err != nil {
		s.fail(w, err)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

// mutePostMatch mutes or unmutes a participant in a match's post-match chat,
// for the player only (ADR-046 §5).
func (s *Server) mutePostMatch(muted bool) func(http.ResponseWriter, *http.Request, string) {
	return func(w http.ResponseWriter, r *http.Request, actor string) {
		if err := s.Chat.MutePostMatch(r.Context(), actor, r.PathValue("matchId"), r.PathValue("accountId"), muted); err != nil {
			s.fail(w, err)
			return
		}
		w.WriteHeader(http.StatusNoContent)
	}
}
