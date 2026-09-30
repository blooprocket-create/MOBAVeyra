package postgres

import (
	"context"
	"io"
	"log/slog"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/catalog"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
)

// A custom lobby's select keeps its bots and rules, and the match it creates
// carries them (ADR-021 §2–§3).
func TestACustomSelectAndItsMatchInPostgres(t *testing.T) {
	f := newMatchFixture(t, "DevOne", "DevTwo")
	ctx := context.Background()
	c := catalog.New(catalog.Settings{Released: []string{"cairn", "qazharr", "oriel", "bryn"}, Starters: []string{"cairn", "qazharr", "oriel"},
		RotationSlots: 12, StandIn: catalog.StandInNone})
	accounts := account.NewService(f.store.Account(), c, func() time.Time { return f.now })
	one, two := f.ids["DevOne"], f.ids["DevTwo"]
	for id, starter := range map[string]string{one: "oriel", two: "oriel"} {
		if _, err := accounts.ChooseStarter(ctx, id, starter); err != nil {
			t.Fatal(err)
		}
	}
	names := match.AccountsFunc(func(ctx context.Context, ids []string) (map[string]string, error) {
		list, err := f.store.AccountsByIDs(ctx, ids)
		out := map[string]string{}
		for _, a := range list {
			out[a.ID] = a.DisplayName
		}
		return out, err
	})
	notQueued := selection.PartiesFunc(func(context.Context, string) (bool, error) { return false, nil })
	svc := selection.NewService(f.store.Selection(), accounts, names, f.svc, notQueued, social.NewService(f.store.Social()), selection.Settings{
		Custom:          selection.CustomSettings{Mode: "custom_game", PickDuration: time.Minute},
		StartingTimeout: time.Minute,
		FluxSpells:      []string{"blink", "scorch"},
	}, func() time.Time { return f.now }, slog.New(slog.NewTextHandler(io.Discard, nil)))

	gold := 2000.0
	bots := []match.Bot{{Side: match.SideB, VanguardID: "bryn", Difficulty: match.BotIntermediate}, {Side: match.SideB, VanguardID: "cairn", Difficulty: match.BotBeginner}}
	opened, err := svc.OpenCustom(ctx, selection.CustomLaunch{
		LobbyID:       "11111111-2222-4333-8444-555555555555",
		HostAccountID: one,
		Seats:         []selection.CasualSeat{{AccountID: one, Side: match.SideA}, {AccountID: two, Side: match.SideB}},
		Bots:          bots,
		Settings:      match.CustomSettings{VictoryEnabled: true, StartingGold: &gold},
	})
	if err != nil {
		t.Fatalf("OpenCustom: %v", err)
	}
	stored, ok, err := svc.Current(ctx, two)
	if err != nil || !ok || stored.ID != opened.ID || stored.Kind != selection.KindCustom || stored.LobbyID != "11111111-2222-4333-8444-555555555555" {
		t.Fatalf("stored custom select: %+v %v %v", stored, ok, err)
	}
	if len(stored.Bots) != 2 || stored.Bots[0] != bots[0] || stored.Bots[1] != bots[1] || stored.Custom == nil || *stored.Custom.StartingGold != gold {
		t.Fatalf("stored bots and rules: %+v %+v", stored.Bots, stored.Custom)
	}

	if _, err := svc.Lock(ctx, one, "oriel"); err != nil {
		t.Fatal(err)
	}
	started, err := svc.Lock(ctx, two, "oriel")
	if err != nil || started.State != selection.Started {
		t.Fatalf("a mirror across sides starts the match: %+v %v", started, err)
	}
	m, found, err := f.svc.BySelect(ctx, started.ID)
	if err != nil || !found {
		t.Fatalf("BySelect: %v %v", found, err)
	}
	if m.Rules != match.RulesCustom || m.HostAccountID != one || len(m.Bots) != 2 || m.Bots[1] != bots[1] {
		t.Fatalf("custom match: %+v", m)
	}
	if m.Custom == nil || !m.Custom.VictoryEnabled || m.Custom.StartingGold == nil || *m.Custom.StartingGold != gold {
		t.Fatalf("custom match settings: %+v", m.Custom)
	}
}
