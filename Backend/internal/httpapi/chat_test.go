package httpapi

import (
	"context"
	"fmt"
	"net/http"
	"slices"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/chat"
)

// chatDomains stands in for the domains chat asks about readers: every test
// account shares one party, and the first two have played one ended match.
type chatDomains struct {
	friends map[string]bool
	played  chat.PlayedMatch
}

func (d *chatDomains) PartyOf(context.Context, string) (chat.Membership, bool, error) {
	return chat.Membership{PartyID: "party-1"}, true, nil
}
func (d *chatDomains) AreFriends(_ context.Context, a, b string) (bool, error) {
	return d.friends[chat.DirectKey(a, b)], nil
}
func (d *chatDomains) BlockedWithAny(context.Context, string, []string) (bool, error) {
	return false, nil
}
func (d *chatDomains) TeamOf(context.Context, string) (chat.SelectTeam, bool, error) {
	return chat.SelectTeam{}, false, nil
}
func (d *chatDomains) Played(_ context.Context, accountID, matchID string) (chat.PlayedMatch, error) {
	if matchID != "match-1" || !slices.Contains(d.played.Participants, accountID) {
		return chat.PlayedMatch{}, chat.ErrNotParticipant
	}
	return d.played, nil
}
func (d *chatDomains) Live(context.Context, string) (string, bool, error) { return "", false, nil }
func (d *chatDomains) AllChatOn(context.Context, string) (bool, error)    { return true, nil }
func (d *chatDomains) DisplayNames(_ context.Context, ids []string) (map[string]string, error) {
	out := map[string]string{}
	for _, id := range ids {
		out[id] = "Name of " + id
	}
	return out, nil
}

func TestChatOverHTTP(t *testing.T) {
	d := newTestDeps(t, true)
	domains := &chatDomains{friends: map[string]bool{}}
	// Fixture tuning, independent of the committed config.
	d.Chat = chat.NewService(chat.NewMemStore(), chat.Domains{Parties: domains, Friends: domains, Selects: domains, Matches: domains,
		Preferences: domains, Names: domains}, chat.Tuning{MaxCharacters: 40, MaxPerWindow: 3, Window: time.Minute, HistoryMessages: 10,
		PageSize: 10, Retention: time.Hour, PostMatchWindow: time.Hour}, time.Now)
	srv := serve(t, d)
	one, oneID := gameSession(t, srv, testAccounts[0])
	two, twoID := gameSession(t, srv, testAccounts[1])
	three, _ := gameSession(t, srv, testAccounts[2])
	domains.played = chat.PlayedMatch{EndedAt: time.Now(), Participants: []string{oneID, twoID}}
	ids := 0
	body := func(text string) map[string]string {
		ids++
		return map[string]string{"clientId": fmt.Sprintf("http-client-%04d", ids), "text": text}
	}

	if status, _ := call(t, srv, "GET", "/v1/me/chat", "", nil); status != http.StatusUnauthorized {
		t.Fatalf("without a session: %d", status)
	}
	status, got := call(t, srv, "POST", "/v1/me/chat/party/party-1", one, body("ready?"))
	sent, _ := got["message"].(map[string]any)
	if status != http.StatusOK || sent["kind"] != "party" || sent["text"] != "ready?" || sent["recipientId"] != nil {
		t.Fatalf("party send: %d %v", status, got)
	}
	status, got = call(t, srv, "GET", "/v1/me/chat?after=0", two, nil)
	messages, _ := got["messages"].([]any)
	if status != http.StatusOK || len(messages) != 1 || got["next"].(float64) != sent["seq"].(float64) || got["more"] != false {
		t.Fatalf("the other member's poll: %d %v", status, got)
	}
	if sender := messages[0].(map[string]any)["sender"].(map[string]any); sender["id"] != oneID || sender["displayName"] != "Name of "+oneID {
		t.Fatalf("sender: %v", sender)
	}
	if status, got = call(t, srv, "GET", "/v1/me/chat", two, nil); status != http.StatusOK || len(got["messages"].([]any)) != 1 {
		t.Fatalf("history without a cursor: %d %v", status, got)
	}
	if status, got = call(t, srv, "GET", "/v1/me/chat?after=x", two, nil); status != http.StatusBadRequest || got["error"] != "invalid_cursor" {
		t.Fatalf("a bad cursor: %d %v", status, got)
	}

	// Refusals carry stable codes.
	for _, c := range []struct {
		path, token string
		body        any
		status      int
		code        string
	}{
		{"/v1/me/chat/party/party-1", one, map[string]string{"clientId": "bad id", "text": "hi"}, http.StatusBadRequest, "invalid_message"},
		{"/v1/me/chat/party/party-1", one, body("   "), http.StatusBadRequest, "empty_message"},
		{"/v1/me/chat/direct/" + twoID, one, body("psst"), http.StatusForbidden, "not_friends"},
		{"/v1/me/chat/select/s1", one, body("mid?"), http.StatusConflict, "no_select"},
		{"/v1/me/chat/party/party-0", one, body("old party"), http.StatusConflict, "conversation_changed"},
		{"/v1/me/chat/matches/match-1", three, body("gg"), http.StatusNotFound, "not_participant"},
	} {
		if status, got := call(t, srv, "POST", c.path, c.token, c.body); status != c.status || got["error"] != c.code {
			t.Errorf("%s: %d %v, want %d %s", c.path, status, got, c.status, c.code)
		}
	}

	domains.friends[chat.DirectKey(oneID, twoID)] = true
	status, got = call(t, srv, "POST", "/v1/me/chat/direct/"+twoID, one, body("psst"))
	if status != http.StatusOK || got["message"].(map[string]any)["recipientId"] != twoID {
		t.Fatalf("direct send: %d %v", status, got)
	}

	// Post-match chat: the first message opts in; leaving ends it; mutes are the player's own.
	if status, got = call(t, srv, "POST", "/v1/me/chat/matches/match-1", one, body("gg")); status != http.StatusOK {
		t.Fatalf("post-match send: %d %v", status, got)
	}
	if status, _ = call(t, srv, "PUT", "/v1/me/chat/matches/match-1/mutes/"+twoID, one, nil); status != http.StatusNoContent {
		t.Fatalf("mute: %d", status)
	}
	if status, _ = call(t, srv, "DELETE", "/v1/me/chat/matches/match-1/mutes/"+twoID, one, nil); status != http.StatusNoContent {
		t.Fatalf("unmute: %d", status)
	}
	if status, _ = call(t, srv, "DELETE", "/v1/me/chat/matches/match-1", one, nil); status != http.StatusNoContent {
		t.Fatalf("leave: %d", status)
	}
	if status, got = call(t, srv, "POST", "/v1/me/chat/matches/match-1", one, body("back")); status != http.StatusConflict || got["error"] != "postmatch_closed" {
		t.Fatalf("after leaving: %d %v", status, got)
	}
	// The fourth message in the window is one too many.
	if status, got = call(t, srv, "POST", "/v1/me/chat/party/party-1", one, body("again")); status != http.StatusTooManyRequests || got["error"] != "rate_limited" {
		t.Fatalf("over the limit: %d %v", status, got)
	}
}
