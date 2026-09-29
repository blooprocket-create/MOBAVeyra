package httpapi

import (
	"errors"
	"net/http"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// routeMatch registers the match routes (ADR-007). The development-only
// match-creation routes stand in for Match Found and exist only when enabled.
func (s *Server) routeMatch(mux *http.ServeMux) {
	if s.Match == nil {
		return
	}
	if s.DevMatches {
		mux.HandleFunc("POST /v1/dev/matches", s.createDevMatch)
		mux.HandleFunc("GET /v1/dev/matches/{matchId}", s.getDevMatch)
	}
	mux.HandleFunc("GET /v1/me/match", s.authed(s.myMatch))
	mux.HandleFunc("GET /v1/me/matches", s.authed(s.myMatchHistory))
	mux.HandleFunc("GET /v1/me/matches/{matchId}", s.authed(s.myMatchResult))
	mux.HandleFunc("POST /v1/server/matches/{matchId}/ready", s.serverReady)
	mux.HandleFunc("POST /v1/server/matches/{matchId}/result", s.serverResult)
}

type participantJSON struct {
	AccountID   string  `json:"accountId"`
	DisplayName string  `json:"displayName"`
	Side        string  `json:"side"`
	VanguardID  *string `json:"vanguardId"`
}

type participantResultJSON struct {
	AccountID      string `json:"accountId"`
	Joined         bool   `json:"joined"`
	ConnectedAtEnd bool   `json:"connectedAtEnd"`
}

type resultJSON struct {
	EndReason       string                  `json:"endReason"`
	Winner          *string                 `json:"winner"`
	DurationSeconds float64                 `json:"durationSeconds"`
	Participants    []participantResultJSON `json:"participants"`
	// Players is the scoreboard (ADR-017 §5); absent from servers that send none.
	Players []scoreboardLineJSON `json:"players"`
	// Wells are the Flux Wells secured, in order; absent from servers that send none.
	Wells []match.WellCapture `json:"wells"`
}

// scoreboardLineJSON is one player's line as the match server reports it. A
// bot has a null account.
type scoreboardLineJSON struct {
	Side       string                 `json:"side"`
	Name       string                 `json:"name"`
	AccountID  *string                `json:"accountId"`
	VanguardID string                 `json:"vanguardId"`
	Statistics match.PlayerStatistics `json:"statistics"`
	Items      []string               `json:"items"`
	FluxSpells []string               `json:"fluxSpells"`
}

// scoreboardPlayerJSON is one player's line as a participant sees it: no
// account IDs, and You on their own line.
type scoreboardPlayerJSON struct {
	Side       string                 `json:"side"`
	Name       string                 `json:"name"`
	VanguardID string                 `json:"vanguardId"`
	You        bool                   `json:"you"`
	Statistics match.PlayerStatistics `json:"statistics"`
	Items      []string               `json:"items"`
	FluxSpells [2]string              `json:"fluxSpells"`
}

// scoreboardFor is a result's scoreboard as the participant actor sees it;
// nil when the result has none.
func scoreboardFor(players []match.PlayerResult, actor string) []scoreboardPlayerJSON {
	if players == nil {
		return nil
	}
	out := make([]scoreboardPlayerJSON, 0, len(players))
	for _, p := range players {
		items := p.Items
		if items == nil {
			items = []string{}
		}
		out = append(out, scoreboardPlayerJSON{Side: string(p.Side), Name: p.Name, VanguardID: p.VanguardID, You: p.AccountID != "" && p.AccountID == actor,
			Statistics: p.Statistics, Items: items, FluxSpells: p.FluxSpells})
	}
	return out
}

// playersFrom reads a reported scoreboard. It refuses a line whose shape the
// domain cannot hold: an empty account, or other than two spell slots.
func playersFrom(lines []scoreboardLineJSON) ([]match.PlayerResult, bool) {
	// A scoreboard with nobody on it is none: the results say no statistics
	// were recorded rather than showing two empty teams (ADR-017 §5).
	if len(lines) == 0 {
		return nil, true
	}
	out := make([]match.PlayerResult, 0, len(lines))
	for _, l := range lines {
		if (l.AccountID != nil && *l.AccountID == "") || len(l.FluxSpells) != 2 || l.Items == nil {
			return nil, false
		}
		p := match.PlayerResult{Side: match.Side(l.Side), Name: l.Name, VanguardID: l.VanguardID, Statistics: l.Statistics, Items: l.Items,
			FluxSpells: [2]string{l.FluxSpells[0], l.FluxSpells[1]}}
		if l.AccountID != nil {
			p.AccountID = *l.AccountID
		}
		out = append(out, p)
	}
	return out, true
}

// devMatchJSON is a development view of a match. It carries no secrets.
type devMatchJSON struct {
	ID            string            `json:"id"`
	Mode          string            `json:"mode"`
	Rules         string            `json:"rules"`
	HostAccountID *string           `json:"hostAccountId"`
	State         string            `json:"state"`
	Participants  []participantJSON `json:"participants"`
	HostPort      int               `json:"hostPort"`
	CreatedAt     time.Time         `json:"createdAt"`
	ReadyAt       *time.Time        `json:"readyAt"`
	EndedAt       *time.Time        `json:"endedAt"`
	ServerRemoved bool              `json:"serverRemoved"`
	FailureReason *string           `json:"failureReason"`
	Result        *resultJSON       `json:"result"`
}

func toDevMatchJSON(m match.Match) devMatchJSON {
	out := devMatchJSON{
		ID:            m.ID,
		Mode:          m.Mode,
		Rules:         string(m.Rules),
		HostAccountID: textOrNil(m.HostAccountID),
		State:         string(m.State),
		HostPort:      m.Server.HostPort,
		CreatedAt:     m.CreatedAt,
		ReadyAt:       timeOrNil(m.ReadyAt),
		EndedAt:       timeOrNil(m.EndedAt),
		ServerRemoved: !m.Server.RemovedAt.IsZero(),
		FailureReason: textOrNil(string(m.FailureReason)),
		Participants:  []participantJSON{},
	}
	for _, p := range m.Participants {
		out.Participants = append(out.Participants, participantJSON{AccountID: p.AccountID, DisplayName: p.DisplayName, Side: string(p.Side),
			VanguardID: textOrNil(p.VanguardID)})
	}
	if r := m.Result; r != nil {
		out.Result = &resultJSON{EndReason: string(r.EndReason), Winner: textOrNil(string(r.Winner)), DurationSeconds: r.DurationSeconds,
			Participants: []participantResultJSON{}}
		for _, p := range r.Participants {
			out.Result.Participants = append(out.Result.Participants, participantResultJSON(p))
		}
	}
	return out
}

// createDevMatch creates a match from a roster in development, standing in
// for champion select in scripts (ADR-010 §9). Rules default to standard; a
// practice match names its host. Any Vanguard the game defines is accepted,
// including developer ones: the match server refuses unknown Vanguards. The
// map defaults to the development grey box; "play" asks for the battleground
// (ADR-011 §12).
func (s *Server) createDevMatch(w http.ResponseWriter, r *http.Request) {
	var req struct {
		Mode          string `json:"mode"`
		Rules         string `json:"rules"`
		Map           string `json:"map"`
		HostAccountID string `json:"hostAccountId"`
		Participants  []struct {
			AccountID  string `json:"accountId"`
			Side       string `json:"side"`
			VanguardID string `json:"vanguardId"`
		} `json:"participants"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	spec := match.Spec{Mode: req.Mode, Rules: match.Rules(req.Rules), HostAccountID: req.HostAccountID, Map: match.MapKind(req.Map)}
	if spec.Rules == "" {
		spec.Rules = match.RulesStandard
	}
	if spec.Map == "" {
		spec.Map = match.MapDevelopment
	}
	for _, p := range req.Participants {
		spec.Seats = append(spec.Seats, match.Seat{AccountID: p.AccountID, Side: match.Side(p.Side), VanguardID: p.VanguardID})
	}
	m, err := s.Match.Create(r.Context(), spec)
	if err != nil {
		if errors.Is(err, match.ErrAllocationFailed) {
			s.Log.Error("match server did not start", "err", err)
		}
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusCreated, map[string]any{"match": toDevMatchJSON(m)})
}

func (s *Server) getDevMatch(w http.ResponseWriter, r *http.Request) {
	m, err := s.Match.Get(r.Context(), r.PathValue("matchId"))
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"match": toDevMatchJSON(m)})
}

// myMatch answers a player's "what is my match" (ADR-007 §10). The server
// address and the join ticket appear only once the match is ready.
func (s *Server) myMatch(w http.ResponseWriter, r *http.Request, actor string) {
	pm, ok, err := s.Match.Current(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	if !ok {
		writeJSON(w, http.StatusOK, map[string]any{"match": nil})
		return
	}
	type serverJSON struct {
		Host string `json:"host"`
		Port int    `json:"port"`
	}
	out := struct {
		ID         string      `json:"id"`
		Mode       string      `json:"mode"`
		Rules      string      `json:"rules"`
		State      string      `json:"state"`
		Side       string      `json:"side"`
		VanguardID *string     `json:"vanguardId"`
		Server     *serverJSON `json:"server"`
		Ticket     *string     `json:"ticket"`
	}{ID: pm.MatchID, Mode: pm.Mode, Rules: string(pm.Rules), State: string(pm.State), Side: string(pm.Side), VanguardID: textOrNil(pm.VanguardID)}
	if pm.Ticket != "" {
		out.Server = &serverJSON{Host: pm.ServerHost, Port: pm.ServerPort}
		out.Ticket = &pm.Ticket
	}
	writeJSON(w, http.StatusOK, map[string]any{"match": out})
}

// myMatchResult answers a participant's "how did my match end": the verified
// result the client shows after a match (ADR-010 §3). The result is the
// server's report as the backend recorded it, with only the player's own
// participation; it is null until the match has ended. A match the player was
// not in does not exist for them.
func (s *Server) myMatchResult(w http.ResponseWriter, r *http.Request, actor string) {
	m, p, err := s.Match.ForParticipant(r.Context(), actor, r.PathValue("matchId"))
	if err != nil {
		s.fail(w, err)
		return
	}
	type playerResultJSON struct {
		EndReason       string  `json:"endReason"`
		Winner          *string `json:"winner"`
		DurationSeconds float64 `json:"durationSeconds"`
		Joined          bool    `json:"joined"`
		ConnectedAtEnd  bool    `json:"connectedAtEnd"`
		// Players is the scoreboard, null when the server sent none (ADR-017 §5).
		Players []scoreboardPlayerJSON `json:"players"`
		// Wells are the Flux Wells secured, null when the server sent none.
		Wells []match.WellCapture `json:"wells"`
	}
	out := struct {
		ID            string            `json:"id"`
		Mode          string            `json:"mode"`
		Rules         string            `json:"rules"`
		State         string            `json:"state"`
		Side          string            `json:"side"`
		VanguardID    *string           `json:"vanguardId"`
		FailureReason *string           `json:"failureReason"`
		Result        *playerResultJSON `json:"result"`
	}{ID: m.ID, Mode: m.Mode, Rules: string(m.Rules), State: string(m.State), Side: string(p.Side), VanguardID: textOrNil(p.VanguardID),
		FailureReason: textOrNil(string(m.FailureReason))}
	if res := m.Result; res != nil {
		out.Result = &playerResultJSON{EndReason: string(res.EndReason), Winner: textOrNil(string(res.Winner)), DurationSeconds: res.DurationSeconds,
			Players: scoreboardFor(res.Players, actor), Wells: res.Wells}
		for _, pr := range res.Participants {
			if pr.AccountID == actor {
				out.Result.Joined, out.Result.ConnectedAtEnd = pr.Joined, pr.ConnectedAtEnd
			}
		}
	}
	writeJSON(w, http.StatusOK, map[string]any{"match": out})
}

// myMatchHistory lists the player's completed matches newest first (Pre-Game
// Client UX Bible 51, 64, 67): each match's date, mode, duration, the player's
// Vanguard and personal outcome. The query's vanguard, mode and outcome filter
// it; cursor continues from the previous page's next.
func (s *Server) myMatchHistory(w http.ResponseWriter, r *http.Request, actor string) {
	q := r.URL.Query()
	filter := match.HistoryFilter{VanguardID: q.Get("vanguard"), Mode: q.Get("mode"), Outcome: match.Outcome(q.Get("outcome"))}
	entries, next, err := s.Match.History(r.Context(), actor, filter, q.Get("cursor"))
	if err != nil {
		s.fail(w, err)
		return
	}
	// Every mode with a saved match, whatever the filter and the pages read (UX-67).
	modes, err := s.Match.HistoryModes(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	if modes == nil {
		modes = []string{}
	}
	type entryJSON struct {
		ID              string    `json:"id"`
		Mode            string    `json:"mode"`
		Rules           string    `json:"rules"`
		EndedAt         time.Time `json:"endedAt"`
		DurationSeconds float64   `json:"durationSeconds"`
		Side            string    `json:"side"`
		VanguardID      *string   `json:"vanguardId"`
		Outcome         string    `json:"outcome"`
	}
	out := make([]entryJSON, 0, len(entries))
	for _, e := range entries {
		out = append(out, entryJSON{ID: e.MatchID, Mode: e.Mode, Rules: string(e.Rules), EndedAt: e.EndedAt.UTC(), DurationSeconds: e.DurationSeconds,
			Side: string(e.Side), VanguardID: textOrNil(e.VanguardID), Outcome: string(e.Outcome)})
	}
	writeJSON(w, http.StatusOK, map[string]any{"matches": out, "next": textOrNil(next), "modes": modes})
}

func (s *Server) serverReady(w http.ResponseWriter, r *http.Request) {
	credential, ok := bearer(r)
	if !ok {
		writeError(w, http.StatusUnauthorized, "invalid_credentials")
		return
	}
	var req struct{}
	if !s.decode(w, r, &req) {
		return
	}
	if err := s.Match.ServerReady(r.Context(), credential, r.PathValue("matchId")); err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]string{"status": "ready"})
}

func (s *Server) serverResult(w http.ResponseWriter, r *http.Request) {
	credential, ok := bearer(r)
	if !ok {
		writeError(w, http.StatusUnauthorized, "invalid_credentials")
		return
	}
	var req resultJSON
	if !s.decode(w, r, &req) {
		return
	}
	result := match.Result{EndReason: match.EndReason(req.EndReason), DurationSeconds: req.DurationSeconds}
	if req.Winner != nil {
		if *req.Winner == "" {
			writeError(w, http.StatusBadRequest, "invalid_result")
			return
		}
		result.Winner = match.Side(*req.Winner)
	}
	for _, p := range req.Participants {
		result.Participants = append(result.Participants, match.ParticipantResult(p))
	}
	players, ok := playersFrom(req.Players)
	if !ok {
		writeError(w, http.StatusBadRequest, "invalid_result")
		return
	}
	result.Players = players
	result.Wells = req.Wells
	if err := s.Match.ServerResult(r.Context(), credential, r.PathValue("matchId"), result); err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]string{"status": "recorded"})
}

func timeOrNil(t time.Time) *time.Time {
	if t.IsZero() {
		return nil
	}
	return &t
}

func textOrNil(s string) *string {
	if s == "" {
		return nil
	}
	return &s
}
