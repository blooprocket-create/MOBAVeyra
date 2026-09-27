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

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/catalog"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/config"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/docker"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/httpapi"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/postgres"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/social"
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

	soc := social.NewService(store.Social())
	rules := party.Rules{MaxSize: cfg.Party.MaxSize, Modes: map[string]party.Mode{}}
	var modes []httpapi.ModeInfo
	for _, m := range cfg.Modes {
		rules.Modes[m.ID] = party.Mode{ID: m.ID, Enabled: m.Enabled, HumanPlayersPerTeam: m.HumanPlayersPerTeam}
		modes = append(modes, httpapi.ModeInfo{ID: m.ID, Enabled: m.Enabled, HumanPlayersPerTeam: m.HumanPlayersPerTeam})
	}
	parties := party.NewService(store.Party(), soc, party.Settings{
		Rules:          rules,
		InviteLifetime: cfg.Party.InviteLifetime,
		DefaultPrivacy: party.Privacy(cfg.Party.DefaultPrivacy),
	}, time.Now)

	vanguards := catalog.New(catalog.Settings{
		Released:      cfg.Vanguards.Released,
		Starters:      cfg.Vanguards.Starters,
		RotationSlots: cfg.Vanguards.RotationSlots,
		StandIn:       catalog.StandIn(cfg.Vanguards.RotationStandIn),
	})
	accounts := account.NewService(store.Account(), vanguards, time.Now)

	matches, err := newMatchService(cfg, store, svc)
	if err != nil {
		return err
	}
	if cfg.Matches.DevCreate {
		log.Warn("development match creation is enabled; never expose this backend publicly")
	}
	go matches.RunReaper(ctx, cfg.Matches.ReapInterval, log)

	queued := selection.PartiesFunc(func(ctx context.Context, accountID string) (bool, error) {
		p, err := parties.Get(ctx, accountID)
		if errors.Is(err, party.ErrNotInParty) {
			return false, nil
		}
		return err == nil && p.Status == party.Queued, err
	})
	selects := selection.NewService(store.Selection(), accounts, displayNames(svc), matches, queued, selection.Settings{
		Practice: selection.PracticeSettings{
			Enabled:      cfg.CustomPractice.Enabled,
			Mode:         cfg.CustomPractice.Mode,
			HostSide:     match.Side(cfg.CustomPractice.HostSide),
			PickDuration: cfg.CustomPractice.PickDuration,
		},
		StartingTimeout: cfg.Selection.StartingTimeout,
	}, time.Now, log)
	go selects.RunTicker(ctx, cfg.Selection.TickInterval)

	srv := &http.Server{
		Addr: cfg.ListenAddress,
		Handler: httpapi.New(httpapi.Deps{
			Identity:       svc,
			Social:         soc,
			Party:          parties,
			Match:          matches,
			Account:        accounts,
			Selection:      selects,
			Modes:          modes,
			Ready:          store,
			Atomic:         store.Atomic,
			BodyLimitBytes: cfg.RequestBodyLimitBytes,
			DevLogin:       cfg.DevLogin.Enabled,
			DevAccounts:    cfg.DevLogin.Accounts,
			DevMatches:     cfg.Matches.DevCreate,
			Log:            log,
		}),
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

// newMatchService builds the match service with the configured allocator.
func newMatchService(cfg config.Config, store *postgres.Store, ids *identity.Service) (*match.Service, error) {
	var allocator match.Allocator = noAllocator{}
	settings := match.Settings{
		Modes: map[string]match.Mode{},
		Practice: match.PracticeSettings{
			Enabled:  cfg.CustomPractice.Enabled,
			Mode:     cfg.CustomPractice.Mode,
			HostSide: match.Side(cfg.CustomPractice.HostSide),
		},
		ReadyTimeout:      cfg.Matches.ReadyTimeout,
		MaxDuration:       cfg.Matches.MaxDuration,
		RemoveServerAfter: cfg.Matches.RemoveServerAfter,
	}
	for _, m := range cfg.Modes {
		settings.Modes[m.ID] = match.Mode{ID: m.ID, Enabled: m.Enabled, HumanPlayersPerTeam: m.HumanPlayersPerTeam}
	}
	if d := cfg.Allocator.Docker; cfg.Allocator.Kind == config.AllocatorDocker && d != nil {
		dockerAllocator, err := docker.New(docker.Config{
			Endpoint:       d.Endpoint,
			APIVersion:     d.APIVersion,
			RequestTimeout: d.RequestTimeout,
			Image:          d.Image,
			Network:        d.Network,
			NamePrefix:     d.ContainerNamePrefix,
			ContainerPort:  d.ContainerPort,
			HostIP:         d.HostIP,
			ServerArgs:     d.ServerArgs,
			StopTimeout:    d.StopTimeout,
		})
		if err != nil {
			return nil, err
		}
		allocator = dockerAllocator
		settings.HostPortMin, settings.HostPortMax = d.HostPortMin, d.HostPortMax
		settings.PublicHost, settings.BackendURL = d.PublicHost, d.BackendURL
	}
	return match.NewService(store.Match(), displayNames(ids), allocator, settings, time.Now), nil
}

// displayNames resolves accounts' display names through identity.
func displayNames(ids *identity.Service) match.AccountsFunc {
	return func(ctx context.Context, accountIDs []string) (map[string]string, error) {
		found, err := ids.Accounts(ctx, accountIDs)
		if err != nil {
			return nil, err
		}
		names := make(map[string]string, len(found))
		for id, a := range found {
			names[id] = a.DisplayName
		}
		return names, nil
	}
}

// noAllocator is used when this backend starts no match servers
// (allocator.kind "none"). Config refuses dev match creation without an
// allocator, so Start is never reached through the API.
type noAllocator struct{}

func (noAllocator) Start(context.Context, match.ServerSpec) error {
	return errors.New("this backend starts no match servers")
}

func (noAllocator) Status(context.Context, string) (match.ServerStatus, error) {
	return match.ServerStatus{Missing: true}, nil
}

func (noAllocator) Remove(context.Context, string) error { return nil }
