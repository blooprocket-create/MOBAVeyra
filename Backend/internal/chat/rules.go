// Package chat owns every message outside a live match's Team and All Chat
// (ADR-046): Party Chat, friend direct messages, champion-select team chat
// and post-match chat. Each message belongs to one conversation, whose
// readers are checked when the message is sent and again each time it is
// delivered.
package chat

import (
	"errors"
	"regexp"
	"strings"
	"time"
	"unicode"
	"unicode/utf8"
)

// Errors describing rule violations. Callers map them to response codes.
var (
	ErrNotInParty      = errors.New("the account is in no party")
	ErrNotFriends      = errors.New("the accounts are not friends")
	ErrBlocked         = errors.New("one account blocks the other")
	ErrNoSelect        = errors.New("the account is in no champion select")
	ErrNotParticipant  = errors.New("the account did not play the match")
	ErrPostMatchClosed = errors.New("the match's post-match chat is closed to the account")
	ErrAllChatOff      = errors.New("the account has All Chat off")
	ErrEmptyMessage    = errors.New("the message is empty")
	ErrMessageTooLong  = errors.New("the message is too long")
	ErrRateLimited     = errors.New("the account is sending too fast")
	ErrInvalidMessage  = errors.New("invalid message")
	// ErrClientIDConflict is a resend whose client message ID the sender
	// already used for a message to another conversation.
	ErrClientIDConflict = errors.New("the client message ID was already used for another conversation")
	// ErrConversationChanged is a party or select send whose conversation is no longer the sender's: it left
	// that party or select before the message arrived.
	ErrConversationChanged = errors.New("the sender is no longer in that conversation")
	// ErrMessageNotFound is a store's answer for an unknown client message ID.
	ErrMessageNotFound = errors.New("message not found")
	// ErrNotJoined is a store's answer for an account with no post-match
	// participation record.
	ErrNotJoined = errors.New("no post-match participation")
)

// Kind is a conversation kind (ADR-046 §2).
type Kind string

const (
	// KindParty is a party's conversation, keyed by the party.
	KindParty Kind = "party"
	// KindDirect is two friends' conversation, keyed by both accounts.
	KindDirect Kind = "direct"
	// KindSelect is one side's conversation in a champion select, keyed by
	// the select and the side.
	KindSelect Kind = "select"
	// KindPostMatch is a match's cross-team conversation on the results
	// screen, keyed by the match.
	KindPostMatch Kind = "postmatch"
)

// Tuning is the chat configuration, validated by the config package
// (ADR-046 §7).
type Tuning struct {
	// MaxCharacters is the longest a cleaned message may be, in characters.
	MaxCharacters int
	// MaxPerWindow messages a sender may send in any Window.
	MaxPerWindow int
	Window       time.Duration
	// HistoryMessages is how many of its newest messages a poll without a
	// cursor returns.
	HistoryMessages int
	// PageSize is the most messages one poll with a cursor returns.
	PageSize int
	// Retention is how long a message is served and kept.
	Retention time.Duration
	// PostMatchWindow is how long after a match ends its post-match chat
	// stays open.
	PostMatchWindow time.Duration
}

// clientIDPattern is the shape of the IDs clients generate for messages.
var clientIDPattern = regexp.MustCompile(`^[A-Za-z0-9-]{8,64}$`)

// ValidClientID reports whether a client message ID has the expected shape.
func ValidClientID(id string) bool { return clientIDPattern.MatchString(id) }

// Clean returns a message's text as it is stored: invalid UTF-8 replaced,
// control characters turned into spaces and the ends trimmed (ADR-046 §3).
func Clean(text string) string {
	text = strings.ToValidUTF8(text, "�")
	text = strings.Map(func(r rune) rune {
		if unicode.IsControl(r) {
			return ' '
		}
		return r
	}, text)
	return strings.TrimSpace(text)
}

// CheckText returns why a cleaned text may not be sent, or nil.
func CheckText(t Tuning, cleaned string) error {
	if cleaned == "" {
		return ErrEmptyMessage
	}
	if utf8.RuneCountInString(cleaned) > t.MaxCharacters {
		return ErrMessageTooLong
	}
	return nil
}

// DirectKey is the key of two accounts' direct conversation, the same
// whichever of them sends.
func DirectKey(a, b string) string {
	if a > b {
		a, b = b, a
	}
	return a + "|" + b
}

// SelectKey is the key of one side's conversation in a champion select.
func SelectKey(selectID, side string) string { return selectID + "|" + side }

// PostMatchOpen reports whether a match that ended at endedAt still has an
// open post-match chat at now. A match not yet ended has none.
func PostMatchOpen(t Tuning, endedAt, now time.Time) bool {
	return !endedAt.IsZero() && now.Before(endedAt.Add(t.PostMatchWindow))
}
