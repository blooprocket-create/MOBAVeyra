package profile

import (
	"context"
	"errors"
	"testing"
)

// fixtureCatalog is a small catalog, independent of the committed config.
var fixtureCatalog = Catalog{
	Icons:             []string{"default", "vanguard_cairn", "vanguard_oriel"},
	Backgrounds:       []string{"default", "vanguard_cairn"},
	DefaultIcon:       "default",
	DefaultBackground: "default",
}

// fakeAccounts names three accounts.
type fakeAccounts struct{}

func (fakeAccounts) ByName(_ context.Context, name string) (string, string, error) {
	switch name {
	case "DevOne":
		return "acc-1", "DevOne", nil
	case "DevTwo":
		return "acc-2", "DevTwo", nil
	case "DevThree":
		return "acc-3", "DevThree", nil
	}
	return "", "", ErrUnavailable
}

// fakeProgress: every account is level 12; acc-1 owns cairn (Mastery 4); the
// rotation lends oriel, which ownership never counts.
type fakeProgress struct{ owned map[string][]string }

func (p *fakeProgress) Level(context.Context, string) (int, error) { return 12, nil }

func (p *fakeProgress) Owns(_ context.Context, accountID, vanguardID string) (bool, error) {
	for _, id := range p.owned[accountID] {
		if id == vanguardID {
			return true, nil
		}
	}
	return false, nil
}

func (p *fakeProgress) Owned(_ context.Context, accountID string) ([]string, error) {
	return p.owned[accountID], nil
}

func (p *fakeProgress) MasteryLevel(_ context.Context, _, vanguardID string) (int, error) {
	if vanguardID == "cairn" {
		return 4, nil
	}
	return 1, nil
}

type fakeBlocks struct{ pairs map[[2]string]bool }

func (b fakeBlocks) BlockedWithAny(_ context.Context, account string, others []string) (bool, error) {
	for _, o := range others {
		if b.pairs[[2]string{account, o}] || b.pairs[[2]string{o, account}] {
			return true, nil
		}
	}
	return false, nil
}

type fixture struct {
	svc      *Service
	progress *fakeProgress
	blocks   fakeBlocks
}

func newFixture() *fixture {
	f := &fixture{progress: &fakeProgress{owned: map[string][]string{"acc-1": {"cairn"}}}, blocks: fakeBlocks{pairs: map[[2]string]bool{}}}
	f.svc = NewService(NewMemStore(), fakeAccounts{}, f.progress, f.blocks, fixtureCatalog)
	return f
}

func TestAnAccountThatNeverChoseShowsTheDefaultsAndPrivateHistory(t *testing.T) {
	f := newFixture()
	ctx := context.Background()
	a, err := f.svc.Settings(ctx, "acc-2")
	if err != nil || a != (Appearance{Icon: "default", Background: "default"}) {
		t.Fatalf("settings: %+v %v", a, err)
	}
	p, err := f.svc.View(ctx, "acc-1", "DevTwo")
	if err != nil || p.Name != "DevTwo" || p.Icon != "default" || p.Level != 12 || p.Featured != nil || p.SharesMatchHistory {
		t.Fatalf("profile: %+v %v", p, err)
	}
	if _, err := f.svc.HistoryOwner(ctx, "acc-1", "DevTwo"); !errors.Is(err, ErrHistoryPrivate) {
		t.Fatalf("history is private by default: %v", err)
	}
}

func TestOnlyCatalogChoicesAndOwnedVanguardsAreSaved(t *testing.T) {
	f := newFixture()
	ctx := context.Background()
	if _, err := f.svc.SaveSettings(ctx, "acc-1", Appearance{Icon: "uploaded", Background: "default"}); !errors.Is(err, ErrInvalidIcon) {
		t.Fatalf("want ErrInvalidIcon, got %v", err)
	}
	if _, err := f.svc.SaveSettings(ctx, "acc-1", Appearance{Icon: "default", Background: "vanguard_oriel"}); !errors.Is(err, ErrInvalidBackground) {
		t.Fatalf("want ErrInvalidBackground, got %v", err)
	}
	if _, err := f.svc.SaveSettings(ctx, "acc-1", Appearance{Icon: "default", Background: "default", FeaturedVanguard: "oriel"}); !errors.Is(err, ErrNotOwned) {
		t.Fatalf("a rotation Vanguard is not owned: %v", err)
	}
	if choices, err := f.svc.FeaturedChoices(ctx, "acc-1"); err != nil || len(choices) != 1 || choices[0] != "cairn" {
		t.Fatalf("featured choices are the owned Vanguards: %v %v", choices, err)
	}
	saved := Appearance{Icon: "vanguard_oriel", Background: "vanguard_cairn", FeaturedVanguard: "cairn", ShowMatchHistory: true}
	if _, err := f.svc.SaveSettings(ctx, "acc-1", saved); err != nil {
		t.Fatal(err)
	}
	p, err := f.svc.View(ctx, "acc-2", "DevOne")
	if err != nil || p.Icon != "vanguard_oriel" || p.Background != "vanguard_cairn" || p.Featured == nil || *p.Featured != (Featured{VanguardID: "cairn", MasteryLevel: 4}) ||
		!p.SharesMatchHistory {
		t.Fatalf("profile: %+v %v", p, err)
	}
	// Clearing the featured Vanguard is allowed.
	if _, err := f.svc.SaveSettings(ctx, "acc-1", Appearance{Icon: "default", Background: "default"}); err != nil {
		t.Fatal(err)
	}
	if p, _ := f.svc.View(ctx, "acc-2", "DevOne"); p.Featured != nil {
		t.Fatalf("cleared: %+v", p)
	}
}

func TestAVanguardNoLongerOwnedIsNoLongerFeatured(t *testing.T) {
	f := newFixture()
	ctx := context.Background()
	if _, err := f.svc.SaveSettings(ctx, "acc-1", Appearance{Icon: "default", Background: "default", FeaturedVanguard: "cairn"}); err != nil {
		t.Fatal(err)
	}
	f.progress.owned["acc-1"] = nil
	if a, _ := f.svc.Settings(ctx, "acc-1"); a.FeaturedVanguard != "" {
		t.Fatalf("settings: %+v", a)
	}
	if p, _ := f.svc.View(ctx, "acc-2", "DevOne"); p.Featured != nil {
		t.Fatalf("profile: %+v", p)
	}
}

func TestSharedHistoryOpensUntilTheOwnerStopsSharing(t *testing.T) {
	f := newFixture()
	ctx := context.Background()
	if _, err := f.svc.SaveSettings(ctx, "acc-2", Appearance{Icon: "default", Background: "default", ShowMatchHistory: true}); err != nil {
		t.Fatal(err)
	}
	if owner, err := f.svc.HistoryOwner(ctx, "acc-1", "DevTwo"); err != nil || owner != "acc-2" {
		t.Fatalf("shared: %q %v", owner, err)
	}
	if _, err := f.svc.SaveSettings(ctx, "acc-2", Appearance{Icon: "default", Background: "default"}); err != nil {
		t.Fatal(err)
	}
	if _, err := f.svc.HistoryOwner(ctx, "acc-1", "DevTwo"); !errors.Is(err, ErrHistoryPrivate) {
		t.Fatalf("no longer shared: %v", err)
	}
	if owner, err := f.svc.HistoryOwner(ctx, "acc-2", "DevTwo"); err != nil || owner != "acc-2" {
		t.Fatalf("the owner always reads their own: %q %v", owner, err)
	}
}

func TestABlockEitherWayMakesAProfileUnavailableAsAnUnknownNameIs(t *testing.T) {
	f := newFixture()
	ctx := context.Background()
	if _, err := f.svc.SaveSettings(ctx, "acc-2", Appearance{Icon: "default", Background: "default", ShowMatchHistory: true}); err != nil {
		t.Fatal(err)
	}
	f.blocks.pairs[[2]string{"acc-2", "acc-1"}] = true
	for _, viewer := range []string{"acc-1"} {
		if _, err := f.svc.View(ctx, viewer, "DevTwo"); !errors.Is(err, ErrUnavailable) {
			t.Fatalf("blocked by the owner: %v", err)
		}
		if _, err := f.svc.HistoryOwner(ctx, viewer, "DevTwo"); !errors.Is(err, ErrUnavailable) {
			t.Fatalf("blocked history: %v", err)
		}
	}
	if _, err := f.svc.View(ctx, "acc-2", "DevOne"); !errors.Is(err, ErrUnavailable) {
		t.Fatalf("the blocker cannot view either: %v", err)
	}
	if _, err := f.svc.View(ctx, "acc-1", "Nobody"); !errors.Is(err, ErrUnavailable) {
		t.Fatalf("an unknown name: %v", err)
	}
	if _, err := f.svc.View(ctx, "acc-3", "DevTwo"); err != nil {
		t.Fatalf("anyone else still views it: %v", err)
	}
}
