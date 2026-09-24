package postgres

import (
	"context"
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
