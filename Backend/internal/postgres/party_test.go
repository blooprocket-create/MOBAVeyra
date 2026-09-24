package postgres

import (
	"context"
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

var testPartyRules = party.Rules{
	MaxSize: 5,
	Modes:   map[string]party.Mode{"casual": {ID: "casual", Enabled: true, HumanPlayersPerTeam: 5}},
}

type partyFixture struct {
	store   *Store
	social  *social.Service
	parties *party.Service
	ids     map[string]string
}

func newPartyFixture(t *testing.T, names ...string) partyFixture {
	t.Helper()
	store := openTestStore(t)
	ctx := context.Background()
	ids := map[string]string{}
	for _, n := range names {
		a, err := store.EnsureDevAccount(ctx, n)
		if err != nil {
			t.Fatal(err)
		}
		ids[n] = a.ID
	}
	soc := social.NewService(store.Social())
	parties := party.NewService(store.Party(), soc, party.Settings{
		Rules:          testPartyRules,
		InviteLifetime: time.Minute,
		DefaultPrivacy: party.Private,
	}, time.Now)
	return partyFixture{store: store, social: soc, parties: parties, ids: ids}
}

func (f partyFixture) befriend(t *testing.T, a, b string) {
	t.Helper()
	ctx := context.Background()
	if _, err := f.social.SendFriendRequest(ctx, f.ids[a], f.ids[b]); err != nil {
		t.Fatal(err)
	}
	if err := f.social.AcceptFriendRequest(ctx, f.ids[b], f.ids[a]); err != nil {
		t.Fatal(err)
	}
}

func TestSocialGraphInPostgres(t *testing.T) {
	f := newPartyFixture(t, "A", "B", "C")
	ctx := context.Background()
	a, b, c := f.ids["A"], f.ids["B"], f.ids["C"]
	f.befriend(t, "A", "B")
	if friends, _ := f.social.Friends(ctx, a); len(friends) != 1 || friends[0] != b {
		t.Fatalf("friends of A: %v", friends)
	}
	if ok, _ := f.social.FriendOfAny(ctx, c, []string{a, b}); ok {
		t.Fatal("C has no friends")
	}
	if err := f.social.Block(ctx, b, a); err != nil {
		t.Fatal(err)
	}
	if ok, _ := f.social.AreFriends(ctx, a, b); ok {
		t.Fatal("block must end the friendship")
	}
	if ok, _ := f.social.BlockedWithAny(ctx, a, []string{c, b}); !ok {
		t.Fatal("block must be visible from both sides")
	}
	if _, err := f.social.SendFriendRequest(ctx, a, b); !errors.Is(err, social.ErrBlocked) {
		t.Fatalf("want ErrBlocked, got %v", err)
	}
	if _, err := f.social.SendFriendRequest(ctx, a, "not-a-uuid"); !errors.Is(err, social.ErrAccountNotFound) {
		t.Fatalf("want ErrAccountNotFound, got %v", err)
	}
}

func TestPartyLifecycleInPostgres(t *testing.T) {
	f := newPartyFixture(t, "A", "B", "C")
	ctx := context.Background()
	a, b, c := f.ids["A"], f.ids["B"], f.ids["C"]
	f.befriend(t, "A", "B")
	f.befriend(t, "A", "C")

	inv, err := f.parties.Invite(ctx, a, b)
	if err != nil {
		t.Fatal(err)
	}
	if list, _ := f.parties.Invites(ctx, b); len(list) != 1 {
		t.Fatalf("B's invites: %+v", list)
	}
	if _, err := f.parties.AcceptInvite(ctx, b, inv.ID); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.SelectMode(ctx, a, "casual"); err != nil {
		t.Fatal(err)
	}
	for _, id := range []string{a, b} {
		if _, err := f.parties.SetReady(ctx, id, true); err != nil {
			t.Fatal(err)
		}
	}
	p, err := f.parties.StartQueue(ctx, a)
	if err != nil || p.Status != party.Queued {
		t.Fatalf("StartQueue: %+v %v", p, err)
	}

	late, err := f.parties.Invite(ctx, a, c)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.AcceptInvite(ctx, c, late.ID); !errors.Is(err, party.ErrPartyLocked) {
		t.Fatalf("want ErrPartyLocked, got %v", err)
	}

	if err := f.parties.Leave(ctx, a); err != nil {
		t.Fatal(err)
	}
	p, err = f.parties.Get(ctx, b)
	if err != nil {
		t.Fatal(err)
	}
	if p.LeaderID != b || p.Status != party.Idle || p.Members[0].Ready {
		t.Fatalf("leader leaving during queue: %+v", p)
	}
	if err := f.parties.Leave(ctx, b); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.Get(ctx, b); !errors.Is(err, party.ErrNotInParty) {
		t.Fatalf("empty party must be deleted: %v", err)
	}
}

// Two players racing for the last slot: exactly one gets in.
func TestConcurrentAcceptsRespectCapacity(t *testing.T) {
	names := []string{"L", "M1", "M2", "M3", "X", "Y"}
	f := newPartyFixture(t, names...)
	ctx := context.Background()
	for _, n := range names[1:] {
		f.befriend(t, "L", n)
	}
	for _, n := range []string{"M1", "M2", "M3"} {
		inv, err := f.parties.Invite(ctx, f.ids["L"], f.ids[n])
		if err != nil {
			t.Fatal(err)
		}
		if _, err := f.parties.AcceptInvite(ctx, f.ids[n], inv.ID); err != nil {
			t.Fatal(err)
		}
	}
	var invites []party.Invite
	for _, n := range []string{"X", "Y"} {
		inv, err := f.parties.Invite(ctx, f.ids["L"], f.ids[n])
		if err != nil {
			t.Fatal(err)
		}
		invites = append(invites, inv)
	}

	var wg sync.WaitGroup
	errs := make([]error, len(invites))
	for i, inv := range invites {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, errs[i] = f.parties.AcceptInvite(ctx, inv.InviteeID, inv.ID)
		}()
	}
	wg.Wait()

	full := 0
	for _, err := range errs {
		switch {
		case err == nil:
		case errors.Is(err, party.ErrPartyFull):
			full++
		default:
			t.Fatalf("unexpected error: %v", err)
		}
	}
	p, _ := f.parties.Get(ctx, f.ids["L"])
	if full != 1 || len(p.Members) != 5 {
		t.Fatalf("want one rejection and a full party, got %d rejections, %d members", full, len(p.Members))
	}
}

// One player accepting two invites at once ends up in exactly one party.
func TestConcurrentAcceptsKeepOnePartyPerAccount(t *testing.T) {
	f := newPartyFixture(t, "A", "B", "Z")
	ctx := context.Background()
	f.befriend(t, "A", "Z")
	f.befriend(t, "B", "Z")
	var invites []party.Invite
	for _, n := range []string{"A", "B"} {
		inv, err := f.parties.Invite(ctx, f.ids[n], f.ids["Z"])
		if err != nil {
			t.Fatal(err)
		}
		invites = append(invites, inv)
	}
	var wg sync.WaitGroup
	for _, inv := range invites {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, _ = f.parties.AcceptInvite(ctx, f.ids["Z"], inv.ID)
		}()
	}
	wg.Wait()
	var memberships int
	if err := f.store.pool.QueryRow(ctx, `SELECT count(*) FROM party.members WHERE account_id = $1::uuid`, f.ids["Z"]).Scan(&memberships); err != nil {
		t.Fatal(err)
	}
	if memberships != 1 {
		t.Fatalf("Z must be in exactly one party, found %d", memberships)
	}
}

func TestBlockRemovesMemberInPostgres(t *testing.T) {
	f := newPartyFixture(t, "A", "B")
	ctx := context.Background()
	a, b := f.ids["A"], f.ids["B"]
	f.befriend(t, "A", "B")
	inv, err := f.parties.Invite(ctx, a, b)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.AcceptInvite(ctx, b, inv.ID); err != nil {
		t.Fatal(err)
	}
	if err := f.social.Block(ctx, a, b); err != nil {
		t.Fatal(err)
	}
	if err := f.parties.OnBlock(ctx, a, b); err != nil {
		t.Fatal(err)
	}
	if _, err := f.parties.Get(ctx, b); !errors.Is(err, party.ErrNotInParty) {
		t.Fatalf("blocked member must be removed: %v", err)
	}
}
