// Command veyra-backend runs the Veyra backend service (ADR-005).
//
// Usage: VEYRA_DATABASE_URL=postgres://... veyra-backend -config config/local.json
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/config"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/httpapi"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/postgres"
)

func main() {
	log := slog.New(slog.NewJSONHandler(os.Stdout, nil))
	if err := run(log); err != nil {
		log.Error("backend stopped", "err", err)
		os.Exit(1)
	}
}

func run(log *slog.Logger) error {
	configPath := flag.String("config", "", "path to the backend config JSON (required)")
	flag.Parse()
	if *configPath == "" {
		return errors.New("-config is required")
	}
	cfg, err := config.Load(*configPath)
	if err != nil {
		return err
	}
	databaseURL := os.Getenv(config.DatabaseURLEnv)
	if databaseURL == "" {
		return fmt.Errorf("%s is required", config.DatabaseURLEnv)
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	store, err := postgres.Open(ctx, databaseURL)
	if err != nil {
		return err
	}
	defer store.Close()

	if cfg.DevLogin.Enabled {
		for _, name := range cfg.DevLogin.Accounts {
			if _, err := store.EnsureDevAccount(ctx, name); err != nil {
				return fmt.Errorf("seed dev account %s: %w", name, err)
			}
		}
		log.Warn("development login is enabled; never expose this backend publicly",
			"environment", cfg.Environment, "accounts", len(cfg.DevLogin.Accounts))
	}

	svc := identity.NewService(store, identity.Settings{
		LauncherSessionLifetime: cfg.Sessions.Launcher,
		GameSessionLifetime:     cfg.Sessions.Game,
		LaunchCodeLifetime:      cfg.LaunchCodeLifetime,
		DevLoginEnabled:         cfg.DevLogin.Enabled,
	}, time.Now)

	srv := &http.Server{
		Addr:         cfg.ListenAddress,
		Handler:      httpapi.New(svc, store, cfg.RequestBodyLimitBytes, cfg.DevLogin.Enabled, log),
		ReadTimeout:  cfg.HTTP.Read,
		WriteTimeout: cfg.HTTP.Write,
		IdleTimeout:  cfg.HTTP.Idle,
	}

	errs := make(chan error, 1)
	go func() {
		log.Info("listening", "address", cfg.ListenAddress, "environment", cfg.Environment)
		errs <- srv.ListenAndServe()
	}()

	select {
	case err := <-errs:
		return err
	case <-ctx.Done():
	}
	shutdownCtx, cancel := context.WithTimeout(context.Background(), cfg.HTTP.Shutdown)
	defer cancel()
	log.Info("shutting down")
	return srv.Shutdown(shutdownCtx)
}
