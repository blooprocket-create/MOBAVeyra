package httpapi

import (
	"context"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/presence"
)

// presenceParties is the party service as presence uses it, as the backend's
// own adapter is.
type presenceParties struct{ *party.Service }

func (p presenceParties) PartyMembers(ctx context.Context, id string) ([]string, error) {
	pt, err := p.Get(ctx, id)
	if errors.Is(err, party.ErrNotInParty) {
		return nil, nil
	}
	return pt.MemberIDs(), err
}

// queuedActivity says who is queued; the test servers play no matches.
type queuedActivity struct{ parties *party.Service }

func (a queuedActivity) Activity(ctx context.Context, ids []string) (map[string]presence.Status, error) {
	parties, err := a.parties.PartiesOf(ctx, ids)
	out := map[string]presence.Status{}
	for id, p := range parties {
		if p.Status == party.Queued || p.Status == party.Found {
			out[id] = presence.InQueue
		}
	}
	return out, err
}

var presenceSettings = presence.Settings{OfflineAfter: 30 * time.Second, TouchEvery: 5 * time.Second, PostMatchGrace: 2 * time.Minute}

// newPresenceTestServer serves presence on a clock the test moves.
func newPresenceTestServer(t *testing.T) (*httptest.Server, *presence.Service, *time.Time) {
	t.Helper()
	d := newTestDeps(t, true)
	now := time.Date(2026, 10, 2, 12, 0, 0, 0, time.UTC)
	clock := &now
	d.Presence = presence.NewService(presence.NewMemStore(), presenceSettings, func() time.Time { return *clock })
	d.Presence.SetActivity(queuedActivity{d.Party})
	d.Presence.SetParties(presenceParties{d.Party})
	d.Party.SetPresence(d.Presence)
	return serve(t, d), d.Presence, clock
}

// befriend makes a and b friends, b asking.
func befriend(t *testing.T, srv *httptest.Server, a, aID, b, bID string) {
	t.Helper()
	call(t, srv, "POST", "/v1/friends/requests", b, map[string]string{"accountId": aID})
	if status, _ := call(t, srv, "POST", "/v1/friends/requests/"+bID+"/accept", a, nil); status != http.StatusNoContent {
		t.Fatalf("befriend: %d", status)
	}
}

// joinParty has inviter invite the invitee, who accepts.
func joinParty(t *testing.T, srv *httptest.Server, inviter, invitee, inviteeID string) {
	t.Helper()
	if status, body := call(t, srv, "POST", "/v1/party/invites", inviter, map[string]string{"accountId": inviteeID}); status != http.StatusOK {
		t.Fatalf("invite: %d %v", status, body)
	}
	_, invites := call(t, srv, "GET", "/v1/party/invites", invitee, nil)
	list := invites["invites"].([]any)
	if len(list) != 1 {
		t.Fatalf("invites: %v", invites)
	}
	if status, body := call(t, srv, "POST", "/v1/party/invites/"+list[0].(map[string]any)["id"].(string)+"/accept", invitee, nil); status != http.StatusOK {
		t.Fatalf("join: %d %v", status, body)
	}
}

func friendStatus(t *testing.T, srv *httptest.Server, token, friendID string) string {
	t.Helper()
	_, friends := call(t, srv, "GET", "/v1/friends", token, nil)
	status, _ := friends["presence"].(map[string]any)[friendID].(string)
	return status
}

func TestFriendsSeeEachOthersPresenceAndAppearOfflineHidesFromOutsiders(t *testing.T) {
	srv, _, clock := newPresenceTestServer(t)
	one, oneID := gameSession(t, srv, "DevOne")
	two, twoID := gameSession(t, srv, "DevTwo")
	three, threeID := gameSession(t, srv, "DevThree")
	befriend(t, srv, one, oneID, two, twoID)
	befriend(t, srv, one, oneID, three, threeID)
	if got := friendStatus(t, srv, one, twoID); got != "online" {
		t.Fatalf("two shows %q, want online", got)
	}

	// Three appears offline: one, outside their party, sees them offline and cannot invite them.
	status, mine := call(t, srv, "PUT", "/v1/me/presence", three, map[string]bool{"appearOffline": true})
	if status != http.StatusOK || mine["appearOffline"] != true || mine["status"] != "online" {
		t.Fatalf("appear offline: %d %v", status, mine)
	}
	if got := friendStatus(t, srv, one, threeID); got != "offline" {
		t.Fatalf("three appearing offline shows %q to one", got)
	}
	if status, body := call(t, srv, "POST", "/v1/party/invites", one, map[string]string{"accountId": threeID}); status != http.StatusConflict || body["error"] != "invitee_offline" {
		t.Fatalf("an invitation to a friend appearing offline: %d %v", status, body)
	}

	// Two joins one's party, then appears offline: their party still sees them.
	joinParty(t, srv, one, two, twoID)
	call(t, srv, "PUT", "/v1/me/presence", two, map[string]bool{"appearOffline": true})
	if got := friendStatus(t, srv, one, twoID); got != "online" {
		t.Fatalf("two shows %q to their own party, want online", got)
	}

	// A friend who goes quiet goes offline, and cannot be invited either.
	call(t, srv, "PUT", "/v1/me/presence", three, map[string]bool{"appearOffline": false})
	*clock = clock.Add(presenceSettings.OfflineAfter)
	if got := friendStatus(t, srv, one, threeID); got != "offline" {
		t.Fatalf("three, quiet, shows %q", got)
	}
	if status, body := call(t, srv, "POST", "/v1/party/invites", one, map[string]string{"accountId": threeID}); status != http.StatusConflict || body["error"] != "invitee_offline" {
		t.Fatalf("an invitation to a quiet friend: %d %v", status, body)
	}
}

func TestAQuietMemberLeavesTheirPartyAndLeadershipPasses(t *testing.T) {
	srv, here, clock := newPresenceTestServer(t)
	one, oneID := gameSession(t, srv, "DevOne")
	two, twoID := gameSession(t, srv, "DevTwo")
	befriend(t, srv, one, oneID, two, twoID)
	joinParty(t, srv, one, two, twoID)

	// Within the threshold, nobody goes.
	*clock = clock.Add(presenceSettings.OfflineAfter - time.Second)
	if removed, err := here.Sweep(context.Background()); err != nil || removed != 0 {
		t.Fatalf("early sweep: %d %v", removed, err)
	}
	// The leader goes quiet past it; two stays.
	*clock = clock.Add(2 * time.Second)
	call(t, srv, "GET", "/v1/party", two, nil)
	if removed, err := here.Sweep(context.Background()); err != nil || removed != 1 {
		t.Fatalf("sweep: %d %v", removed, err)
	}
	_, p := call(t, srv, "GET", "/v1/party", two, nil)
	members := p["party"].(map[string]any)["members"].([]any)
	if len(members) != 1 || members[0].(map[string]any)["accountId"] != twoID || members[0].(map[string]any)["leader"] != true {
		t.Fatalf("two leads alone: %v", p)
	}
	// Back again, the leader is not put back (Parties & Social Bible §4).
	if _, mine := call(t, srv, "GET", "/v1/party", one, nil); mine["party"] != nil {
		t.Fatalf("one, back: %v", mine)
	}
}
