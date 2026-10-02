package chat

import (
	"context"
	"errors"
	"fmt"
	"slices"
	"strings"
	"testing"
	"time"
)

var (
	ctx = context.Background()
	t0  = time.Date(2026, 10, 2, 12, 0, 0, 0, time.UTC)
)

// fixtureTuning holds fixture values, independent of the committed config.
var fixtureTuning = Tuning{MaxCharacters: 20, MaxPerWindow: 3, Window: 5 * time.Second, HistoryMessages: 4, PageSize: 3,
	Retention: time.Hour, PostMatchWindow: 10 * time.Minute}

// fakeDomains stands in for the party, social, selection, match and settings
// domains.
type fakeDomains struct {
	parties    map[string]Membership
	friends    map[string]bool
	blocks     map[string]bool
	selects    map[string]SelectTeam
	matches    map[string]PlayedMatch
	live       map[string]string
	allChatOff map[string]bool
}

func (d *fakeDomains) PartyOf(_ context.Context, accountID string) (Membership, bool, error) {
	m, ok := d.parties[accountID]
	return m, ok, nil
}

func (d *fakeDomains) AreFriends(_ context.Context, a, b string) (bool, error) {
	return d.friends[DirectKey(a, b)], nil
}

func (d *fakeDomains) BlockedWithAny(_ context.Context, account string, others []string) (bool, error) {
	return slices.ContainsFunc(others, func(o string) bool { return d.blocks[DirectKey(account, o)] }), nil
}

func (d *fakeDomains) TeamOf(_ context.Context, accountID string) (SelectTeam, bool, error) {
	t, ok := d.selects[accountID]
	return t, ok, nil
}

func (d *fakeDomains) Played(_ context.Context, accountID, matchID string) (PlayedMatch, error) {
	m, ok := d.matches[matchID]
	if !ok || !slices.Contains(m.Participants, accountID) {
		return PlayedMatch{}, ErrNotParticipant
	}
	return m, nil
}

func (d *fakeDomains) Live(_ context.Context, accountID string) (string, bool, error) {
	id, ok := d.live[accountID]
	return id, ok, nil
}

func (d *fakeDomains) AllChatOn(_ context.Context, accountID string) (bool, error) {
	return !d.allChatOff[accountID], nil
}

func (d *fakeDomains) DisplayNames(_ context.Context, ids []string) (map[string]string, error) {
	out := map[string]string{}
	for _, id := range ids {
		out[id] = "Name of " + id
	}
	return out, nil
}

type fixture struct {
	svc   *Service
	store *MemStore
	d     *fakeDomains
	now   time.Time
	ids   int
}

func newFixture(t *testing.T) *fixture {
	t.Helper()
	f := &fixture{store: NewMemStore(), now: t0, d: &fakeDomains{parties: map[string]Membership{}, friends: map[string]bool{},
		blocks: map[string]bool{}, selects: map[string]SelectTeam{}, matches: map[string]PlayedMatch{}, live: map[string]string{},
		allChatOff: map[string]bool{}}}
	f.svc = NewService(f.store, Domains{Parties: f.d, Friends: f.d, Selects: f.d, Matches: f.d, Preferences: f.d, Names: f.d},
		fixtureTuning, func() time.Time { return f.now })
	return f
}

func (f *fixture) join(partyID string, accounts ...string) {
	for _, a := range accounts {
		f.d.parties[a] = Membership{PartyID: partyID, JoinedAt: f.now}
	}
}

// trySend sends after two seconds, which keeps ordinary tests inside the rate window.
func (f *fixture) trySend(actor string, kind Kind, target, text string) (Message, error) {
	f.now = f.now.Add(2 * time.Second)
	f.ids++
	return f.svc.Send(ctx, actor, Send{Kind: kind, Target: target, ClientID: fmt.Sprintf("client-%04d", f.ids), Text: text})
}

func (f *fixture) send(t *testing.T, actor string, kind Kind, target, text string) Message {
	t.Helper()
	m, err := f.trySend(actor, kind, target, text)
	if err != nil {
		t.Fatalf("%s sends %q: %v", actor, text, err)
	}
	return m
}

// read returns the texts the actor may read, from the start.
func (f *fixture) read(t *testing.T, actor string) []string {
	t.Helper()
	page, err := f.svc.Poll(ctx, actor, 0, true)
	if err != nil {
		t.Fatalf("Poll: %v", err)
	}
	var out []string
	for _, m := range page.Messages {
		out = append(out, m.Text)
	}
	return out
}

func expectTexts(t *testing.T, who string, got []string, want ...string) {
	t.Helper()
	if !slices.Equal(got, want) {
		t.Fatalf("%s reads %q, want %q", who, got, want)
	}
}

func TestPartyChatReachesTheCurrentMembersOnly(t *testing.T) {
	f := newFixture(t)
	f.join("p1", "acc-a", "acc-b")
	m := f.send(t, "acc-a", KindParty, "", "ready?")
	if m.Key != "p1" || m.SenderName != "Name of acc-a" || m.Seq == 0 {
		t.Fatalf("stored %+v", m)
	}
	expectTexts(t, "acc-b", f.read(t, "acc-b"), "ready?")
	expectTexts(t, "acc-a", f.read(t, "acc-a"), "ready?")
	expectTexts(t, "acc-c", f.read(t, "acc-c"))
	if _, err := f.trySend("acc-c", KindParty, "", "hi"); !errors.Is(err, ErrNotInParty) {
		t.Fatalf("outside a party: %v", err)
	}
	// Leaving ends the conversation.
	delete(f.d.parties, "acc-b")
	expectTexts(t, "acc-b after leaving", f.read(t, "acc-b"))
}

func TestAMemberReadsPartyChatOnlyFromWhenItJoined(t *testing.T) {
	f := newFixture(t)
	f.join("p1", "acc-a")
	f.send(t, "acc-a", KindParty, "", "before")
	f.now = f.now.Add(time.Second)
	f.join("p1", "acc-b")
	f.send(t, "acc-a", KindParty, "", "after")
	expectTexts(t, "the newcomer", f.read(t, "acc-b"), "after")
	expectTexts(t, "the member", f.read(t, "acc-a"), "before", "after")
}

func TestDirectMessagesNeedFriendsWithoutABlock(t *testing.T) {
	f := newFixture(t)
	f.d.friends[DirectKey("acc-a", "acc-b")] = true
	m := f.send(t, "acc-a", KindDirect, "acc-b", "hello")
	if m.RecipientID != "acc-b" || m.Key != DirectKey("acc-a", "acc-b") {
		t.Fatalf("stored %+v", m)
	}
	expectTexts(t, "the recipient", f.read(t, "acc-b"), "hello")
	expectTexts(t, "the sender", f.read(t, "acc-a"), "hello")
	expectTexts(t, "a stranger", f.read(t, "acc-c"))
	if _, err := f.trySend("acc-c", KindDirect, "acc-a", "hi"); !errors.Is(err, ErrNotFriends) {
		t.Fatalf("a stranger: %v", err)
	}
	if _, err := f.trySend("acc-a", KindDirect, "acc-a", "me"); !errors.Is(err, ErrInvalidMessage) {
		t.Fatalf("to oneself: %v", err)
	}
	// Unfriending ends the conversation for both.
	f.d.friends[DirectKey("acc-a", "acc-b")] = false
	expectTexts(t, "the recipient after unfriending", f.read(t, "acc-b"))
	expectTexts(t, "the sender after unfriending", f.read(t, "acc-a"))
	// A block is refused as a block, even between friends.
	f.d.friends[DirectKey("acc-a", "acc-b")] = true
	f.d.blocks[DirectKey("acc-b", "acc-a")] = true
	if _, err := f.trySend("acc-a", KindDirect, "acc-b", "hi"); !errors.Is(err, ErrBlocked) {
		t.Fatalf("blocked: %v", err)
	}
	expectTexts(t, "the blocked recipient", f.read(t, "acc-b"))
}

func TestSelectChatReachesOneSideWhileTheSelectLasts(t *testing.T) {
	f := newFixture(t)
	f.d.selects["acc-a"] = SelectTeam{SelectID: "s1", Side: "A"}
	f.d.selects["acc-b"] = SelectTeam{SelectID: "s1", Side: "A"}
	f.d.selects["acc-c"] = SelectTeam{SelectID: "s1", Side: "B"}
	f.send(t, "acc-a", KindSelect, "", "mid?")
	expectTexts(t, "a teammate", f.read(t, "acc-b"), "mid?")
	expectTexts(t, "an opponent", f.read(t, "acc-c"))
	if _, err := f.trySend("acc-d", KindSelect, "", "hi"); !errors.Is(err, ErrNoSelect) {
		t.Fatalf("outside a select: %v", err)
	}
	delete(f.d.selects, "acc-b")
	expectTexts(t, "after the select", f.read(t, "acc-b"))
}

// endMatch records a match ended now with its human participants.
func (f *fixture) endMatch(matchID string, participants ...string) {
	f.d.matches[matchID] = PlayedMatch{EndedAt: f.now, Participants: participants}
}

func TestPostMatchChatIsOptInByFirstMessage(t *testing.T) {
	f := newFixture(t)
	f.endMatch("m1", "acc-a", "acc-b", "acc-c")
	f.send(t, "acc-a", KindPostMatch, "m1", "gg")
	expectTexts(t, "the first to speak", f.read(t, "acc-a"), "gg")
	expectTexts(t, "a player who has not spoken", f.read(t, "acc-b"))
	f.send(t, "acc-b", KindPostMatch, "m1", "wp")
	expectTexts(t, "the second, from its own first message", f.read(t, "acc-b"), "wp")
	expectTexts(t, "the first", f.read(t, "acc-a"), "gg", "wp")
	if _, err := f.trySend("acc-x", KindPostMatch, "m1", "hi"); !errors.Is(err, ErrNotParticipant) {
		t.Fatalf("not a participant: %v", err)
	}
}

func TestPostMatchChatEndsOnLeavingOrMovingOn(t *testing.T) {
	f := newFixture(t)
	f.endMatch("m1", "acc-a", "acc-b", "acc-c", "acc-d")
	for _, a := range []string{"acc-a", "acc-b", "acc-c", "acc-d"} {
		f.send(t, a, KindPostMatch, "m1", "gg")
	}
	// Leaving the results screen ends it for good.
	if err := f.svc.LeavePostMatch(ctx, "acc-a", "m1"); err != nil {
		t.Fatalf("Leave: %v", err)
	}
	if _, err := f.trySend("acc-a", KindPostMatch, "m1", "back"); !errors.Is(err, ErrPostMatchClosed) {
		t.Fatalf("after leaving: %v", err)
	}
	expectTexts(t, "a player who left", f.read(t, "acc-a"))
	// Entering a select or another match ends it too.
	f.d.selects["acc-b"] = SelectTeam{SelectID: "s2", Side: "A"}
	f.d.live["acc-c"] = "m2"
	for _, a := range []string{"acc-b", "acc-c"} {
		if _, err := f.trySend(a, KindPostMatch, "m1", "hi"); !errors.Is(err, ErrPostMatchClosed) {
			t.Fatalf("%s moved on: %v", a, err)
		}
		expectTexts(t, a+" moved on", f.read(t, a))
	}
	// And the window closes it.
	expectTexts(t, "within the window, from its own first message", f.read(t, "acc-d"), "gg")
	f.now = f.d.matches["m1"].EndedAt.Add(fixtureTuning.PostMatchWindow)
	if _, err := f.svc.Send(ctx, "acc-d", Send{Kind: KindPostMatch, Target: "m1", ClientID: "late-0001", Text: "late"}); !errors.Is(err, ErrPostMatchClosed) {
		t.Fatalf("after the window: %v", err)
	}
	expectTexts(t, "after the window", f.read(t, "acc-d"))
	// A match not yet ended has none.
	f.d.matches["m3"] = PlayedMatch{Participants: []string{"acc-d"}}
	if _, err := f.trySend("acc-d", KindPostMatch, "m3", "hi"); !errors.Is(err, ErrPostMatchClosed) {
		t.Fatalf("before the end: %v", err)
	}
}

func TestAllChatOffKeepsAPlayerOutOfPostMatchChat(t *testing.T) {
	f := newFixture(t)
	f.endMatch("m1", "acc-a", "acc-b")
	f.d.allChatOff["acc-a"] = true
	if _, err := f.trySend("acc-a", KindPostMatch, "m1", "gg"); !errors.Is(err, ErrAllChatOff) {
		t.Fatalf("All Chat off: %v", err)
	}
	f.d.allChatOff["acc-a"] = false
	f.send(t, "acc-a", KindPostMatch, "m1", "gg")
	f.send(t, "acc-b", KindPostMatch, "m1", "wp")
	// Turning it off after opting in stops delivery.
	f.d.allChatOff["acc-a"] = true
	expectTexts(t, "a player who turned All Chat off", f.read(t, "acc-a"))
}

func TestAPostMatchMuteHidesTheMutedPlayerFromTheMuterOnly(t *testing.T) {
	f := newFixture(t)
	f.endMatch("m1", "acc-a", "acc-b", "acc-c")
	for _, a := range []string{"acc-a", "acc-b", "acc-c"} {
		f.send(t, a, KindPostMatch, "m1", "gg "+a)
	}
	if err := f.svc.MutePostMatch(ctx, "acc-a", "m1", "acc-b", true); err != nil {
		t.Fatalf("Mute: %v", err)
	}
	expectTexts(t, "the muter", f.read(t, "acc-a"), "gg acc-a", "gg acc-c")
	expectTexts(t, "another player", f.read(t, "acc-c"), "gg acc-c")
	if err := f.svc.MutePostMatch(ctx, "acc-a", "m1", "acc-b", false); err != nil {
		t.Fatalf("Unmute: %v", err)
	}
	expectTexts(t, "after unmuting", f.read(t, "acc-a"), "gg acc-a", "gg acc-b", "gg acc-c")
	for _, target := range []string{"acc-a", "acc-x"} {
		if err := f.svc.MutePostMatch(ctx, "acc-a", "m1", target, true); !errors.Is(err, ErrNotParticipant) {
			t.Fatalf("muting %s: %v", target, err)
		}
	}
}

func TestBlocksDropMessagesAtDeliveryInEveryKind(t *testing.T) {
	f := newFixture(t)
	f.join("p1", "acc-a", "acc-b", "acc-c")
	f.send(t, "acc-b", KindParty, "", "from b")
	f.send(t, "acc-c", KindParty, "", "from c")
	f.d.blocks[DirectKey("acc-a", "acc-b")] = true
	expectTexts(t, "the blocker", f.read(t, "acc-a"), "from c")
	expectTexts(t, "the blocked", f.read(t, "acc-b"), "from b", "from c")
	// The cursor still passes a message the actor may not read.
	page, err := f.svc.Poll(ctx, "acc-a", 0, true)
	if err != nil || page.Next != 2 {
		t.Fatalf("Poll: %+v %v", page, err)
	}
}

func TestAResendReturnsTheFirstMessage(t *testing.T) {
	f := newFixture(t)
	f.join("p1", "acc-a", "acc-b")
	first, err := f.svc.Send(ctx, "acc-a", Send{Kind: KindParty, ClientID: "retry-0001", Text: "once"})
	if err != nil {
		t.Fatalf("Send: %v", err)
	}
	again, err := f.svc.Send(ctx, "acc-a", Send{Kind: KindParty, ClientID: "retry-0001", Text: "once"})
	if err != nil || again.Seq != first.Seq {
		t.Fatalf("resend: %+v %v", again, err)
	}
	expectTexts(t, "the member", f.read(t, "acc-b"), "once")
	if _, err := f.svc.Send(ctx, "acc-a", Send{Kind: KindSelect, ClientID: "retry-0001", Text: "once"}); !errors.Is(err, ErrClientIDConflict) {
		t.Fatalf("the ID for another conversation: %v", err)
	}
}

func TestTheRateWindowRefusesAFlood(t *testing.T) {
	f := newFixture(t)
	f.join("p1", "acc-a")
	send := func(id string) error {
		_, err := f.svc.Send(ctx, "acc-a", Send{Kind: KindParty, ClientID: id, Text: "spam"})
		return err
	}
	for i := range fixtureTuning.MaxPerWindow {
		if err := send(fmt.Sprintf("flood-%04d", i)); err != nil {
			t.Fatalf("message %d: %v", i, err)
		}
	}
	if err := send("flood-over"); !errors.Is(err, ErrRateLimited) {
		t.Fatalf("over the limit: %v", err)
	}
	if err := send("flood-0000"); err != nil {
		t.Fatalf("a resend is not a new message: %v", err)
	}
	f.now = f.now.Add(fixtureTuning.Window + time.Second)
	if err := send("flood-later"); err != nil {
		t.Fatalf("after the window: %v", err)
	}
}

func TestAPollWithoutACursorReturnsTheNewestHistory(t *testing.T) {
	f := newFixture(t)
	f.join("p1", "acc-a")
	for i := range 6 {
		f.send(t, "acc-a", KindParty, "", fmt.Sprintf("m%d", i))
	}
	page, err := f.svc.Poll(ctx, "acc-a", 0, false)
	if err != nil || len(page.Messages) != fixtureTuning.HistoryMessages || page.Messages[0].Text != "m2" || page.Next != 6 || page.More {
		t.Fatalf("history: %+v %v", page, err)
	}
	if next, _ := f.svc.Poll(ctx, "acc-a", page.Next, true); len(next.Messages) != 0 || next.Next != 6 {
		t.Fatalf("nothing new: %+v", next)
	}
	// With nothing readable, the cursor starts at the newest message anyway.
	if empty, _ := f.svc.Poll(ctx, "acc-z", 0, false); len(empty.Messages) != 0 || empty.Next != 6 {
		t.Fatalf("a stranger's history: %+v", empty)
	}
}

func TestAFullPageAsksForMore(t *testing.T) {
	f := newFixture(t)
	f.join("p1", "acc-a")
	for i := range 5 {
		f.send(t, "acc-a", KindParty, "", fmt.Sprintf("m%d", i))
	}
	first, _ := f.svc.Poll(ctx, "acc-a", 0, true)
	if len(first.Messages) != fixtureTuning.PageSize || !first.More || first.Next != 3 {
		t.Fatalf("first page: %+v", first)
	}
	second, _ := f.svc.Poll(ctx, "acc-a", first.Next, true)
	if len(second.Messages) != 2 || second.More || second.Next != 5 {
		t.Fatalf("second page: %+v", second)
	}
}

func TestOldMessagesAreNeitherServedNorKept(t *testing.T) {
	f := newFixture(t)
	f.join("p1", "acc-a")
	f.send(t, "acc-a", KindParty, "", "old")
	f.now = f.now.Add(fixtureTuning.Retention + time.Second)
	expectTexts(t, "after the retention", f.read(t, "acc-a"))
	f.send(t, "acc-a", KindParty, "", "new")
	if kept, _ := f.store.Messages(ctx, Query{Rooms: []Room{{Kind: KindParty, Key: "p1"}}, Limit: 10}); len(kept) != 1 || kept[0].Text != "new" {
		t.Fatalf("kept %+v", kept)
	}
}

func TestInvalidSendsAreRefused(t *testing.T) {
	f := newFixture(t)
	f.join("p1", "acc-a")
	for _, c := range []struct {
		send Send
		want error
	}{
		{Send{Kind: KindParty, ClientID: "bad id", Text: "hi"}, ErrInvalidMessage},
		{Send{Kind: KindParty, ClientID: "valid-0001", Text: " \n "}, ErrEmptyMessage},
		{Send{Kind: KindParty, ClientID: "valid-0002", Text: strings.Repeat("x", fixtureTuning.MaxCharacters+1)}, ErrMessageTooLong},
		{Send{Kind: "shout", ClientID: "valid-0003", Text: "hi"}, ErrInvalidMessage},
	} {
		if _, err := f.svc.Send(ctx, "acc-a", c.send); !errors.Is(err, c.want) {
			t.Errorf("%+v: %v, want %v", c.send, err, c.want)
		}
	}
}
