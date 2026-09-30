package main

import (
	"testing"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/config"
	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/lobby"
)

// A backend with custom lobbies switched off serves no lobby routes, so no lobby
// may keep its members busy for parties and queues.
func TestBusyCheckerIgnoresLobbiesWhileTheyAreSwitchedOff(t *testing.T) {
	lobbies := &lobby.Service{}
	var cfg config.Config

	cfg.CustomLobby.Enabled = false
	if busy := busyChecker(cfg, nil, nil, lobbies); busy.lobbies != nil {
		t.Fatal("lobbies switched off still keep their members busy")
	}
	cfg.CustomLobby.Enabled = true
	if busy := busyChecker(cfg, nil, nil, lobbies); busy.lobbies != lobbies {
		t.Fatal("lobbies switched on must keep their members busy")
	}
}
