package config

import "time"

// Chat configures Party Chat, friend messages, champion-select chat and
// post-match chat (ADR-046 §7). Every value is Provisional tuning.
type Chat struct {
	// MaxCharacters is the longest a cleaned message may be.
	MaxCharacters int
	// MaxPerWindow messages a sender may send in any Window.
	MaxPerWindow int
	Window       time.Duration
	// HistoryMessages is how many of an account's newest messages a poll
	// without a cursor returns.
	HistoryMessages int
	// PageSize is the most messages one poll with a cursor returns.
	PageSize int
	// Retention is how long a message is served and kept.
	Retention time.Duration
	// PostMatchWindow is how long after a match ends its post-match chat
	// stays open.
	PostMatchWindow time.Duration
	// PruneInterval is how often expired messages are removed, whether or not
	// anyone sends.
	PruneInterval time.Duration
}

type fileChat struct {
	MaxCharacters   *int      `json:"maxCharacters"`
	MaxPerWindow    *int      `json:"maxPerWindow"`
	Window          *Duration `json:"window"`
	HistoryMessages *int      `json:"historyMessages"`
	PageSize        *int      `json:"pageSize"`
	Retention       *Duration `json:"retention"`
	PostMatchWindow *Duration `json:"postMatchWindow"`
	PruneInterval   *Duration `json:"pruneInterval"`
}

// parseChat validates the chat section, reporting through missing, problem
// and positive.
func parseChat(f *fileChat, missing, problem func(string), positive func(string, *Duration) time.Duration) Chat {
	var c Chat
	if f == nil {
		missing("chat")
		return c
	}
	atLeastOne := func(field string, v *int) int {
		switch {
		case v == nil:
			missing(field)
		case *v < 1:
			problem(field + " must be at least 1")
		default:
			return *v
		}
		return 0
	}
	c.MaxCharacters = atLeastOne("chat.maxCharacters", f.MaxCharacters)
	c.MaxPerWindow = atLeastOne("chat.maxPerWindow", f.MaxPerWindow)
	c.Window = positive("chat.window", f.Window)
	c.HistoryMessages = atLeastOne("chat.historyMessages", f.HistoryMessages)
	c.PageSize = atLeastOne("chat.pageSize", f.PageSize)
	c.Retention = positive("chat.retention", f.Retention)
	c.PostMatchWindow = positive("chat.postMatchWindow", f.PostMatchWindow)
	c.PruneInterval = positive("chat.pruneInterval", f.PruneInterval)
	if c.Retention > 0 && c.Window > c.Retention {
		problem("chat.window must not exceed chat.retention")
	}
	return c
}
