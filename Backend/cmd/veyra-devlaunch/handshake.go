package main

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"strings"
)

// The launch handshake's lines (ADR-010 §5), as
// Game/Source/VeyraServices/Contracts/LaunchHandshake.json fixes them; a test
// compares them with the file.
const (
	awaitingLaunchCode = "veyra-handoff/1 awaiting-launch-code"
	signedIn           = "veyra-handoff/1 signed-in"
	failedPrefix       = "veyra-handoff/1 failed"
)

// outcome is how a handshake ended.
type outcome int

const (
	// outcomeSignedIn: the game redeemed its code.
	outcomeSignedIn outcome = iota
	// outcomeFailed: the game reported why it could not sign in.
	outcomeFailed
)

// errNoHandshake means the game's output ended before the handshake did: it
// exited, or it is not a game that speaks the handshake.
var errNoHandshake = errors.New("the game stopped before signing in")

// handshake runs the launcher's side of the launch handshake. It reads the
// game's standard output line by line, copying every other line to
// passthrough; when the game asks, it issues a launch code, writes it to the
// game's standard input and closes that. It returns once the game has signed
// in or failed, with the failure code. The code is never copied anywhere
// else.
func handshake(gameOutput *bufio.Reader, gameInput io.WriteCloser, passthrough io.Writer, issue func() (string, error)) (outcome, string, error) {
	sent := false
	for {
		line, err := gameOutput.ReadString('\n')
		if line != "" {
			trimmed := strings.TrimRight(line, "\r\n")
			switch {
			case trimmed == awaitingLaunchCode && !sent:
				code, issueErr := issue()
				if issueErr != nil {
					_ = gameInput.Close()
					return outcomeFailed, "", fmt.Errorf("launch code: %w", issueErr)
				}
				_, writeErr := io.WriteString(gameInput, code+"\n")
				closeErr := gameInput.Close()
				if err := errors.Join(writeErr, closeErr); err != nil {
					return outcomeFailed, "", fmt.Errorf("give the game its launch code: %w", err)
				}
				sent = true
			case trimmed == signedIn:
				return outcomeSignedIn, "", nil
			case strings.HasPrefix(trimmed, failedPrefix+" "):
				return outcomeFailed, strings.TrimPrefix(trimmed, failedPrefix+" "), nil
			default:
				_, _ = io.WriteString(passthrough, line)
			}
		}
		if err != nil {
			if !sent {
				_ = gameInput.Close()
			}
			return outcomeFailed, "", errNoHandshake
		}
	}
}
