package postgres

import (
	"context"
	"io"
	"log/slog"
	"reflect"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/catalog"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

// A draft keeps its phase, turn, timing, bans, ban hovers and trade offers
// across transactions, and its match takes the traded picks (ADR-042).
func TestADraftAndItsTradesInPostgres(t *testing.T) {
	f := newMatchFixture(t, "DevOne", "DevTwo", "DevThree")
	ctx := context.Background()
	// Five released, all in the rotation, so anyone may pick anything.
	c := catalog.New(catalog.Settings{Released: []string{"cairn", "qazharr", "oriel", "bryn", "silt"}, Starters: []string{"cairn", "qazharr", "oriel"},
		Rotation: catalog.RotationSettings{Slots: 5, Epoch: time.Date(2026, 9, 28, 0, 0, 0, 0, time.UTC), Week: 7 * 24 * time.Hour, Seed: "test"}}, time.Now)
	accounts := account.NewService(f.store.Account(), c, func() time.Time { return f.now })
	names := match.AccountsFunc(func(ctx context.Context, ids []string) (map[string]string, error) {
		list, err := f.store.AccountsByIDs(ctx, ids)
		out := map[string]string{}
		for _, a := range list {
			out[a.ID] = a.DisplayName
		}
		return out, err
	})
	timing := selection.Timing{
		Turns: []selection.Turn{{Ban: true, Side: match.SideA, Count: 1}, {Ban: true, Side: match.SideB, Count: 1},
			{Side: match.SideA, Count: 2}, {Side: match.SideB, Count: 1}},
		Ban: 20 * time.Second, Pick: 30 * time.Second, Final: 10 * time.Second,
	}
	notQueued := selection.PartiesFunc(func(context.Context, string) (bool, error) { return false, nil })
	svc := selection.NewService(f.store.Selection(), accounts, names, f.svc, notQueued, social.NewService(f.store.Social()), selection.Settings{
		Draft:           selection.DraftSettings{Timing: timing, PresenceTimeout: time.Hour},
		StartingTimeout: time.Minute,
		FluxSpells:      []string{"blink", "scorch"},
	}, func() time.Time { return f.now }, slog.New(slog.NewTextHandler(io.Discard, nil)))
	svc.SetMatchmaking(noMatchmaking{})

	one, two, three := f.ids["DevOne"], f.ids["DevTwo"], f.ids["DevThree"]
	id, err := svc.OpenDraft(ctx, "casual", []selection.CasualSeat{{AccountID: one, Side: match.SideA}, {AccountID: three, Side: match.SideA},
		{AccountID: two, Side: match.SideB}})
	if err != nil {
		t.Fatalf("OpenDraft: %v", err)
	}
	stored, err := svc.ForParticipant(ctx, one, id)
	if err != nil || stored.Kind != selection.KindDraft || stored.Phase != selection.PhaseBanning || stored.Turn != 0 ||
		!reflect.DeepEqual(stored.Timing, timing) || !stored.Deadline.Equal(f.now.Add(timing.Ban)) {
		t.Fatalf("the stored draft: %+v %v", stored, err)
	}

	if hovered, err := svc.HoverBan(ctx, one, "bryn"); err != nil || hovered.Seats[0].BanHover != "bryn" {
		t.Fatalf("HoverBan: %+v %v", hovered, err)
	}
	if again, _ := svc.ForParticipant(ctx, three, id); again.Seats[0].BanHover != "bryn" {
		t.Fatalf("the stored ban hover: %+v", again.Seats[0])
	}
	if _, err := svc.Ban(ctx, one, "bryn"); err != nil {
		t.Fatalf("Ban: %v", err)
	}
	picking, err := svc.Ban(ctx, two, "cairn")
	if err != nil {
		t.Fatalf("Ban: %v", err)
	}
	stored, _ = svc.ForParticipant(ctx, two, id)
	wantBans := []selection.Ban{{Side: match.SideA, AccountID: one, VanguardID: "bryn"}, {Side: match.SideB, AccountID: two, VanguardID: "cairn"}}
	if !reflect.DeepEqual(stored.Bans, wantBans) || stored.Phase != selection.PhasePicking || stored.Turn != 2 || stored.TurnDone != 0 ||
		!stored.Deadline.Equal(picking.Deadline) || stored.Seats[0].BanHover != "" {
		t.Fatalf("the stored bans and turn: %+v", stored)
	}

	if _, err := svc.Lock(ctx, one, "oriel"); err != nil {
		t.Fatalf("Lock: %v", err)
	}
	if mid, _ := svc.ForParticipant(ctx, one, id); mid.TurnDone != 1 || mid.Turn != 2 {
		t.Fatalf("one of the turn's two picks: %+v", mid)
	}
	if _, err := svc.Lock(ctx, three, "qazharr"); err != nil {
		t.Fatalf("Lock: %v", err)
	}
	if _, err := svc.OfferTrade(ctx, one, 1); err != nil {
		t.Fatalf("OfferTrade: %v", err)
	}
	if offered, _ := svc.ForParticipant(ctx, three, id); !reflect.DeepEqual(offered.Trades, []selection.Trade{{From: one, To: three}}) {
		t.Fatalf("the stored offer: %+v", offered.Trades)
	}
	if _, err := svc.AcceptTrade(ctx, three, 0); err != nil {
		t.Fatalf("AcceptTrade: %v", err)
	}
	final, err := svc.Lock(ctx, two, "silt")
	if err != nil || final.Phase != selection.PhaseFinal {
		t.Fatalf("Lock: %+v %v", final, err)
	}
	stored, _ = svc.ForParticipant(ctx, one, id)
	if stored.Phase != selection.PhaseFinal || len(stored.Trades) != 0 || stored.Seats[0].Locked != "qazharr" || stored.Seats[1].Locked != "oriel" {
		t.Fatalf("the stored final window: %+v", stored)
	}

	f.now = stored.Deadline
	if err := svc.Tick(ctx); err != nil {
		t.Fatalf("Tick: %v", err)
	}
	m, found, err := f.svc.BySelect(ctx, id)
	if err != nil || !found || m.Participants[0].VanguardID != "qazharr" || m.Participants[1].VanguardID != "oriel" || m.Participants[2].VanguardID != "silt" {
		t.Fatalf("the draft's match: %+v %v %v", m, found, err)
	}
}

// noMatchmaking stands in for the matchmaker a matchmade select reports its
// end to.
type noMatchmaking struct{}

func (noMatchmaking) SelectEnded(context.Context, []string, []string, bool) error { return nil }
