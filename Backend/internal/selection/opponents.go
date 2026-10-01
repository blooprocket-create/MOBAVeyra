package selection

import (
	"errors"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// ErrNoOpponents: a co-op select needs its enemy AI team, and there are too
// few Vanguards to draw it from.
var ErrNoOpponents = errors.New("selection: too few Vanguards for the enemy AI team")

// DrawOpponents draws a co-op match's enemy AI team (Modes & Access Bible §4;
// ADR-038 §3): count distinct Vanguards from pool, each a bot of difficulty on
// side, in pool order shuffled by shuffle (rand.Shuffle in production, a fixed
// order in tests).
func DrawOpponents(pool []string, count int, side match.Side, difficulty match.BotDifficulty, shuffle func(n int, swap func(i, j int))) ([]match.Bot, error) {
	distinct := []string{}
	seen := map[string]bool{}
	for _, id := range pool {
		if !seen[id] {
			seen[id] = true
			distinct = append(distinct, id)
		}
	}
	if count < 1 || len(distinct) < count || !difficulty.Valid() || (side != match.SideA && side != match.SideB) {
		return nil, ErrNoOpponents
	}
	shuffle(len(distinct), func(i, j int) { distinct[i], distinct[j] = distinct[j], distinct[i] })
	bots := make([]match.Bot, count)
	for i := range bots {
		bots[i] = match.Bot{Side: side, VanguardID: distinct[i], Difficulty: difficulty}
	}
	return bots, nil
}
