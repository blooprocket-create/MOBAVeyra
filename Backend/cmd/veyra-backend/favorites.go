package main

import (
	"context"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
)

// favoriteActivity answers whether an account is in a champion select, where
// favorites only show, or a match (ADR-058 §5).
type favoriteActivity struct {
	matches *match.Service
	selects *selection.Service
}

func (f favoriteActivity) Playing(ctx context.Context, accountID string) (bool, error) {
	if _, inMatch, err := f.matches.Current(ctx, accountID); err != nil || inMatch {
		return inMatch, err
	}
	_, selecting, err := f.selects.Current(ctx, accountID)
	return selecting, err
}
