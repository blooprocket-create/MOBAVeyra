package postgres

import (
	"context"
	"crypto/sha256"
	"errors"
	"os"
	"sync"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
)

// testDatabaseURLEnv points integration tests at a disposable Postgres
// database. The tests are skipped when it is unset.
const testDatabaseURLEnv = "VEYRA_TEST_DATABASE_URL"

func openTestStore(t *testing.T) *Store {
	t.Helper()
	url := os.Getenv(testDatabaseURLEnv)
	if url == "" {
		t.Skipf("%s not set; skipping Postgres integration test", testDatabaseURLEnv)
	}
	ctx := context.Background()
	store, err := Open(ctx, url)
	if err != nil {
		t.Fatalf("Open: %v", err)
	}
	t.Cleanup(store.Close)
	if _, err := store.pool.Exec(ctx, `TRUNCATE identity.launch_codes, identity.sessions, identity.accounts`); err != nil {
		t.Fatalf("truncate: %v", err)
	}
	return store
}

func TestMigrationsAreIdempotent(t *testing.T) {
	store := openTestStore(t)
	if err := migrate(context.Background(), store.pool); err != nil {
		t.Fatalf("second migrate: %v", err)
	}
}

func TestHandoffAgainstPostgres(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	if _, err := store.EnsureDevAccount(ctx, "DevOne"); err != nil {
		t.Fatal(err)
	}
	again, err := store.EnsureDevAccount(ctx, "DevOne")
	if err != nil {
		t.Fatalf("EnsureDevAccount must be idempotent: %v", err)
	}

	svc := identity.NewService(store, identity.Settings{
		LauncherSessionLifetime: time.Hour,
		GameSessionLifetime:     time.Hour,
		LaunchCodeLifetime:      20 * time.Second,
		DevLoginEnabled:         true,
	}, time.Now)

	login, _, err := svc.DevLogin(ctx, "DevOne")
	if err != nil {
		t.Fatal(err)
	}
	code, err := svc.IssueLaunchCode(ctx, login.Token, "dev-1")
	if err != nil {
		t.Fatal(err)
	}
	game, acct, err := svc.RedeemLaunchCode(ctx, code.Token, "dev-1")
	if err != nil {
		t.Fatal(err)
	}
	if acct.ID != again.ID {
		t.Fatalf("redeemed for wrong account: %s vs %s", acct.ID, again.ID)
	}
	if _, err := svc.AuthenticateGame(ctx, game.Token); err != nil {
		t.Fatalf("AuthenticateGame: %v", err)
	}
}

// Two racing redemptions of the same code must yield exactly one session.
func TestConcurrentRedeemSucceedsOnce(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	if _, err := store.EnsureDevAccount(ctx, "DevOne"); err != nil {
		t.Fatal(err)
	}
	svc := identity.NewService(store, identity.Settings{
		LauncherSessionLifetime: time.Hour,
		GameSessionLifetime:     time.Hour,
		LaunchCodeLifetime:      20 * time.Second,
		DevLoginEnabled:         true,
	}, time.Now)
	login, _, err := svc.DevLogin(ctx, "DevOne")
	if err != nil {
		t.Fatal(err)
	}
	code, err := svc.IssueLaunchCode(ctx, login.Token, "dev-1")
	if err != nil {
		t.Fatal(err)
	}

	const racers = 8
	var wg sync.WaitGroup
	results := make(chan error, racers)
	for range racers {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, _, err := svc.RedeemLaunchCode(ctx, code.Token, "dev-1")
			results <- err
		}()
	}
	wg.Wait()
	close(results)

	successes := 0
	for err := range results {
		switch {
		case err == nil:
			successes++
		case errors.Is(err, identity.ErrInvalidCredentials):
		default:
			t.Fatalf("unexpected error: %v", err)
		}
	}
	if successes != 1 {
		t.Fatalf("want exactly 1 successful redemption, got %d", successes)
	}
}

func testService(store *Store) *identity.Service {
	return identity.NewService(store, identity.Settings{
		LauncherSessionLifetime: time.Hour,
		GameSessionLifetime:     time.Hour,
		LaunchCodeLifetime:      20 * time.Second,
		DevLoginEnabled:         true,
	}, time.Now)
}

// If issuing the game session fails, the launch code must stay redeemable.
func TestFailedSessionInsertLeavesCodeUnused(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	if _, err := store.EnsureDevAccount(ctx, "DevOne"); err != nil {
		t.Fatal(err)
	}
	svc := testService(store)
	login, _, err := svc.DevLogin(ctx, "DevOne")
	if err != nil {
		t.Fatal(err)
	}
	code, err := svc.IssueLaunchCode(ctx, login.Token, "dev-1")
	if err != nil {
		t.Fatal(err)
	}
	codeHash := sha256.Sum256([]byte(code.Token))
	launcherHash := sha256.Sum256([]byte(login.Token))

	// Reusing the launcher session's hash makes the session insert violate
	// the primary key, simulating a failure after the code is consumed.
	now := time.Now()
	clash := identity.Session{TokenHash: launcherHash[:], Kind: identity.SessionGame, BuildVersion: "dev-1", CreatedAt: now, ExpiresAt: now.Add(time.Hour)}
	if _, err := store.RedeemLaunchCode(ctx, codeHash[:], "dev-1", now, clash); err == nil {
		t.Fatal("expected the clashing session insert to fail")
	}

	if _, _, err := svc.RedeemLaunchCode(ctx, code.Token, "dev-1"); err != nil {
		t.Fatalf("code must still be redeemable after a failed issuance: %v", err)
	}
}

func TestDevLoginRefusesNonDevAccountsInPostgres(t *testing.T) {
	store := openTestStore(t)
	ctx := context.Background()
	if _, err := store.pool.Exec(ctx, `INSERT INTO identity.accounts (display_name, dev_seeded) VALUES ('RealPlayer', false)`); err != nil {
		t.Fatal(err)
	}
	if _, err := store.EnsureDevAccount(ctx, "RealPlayer"); !errors.Is(err, identity.ErrNotDevAccount) {
		t.Fatalf("seeding over a real account: want ErrNotDevAccount, got %v", err)
	}
	if _, _, err := testService(store).DevLogin(ctx, "RealPlayer"); !errors.Is(err, identity.ErrInvalidCredentials) {
		t.Fatalf("dev login to a real account: want ErrInvalidCredentials, got %v", err)
	}
}
