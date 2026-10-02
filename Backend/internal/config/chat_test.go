package config

import (
	"strings"
	"testing"
	"time"
)

func TestParseChat(t *testing.T) {
	c, err := Parse([]byte(validJSON))
	if err != nil {
		t.Fatalf("Parse: %v", err)
	}
	want := Chat{MaxCharacters: 250, MaxPerWindow: 5, Window: 5 * time.Second, HistoryMessages: 100, PageSize: 200,
		Retention: 168 * time.Hour, PostMatchWindow: 10 * time.Minute}
	if c.Chat != want {
		t.Fatalf("chat: %+v", c.Chat)
	}
}

func TestChatRejectsBadTuning(t *testing.T) {
	cases := []struct {
		name, old, new, want string
	}{
		{"no characters", `"maxCharacters": 250`, `"maxCharacters": 0`, "chat.maxCharacters must be at least 1"},
		{"a missing limit", `"maxPerWindow": 5, `, ``, "chat.maxPerWindow is required"},
		{"an empty window", `"window": "5s"`, `"window": "0s"`, "chat.window must be positive"},
		{"a window longer than retention", `"retention": "168h"`, `"retention": "1s"`, "chat.window must not exceed chat.retention"},
		{"no post-match time", `"postMatchWindow": "10m"`, `"postMatchWindow": "-1m"`, "chat.postMatchWindow must be positive"},
		{"an unknown field", `"pageSize": 200`, `"pageSize": 200, "loudness": 11`, "unknown field"},
	}
	for _, c := range cases {
		raw := strings.Replace(validJSON, c.old, c.new, 1)
		if raw == validJSON {
			t.Fatalf("%s: the fixture has no %q", c.name, c.old)
		}
		if _, err := Parse([]byte(raw)); err == nil || !strings.Contains(err.Error(), c.want) {
			t.Errorf("%s: %v", c.name, err)
		}
	}
}
