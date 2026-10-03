package main

import (
	"context"
	"errors"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/presence"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
)

// presenceActivity says what accounts are busy with, for friends' statuses
// and the offline sweep (ADR-061 §2): a live match, Reconnect-only included;
// a champion select; or a party in matchmaking.
type presenceActivity struct {
	matches *match.Service
	selects *selection.Service
	parties *party.Service
}

func (a presenceActivity) Activity(ctx context.Context, accountIDs []string) (map[string]presence.Status, error) {
	parties, err := a.parties.PartiesOf(ctx, accountIDs)
	if err != nil {
		return nil, err
	}
	out := map[string]presence.Status{}
	for _, id := range accountIDs {
		if _, inMatch, err := a.matches.Current(ctx, id); err != nil {
			return nil, err
		} else if inMatch {
			out[id] = presence.InMatch
			continue
		}
		if _, selecting, err := a.selects.Current(ctx, id); err != nil {
			return nil, err
		} else if selecting {
			out[id] = presence.InSelect
			continue
		}
		if p, ok := parties[id]; ok && (p.Status == party.Queued || p.Status == party.Found) {
			out[id] = presence.InQueue
		}
	}
	return out, nil
}

// presenceParties is the party service as presence uses it: who shares the
// viewer's party, and the offline sweep's members and removals.
type presenceParties struct{ *party.Service }

func (p presenceParties) PartyMembers(ctx context.Context, accountID string) ([]string, error) {
	pt, err := p.Get(ctx, accountID)
	if errors.Is(err, party.ErrNotInParty) {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	return pt.MemberIDs(), nil
}
