package httpapi

import (
	"errors"
	"net/http"
	"slices"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/progression"
)

// routeProgression registers a player's progression, Collection and purchase
// routes (ADR-045 §7), and the development route that grants currency.
func (s *Server) routeProgression(mux *http.ServeMux) {
	if s.Progression == nil {
		return
	}
	mux.HandleFunc("GET /v1/me/progression", s.authed(s.myProgression))
	mux.HandleFunc("GET /v1/me/collection", s.authed(s.myCollection))
	mux.HandleFunc("POST /v1/me/purchases", s.authed(s.purchase))
	if s.DevLogin && s.Progression.DevGrantEnabled() {
		mux.HandleFunc("POST /v1/dev/accounts/{name}/progression-grant", s.devProgressionGrant)
	}
}

// progressionJSON is an account's level, XP and balances. Flux and Refined
// Flux are the persistent account currencies, never in-match Team Flux.
type progressionJSON struct {
	Level       int   `json:"level"`
	LevelXP     int64 `json:"levelXp"`
	LevelNeed   int64 `json:"levelNeed"`
	LifetimeXP  int64 `json:"lifetimeXp"`
	Flux        int64 `json:"flux"`
	RefinedFlux int64 `json:"refinedFlux"`
}

func toProgressionJSON(s progression.Summary) progressionJSON {
	return progressionJSON{Level: s.Level, LevelXP: s.LevelXP, LevelNeed: s.LevelNeed, LifetimeXP: s.LifetimeXP, Flux: s.Flux, RefinedFlux: s.RefinedFlux}
}

type priceJSON struct {
	Flux        int64 `json:"flux"`
	RefinedFlux int64 `json:"refinedFlux"`
}

func (s *Server) myProgression(w http.ResponseWriter, r *http.Request, actor string) {
	summary, err := s.Progression.Progression(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"progression": toProgressionJSON(summary)})
}

// myCollection lists every released Vanguard with the player's ownership and
// Mastery of each (Bible §4): visibility, never permission to pick.
func (s *Server) myCollection(w http.ResponseWriter, r *http.Request, actor string) {
	entries, err := s.Progression.Collection(r.Context(), actor)
	if err != nil {
		s.fail(w, err)
		return
	}
	type masteryJSON struct {
		Level          int   `json:"level"`
		LevelPoints    int64 `json:"levelPoints"`
		LevelNeed      int64 `json:"levelNeed"`
		LifetimePoints int64 `json:"lifetimePoints"`
		EmoteTier      int   `json:"emoteTier"`
	}
	type entryJSON struct {
		VanguardID  string      `json:"vanguardId"`
		Owned       bool        `json:"owned"`
		Source      *string     `json:"source"`
		Rotation    bool        `json:"rotation"`
		Price       priceJSON   `json:"price"`
		Purchasable bool        `json:"purchasable"`
		Mastery     masteryJSON `json:"mastery"`
	}
	out := make([]entryJSON, 0, len(entries))
	for _, e := range entries {
		out = append(out, entryJSON{VanguardID: e.VanguardID, Owned: e.Owned, Source: textOrNil(string(e.Source)), Rotation: e.Rotation,
			Price: priceJSON{Flux: e.Price.Flux, RefinedFlux: e.Price.RefinedFlux}, Purchasable: e.Purchasable,
			Mastery: masteryJSON{Level: e.Mastery.Level, LevelPoints: e.Mastery.LevelPoints, LevelNeed: e.MasteryNeed, LifetimePoints: e.Mastery.LifetimePoints,
				EmoteTier: e.EmoteTier}})
	}
	writeJSON(w, http.StatusOK, map[string]any{"vanguards": out})
}

// purchase buys a Vanguard with one account currency. The client generates
// purchaseId, so a retry after a lost response returns the same purchase and
// never spends twice (Bible §6, §7).
func (s *Server) purchase(w http.ResponseWriter, r *http.Request, actor string) {
	var req struct {
		PurchaseID string `json:"purchaseId"`
		VanguardID string `json:"vanguardId"`
		Currency   string `json:"currency"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	bought, summary, err := s.Progression.Buy(r.Context(), actor, req.PurchaseID, req.VanguardID, progression.Currency(req.Currency))
	if err != nil {
		s.fail(w, err)
		return
	}
	type purchaseJSON struct {
		PurchaseID string `json:"purchaseId"`
		VanguardID string `json:"vanguardId"`
		Currency   string `json:"currency"`
		Price      int64  `json:"price"`
	}
	writeJSON(w, http.StatusOK, map[string]any{
		"purchase":    purchaseJSON{PurchaseID: bought.PurchaseID, VanguardID: bought.VanguardID, Currency: string(bought.Currency), Price: bought.Price},
		"progression": toProgressionJSON(summary),
	})
}

// devProgressionGrant gives a development account currency, so scripted runs
// can buy Vanguards. Local only (ADR-045 §6).
func (s *Server) devProgressionGrant(w http.ResponseWriter, r *http.Request) {
	name := r.PathValue("name")
	if !slices.Contains(s.DevAccounts, name) {
		writeError(w, http.StatusNotFound, "account_not_found")
		return
	}
	var req struct {
		Flux        int64 `json:"flux"`
		RefinedFlux int64 `json:"refinedFlux"`
	}
	if !s.decode(w, r, &req) {
		return
	}
	acct, err := s.Identity.LookupAccount(r.Context(), name)
	if err != nil {
		s.fail(w, err)
		return
	}
	summary, err := s.Progression.DevGrant(r.Context(), acct.ID, req.Flux, req.RefinedFlux)
	if err != nil {
		s.fail(w, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"progression": toProgressionJSON(summary)})
}

// rewardsJSON is what a match gave the player (ADR-045 §7). Account XP,
// Flux and Refined Flux are apart from the match's own Gold, XP and Team
// Flux; Reason names why nothing was earned.
type rewardsJSON struct {
	Reason        *string `json:"reason"`
	AccountXP     int64   `json:"accountXp"`
	LevelBefore   int     `json:"levelBefore"`
	LevelAfter    int     `json:"levelAfter"`
	Flux          int64   `json:"flux"`
	RefinedFlux   int64   `json:"refinedFlux"`
	VanguardID    *string `json:"vanguardId"`
	MasteryPoints int64   `json:"masteryPoints"`
	MasteryBefore int     `json:"masteryBefore"`
	MasteryAfter  int     `json:"masteryAfter"`
}

// matchRewards returns the player's rewards from a match, or nil while none
// are recorded: before adjudication, or without progression.
func (s *Server) matchRewards(r *http.Request, matchID, actor string) (*rewardsJSON, error) {
	if s.Progression == nil {
		return nil, nil
	}
	g, err := s.Progression.MatchRewards(r.Context(), matchID, actor)
	if errors.Is(err, progression.ErrNoGrant) {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	return &rewardsJSON{Reason: textOrNil(string(g.Reason)), AccountXP: g.AccountXP, LevelBefore: g.LevelBefore, LevelAfter: g.LevelAfter,
		Flux: g.Flux, RefinedFlux: g.RefinedFlux, VanguardID: textOrNil(g.VanguardID), MasteryPoints: g.MasteryPoints,
		MasteryBefore: g.MasteryBefore, MasteryAfter: g.MasteryAfter}, nil
}
