// Command veyra-devlaunch stands in for the launcher during development.
//
// It logs in to the local backend as a seeded dev account, requests a launch
// code, and starts the given program with the code written to its standard
// input: the same private channel the real launcher uses (ADR-005). The code
// never appears on a command line.
//
//	veyra-devlaunch -backend http://localhost:8080 -account DevOne -build dev -- path\to\Veyra.exe [args]
//
// With -check instead of a program, it redeems the code itself and prints the
// account, to verify the whole handoff without the game.
package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
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
	check := flag.Bool("check", false, "redeem the code here and print the account instead of launching a program")
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
	var code tokenResponse
	if err := post(client, base+"/v1/launch-codes", login.Token, map[string]string{"buildVersion": *build}, &code); err != nil {
		return fmt.Errorf("launch code: %w", err)
	}

	if *check {
		var game tokenResponse
		if err := post(client, base+"/v1/game-sessions", "", map[string]string{"launchCode": code.Token, "buildVersion": *build}, &game); err != nil {
			return fmt.Errorf("redeem: %w", err)
		}
		fmt.Printf("handoff OK: game session issued for %s (%s)\n", game.Account.DisplayName, game.Account.ID)
		return nil
	}

	cmd := exec.Command(flag.Arg(0), flag.Args()[1:]...)
	cmd.Stdin = strings.NewReader(code.Token + "\n")
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return cmd.Run()
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
