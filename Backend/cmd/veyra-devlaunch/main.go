// Command veyra-devlaunch stands in for the launcher during development.
//
// It logs in to the local backend as a seeded dev account and starts the
// given program with pipes for its standard input and output. When the game
// says it is ready for its launch code, it requests one and writes it to the
// game's standard input: the same private channel and launch handshake the
// real launcher uses (ADR-005 L3, ADR-010 §5). The code never appears on a
// command line.
//
//	veyra-devlaunch -backend http://localhost:8080 -account DevOne -build 0.1.0 -- path\to\VeyraClient.exe -VeyraLaunchCode=stdin
//
// It keeps copying the game's other output and waits for the game to exit,
// or with -detach it returns as soon as the game has signed in, as the
// launcher does. With -check instead of a program, it redeems a code itself
// and prints the account, to verify the handoff without the game.
package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"strings"
	"time"
)

type tokenResponse struct {
	Token   string `json:"token"`
	Account *struct {
		ID          string `json:"id"`
		DisplayName string `json:"displayName"`
	} `json:"account"`
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "veyra-devlaunch:", err)
		os.Exit(1)
	}
}

func run() error {
	backend := flag.String("backend", "", "backend base URL, e.g. http://localhost:8080 (required)")
	account := flag.String("account", "", "seeded dev account name (required)")
	build := flag.String("build", "", "client build version the code is bound to (required)")
	timeout := flag.Duration("timeout", 10*time.Second, "HTTP request timeout")
	check := flag.Bool("check", false, "redeem a code here and print the account instead of launching a program")
	detach := flag.Bool("detach", false, "return once the game has signed in instead of waiting for it to exit")
	flag.Parse()

	if *backend == "" || *account == "" || *build == "" {
		return errors.New("-backend, -account and -build are required")
	}
	if !*check && flag.NArg() == 0 {
		return errors.New("give a program to launch after --, or use -check")
	}

	client := &http.Client{Timeout: *timeout}
	base := strings.TrimRight(*backend, "/")

	var login tokenResponse
	if err := post(client, base+"/v1/dev/login", "", map[string]string{"accountName": *account}, &login); err != nil {
		return fmt.Errorf("dev login: %w", err)
	}
	issue := func() (string, error) {
		var code tokenResponse
		err := post(client, base+"/v1/launch-codes", login.Token, map[string]string{"buildVersion": *build}, &code)
		return code.Token, err
	}

	if *check {
		code, err := issue()
		if err != nil {
			return fmt.Errorf("launch code: %w", err)
		}
		var game tokenResponse
		if err := post(client, base+"/v1/game-sessions", "", map[string]string{"launchCode": code, "buildVersion": *build}, &game); err != nil {
			return fmt.Errorf("redeem: %w", err)
		}
		fmt.Printf("handoff OK: game session issued for %s (%s)\n", game.Account.DisplayName, game.Account.ID)
		return nil
	}

	cmd := exec.Command(flag.Arg(0), flag.Args()[1:]...)
	cmd.Stderr = os.Stderr
	gameInput, err := cmd.StdinPipe()
	if err != nil {
		return err
	}
	gameOutput, err := cmd.StdoutPipe()
	if err != nil {
		return err
	}
	if err := cmd.Start(); err != nil {
		return err
	}
	output := bufio.NewReader(gameOutput)
	result, failure, err := handshake(output, gameInput, os.Stdout, issue)
	switch {
	case err != nil:
		_ = cmd.Wait()
		return err
	case result == outcomeFailed:
		_ = cmd.Wait()
		return fmt.Errorf("the game could not sign in: %s", failure)
	}
	fmt.Fprintln(os.Stderr, "veyra-devlaunch: the game signed in")
	if *detach {
		// The game keeps running; its output pipe closes with this process.
		return nil
	}
	_, _ = io.Copy(os.Stdout, output)
	return cmd.Wait()
}

func post(client *http.Client, url, bearer string, body, out any) error {
	b, err := json.Marshal(body)
	if err != nil {
		return err
	}
	req, err := http.NewRequest(http.MethodPost, url, bytes.NewReader(b))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "application/json")
	if bearer != "" {
		req.Header.Set("Authorization", "Bearer "+bearer)
	}
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		var e struct {
			Error string `json:"error"`
		}
		_ = json.NewDecoder(resp.Body).Decode(&e)
		return fmt.Errorf("%s: %s", resp.Status, e.Error)
	}
	return json.NewDecoder(resp.Body).Decode(out)
}
