package identity

import (
	"context"
	"errors"
	"testing"
	"time"
)

var testSettings = Settings{
	LauncherSessionLifetime: time.Hour,
	GameSessionLifetime:     30 * time.Minute,
	LaunchCodeLifetime:      20 * time.Second,
	DevLoginEnabled:         true,
}

type fakeClock struct{ t time.Time }

func (c *fakeClock) now() time.Time          { return c.t }
func (c *fakeClock) advance(d time.Duration) { c.t = c.t.Add(d) }

func newTestService(t *testing.T, settings Settings) (*Service, *fakeClock) {
	t.Helper()
	store := NewMemStore()
	if _, err := store.EnsureDevAccount(context.Background(), "DevOne"); err != nil {
		t.Fatal(err)
	}
	clock := &fakeClock{t: time.Date(2026, 9, 24, 12, 0, 0, 0, time.UTC)}
	return NewService(store, settings, clock.now), clock
}

func launchCode(t *testing.T, svc *Service, build string) string {
	t.Helper()
	ctx := context.Background()
	login, _, err := svc.DevLogin(ctx, "DevOne")
	if err != nil {
		t.Fatalf("DevLogin: %v", err)
	}
	code, err := svc.IssueLaunchCode(ctx, login.Token, build)
	if err != nil {
		t.Fatalf("IssueLaunchCode: %v", err)
	}
	return code.Token
}

func TestFullHandoff(t *testing.T) {
	svc, _ := newTestService(t, testSettings)
	ctx := context.Background()
	code := launchCode(t, svc, "dev-1")

	game, acct, err := svc.RedeemLaunchCode(ctx, code, "dev-1")
	if err != nil {
		t.Fatalf("RedeemLaunchCode: %v", err)
	}
	if acct.DisplayName != "DevOne" {
		t.Fatalf("wrong account: %+v", acct)
	}
	got, err := svc.AuthenticateGame(ctx, game.Token)
	if err != nil || got.ID != acct.ID {
		t.Fatalf("AuthenticateGame = %+v, %v", got, err)
	}
}

func TestLaunchCodeIsSingleUse(t *testing.T) {
	svc, _ := newTestService(t, testSettings)
	code := launchCode(t, svc, "dev-1")
	if _, _, err := svc.RedeemLaunchCode(context.Background(), code, "dev-1"); err != nil {
		t.Fatal(err)
	}
	if _, _, err := svc.RedeemLaunchCode(context.Background(), code, "dev-1"); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("second redeem: want ErrInvalidCredentials, got %v", err)
	}
}

func TestLaunchCodeExpires(t *testing.T) {
	svc, clock := newTestService(t, testSettings)
	code := launchCode(t, svc, "dev-1")
	clock.advance(testSettings.LaunchCodeLifetime)
	if _, _, err := svc.RedeemLaunchCode(context.Background(), code, "dev-1"); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("want ErrInvalidCredentials, got %v", err)
	}
}

func TestBuildMismatchConsumesCode(t *testing.T) {
	svc, _ := newTestService(t, testSettings)
	code := launchCode(t, svc, "dev-1")
	if _, _, err := svc.RedeemLaunchCode(context.Background(), code, "dev-2"); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("mismatched build: want ErrInvalidCredentials, got %v", err)
	}
	if _, _, err := svc.RedeemLaunchCode(context.Background(), code, "dev-1"); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("code must be consumed after a mismatch, got %v", err)
	}
}

func TestSessionKindsAreNotInterchangeable(t *testing.T) {
	svc, _ := newTestService(t, testSettings)
	ctx := context.Background()
	login, _, err := svc.DevLogin(ctx, "DevOne")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := svc.AuthenticateGame(ctx, login.Token); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("launcher token accepted as game token: %v", err)
	}
	code, err := svc.IssueLaunchCode(ctx, login.Token, "dev-1")
	if err != nil {
		t.Fatal(err)
	}
	game, _, err := svc.RedeemLaunchCode(ctx, code.Token, "dev-1")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := svc.IssueLaunchCode(ctx, game.Token, "dev-1"); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("game token accepted as launcher token: %v", err)
	}
	if _, err := svc.AuthenticateGame(ctx, code.Token); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("launch code accepted as game token: %v", err)
	}
}

func TestSessionsExpire(t *testing.T) {
	svc, clock := newTestService(t, testSettings)
	ctx := context.Background()
	login, _, err := svc.DevLogin(ctx, "DevOne")
	if err != nil {
		t.Fatal(err)
	}
	clock.advance(testSettings.LauncherSessionLifetime)
	if _, err := svc.IssueLaunchCode(ctx, login.Token, "dev-1"); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("expired launcher session accepted: %v", err)
	}
}

func TestDevLogin(t *testing.T) {
	svc, _ := newTestService(t, testSettings)
	if _, _, err := svc.DevLogin(context.Background(), "Nobody"); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("unknown account: want ErrInvalidCredentials, got %v", err)
	}
	disabled := testSettings
	disabled.DevLoginEnabled = false
	svc, _ = newTestService(t, disabled)
	if _, _, err := svc.DevLogin(context.Background(), "DevOne"); !errors.Is(err, ErrDevLoginDisabled) {
		t.Fatalf("want ErrDevLoginDisabled, got %v", err)
	}
}

func TestBuildVersionValidation(t *testing.T) {
	svc, _ := newTestService(t, testSettings)
	login, _, err := svc.DevLogin(context.Background(), "DevOne")
	if err != nil {
		t.Fatal(err)
	}
	for _, bad := range []string{"", "has space", "slash/1", string(make([]byte, maxBuildVersionLength+1))} {
		if _, err := svc.IssueLaunchCode(context.Background(), login.Token, bad); !errors.Is(err, ErrInvalidBuildVersion) {
			t.Fatalf("build %q: want ErrInvalidBuildVersion, got %v", bad, err)
		}
	}
}

func TestDevLoginRefusesNonDevAccounts(t *testing.T) {
	store := NewMemStore()
	if _, err := store.CreateAccount("RealPlayer"); err != nil {
		t.Fatal(err)
	}
	if _, err := store.EnsureDevAccount(context.Background(), "RealPlayer"); !errors.Is(err, ErrNotDevAccount) {
		t.Fatalf("seeding over a real account: want ErrNotDevAccount, got %v", err)
	}
	svc := NewService(store, testSettings, time.Now)
	if _, _, err := svc.DevLogin(context.Background(), "RealPlayer"); !errors.Is(err, ErrInvalidCredentials) {
		t.Fatalf("dev login to a real account: want ErrInvalidCredentials, got %v", err)
	}
}
