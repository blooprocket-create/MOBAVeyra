package httpapi

import (
	"net/http"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
)

// routeSelection registers Custom practice and champion select (ADR-010 §7–8).
func (s *Server) routeSelection(mux *http.ServeMux) {
	if s.Selection == nil {
		return
	}
	mux.HandleFunc("POST /v1/practice", s.authed(s.startPractice))
	mux.HandleFunc("GET /v1/me/select", s.authed(s.mySelect))
	mux.HandleFunc("GET /v1/me/selects/{selectId}", s.authed(s.mySelectByID))
	mux.HandleFunc("PUT /v1/me/select/hover", s.authed(s.hoverVanguard))
	mux.HandleFunc("POST /v1/me/select/lock", s.authed(s.lockVanguard))
	mux.HandleFunc("PUT /v1/me/select/spells", s.authed(s.setFluxSpells))
	mux.HandleFunc("POST /v1/me/select/leave", s.authed(s.leaveSelect))
}

type selectSeatJSON struct {
	DisplayName string `json:"displayName"`
	Side        string `json:"side"`
	You         bool   `json:"you"`
	// Hover is shown only to the seat's team (Battleground Bible §15).
	Hover  *string `json:"hover"`
	Locked *string `json:"locked"`
	// FluxSpells are the seat's starting Flux Spells in slot order, "" for an
	// empty slot, shown only to their player (ADR-015 §5).
	FluxSpells []string `json:"fluxSpells"`
}

type selectJSON struct {
	ID    string `json:"id"`
	Kind  string `json:"kind"`
	Mode  string `json:"mode"`
	State string `json:"state"`
	// Deadline is the server's; RemainingSeconds is what is left of it by the
	// server's clock, so a client counts down without trusting its own.
	Deadline         time.Time `json:"deadline"`
	RemainingSeconds float64   `json:"remainingSeconds"`
	// PickSeconds is the pick timer's full length, so a client can draw how
	// much of it is left.
	PickSeconds float64          `json:"pickSeconds"`
	Seats       []selectSeatJSON `json:"seats"`
	// Bots are a custom select's bots, chosen in the lobby: seats already
	// locked, in each side's seat order. Empty for other kinds.
	Bots         []selectBotJSON `json:"bots"`
	MatchID      *string         `json:"matchId"`
	CancelReason *string         `json:"cancelReason"`
}

type selectBotJSON struct {
	Side       string `json:"side"`
	VanguardID string `json:"vanguardId"`
	Difficulty string `json:"difficulty"`
}

// toSelectJSON is a select as one of its players sees it.
func (s *Server) toSelectJSON(session selection.Session, actor string) selectJSON {
	out := selectJSON{
		ID:               session.ID,
		Kind:             string(session.Kind),
		Mode:             session.Mode,
		State:            string(session.State),
		Deadline:         session.Deadline,
		RemainingSeconds: s.Selection.RemainingPick(session).Seconds(),
		PickSeconds:      session.Deadline.Sub(session.CreatedAt).Seconds(),
		Seats:            []selectSeatJSON{},
		Bots:             []selectBotJSON{},
		MatchID:          textOrNil(session.MatchID),
		CancelReason:     textOrNil(string(session.CancelReason)),
	}
	var actorSide string
	for _, bot := range session.Bots {
		out.Bots = append(out.Bots, selectBotJSON{Side: string(bot.Side), VanguardID: bot.VanguardID, Difficulty: string(bot.Difficulty)})
	}
	for _, seat := range session.Seats {
		if seat.AccountID == actor {
			actorSide = string(seat.Side)
		}
	}
	for _, seat := range session.Seats {
		view := selectSeatJSON{DisplayName: seat.DisplayName, Side: string(seat.Side), You: seat.AccountID == actor, Locked: textOrNil(seat.Locked)}
		if string(seat.Side) == actorSide {
			view.Hover = textOrNil(seat.Hover)
		}
		if view.You {
			view.FluxSpells = seat.FluxSpells[:]
		}
		out.Seats = append(out.Seats, view)
	}
	return out
}

// startPractice opens a solo practice select: the player alone, picking from
// their available Vanguards (ADR-010 §7).
func (s *Server) startPractice(w http.ResponseWriter, r *http.Request, actor string) {
	session, err := s.Selection.StartPractice(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusCreated, map[string]any{"select": s.toSelectJSON(session, actor)})
}

// mySelect answers "am I in a champion select": the client resumes one after
// a restart, and polls it while picking, which is its presence there.
func (s *Server) mySelect(w http.ResponseWriter, r *http.Request, actor string) {
	session, ok, err := s.Selection.Poll(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	if !ok {
		writeJSON(w, http.StatusOK, map[string]any{"select": nil})
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"select": s.toSelectJSON(session, actor)})
}

// mySelectByID returns a select the player was in, over or not, so the client
// learns how it ended: the match it started, or why it was cancelled.
func (s *Server) mySelectByID(w http.ResponseWriter, r *http.Request, actor string) {
	session, err := s.Selection.ForParticipant(r.Context(), actor, r.PathValue("selectId"))
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"select": s.toSelectJSON(session, actor)})
}

func (s *Server) hoverVanguard(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		VanguardID string `json:"vanguardId"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	session, err := s.Selection.Hover(r.Context(), actor, req.VanguardID)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"select": s.toSelectJSON(session, actor)})
}

// setFluxSpells sets the player's starting Flux Spells, two slots in slot
// order with "" for an empty one (ADR-015 §5).
func (s *Server) setFluxSpells(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		FluxSpells []string `json:"fluxSpells"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	var spells [2]string
	if len(req.FluxSpells) != len(spells) {
		s.fail(w, match.ErrInvalidFluxSpells)
		return
	}
	copy(spells[:], req.FluxSpells)
	session, err := s.Selection.SetFluxSpells(r.Context(), actor, spells)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"select": s.toSelectJSON(session, actor)})
}

// leaveSelect leaves a Casual Select, which cancels it for everyone: a dodge
// (Match Flow Bible §2). The others return to the queue.
func (s *Server) leaveSelect(w http.ResponseWriter, r *http.Request, actor string) {
	session, err := s.Selection.Leave(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"select": s.toSelectJSON(session, actor)})
}

// lockVanguard locks the player's pick. When every pick is locked the select
// creates its match before answering, so the answer says whether it started.
func (s *Server) lockVanguard(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		VanguardID string `json:"vanguardId"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	session, err := s.Selection.Lock(r.Context(), actor, req.VanguardID)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"select": s.toSelectJSON(session, actor)})
}
