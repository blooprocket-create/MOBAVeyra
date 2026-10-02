package chat

import (
	"strings"
	"testing"
	"time"
)

func TestCleaningTurnsControlCharactersIntoSpacesAndTrims(t *testing.T) {
	for in, want := range map[string]string{
		"  hello  ":        "hello",
		"line\nbreak\ttab": "line break tab",
		"\x00\x07":         "",
		"bad \xff utf-8":   "bad � utf-8",
		"Vanguard été":     "Vanguard été",
	} {
		if got := Clean(in); got != want {
			t.Errorf("Clean(%q) = %q, want %q", in, got, want)
		}
	}
}

func TestATextMustBeNonEmptyAndShortEnough(t *testing.T) {
	tuning := Tuning{MaxCharacters: 5}
	if err := CheckText(tuning, ""); err != ErrEmptyMessage {
		t.Errorf("empty: %v", err)
	}
	if err := CheckText(tuning, "fünf!"); err != nil {
		t.Errorf("five characters, six bytes: %v", err)
	}
	if err := CheckText(tuning, strings.Repeat("x", 6)); err != ErrMessageTooLong {
		t.Errorf("six: %v", err)
	}
}

func TestADirectConversationHasOneKeyForBothAccounts(t *testing.T) {
	if DirectKey("acc-b", "acc-a") != DirectKey("acc-a", "acc-b") {
		t.Fatal("the key depends on who sends")
	}
}

func TestClientIDsHaveAShape(t *testing.T) {
	for id, want := range map[string]bool{"0123abcd": true, "a1b2c3d4-e5f6": true, "short": false, "has space x": false, strings.Repeat("a", 65): false} {
		if ValidClientID(id) != want {
			t.Errorf("ValidClientID(%q) != %v", id, want)
		}
	}
}

func TestAPostMatchChatClosesAfterItsWindow(t *testing.T) {
	tuning := Tuning{PostMatchWindow: 10 * time.Minute}
	ended := time.Date(2026, 10, 2, 12, 0, 0, 0, time.UTC)
	if PostMatchOpen(tuning, time.Time{}, ended) {
		t.Error("a match not yet ended has no post-match chat")
	}
	if !PostMatchOpen(tuning, ended, ended.Add(9*time.Minute)) {
		t.Error("closed within the window")
	}
	if PostMatchOpen(tuning, ended, ended.Add(10*time.Minute)) {
		t.Error("open at the window's end")
	}
}
