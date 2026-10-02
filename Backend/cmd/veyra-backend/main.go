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
	"math/rand/v2"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/account"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/catalog"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/config"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/docker"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/favorites"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/firebaseauth"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/httpapi"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/identity"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/lobby"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/matchmaking"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/postgres"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/progression"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/settings"
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

	playerLogin, err := newPlayerLogin(cfg.PlayerLogin)
	if err != nil {
		return err
	}
	if playerLogin != nil {
		log.Info("player login enabled", "provider", cfg.PlayerLogin.Provider, "project", cfg.PlayerLogin.Firebase.ProjectID)
	}
	svc := identity.NewService(store, identity.Settings{
		LauncherSessionLifetime: cfg.Sessions.Launcher,
		GameSessionLifetime:     cfg.Sessions.Game,
		LaunchCodeLifetime:      cfg.LaunchCodeLifetime,
		DevLoginEnabled:         cfg.DevLogin.Enabled,
		PlayerLogin:             playerLogin,
	}, time.Now)

	soc := social.NewService(store.Social())
	rules := party.Rules{MaxSize: cfg.Party.MaxSize, Modes: map[string]party.Mode{}}
	var modes []httpapi.ModeInfo
	for _, m := range cfg.Modes {
		matchmade := m.Matchmaking == config.MatchmakingCasualSelect || m.Matchmaking == config.MatchmakingCoop || m.Matchmaking == config.MatchmakingDraftPick
		rules.Modes[m.ID] = party.Mode{ID: m.ID, Enabled: m.Enabled, HumanPlayersPerTeam: m.HumanPlayersPerTeam, Matchmade: matchmade}
		modes = append(modes, httpapi.ModeInfo{ID: m.ID, Enabled: m.Enabled, Category: m.Category, HumanPlayersPerTeam: m.HumanPlayersPerTeam, Matchmaking: m.Matchmaking})
	}
	parties := party.NewService(store.Party(), soc, party.Settings{
		Rules:          rules,
		InviteLifetime: cfg.Party.InviteLifetime,
		DefaultPrivacy: party.Privacy(cfg.Party.DefaultPrivacy),
	}, time.Now)

	vanguards := catalog.New(catalog.Settings{
		Released: cfg.Vanguards.Released,
		Starters: cfg.Vanguards.Starters,
		Rotation: catalog.RotationSettings{
			Slots:    cfg.Vanguards.RotationSlots,
			Epoch:    cfg.Vanguards.RotationEpoch,
			Week:     cfg.Vanguards.RotationWeek,
			Seed:     cfg.Vanguards.RotationSeed,
			Releases: cfg.Vanguards.RotationReleases,
		},
	}, time.Now)
	// A week with too few eligible Vanguards has no rotation, a launch precondition (Modes & Access Bible §3).
	if week, ok := vanguards.WeekAt(time.Now()); ok && len(vanguards.Rotation()) == 0 {
		log.Warn("the rotation offers nothing: fewer eligible Vanguards than its slots", "week", week, "slots", cfg.Vanguards.RotationSlots)
	}
	accounts := account.NewService(store.Account(), vanguards, time.Now)

	matches, err := newMatchService(cfg, store, svc)
	if err != nil {
		return err
	}
	if cfg.Matches.DevCreate {
		log.Warn("development match creation is enabled; never expose this backend publicly")
	}
	// Each verified result grants account progression in its own transaction (ADR-045 §1).
	progress := progression.NewService(store.Progression(), accounts, progressionTuning(cfg.Progression), modeCategories(cfg.Modes), time.Now)
	matches.SetRewards(progress)
	// Each assignment carries its players' Mastery, for the mastery emote (ADR-045 §9).
	matches.SetMasteries(progress)
	// Names change in one unit of work with their charge (ADR-049).
	enableNameChanges(svc, cfg.Names, progress, store.Atomic)
	if cfg.Progression.DevGrant {
		log.Warn("the development currency grant is enabled; never expose this backend publicly")
	}
	go matches.RunReaper(ctx, cfg.Matches.ReapInterval, log)

	queued := selection.PartiesFunc(func(ctx context.Context, accountID string) (bool, error) {
		p, err := parties.Get(ctx, accountID)
		if errors.Is(err, party.ErrNotInParty) {
			return false, nil
		}
		return err == nil && p.Status.InMatchmaking(), err
	})
	selects := selection.NewService(store.Selection(), accounts, displayNames(svc), matches, queued, soc, selection.Settings{
		Practice: selection.PracticeSettings{
			Enabled:      cfg.CustomPractice.Enabled,
			Mode:         cfg.CustomPractice.Mode,
			HostSide:     match.Side(cfg.CustomPractice.HostSide),
			PickDuration: cfg.CustomPractice.PickDuration,
		},
		Casual: selection.CasualSettings{
			PickDuration:    cfg.CasualSelect.PickDuration,
			PresenceTimeout: cfg.CasualSelect.PresenceTimeout,
			FinalDuration:   cfg.CasualSelect.FinalDuration,
		},
		Draft: selection.DraftSettings{
			Timing:          draftTiming(cfg.DraftPick),
			PresenceTimeout: cfg.DraftPick.PresenceTimeout,
		},
		Custom: selection.CustomSettings{
			Mode:         cfg.CustomLobby.Mode,
			PickDuration: cfg.CustomLobby.PickDuration,
		},
		StartingTimeout: cfg.Selection.StartingTimeout,
		FluxSpells:      cfg.FluxSpells.Roster,
	}, time.Now, log)
	go selects.RunTicker(ctx, cfg.Selection.TickInterval)

	// A custom lobby's members are busy for queues and parties, and a lobby asks
	// whether its members are in a match, a select or a queue (ADR-021 §1).
	released := map[string]bool{}
	for _, id := range cfg.Vanguards.Released {
		released[id] = true
	}
	lobbies := lobby.NewService(store.Lobby(), soc, lobbyActivity{activity: activity{matches: matches, selects: selects}, queued: queued}, lobby.Limits{
		PlayersPerSide:  cfg.CustomLobby.PlayersPerSide,
		Released:        released,
		StartingGoldMin: cfg.CustomLobby.StartingGold.Min,
		StartingGoldMax: cfg.CustomLobby.StartingGold.Max,
	}, cfg.CustomLobby.InviteLifetime, time.Now)
	lobbies.SetLauncher(customSelects{selects})
	selects.SetLobbies(lobbies)
	busy := busyChecker(cfg, matches, selects, lobbies)
	parties.SetActivity(busy)
	mmSettings := matchmaking.Settings{AcceptDuration: cfg.MatchFound.AcceptDuration, SearchLimit: cfg.Matchmaking.SearchLimit}
	for _, m := range cfg.Modes {
		if m.Enabled && (m.Matchmaking == config.MatchmakingCasualSelect || m.Matchmaking == config.MatchmakingCoop || m.Matchmaking == config.MatchmakingDraftPick) {
			mmSettings.Modes = append(mmSettings.Modes, matchmaking.Mode{ID: m.ID, TeamSize: m.HumanPlayersPerTeam, VersusAI: m.Matchmaking == config.MatchmakingCoop})
		}
	}
	draft := map[string]bool{}
	for _, m := range cfg.Modes {
		draft[m.ID] = m.Matchmaking == config.MatchmakingDraftPick
	}
	coop := map[string]config.Mode{}
	for _, m := range cfg.Modes {
		if m.Matchmaking == config.MatchmakingCoop {
			coop[m.ID] = m
		}
	}
	matchmaker := matchmaking.NewService(store.Matchmaking(), parties, soc, busy, casualSelects{selects: selects, vanguards: vanguards, coop: coop, draft: draft},
		mmSettings, time.Now, log)
	selects.SetMatchmaking(matchmaker)
	go matchmaker.Run(ctx, cfg.Matchmaking.Interval)

	prefs := settings.NewService(store.Settings(), cfg.Settings.MaxDocumentBytes)
	// Chat asks the party, social, selection, match and settings domains who reads each conversation (ADR-046 §1).
	talk := newChatService(store.Chat(), cfg.Chat, parties, soc, selects, matches, prefs, displayNames(svc))
	// Expired messages go whether or not anyone sends again (ADR-046 §4).
	go talk.RunPruner(ctx, cfg.Chat.PruneInterval, log)
	// Reports and commendation name players as the match recorded them (ADR-047 §5).
	conductService := newConductService(store.Conduct(), cfg.Conduct, matches)
	// Profiles read levels, ownership and Mastery from progression, and blocks from social (ADR-048 §1).
	profiles := newProfileService(store.Profile(), cfg.Profile, svc, progress, accounts, soc)
	// Favorites read the released roster from the catalog, and champion select and matches from their services (ADR-058 §5).
	favs := favorites.NewService(store.Favorites(), vanguards, favoriteActivity{matches: matches, selects: selects}, cfg.Favorites.MaxPerAccount)

	srv := &http.Server{
		Addr: cfg.ListenAddress,
		Handler: httpapi.New(httpapi.Deps{
			Identity:       svc,
			Social:         soc,
			Party:          parties,
			Lobby:          customLobbies(cfg, lobbies),
			Match:          matches,
			Account:        accounts,
			Progression:    progress,
			Selection:      selects,
			Matchmaking:    matchmaker,
			Settings:       prefs,
			Chat:           talk,
			Conduct:        conductService,
			Profile:        profiles,
			Favorites:      favs,
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

// activity says whether players are in a match, a champion select or, when
// lobbies is set, a custom lobby, for the party and matchmaking rules that
// keep them out of the queue meanwhile.
type activity struct {
	matches *match.Service
	selects *selection.Service
	lobbies *lobby.Service
}

func (a activity) Busy(ctx context.Context, accounts []string) (bool, error) {
	for _, id := range accounts {
		if _, inMatch, err := a.matches.Current(ctx, id); err != nil || inMatch {
			return inMatch, err
		}
		if _, selecting, err := a.selects.Current(ctx, id); err != nil || selecting {
			return selecting, err
		}
		if a.lobbies == nil {
			continue
		}
		if _, err := a.lobbies.Get(ctx, id); err == nil {
			return true, nil
		} else if !errors.Is(err, lobby.ErrNotInLobby) {
			return false, err
		}
	}
	return false, nil
}

// lobbyActivity is what keeps a player from a lobby's changes and launch: a
// match, a champion select, or a party in matchmaking.
type lobbyActivity struct {
	activity
	queued selection.PartiesFunc
}

func (a lobbyActivity) Busy(ctx context.Context, accounts []string) (bool, error) {
	if busy, err := a.activity.Busy(ctx, accounts); err != nil || busy {
		return busy, err
	}
	for _, id := range accounts {
		if queued, err := a.queued(ctx, id); err != nil || queued {
			return queued, err
		}
	}
	return false, nil
}

// customLobbies is the lobby service the routes serve, or nil while custom
// lobbies are disabled.
// busyChecker is what keeps an account from parties and queues. A lobby keeps its
// members busy only while lobbies are served: with them switched off, a member of
// a lobby persisted before could neither see nor leave it.
func busyChecker(cfg config.Config, matches *match.Service, selects *selection.Service, lobbies *lobby.Service) activity {
	return activity{matches: matches, selects: selects, lobbies: customLobbies(cfg, lobbies)}
}

func customLobbies(cfg config.Config, lobbies *lobby.Service) *lobby.Service {
	if !cfg.CustomLobby.Enabled {
		return nil
	}
	return lobbies
}

// customSelects opens a launched custom lobby's champion select.
type customSelects struct{ selects *selection.Service }

func (c customSelects) OpenCustom(ctx context.Context, launch lobby.Launch) error {
	seats := make([]selection.CasualSeat, len(launch.Members))
	for i, m := range launch.Members {
		seats[i] = selection.CasualSeat{AccountID: m.AccountID, Side: match.Side(m.Seat.Side)}
	}
	bots := make([]match.Bot, len(launch.Bots))
	for i, b := range launch.Bots {
		bots[i] = match.Bot{Side: match.Side(b.Seat.Side), VanguardID: b.VanguardID, Difficulty: match.BotDifficulty(b.Difficulty)}
	}
	var gold *float64
	if launch.Settings.StartingGold != nil {
		g := *launch.Settings.StartingGold
		gold = &g
	}
	_, err := c.selects.OpenCustom(ctx, selection.CustomLaunch{
		LobbyID:       launch.LobbyID,
		HostAccountID: launch.HostID,
		Seats:         seats,
		Bots:          bots,
		Settings:      match.CustomSettings{VictoryEnabled: launch.Settings.VictoryEnabled, StartingGold: gold},
	})
	return err
}

// casualSelects opens the matchmaker's Casual Selects, and a co-op mode's
// select with its enemy AI team seated (ADR-039 §3).
type casualSelects struct {
	selects   *selection.Service
	vanguards *catalog.Catalog
	// coop holds each co-op mode's enemy AI team.
	coop map[string]config.Mode
	// draft marks the Draft Pick modes, whose select bans and picks in turns (ADR-042 §3).
	draft map[string]bool
}

func (c casualSelects) OpenCasual(ctx context.Context, mode string, seats []matchmaking.SelectSeat) (string, error) {
	casual := make([]selection.CasualSeat, len(seats))
	opponents := match.SideB
	for i, seat := range seats {
		casual[i] = selection.CasualSeat{AccountID: seat.AccountID, Side: seat.Side}
		if seat.Side == match.SideB {
			opponents = match.SideA
		}
	}
	if c.draft[mode] {
		return c.selects.OpenDraft(ctx, mode, casual)
	}
	coop, ok := c.coop[mode]
	if !ok {
		return c.selects.OpenCasual(ctx, mode, casual)
	}
	// Drawn from this week's rotation; from every released Vanguard while it offers none (ADR-039 §8).
	pool := c.vanguards.Rotation()
	if len(pool) < coop.AIPerTeam {
		pool = c.vanguards.Released()
	}
	bots, err := selection.DrawOpponents(pool, coop.AIPerTeam, opponents, match.BotDifficulty(coop.AIDifficulty), rand.Shuffle)
	if err != nil {
		return "", err
	}
	return c.selects.OpenCoop(ctx, mode, casual, bots)
}

// draftTiming is Draft Pick's turns and phase lengths for its selects.
func draftTiming(d config.DraftPick) selection.Timing {
	timing := selection.Timing{Ban: d.BanDuration, Pick: d.PickDuration, Final: d.FinalDuration}
	for _, t := range d.Turns {
		timing.Turns = append(timing.Turns, selection.Turn{Ban: t.Ban, Side: match.Side(t.Side), Count: t.Count})
	}
	return timing
}

// newMatchService builds the match service with the configured allocator.
func newMatchService(cfg config.Config, store *postgres.Store, ids *identity.Service) (*match.Service, error) {
	var allocator match.Allocator = noAllocator{}
	settings := match.Settings{
		Modes: map[string]match.Mode{},
		Maps: map[match.MapKind]string{
			match.MapPlay:        cfg.Matches.Maps.Play,
			match.MapDevelopment: cfg.Matches.Maps.Development,
		},
		Practice: match.PracticeSettings{
			Enabled:  cfg.CustomPractice.Enabled,
			Mode:     cfg.CustomPractice.Mode,
			HostSide: match.Side(cfg.CustomPractice.HostSide),
		},
		Custom: match.CustomModeSettings{
			Enabled:         cfg.CustomLobby.Enabled,
			Mode:            cfg.CustomLobby.Mode,
			PlayersPerSide:  cfg.CustomLobby.PlayersPerSide,
			StartingGoldMin: cfg.CustomLobby.StartingGold.Min,
			StartingGoldMax: cfg.CustomLobby.StartingGold.Max,
		},
		ReadyTimeout:      cfg.Matches.ReadyTimeout,
		MaxDuration:       cfg.Matches.MaxDuration,
		RemoveServerAfter: cfg.Matches.RemoveServerAfter,
		HistoryPageSize:   cfg.Matches.HistoryPageSize,
	}
	for _, m := range cfg.Modes {
		settings.Modes[m.ID] = match.Mode{ID: m.ID, Enabled: m.Enabled, HumanPlayersPerTeam: m.HumanPlayersPerTeam, AIPerTeam: m.AIPerTeam,
			AIDifficulty: match.BotDifficulty(m.AIDifficulty)}
	}
	for _, b := range cfg.CustomPractice.Bots {
		settings.Practice.Bots = append(settings.Practice.Bots, match.Bot{Side: match.Side(b.Side), VanguardID: b.VanguardID, Difficulty: match.BotDifficulty(b.Difficulty)})
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

// newPlayerLogin builds the identity provider's token verifier (ADR-038), or
// nil when player login is off.
func newPlayerLogin(cfg config.PlayerLogin) (identity.Verifier, error) {
	if cfg.Provider != config.PlayerLoginFirebase {
		return nil, nil
	}
	fb := cfg.Firebase
	return firebaseauth.New(firebaseauth.Config{
		ProjectID: fb.ProjectID,
		KeysURL:   fb.KeysURL,
		ClockSkew: fb.ClockSkew,
		Client:    &http.Client{Timeout: fb.KeysFetchTimeout},
		Now:       time.Now,
	})
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
