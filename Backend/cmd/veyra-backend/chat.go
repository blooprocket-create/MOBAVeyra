package main

import (
	"context"
	"errors"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/chat"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/config"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/party"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/selection"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/settings"
)

// allChatSetting is the account setting that says whether a player takes part
// in All Chat, and so in post-match chat (ADR-029 §4, ADR-046 §5). The backend
// stores only changed values, so an absent value is the default, On.
const (
	allChatSetting = "communication_all_chat"
	allChatOff     = "Off"
)

// chatTuning converts the validated configuration to the chat domain's
// tuning (ADR-046 §7).
func chatTuning(c config.Chat) chat.Tuning {
	return chat.Tuning{MaxCharacters: c.MaxCharacters, MaxPerWindow: c.MaxPerWindow, Window: c.Window, HistoryMessages: c.HistoryMessages,
		PageSize: c.PageSize, Retention: c.Retention, PostMatchWindow: c.PostMatchWindow}
}

// chatParties answers an account's party and when it joined.
type chatParties struct{ parties *party.Service }

func (c chatParties) PartyOf(ctx context.Context, accountID string) (chat.Membership, bool, error) {
	p, err := c.parties.Get(ctx, accountID)
	if errors.Is(err, party.ErrNotInParty) {
		return chat.Membership{}, false, nil
	}
	if err != nil {
		return chat.Membership{}, false, err
	}
	for _, m := range p.Members {
		if m.AccountID == accountID {
			return chat.Membership{PartyID: p.ID, JoinedAt: m.JoinedAt}, true, nil
		}
	}
	return chat.Membership{}, false, nil
}

// chatSelects answers an account's side in its active champion select.
type chatSelects struct{ selects *selection.Service }

func (c chatSelects) TeamOf(ctx context.Context, accountID string) (chat.SelectTeam, bool, error) {
	s, ok, err := c.selects.Current(ctx, accountID)
	if err != nil || !ok {
		return chat.SelectTeam{}, false, err
	}
	for _, seat := range s.Seats {
		if seat.AccountID == accountID {
			return chat.SelectTeam{SelectID: s.ID, Side: string(seat.Side)}, true, nil
		}
	}
	return chat.SelectTeam{}, false, nil
}

// chatMatches answers the matches an account played and its live one.
type chatMatches struct{ matches *match.Service }

func (c chatMatches) Played(ctx context.Context, accountID, matchID string) (chat.PlayedMatch, error) {
	m, _, err := c.matches.ForParticipant(ctx, accountID, matchID)
	if errors.Is(err, match.ErrMatchNotFound) {
		return chat.PlayedMatch{}, chat.ErrNotParticipant
	}
	if err != nil {
		return chat.PlayedMatch{}, err
	}
	out := chat.PlayedMatch{}
	// Only a match with a result has a results screen; a failed one has none.
	if m.State == match.Ended {
		out.EndedAt = m.EndedAt
	}
	for _, p := range m.Participants {
		out.Participants = append(out.Participants, p.AccountID)
	}
	return out, nil
}

func (c chatMatches) Live(ctx context.Context, accountID string) (string, bool, error) {
	m, ok, err := c.matches.Current(ctx, accountID)
	return m.MatchID, ok, err
}

// chatPreferences reads the All Chat preference from the settings store.
type chatPreferences struct{ settings *settings.Service }

func (c chatPreferences) AllChatOn(ctx context.Context, accountID string) (bool, error) {
	doc, err := c.settings.Get(ctx, accountID)
	if err != nil {
		return false, err
	}
	return doc.Values[allChatSetting] != allChatOff, nil
}

// chatNames adapts display names to chat.Names.
type chatNames struct {
	names match.AccountsFunc
}

func (c chatNames) DisplayNames(ctx context.Context, ids []string) (map[string]string, error) {
	return c.names(ctx, ids)
}

// newChatService builds the chat domain over the other domains (ADR-046 §1).
func newChatService(store chat.Store, cfg config.Chat, parties *party.Service, friends chat.Friends, selects *selection.Service,
	matches *match.Service, prefs *settings.Service, names match.AccountsFunc) *chat.Service {
	return chat.NewService(store, chat.Domains{
		Parties:     chatParties{parties: parties},
		Friends:     friends,
		Selects:     chatSelects{selects: selects},
		Matches:     chatMatches{matches: matches},
		Preferences: chatPreferences{settings: prefs},
		Names:       chatNames{names: names},
	}, chatTuning(cfg), time.Now)
}
