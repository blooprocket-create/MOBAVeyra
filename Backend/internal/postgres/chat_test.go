package postgres

import (
	"context"
	"errors"
	"fmt"
	"slices"
	"sync"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/chat"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// chatDomains stands in for the domains chat asks about readers.
type chatDomains struct {
	party   map[string]chat.Membership
	friends map[string]bool
	played  map[string]chat.PlayedMatch
}

func (d *chatDomains) PartyOf(_ context.Context, id string) (chat.Membership, bool, error) {
	m, ok := d.party[id]
	return m, ok, nil
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
	m, ok := d.played[matchID]
	if !ok || !slices.Contains(m.Participants, accountID) {
		return chat.PlayedMatch{}, chat.ErrNotParticipant
	}
	return m, nil
}
func (d *chatDomains) Live(context.Context, string) (string, bool, error) { return "", false, nil }
func (d *chatDomains) AllChatOn(context.Context, string) (bool, error)    { return true, nil }
func (d *chatDomains) DisplayNames(_ context.Context, ids []string) (map[string]string, error) {
	out := map[string]string{}
	for _, id := range ids {
		out[id] = "Name"
	}
	return out, nil
}

// chatTuning is fixture tuning, independent of the committed config.
var chatTuning = chat.Tuning{MaxCharacters: 50, MaxPerWindow: 3, Window: 5 * time.Second, HistoryMessages: 2, PageSize: 10,
	Retention: time.Hour, PostMatchWindow: 10 * time.Minute}

type chatFixture struct {
	*matchFixture
	chat  *chat.Service
	d     *chatDomains
	clock time.Time
	ids   int
	match string
}

func newChatFixture(t *testing.T) *chatFixture {
	t.Helper()
	mf := newMatchFixture(t, "DevOne", "DevTwo")
	m, err := mf.svc.Create(context.Background(), mf.casual(mf.seats(map[string]match.Side{"DevOne": match.SideA, "DevTwo": match.SideB})))
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	f := &chatFixture{matchFixture: mf, clock: time.Now().UTC().Truncate(time.Microsecond), match: m.ID,
		d: &chatDomains{party: map[string]chat.Membership{}, friends: map[string]bool{}, played: map[string]chat.PlayedMatch{}}}
	f.chat = chat.NewService(mf.store.Chat(), chat.Domains{Parties: f.d, Friends: f.d, Selects: f.d, Matches: f.d, Preferences: f.d, Names: f.d},
		chatTuning, func() time.Time { return f.clock })
	return f
}

func (f *chatFixture) send(t *testing.T, name string, kind chat.Kind, target, text string) chat.Message {
	t.Helper()
	f.clock = f.clock.Add(2 * time.Second)
	f.ids++
	m, err := f.chat.Send(context.Background(), f.matchFixture.ids[name], chat.Send{Kind: kind, Target: target, ClientID: fmt.Sprintf("pg-client-%04d", f.ids), Text: text})
	if err != nil {
		t.Fatalf("%s sends %q: %v", name, text, err)
	}
	return m
}

func (f *chatFixture) read(t *testing.T, name string) []string {
	t.Helper()
	page, err := f.chat.Poll(context.Background(), f.matchFixture.ids[name], 0, true)
	if err != nil {
		t.Fatalf("Poll: %v", err)
	}
	var out []string
	for _, m := range page.Messages {
		out = append(out, m.Text)
	}
	return out
}

func TestPartyAndDirectChatInPostgres(t *testing.T) {
	f := newChatFixture(t)
	ctx := context.Background()
	one, two := f.matchFixture.ids["DevOne"], f.matchFixture.ids["DevTwo"]
	f.d.party[one] = chat.Membership{PartyID: "party-1", JoinedAt: f.clock}
	f.d.party[two] = chat.Membership{PartyID: "party-1", JoinedAt: f.clock}
	first := f.send(t, "DevOne", chat.KindParty, "party-1", "one")
	f.send(t, "DevTwo", chat.KindParty, "party-1", "two")
	f.send(t, "DevOne", chat.KindParty, "party-1", "three")
	if got := f.read(t, "DevTwo"); !slices.Equal(got, []string{"one", "two", "three"}) {
		t.Fatalf("party: %q", got)
	}
	// History without a cursor is the newest, oldest first.
	if page, err := f.chat.Poll(ctx, two, 0, false); err != nil || len(page.Messages) != 2 || page.Messages[0].Text != "two" || page.Next != page.Messages[1].Seq {
		t.Fatalf("history: %+v %v", page, err)
	}
	// A resend finds the first message.
	again, err := f.chat.Send(ctx, one, chat.Send{Kind: chat.KindParty, Target: "party-1", ClientID: first.ClientID, Text: "one"})
	if err != nil || again.Seq != first.Seq || !again.SentAt.Equal(first.SentAt) {
		t.Fatalf("resend: %+v %v", again, err)
	}
	// Direct messages carry their recipient.
	f.d.friends[chat.DirectKey(one, two)] = true
	dm := f.send(t, "DevOne", chat.KindDirect, two, "psst")
	if dm.RecipientID != two {
		t.Fatalf("direct: %+v", dm)
	}
	if got := f.read(t, "DevTwo"); !slices.Contains(got, "psst") {
		t.Fatalf("the recipient: %q", got)
	}
	// The rate window counts stored messages.
	f.clock = f.clock.Add(time.Minute)
	for i := range chatTuning.MaxPerWindow {
		if _, err := f.chat.Send(ctx, one, chat.Send{Kind: chat.KindParty, Target: "party-1", ClientID: fmt.Sprintf("pg-flood-%04d", i), Text: "x"}); err != nil {
			t.Fatalf("message %d: %v", i, err)
		}
	}
	if _, err := f.chat.Send(ctx, one, chat.Send{Kind: chat.KindParty, Target: "party-1", ClientID: "pg-flood-over", Text: "x"}); !errors.Is(err, chat.ErrRateLimited) {
		t.Fatalf("over the limit: %v", err)
	}
	// Old messages are pruned by the next send.
	f.clock = f.clock.Add(chatTuning.Retention + time.Second)
	f.d.party[two] = chat.Membership{PartyID: "party-1", JoinedAt: f.clock.Add(-2 * chatTuning.Retention)}
	f.send(t, "DevTwo", chat.KindParty, "party-1", "fresh")
	if got := f.read(t, "DevTwo"); !slices.Equal(got, []string{"fresh"}) {
		t.Fatalf("after retention: %q", got)
	}
}

func TestPostMatchChatInPostgres(t *testing.T) {
	f := newChatFixture(t)
	ctx := context.Background()
	one, two := f.matchFixture.ids["DevOne"], f.matchFixture.ids["DevTwo"]
	f.d.played[f.match] = chat.PlayedMatch{EndedAt: f.clock, Participants: []string{one, two}}
	f.send(t, "DevOne", chat.KindPostMatch, f.match, "gg")
	if got := f.read(t, "DevTwo"); len(got) != 0 {
		t.Fatalf("before opting in: %q", got)
	}
	f.send(t, "DevTwo", chat.KindPostMatch, f.match, "wp")
	if got := f.read(t, "DevOne"); !slices.Equal(got, []string{"gg", "wp"}) {
		t.Fatalf("both in: %q", got)
	}
	if err := f.chat.MutePostMatch(ctx, one, f.match, two, true); err != nil {
		t.Fatalf("Mute: %v", err)
	}
	if got := f.read(t, "DevOne"); !slices.Equal(got, []string{"gg"}) {
		t.Fatalf("muted: %q", got)
	}
	if err := f.chat.LeavePostMatch(ctx, two, f.match); err != nil {
		t.Fatalf("Leave: %v", err)
	}
	if _, err := f.chat.Send(ctx, two, chat.Send{Kind: chat.KindPostMatch, Target: f.match, ClientID: "pg-after-leave", Text: "hi"}); !errors.Is(err, chat.ErrPostMatchClosed) {
		t.Fatalf("after leaving: %v", err)
	}
	if got := f.read(t, "DevTwo"); len(got) != 0 {
		t.Fatalf("after leaving: %q", got)
	}
}

func TestConcurrentSendsAreAllDeliveredInOrderInPostgres(t *testing.T) {
	f := newChatFixture(t)
	ctx := context.Background()
	one, two := f.matchFixture.ids["DevOne"], f.matchFixture.ids["DevTwo"]
	f.d.party[one] = chat.Membership{PartyID: "party-1", JoinedAt: f.clock.Add(-time.Minute)}
	f.d.party[two] = chat.Membership{PartyID: "party-1", JoinedAt: f.clock.Add(-time.Minute)}
	var wg sync.WaitGroup
	errs := make(chan error, 2*chatTuning.MaxPerWindow)
	for _, sender := range []string{one, two} {
		for i := range chatTuning.MaxPerWindow {
			wg.Add(1)
			go func() {
				defer wg.Done()
				_, err := f.chat.Send(ctx, sender, chat.Send{Kind: chat.KindParty, Target: "party-1", ClientID: fmt.Sprintf("pg-race-%s-%d", sender[:8], i), Text: "race"})
				errs <- err
			}()
		}
	}
	wg.Wait()
	close(errs)
	for err := range errs {
		if err != nil {
			t.Fatalf("Send: %v", err)
		}
	}
	page, err := f.chat.Poll(ctx, one, 0, true)
	if err != nil || len(page.Messages) != 2*chatTuning.MaxPerWindow {
		t.Fatalf("Poll: %d messages, %v", len(page.Messages), err)
	}
	if !slices.IsSortedFunc(page.Messages, func(a, b chat.Message) int { return int(a.Seq - b.Seq) }) {
		t.Fatal("messages out of order")
	}
}
