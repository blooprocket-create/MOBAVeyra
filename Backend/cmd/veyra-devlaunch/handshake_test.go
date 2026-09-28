package main

import (
	"bufio"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"testing"
)

// fakeInput records what the launcher writes to the game.
type fakeInput struct {
	strings.Builder
	closed bool
}

func (f *fakeInput) Close() error { f.closed = true; return nil }

func runHandshake(t *testing.T, gameOutput string, issue func() (string, error)) (outcome, string, error, *fakeInput, string) {
	t.Helper()
	in := &fakeInput{}
	var passthrough strings.Builder
	result, failure, err := handshake(bufio.NewReader(strings.NewReader(gameOutput)), in, &passthrough, issue)
	return result, failure, err, in, passthrough.String()
}

func TestTheCodeIsIssuedOnlyWhenTheGameAsks(t *testing.T) {
	issued := 0
	issue := func() (string, error) { issued++; return "vlc_code", nil }
	result, _, err, in, passed := runHandshake(t, "engine starting\n"+awaitingLaunchCode+"\r\nmore output\n"+signedIn+"\nafter\n", issue)
	if err != nil || result != outcomeSignedIn {
		t.Fatalf("outcome %v %v", result, err)
	}
	if issued != 1 || in.String() != "vlc_code\n" || !in.closed {
		t.Fatalf("issued %d, wrote %q, closed %v", issued, in.String(), in.closed)
	}
	if passed != "engine starting\nmore output\n" {
		t.Fatalf("passed through %q: the markers are not copied, nor anything after sign-in", passed)
	}
}

func TestAFailedSignInReportsItsCode(t *testing.T) {
	result, failure, err, _, _ := runHandshake(t, awaitingLaunchCode+"\n"+failedPrefix+" sign_in_refused\n", func() (string, error) { return "vlc_code", nil })
	if err != nil || result != outcomeFailed || failure != "sign_in_refused" {
		t.Fatalf("outcome %v %q %v", result, failure, err)
	}
}

func TestAGameThatStopsEarlyIsAnError(t *testing.T) {
	issued := false
	_, _, err, in, _ := runHandshake(t, "crash\n", func() (string, error) { issued = true; return "vlc_code", nil })
	if !errors.Is(err, errNoHandshake) || issued || !in.closed {
		t.Fatalf("err %v, issued %v, closed %v: no code is issued to a game that never asks", err, issued, in.closed)
	}
}

func TestAnIssueFailureClosesTheGamesInput(t *testing.T) {
	_, _, err, in, _ := runHandshake(t, awaitingLaunchCode+"\n", func() (string, error) { return "", errors.New("backend down") })
	if err == nil || !in.closed || in.String() != "" {
		t.Fatalf("err %v, wrote %q, closed %v", err, in.String(), in.closed)
	}
}

// The lines are the game's contract (ADR-010 §5).
func TestTheLinesAreTheContracts(t *testing.T) {
	raw, err := os.ReadFile(filepath.Join("..", "..", "..", "Game", "Source", "VeyraServices", "Contracts", "LaunchHandshake.json"))
	if err != nil {
		t.Fatalf("read the contract: %v", err)
	}
	var contract struct {
		AwaitingLaunchCode string   `json:"awaitingLaunchCode"`
		SignedIn           string   `json:"signedIn"`
		FailedPrefix       string   `json:"failedPrefix"`
		FailureCodes       []string `json:"failureCodes"`
	}
	if err := json.Unmarshal(raw, &contract); err != nil {
		t.Fatalf("parse the contract: %v", err)
	}
	if contract.AwaitingLaunchCode != awaitingLaunchCode || contract.SignedIn != signedIn || contract.FailedPrefix != failedPrefix {
		t.Fatalf("the contract says %+v", contract)
	}
	if !slices.Contains(contract.FailureCodes, "sign_in_refused") {
		t.Fatalf("failure codes: %v", contract.FailureCodes)
	}
}
