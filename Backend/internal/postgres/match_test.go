package postgres

import (
	"context"
	"encoding/json"
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// Fixture settings, independent of the committed config.
const (
	testPortMin = 7780
	testPortMax = 7781
)

type matchFixture struct {
	store *Store
	svc   *match.Service
	alloc *match.FakeAllocator
	ids   map[string]string
	now   time.Time
}

func newMatchFixture(t *testing.T, names ...string) *matchFixture {
	t.Helper()
	store := openTestStore(t)
	ctx := context.Background()
	f := &matchFixture{store: store, alloc: match.NewFakeAllocator(), ids: map[string]string{}, now: time.Now().UTC().Truncate(time.Microsecond)}
	for _, n := range names {
		a, err := store.EnsureDevAccount(ctx, n)
		if err != nil {
			t.Fatal(err)
		}
		f.ids[n] = a.ID
	}
	accounts := match.AccountsFunc(func(ctx context.Context, ids []string) (map[string]string, error) {
		list, err := store.AccountsByIDs(ctx, ids)
		if err != nil {
			return nil, err
		}
		out := map[string]string{}
		for _, a := range list {
			out[a.ID] = a.DisplayName
		}
		return out, nil
	})
	f.svc = match.NewService(store.Match(), accounts, f.alloc, match.Settings{
		Modes: map[string]match.Mode{"casual": {ID: "casual", Enabled: true, HumanPlayersPerTeam: 5}},
		Maps:  match.FakeMaps,
		Practice: match.PracticeSettings{Enabled: true, Mode: "custom_practice", HostSide: match.SideA,
			Bots: []match.Bot{{Side: match.SideB, VanguardID: "cairn", Difficulty: match.BotBeginner}, {Side: match.SideB, VanguardID: "bryn", Difficulty: match.BotIntermediate}}},
		ReadyTimeout:      time.Minute,
		MaxDuration:       time.Hour,
		RemoveServerAfter: time.Minute,
		HostPortMin:       testPortMin,
		HostPortMax:       testPortMax,
		PublicHost:        "127.0.0.1",
		BackendURL:        "http://backend:8080",
		HistoryPageSize:   1,
	}, func() time.Time { return f.now })
	return f
}

func (f *matchFixture) seats(sides map[string]match.Side) []match.Seat {
	var out []match.Seat
	for name, side := range sides {
		out = append(out, match.Seat{AccountID: f.ids[name], Side: side, VanguardID: "cairn"})
	}
	return out
}

// casual asks for a standard casual match with these seats.
func (f *matchFixture) casual(seats []match.Seat) match.Spec {
	return match.Spec{Mode: "casual", Rules: match.RulesStandard, Seats: seats}
}

func (f *matchFixture) credential(t *testing.T, matchID string) string {
	t.Helper()
	spec, ok := f.alloc.Spec(matchID)
	if !ok {
		t.Fatalf("no server started for %s", matchID)
	}
	var a match.Assignment
	if err := json.Unmarshal(spec.Assignment, &a); err != nil {
		t.Fatal(err)
	}
	return a.ServerCredential
}

func TestMatchLifecycleInPostgres(t *testing.T) {
	f := newMatchFixture(t, "DevOne", "DevTwo")
	ctx := context.Background()
	m, err := f.svc.Create(ctx, f.casual(f.seats(map[string]match.Side{"DevOne": match.SideA, "DevTwo": match.SideB})))
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	stored, err := f.store.Match().MatchByID(ctx, m.ID)
	if err != nil || len(stored.JoinKey) != 32 || len(stored.Participants) != 2 || stored.Server.HostPort != testPortMin {
		t.Fatalf("stored match: %+v %v", stored, err)
	}

	cred := f.credential(t, m.ID)
	if err := f.svc.ServerReady(ctx, cred, m.ID); err != nil {
		t.Fatalf("ServerReady: %v", err)
	}
	pm, ok, err := f.svc.Current(ctx, f.ids["DevTwo"])
	if err != nil || !ok || pm.State != match.Ready || pm.Side != match.SideB || pm.Ticket == "" {
		t.Fatalf("Current: %+v %v %v", pm, ok, err)
	}

	r := match.Result{EndReason: match.EndAbandoned, DurationSeconds: 12.5}
	for _, p := range stored.Participants {
		r.Participants = append(r.Participants, match.ParticipantResult{AccountID: p.AccountID, Joined: true})
	}
	f.now = f.now.Add(time.Minute)
	if err := f.svc.ServerResult(ctx, cred, m.ID, r); err != nil {
		t.Fatalf("ServerResult: %v", err)
	}
	if err := f.svc.ServerResult(ctx, cred, m.ID, r); err != nil {
		t.Fatalf("a replayed result must succeed: %v", err)
	}
	r.DurationSeconds = 13
	if err := f.svc.ServerResult(ctx, cred, m.ID, r); !errors.Is(err, match.ErrResultConflict) {
		t.Fatalf("want ErrResultConflict, got %v", err)
	}

	ended, _ := f.store.Match().MatchByID(ctx, m.ID)
	if ended.State != match.Ended || ended.JoinKey != nil || ended.Result == nil || ended.Result.DurationSeconds != 12.5 || len(ended.Result.Participants) != 2 {
		t.Fatalf("ended match: %+v", ended)
	}
	if _, ok, _ := f.svc.Current(ctx, f.ids["DevOne"]); ok {
		t.Fatal("an ended match must release its players")
	}

	// The server is removed after the grace period, and its port is free again.
	f.now = f.now.Add(time.Minute)
	if err := f.svc.Reap(ctx); err != nil {
		t.Fatalf("Reap: %v", err)
	}
	again, err := f.svc.Create(ctx, f.casual(f.seats(map[string]match.Side{"DevOne": match.SideA})))
	if err != nil || again.Server.HostPort != testPortMin {
		t.Fatalf("reusing the removed server's port: %+v %v", again, err)
	}
}

// A practice match keeps its rules, host, Vanguard and bots, and can end
// host_ended.
func TestPracticeMatchInPostgres(t *testing.T) {
	f := newMatchFixture(t, "DevOne")
	ctx := context.Background()
	host := f.ids["DevOne"]
	m, err := f.svc.Create(ctx, match.Spec{Mode: "custom_practice", Rules: match.RulesPractice, HostAccountID: host,
		Seats: []match.Seat{{AccountID: host, Side: match.SideA, VanguardID: "oriel"}}})
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	stored, err := f.store.Match().MatchByID(ctx, m.ID)
	if err != nil || stored.Rules != match.RulesPractice || stored.HostAccountID != host || stored.Participants[0].VanguardID != "oriel" {
		t.Fatalf("stored practice match: %+v %v", stored, err)
	}
	if len(stored.Bots) != 2 || stored.Bots[0] != (match.Bot{Side: match.SideB, VanguardID: "cairn", Difficulty: match.BotBeginner}) || stored.Bots[1].Difficulty != match.BotIntermediate {
		t.Fatalf("stored bots, in order: %+v", stored.Bots)
	}
	cred := f.credential(t, m.ID)
	if err := f.svc.ServerReady(ctx, cred, m.ID); err != nil {
		t.Fatalf("ServerReady: %v", err)
	}
	r := match.Result{EndReason: match.EndHostEnded, DurationSeconds: 30,
		Participants: []match.ParticipantResult{{AccountID: host, Joined: true, ConnectedAtEnd: true}}}
	if err := f.svc.ServerResult(ctx, cred, m.ID, r); err != nil {
		t.Fatalf("ServerResult: %v", err)
	}
	ended, p, err := f.svc.ForParticipant(ctx, host, m.ID)
	if err != nil || ended.Result == nil || ended.Result.EndReason != match.EndHostEnded || p.VanguardID != "oriel" {
		t.Fatalf("the host's view of the ended match: %+v %+v %v", ended, p, err)
	}
}

// A standard match won by destroying a Prime Well keeps its winner, and the
// schema refuses a winner beside any other end (ADR-011 §13).
func TestVictoryInPostgres(t *testing.T) {
	f := newMatchFixture(t, "DevOne", "DevTwo")
	ctx := context.Background()
	m, err := f.svc.Create(ctx, f.casual(f.seats(map[string]match.Side{"DevOne": match.SideA, "DevTwo": match.SideB})))
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	stored, err := f.store.Match().MatchByID(ctx, m.ID)
	if err != nil {
		t.Fatalf("MatchByID: %v", err)
	}
	cred := f.credential(t, m.ID)
	if err := f.svc.ServerReady(ctx, cred, m.ID); err != nil {
		t.Fatalf("ServerReady: %v", err)
	}
	r := match.Result{EndReason: match.EndPrimeWellDestroyed, Winner: match.SideA, DurationSeconds: 1500}
	for _, p := range stored.Participants {
		r.Participants = append(r.Participants, match.ParticipantResult{AccountID: p.AccountID, Joined: true, ConnectedAtEnd: true})
	}
	if err := f.svc.ServerResult(ctx, cred, m.ID, r); err != nil {
		t.Fatalf("ServerResult: %v", err)
	}
	ended, p, err := f.svc.ForParticipant(ctx, f.ids["DevTwo"], m.ID)
	if err != nil || ended.Result == nil || ended.Result.EndReason != match.EndPrimeWellDestroyed || ended.Result.Winner != match.SideA || p.Side != match.SideB {
		t.Fatalf("the loser's view of the ended match: %+v %+v %v", ended, p, err)
	}
	if ended.Result.Players != nil {
		t.Fatalf("a result sent without a scoreboard keeps none: %+v", ended.Result.Players)
	}

	if _, err := f.store.pool.Exec(ctx, `UPDATE match.results SET end_reason = 'abandoned' WHERE match_id = $1`, m.ID); err == nil {
		t.Fatal("the schema must refuse a winner beside another end")
	}
}

// Racing creations for one account must give it exactly one match.
func TestConcurrentMatchesForOneAccount(t *testing.T) {
	f := newMatchFixture(t, "DevOne", "DevTwo", "DevThree")
	ctx := context.Background()
	others := []string{"DevTwo", "DevThree"}
	var wg sync.WaitGroup
	errs := make(chan error, len(others))
	for _, other := range others {
		wg.Add(1)
		go func() {
			defer wg.Done()
			_, err := f.svc.Create(ctx, f.casual(f.seats(map[string]match.Side{"DevOne": match.SideA, other: match.SideB})))
			errs <- err
		}()
	}
	wg.Wait()
	close(errs)
	created := 0
	for err := range errs {
		switch {
		case err == nil:
			created++
		case errors.Is(err, match.ErrAlreadyInMatch):
		default:
			t.Fatalf("unexpected error: %v", err)
		}
	}
	if created != 1 {
		t.Fatalf("want exactly 1 match, got %d", created)
	}
}

// Racing creations must never share a port.
func TestConcurrentMatchesTakeDistinctPorts(t *testing.T) {
	names := []string{"DevOne", "DevTwo", "DevThree", "DevFour"}
	f := newMatchFixture(t, names...)
	ctx := context.Background()
	var wg sync.WaitGroup
	type outcome struct {
		port int
		err  error
	}
	results := make(chan outcome, len(names))
	for _, n := range names {
		wg.Add(1)
		go func() {
			defer wg.Done()
			m, err := f.svc.Create(ctx, f.casual(f.seats(map[string]match.Side{n: match.SideA})))
			results <- outcome{m.Server.HostPort, err}
		}()
	}
	wg.Wait()
	close(results)
	ports := map[int]bool{}
	for o := range results {
		switch {
		case o.err == nil:
			if ports[o.port] {
				t.Fatalf("port %d given twice", o.port)
			}
			ports[o.port] = true
		case errors.Is(o.err, match.ErrNoServerCapacity):
		default:
			t.Fatalf("unexpected error: %v", o.err)
		}
	}
	if len(ports) != testPortMax-testPortMin+1 {
		t.Fatalf("want every port used once, got %v", ports)
	}
}

func TestAFailedStartLeavesNothingActiveInPostgres(t *testing.T) {
	f := newMatchFixture(t, "DevOne")
	ctx := context.Background()
	f.alloc.FailStarts(errors.New("docker is down"))
	if _, err := f.svc.Create(ctx, f.casual(f.seats(map[string]match.Side{"DevOne": match.SideA}))); !errors.Is(err, match.ErrAllocationFailed) {
		t.Fatalf("want ErrAllocationFailed, got %v", err)
	}
	if _, ok, _ := f.svc.Current(ctx, f.ids["DevOne"]); ok {
		t.Fatal("the player must be free again")
	}
	f.alloc.FailStarts(nil)
	if m, err := f.svc.Create(ctx, f.casual(f.seats(map[string]match.Side{"DevOne": match.SideA}))); err != nil || m.Server.HostPort != testPortMin {
		t.Fatalf("the failed match's port must be free: %+v %v", m, err)
	}
}

// A practice match's scoreboard, its host and its bots, is stored as the
// server reported it and read back the same, so a replay is recognised
// (ADR-017 §5).
func TestAScoreboardInPostgres(t *testing.T) {
	f := newMatchFixture(t, "DevOne")
	ctx := context.Background()
	host := f.ids["DevOne"]
	m, err := f.svc.Create(ctx, match.Spec{Mode: "custom_practice", Rules: match.RulesPractice, HostAccountID: host,
		Seats: []match.Seat{{AccountID: host, Side: match.SideA, VanguardID: "oriel"}}})
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	cred := f.credential(t, m.ID)
	if err := f.svc.ServerReady(ctx, cred, m.ID); err != nil {
		t.Fatalf("ServerReady: %v", err)
	}
	line := func(side match.Side, name, account, vanguard string, kills int) match.PlayerResult {
		return match.PlayerResult{Side: side, Name: name, AccountID: account, VanguardID: vanguard,
			Statistics: match.PlayerStatistics{Kills: kills, Level: 6, VanguardDamage: 2210.75, DamageDealt: match.DamageByType{Physical: 5000.5},
				CrowdControl: match.CrowdControl{Slow: 2.25}, GoldEarned: 2150, GoldBySource: match.GoldBySource{Starting: 500, Minions: 1650}, MinionKills: 70},
			Items: []string{"timing_coil", "", "", "", "", ""}, FluxSpells: [2]string{"blink", "mend"}}
	}
	report := func() match.Result {
		return match.Result{EndReason: match.EndHostEnded, DurationSeconds: 600.5,
			Participants: []match.ParticipantResult{{AccountID: host, Joined: true, ConnectedAtEnd: true}},
			Players:      []match.PlayerResult{line(match.SideA, "DevOne", host, "oriel", 3), line(match.SideB, "Bot 1", "", "cairn", 1), line(match.SideB, "Bot 2", "", "bryn", 0)},
			Wells:        []match.WellCapture{{Site: 1, Side: match.SideB, AtSeconds: 312.25}}}
	}
	if err := f.svc.ServerResult(ctx, cred, m.ID, report()); err != nil {
		t.Fatalf("ServerResult: %v", err)
	}
	stored, err := f.store.Match().MatchByID(ctx, m.ID)
	if err != nil || stored.Result == nil || len(stored.Result.Players) != 3 {
		t.Fatalf("the stored scoreboard: %+v %v", stored.Result, err)
	}
	got := stored.Result.Players
	if got[0].AccountID != host || got[1].AccountID != "" || got[2].Name != "Bot 2" || got[0].Statistics != report().Players[0].Statistics ||
		got[0].Items[0] != "timing_coil" || got[0].FluxSpells != [2]string{"blink", "mend"} ||
		len(stored.Result.Wells) != 1 || stored.Result.Wells[0] != (match.WellCapture{Site: 1, Side: match.SideB, AtSeconds: 312.25}) {
		t.Fatalf("the scoreboard as read back: %+v", got)
	}
	// The server's retry of the same report is the same result; a different scoreboard is not.
	if err := f.svc.ServerResult(ctx, cred, m.ID, report()); err != nil {
		t.Fatalf("a replayed report: %v", err)
	}
	different := report()
	different.Players[1].Statistics.Kills = 2
	if err := f.svc.ServerResult(ctx, cred, m.ID, different); !errors.Is(err, match.ErrResultConflict) {
		t.Fatalf("a different scoreboard: want a conflict, got %v", err)
	}
	if _, err := f.store.pool.Exec(ctx, `UPDATE match.results SET players = '{}'::jsonb WHERE match_id = $1`, m.ID); err == nil {
		t.Fatal("the schema must refuse a scoreboard that is not a list")
	}
}

// Match History reads a player's completed matches from Postgres newest
// first, with their outcome, filtered and paged (ADR-017 §6).
func TestMatchHistoryInPostgres(t *testing.T) {
	f := newMatchFixture(t, "DevOne", "DevTwo")
	ctx := context.Background()
	end := func(r match.Result) string {
		t.Helper()
		m, err := f.svc.Create(ctx, f.casual(f.seats(map[string]match.Side{"DevOne": match.SideA, "DevTwo": match.SideB})))
		if err != nil {
			t.Fatalf("Create: %v", err)
		}
		cred := f.credential(t, m.ID)
		if err := f.svc.ServerReady(ctx, cred, m.ID); err != nil {
			t.Fatalf("ServerReady: %v", err)
		}
		stored, _ := f.store.Match().MatchByID(ctx, m.ID)
		for _, p := range stored.Participants {
			r.Participants = append(r.Participants, match.ParticipantResult{AccountID: p.AccountID, Joined: true, ConnectedAtEnd: true})
		}
		if err := f.svc.ServerResult(ctx, cred, m.ID, r); err != nil {
			t.Fatalf("ServerResult: %v", err)
		}
		f.now = f.now.Add(time.Minute)
		return m.ID
	}
	drawn := end(match.Result{EndReason: match.EndDeveloperRequest, DurationSeconds: 300})
	won := end(match.Result{EndReason: match.EndPrimeWellDestroyed, Winner: match.SideA, DurationSeconds: 1500.5})

	one := f.ids["DevOne"]
	page, next, err := f.svc.History(ctx, one, match.HistoryFilter{}, "")
	if err != nil || len(page) != 1 || page[0].MatchID != won || page[0].Outcome != match.OutcomeWin || page[0].DurationSeconds != 1500.5 || page[0].VanguardID != "cairn" || next == "" {
		t.Fatalf("the newest first: %+v %q %v", page, next, err)
	}
	page, next, err = f.svc.History(ctx, one, match.HistoryFilter{}, next)
	if err != nil || len(page) != 1 || page[0].MatchID != drawn || page[0].Outcome != match.OutcomeNoContest || next != "" {
		t.Fatalf("then the older: %+v %q %v", page, next, err)
	}
	if page, _, err := f.svc.History(ctx, f.ids["DevTwo"], match.HistoryFilter{Outcome: match.OutcomeLoss}, ""); err != nil || len(page) != 1 || page[0].MatchID != won || page[0].Side != match.SideB {
		t.Fatalf("the other side's loss: %+v %v", page, err)
	}
	if page, _, err := f.svc.History(ctx, one, match.HistoryFilter{Mode: "casual", VanguardID: "cairn", Outcome: match.OutcomeNoContest}, ""); err != nil || len(page) != 1 || page[0].MatchID != drawn {
		t.Fatalf("combined filters: %+v %v", page, err)
	}
	if page, _, err := f.svc.History(ctx, one, match.HistoryFilter{VanguardID: "oriel"}, ""); err != nil || len(page) != 0 {
		t.Fatalf("a Vanguard never played: %+v %v", page, err)
	}
	if modes, err := f.svc.HistoryModes(ctx, one); err != nil || len(modes) != 1 || modes[0] != "casual" {
		t.Fatalf("every mode with a saved match: %v %v", modes, err)
	}
}

// A surrender is won by the other side, and a personal loss for absence
// overrides the team's win in the player's history (ADR-019 §5).
func TestSurrenderAndPersonalLossInPostgres(t *testing.T) {
	f := newMatchFixture(t, "DevOne", "DevTwo")
	ctx := context.Background()
	m, err := f.svc.Create(ctx, f.casual(f.seats(map[string]match.Side{"DevOne": match.SideA, "DevTwo": match.SideB})))
	if err != nil {
		t.Fatalf("Create: %v", err)
	}
	cred := f.credential(t, m.ID)
	if err := f.svc.ServerReady(ctx, cred, m.ID); err != nil {
		t.Fatalf("ServerReady: %v", err)
	}
	one, two := f.ids["DevOne"], f.ids["DevTwo"]
	r := match.Result{EndReason: match.EndSurrender, Winner: match.SideA, DurationSeconds: 1200, Participants: []match.ParticipantResult{
		{AccountID: one, Joined: true, ConnectedAtEnd: false, PersonalLoss: true, AbsentSeconds: 400},
		{AccountID: two, Joined: true, ConnectedAtEnd: true},
	}}
	if err := f.svc.ServerResult(ctx, cred, m.ID, r); err != nil {
		t.Fatalf("ServerResult: %v", err)
	}
	stored, err := f.store.Match().MatchByID(ctx, m.ID)
	if err != nil || stored.Result == nil || stored.Result.EndReason != match.EndSurrender || stored.Result.Winner != match.SideA {
		t.Fatalf("the stored result: %+v %v", stored.Result, err)
	}
	for _, p := range stored.Result.Participants {
		if p.AccountID == one && (!p.PersonalLoss || p.AbsentSeconds != 400) {
			t.Fatalf("the absent player's result: %+v", p)
		}
	}
	if page, _, err := f.svc.History(ctx, one, match.HistoryFilter{Outcome: match.OutcomeLoss}, ""); err != nil || len(page) != 1 || !page[0].PersonalLoss {
		t.Fatalf("a personal loss on the winning side is a loss: %+v %v", page, err)
	}
	if page, _, err := f.svc.History(ctx, one, match.HistoryFilter{Outcome: match.OutcomeWin}, ""); err != nil || len(page) != 0 {
		t.Fatalf("and not a win: %+v %v", page, err)
	}
	if page, _, err := f.svc.History(ctx, two, match.HistoryFilter{Outcome: match.OutcomeLoss}, ""); err != nil || len(page) != 1 || page[0].PersonalLoss {
		t.Fatalf("the surrendering side loses: %+v %v", page, err)
	}
}
