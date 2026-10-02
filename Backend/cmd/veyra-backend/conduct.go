package main

import (
	"context"
	"errors"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/conduct"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/config"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// conductMatches answers the matches an account played, with the names and
// sides the match recorded for its human participants (ADR-047 §5).
type conductMatches struct{ matches *match.Service }

func (c conductMatches) Played(ctx context.Context, accountID, matchID string) (conduct.PlayedMatch, error) {
	m, _, err := c.matches.ForParticipant(ctx, accountID, matchID)
	if errors.Is(err, match.ErrMatchNotFound) {
		return conduct.PlayedMatch{}, conduct.ErrNotParticipant
	}
	if err != nil {
		return conduct.PlayedMatch{}, err
	}
	out := conduct.PlayedMatch{}
	// Only a match with a result has results to report or commend from.
	if m.State == match.Ended {
		out.EndedAt = m.EndedAt
	}
	for _, p := range m.Participants {
		out.Participants = append(out.Participants, conduct.Participant{AccountID: p.AccountID, DisplayName: p.DisplayName, Side: string(p.Side)})
	}
	return out, nil
}

// newConductService builds reports and commendation over the match domain.
func newConductService(store conduct.Store, cfg config.Conduct, matches *match.Service) *conduct.Service {
	return conduct.NewService(store, conductMatches{matches: matches}, conduct.Tuning{Reasons: cfg.Reasons, DetailsMaxCharacters: cfg.DetailsMaxCharacters,
		ReportWindow: cfg.ReportWindow, CommendWindow: cfg.CommendWindow}, time.Now)
}
