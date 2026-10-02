package progression

import (
	"context"
	"errors"
	"slices"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

var (
	ctx = context.Background()
	t0  = time.Date(2026, 10, 1, 12, 0, 0, 0, time.UTC)
)

// testCatalog is a fixture catalog: four released Vanguards, two starters,
// one in rotation.
type testCatalog struct{}

func (testCatalog) IsReleased(id string) bool { return slices.Contains(testCatalog{}.Released(), id) }
func (testCatalog) IsStarter(id string) bool  { return slices.Contains(testCatalog{}.Starters(), id) }
func (testCatalog) Released() []string        { return []string{"cairn", "qazharr", "oriel", "bryn"} }
func (testCatalog) Starters() []string        { return []string{"cairn", "qazharr"} }
func (testCatalog) Rotation() []string        { return []string{"oriel"} }

// testTuning is fixture tuning, independent of the committed configuration.
func testTuning() Tuning {
	return Tuning{
		AccountXP:     AccountXP{PerMinute: 6, WinBonus: 30, CoopBelowLevel: 10},
		AccountLevels: Curve{First: 100, Growth: 10, GrowthUntil: 50},
		FluxPerLevel:  400,
		RefinedFlux:   RefinedFlux{Milestones: []Milestone{{3, 250}}, EveryLevels: 5, Amount: 100},
		Mastery: MasteryTuning{PerMinute: 10, WinBonus: 100, PerformanceCap: 300, Weights: Weights{Kills: 15, Assists: 10},
			Levels: Curve{First: 500, Growth: 100, GrowthUntil: 5}, EmoteTierLevels: []int{1, 2, 5}},
		Prices: map[string]Price{"cairn": {1500, 300}, "qazharr": {1500, 300}, "oriel": {1500, 300}, "bryn": {3000, 550}},
	}
}

type fixture struct {
	svc      *Service
	store    *MemStore
	accounts *account.Service
}

func newFixture(t *testing.T) *fixture {
	t.Helper()
	store := NewMemStore()
	accounts := account.NewService(account.NewMemStore(), testCatalog{}, func() time.Time { return t0 })
	categories := map[string]string{"casual_select": CategoryCasual, "coop_beginner": CategoryAI}
	return &fixture{svc: NewService(store, accounts, testTuning(), categories, func() time.Time { return t0 }), store: store, accounts: accounts}
}

// endedMatch is a Standard match won by side A after thirty minutes: acc-a on
// A playing cairn, acc-b on B playing bryn.
func endedMatch(mode string, reason match.EndReason, winner match.Side) *match.Match {
	return &match.Match{ID: "m-1", Mode: mode, Rules: match.RulesStandard, State: match.Ended,
		Participants: []match.Participant{{AccountID: "acc-a", Side: match.SideA, VanguardID: "cairn"}, {AccountID: "acc-b", Side: match.SideB, VanguardID: "bryn"}},
		Result: &match.Result{EndReason: reason, Winner: winner, DurationSeconds: 30 * 60,
			Participants: []match.ParticipantResult{{AccountID: "acc-a", Joined: true, ConnectedAtEnd: true}, {AccountID: "acc-b", Joined: true, ConnectedAtEnd: true}},
			Players:      []match.PlayerResult{{Side: match.SideA, Name: "A", AccountID: "acc-a", VanguardID: "cairn", Statistics: match.PlayerStatistics{Kills: 4, Assists: 2}}}}}
}

func TestAWinGivesAccountXPLevelsFluxAndMastery(t *testing.T) {
	f := newFixture(t)
	m := endedMatch("casual_select", match.EndPrimeWellDestroyed, match.SideA)
	if err := f.svc.Grant(ctx, m); err != nil {
		t.Fatalf("Grant: %v", err)
	}
	// 30 minutes × 6 + 30 = 210 XP: Level 1 → 2 takes 100, 2 → 3 takes 110.
	won, _ := f.svc.Progression(ctx, "acc-a")
	if won.Level != 3 || won.LevelXP != 0 || won.LifetimeXP != 210 || won.Flux != 800 || won.RefinedFlux != 250 || won.LevelNeed != 120 {
		t.Fatalf("winner: %+v", won)
	}
	// The loser: 180 XP, Level 2 with 80 into it, one level-up's Flux.
	lost, _ := f.svc.Progression(ctx, "acc-b")
	if lost.Level != 2 || lost.LevelXP != 80 || lost.Flux != 400 || lost.RefinedFlux != 0 {
		t.Fatalf("loser: %+v", lost)
	}
	g, err := f.svc.MatchRewards(ctx, "m-1", "acc-a")
	// Mastery: 300 + 100 + performance 4×15 + 2×10 = 480, short of Level 2's 500.
	if err != nil || g.Reason != Earned || g.AccountXP != 210 || g.LevelBefore != 1 || g.LevelAfter != 3 || g.VanguardID != "cairn" ||
		g.MasteryPoints != 480 || g.MasteryBefore != 1 || g.MasteryAfter != 1 {
		t.Fatalf("grant: %+v %v", g, err)
	}
	collection, _ := f.svc.Collection(ctx, "acc-a")
	if i := slices.IndexFunc(collection, func(e CollectionEntry) bool { return e.VanguardID == "cairn" }); collection[i].Mastery.LifetimePoints != 480 {
		t.Fatalf("cairn's Mastery: %+v", collection[i])
	}
}

func TestAReplayedResultGrantsNothingTwice(t *testing.T) {
	f := newFixture(t)
	m := endedMatch("casual_select", match.EndPrimeWellDestroyed, match.SideA)
	for range 3 {
		if err := f.svc.Grant(ctx, m); err != nil {
			t.Fatalf("Grant: %v", err)
		}
	}
	if a, _ := f.svc.Progression(ctx, "acc-a"); a.LifetimeXP != 210 || a.Flux != 800 {
		t.Fatalf("granted more than once: %+v", a)
	}
}

func TestIneligibleParticipantsEarnNothingAndLearnWhy(t *testing.T) {
	f := newFixture(t)
	practice := endedMatch("custom_practice", match.EndHostEnded, "")
	practice.Rules = match.RulesPractice
	remake := endedMatch("casual_select", match.EndRemake, "")
	remake.ID = "m-2"
	loss := endedMatch("casual_select", match.EndPrimeWellDestroyed, match.SideA)
	loss.ID = "m-3"
	loss.Result.Participants[0].PersonalLoss = true
	for _, m := range []*match.Match{practice, remake, loss} {
		if err := f.svc.Grant(ctx, m); err != nil {
			t.Fatalf("Grant: %v", err)
		}
	}
	for id, want := range map[string]Reason{"m-1": ReasonCustom, "m-2": ReasonNoContest, "m-3": ReasonPersonalLoss} {
		if g, err := f.svc.MatchRewards(ctx, id, "acc-a"); err != nil || g.Reason != want || g.AccountXP != 0 || g.MasteryPoints != 0 {
			t.Errorf("%s: %+v %v", id, g, err)
		}
	}
	if a, _ := f.svc.Progression(ctx, "acc-a"); a.LifetimeXP != 0 || a.Level != 1 {
		t.Fatalf("earned anyway: %+v", a)
	}
	if _, err := f.svc.MatchRewards(ctx, "m-unknown", "acc-a"); !errors.Is(err, ErrNoGrant) {
		t.Fatalf("an unadjudicated match: %v", err)
	}
}

func TestCoopStopsGivingAccountXPAtItsLevelButKeepsMastery(t *testing.T) {
	f := newFixture(t)
	// Bring acc-a to Level 10 through casual wins.
	for i := 0; ; i++ {
		if a, _ := f.svc.Progression(ctx, "acc-a"); a.Level >= 10 {
			break
		}
		m := endedMatch("casual_select", match.EndPrimeWellDestroyed, match.SideA)
		m.ID = "casual-" + string(rune('a'+i))
		if err := f.svc.Grant(ctx, m); err != nil {
			t.Fatalf("Grant: %v", err)
		}
	}
	before, _ := f.svc.Progression(ctx, "acc-a")
	coop := endedMatch("coop_beginner", match.EndPrimeWellDestroyed, match.SideA)
	coop.ID = "coop-1"
	if err := f.svc.Grant(ctx, coop); err != nil {
		t.Fatalf("Grant: %v", err)
	}
	g, _ := f.svc.MatchRewards(ctx, "coop-1", "acc-a")
	after, _ := f.svc.Progression(ctx, "acc-a")
	if g.Reason != ReasonCoopLevel || g.AccountXP != 0 || g.MasteryPoints == 0 || after.LifetimeXP != before.LifetimeXP {
		t.Fatalf("co-op at Level 10: %+v, %+v → %+v", g, before, after)
	}
	// acc-b, still below Level 10, earns account XP from the same match.
	if b, _ := f.svc.MatchRewards(ctx, "coop-1", "acc-b"); b.Reason != Earned || b.AccountXP == 0 {
		t.Fatalf("co-op below Level 10: %+v", b)
	}
}

func TestARecordedPurchaseAnswersBeforeWhatIsOnSaleNow(t *testing.T) {
	f := newFixture(t)
	if _, err := f.svc.DevGrant(ctx, "acc-a", 4000, 0); err != nil {
		t.Fatalf("DevGrant: %v", err)
	}
	bought, _, err := f.svc.Buy(ctx, "acc-a", "purchase-0001", "bryn", CurrencyFlux)
	if err != nil {
		t.Fatalf("Buy: %v", err)
	}
	// Bryn leaves the storefront: the same purchase, retried, still returns its first outcome.
	tuning := testTuning()
	delete(tuning.Prices, "bryn")
	later := NewService(f.store, f.accounts, tuning, map[string]string{}, func() time.Time { return t0 })
	if again, _, err := later.Buy(ctx, "acc-a", "purchase-0001", "bryn", CurrencyFlux); err != nil || again != bought {
		t.Fatalf("a retry after bryn left the storefront: %+v %v", again, err)
	}
	// Its ID with a Vanguard not on sale is still a conflict, not "not for sale".
	if _, _, err := later.Buy(ctx, "acc-a", "purchase-0001", "nobody", CurrencyFlux); !errors.Is(err, ErrPurchaseConflict) {
		t.Fatalf("the ID for an unreleased Vanguard: %v", err)
	}
	if _, _, err := later.Buy(ctx, "acc-a", "purchase-0002", "nobody", CurrencyFlux); !errors.Is(err, ErrNotForSale) {
		t.Fatalf("a new purchase of an unreleased Vanguard: %v", err)
	}
}

func TestBuyingAVanguardSpendsOnceAndGrantsIt(t *testing.T) {
	f := newFixture(t)
	if _, _, err := f.svc.Buy(ctx, "acc-a", "purchase-0001", "bryn", CurrencyFlux); !errors.Is(err, ErrInsufficient) {
		t.Fatalf("with nothing to spend: %v", err)
	}
	if _, err := f.svc.DevGrant(ctx, "acc-a", 4000, 600); err != nil {
		t.Fatalf("DevGrant: %v", err)
	}
	bought, summary, err := f.svc.Buy(ctx, "acc-a", "purchase-0001", "bryn", CurrencyFlux)
	if err != nil || bought.Price != 3000 || summary.Flux != 1000 || summary.RefinedFlux != 600 {
		t.Fatalf("Buy: %+v %+v %v", bought, summary, err)
	}
	available, _ := f.accounts.Vanguards(ctx, "acc-a")
	if !slices.Contains(available.Owned, "bryn") {
		t.Fatalf("bryn not owned: %+v", available)
	}
	if e, _ := f.accounts.Entitlements(ctx, "acc-a"); len(e) != 1 || e[0].Source != account.SourcePurchase {
		t.Fatalf("entitlement: %+v", e)
	}
	// Repeating the purchase returns it again without spending.
	again, summary, err := f.svc.Buy(ctx, "acc-a", "purchase-0001", "bryn", CurrencyFlux)
	if err != nil || again != bought || summary.Flux != 1000 {
		t.Fatalf("a repeated purchase: %+v %+v %v", again, summary, err)
	}
	if _, _, err := f.svc.Buy(ctx, "acc-a", "purchase-0001", "oriel", CurrencyFlux); !errors.Is(err, ErrPurchaseConflict) {
		t.Fatalf("the ID for another Vanguard: %v", err)
	}
	if _, _, err := f.svc.Buy(ctx, "acc-a", "purchase-0002", "bryn", CurrencyRefinedFlux); !errors.Is(err, ErrAlreadyOwned) {
		t.Fatalf("buying it twice: %v", err)
	}
	if _, summary, err := f.svc.Buy(ctx, "acc-a", "purchase-0003", "oriel", CurrencyRefinedFlux); err != nil || summary.RefinedFlux != 300 {
		t.Fatalf("a rotation Vanguard for Refined Flux: %+v %v", summary, err)
	}
	if _, _, err := f.svc.Buy(ctx, "acc-a", "purchase-0004", "nobody", CurrencyFlux); !errors.Is(err, ErrNotForSale) {
		t.Fatalf("an unreleased Vanguard: %v", err)
	}
	for _, bad := range []struct {
		id       string
		currency Currency
	}{{"short", CurrencyFlux}, {"purchase-0005", "gold"}} {
		if _, _, err := f.svc.Buy(ctx, "acc-a", bad.id, "qazharr", bad.currency); !errors.Is(err, ErrInvalidPurchase) {
			t.Fatalf("%+v: %v", bad, err)
		}
	}
	if adj := f.store.Adjustments(); len(adj) != 1 || adj[0].Flux != 4000 {
		t.Fatalf("the development grant is recorded: %+v", adj)
	}

	// The development reset takes the bought Vanguards back and forgets their purchases; the balances stay.
	if err := f.svc.DevResetPurchases(ctx, "acc-a"); err != nil {
		t.Fatalf("DevResetPurchases: %v", err)
	}
	if available, _ := f.accounts.Vanguards(ctx, "acc-a"); len(available.Owned) != 0 {
		t.Fatalf("still owned after the reset: %+v", available.Owned)
	}
	if _, summary, err := f.svc.Buy(ctx, "acc-a", "purchase-0001", "oriel", CurrencyRefinedFlux); err != nil || summary.RefinedFlux != 0 {
		t.Fatalf("the forgotten purchase ID buys again: %+v %v", summary, err)
	}
}

func TestTheAssignmentLearnsEachPlayersMastery(t *testing.T) {
	f := newFixture(t)
	if err := f.svc.Grant(ctx, endedMatch("casual_select", match.EndPrimeWellDestroyed, match.SideA)); err != nil {
		t.Fatalf("Grant: %v", err)
	}
	// 480 points short of 500 keep Cairn at Level 1, emote tier 1; Bryn, never played, is the same.
	for _, c := range []struct {
		vanguard string
		want     match.ParticipantMastery
	}{{"cairn", match.ParticipantMastery{Level: 1, EmoteTier: 1}}, {"bryn", match.ParticipantMastery{Level: 1, EmoteTier: 1}}} {
		if got, err := f.svc.MasteryOf(ctx, "acc-a", c.vanguard); err != nil || got != c.want {
			t.Errorf("%s: %+v %v", c.vanguard, got, err)
		}
	}
	// Another win takes Cairn past 500 points: Level 2, the second emote tier.
	m := endedMatch("casual_select", match.EndPrimeWellDestroyed, match.SideA)
	m.ID = "m-2"
	if err := f.svc.Grant(ctx, m); err != nil {
		t.Fatalf("Grant: %v", err)
	}
	if got, _ := f.svc.MasteryOf(ctx, "acc-a", "cairn"); got != (match.ParticipantMastery{Level: 2, EmoteTier: 2}) {
		t.Fatalf("after two wins: %+v", got)
	}
}

func TestTheCollectionShowsEveryReleasedVanguard(t *testing.T) {
	f := newFixture(t)
	if _, err := f.accounts.ChooseStarter(ctx, "acc-a", "cairn"); err != nil {
		t.Fatalf("ChooseStarter: %v", err)
	}
	collection, err := f.svc.Collection(ctx, "acc-a")
	if err != nil || len(collection) != 4 {
		t.Fatalf("Collection: %+v %v", collection, err)
	}
	ids := []string{}
	for _, e := range collection {
		ids = append(ids, e.VanguardID)
	}
	if !slices.Equal(ids, testCatalog{}.Released()) {
		t.Fatalf("not in the catalog's order: %v", ids)
	}
	cairn, oriel, bryn := collection[0], collection[2], collection[3]
	if !cairn.Owned || cairn.Source != account.SourceStarter || cairn.Purchasable {
		t.Errorf("the starter: %+v", cairn)
	}
	if oriel.Owned || !oriel.Rotation || !oriel.Purchasable {
		t.Errorf("a rotation Vanguard: %+v", oriel)
	}
	if bryn.Owned || bryn.Rotation || bryn.Price != (Price{3000, 550}) || bryn.Mastery.Level != 1 || bryn.MasteryNeed != 500 || bryn.EmoteTier != 1 {
		t.Errorf("an unowned Vanguard: %+v", bryn)
	}
}

func TestASpendTakesTheBalanceAndIsRecordedOrChangesNothing(t *testing.T) {
	f := newFixture(t)
	ctx := context.Background()
	if _, err := f.svc.DevGrant(ctx, "acc-a", 7000, 100); err != nil {
		t.Fatal(err)
	}
	if err := f.svc.Spend(ctx, "acc-a", "display_name_change", CurrencyFlux, 6000); err != nil {
		t.Fatal(err)
	}
	if err := f.svc.Spend(ctx, "acc-a", "display_name_change", CurrencyRefinedFlux, 600); !errors.Is(err, ErrInsufficient) {
		t.Fatalf("100 Refined Flux does not cover 600: %v", err)
	}
	if err := f.svc.Spend(ctx, "acc-a", "display_name_change", CurrencyFlux, 0); !errors.Is(err, ErrInvalidPurchase) {
		t.Fatalf("nothing to spend: %v", err)
	}
	s, _ := f.svc.Progression(ctx, "acc-a")
	if s.Flux != 1000 || s.RefinedFlux != 100 {
		t.Fatalf("balances: %+v", s)
	}
	if spends := f.store.Spends(); len(spends) != 1 || spends[0].Reason != "display_name_change" || spends[0].Amount != 6000 {
		t.Fatalf("recorded: %+v", spends)
	}
}
