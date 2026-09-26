package docker

import (
	"bytes"
	"context"
	"encoding/binary"
	"io"
	"net/http"
	"os"
	"strings"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// Environment for the real-engine test. It needs a Docker endpoint and an
// image whose entrypoint runs the command it is given and has sh and cat
// (postgres:16, which the local stack already uses, qualifies).
const (
	testEngineEndpointEnv = "VEYRA_TEST_DOCKER_ENDPOINT"
	testEngineImageEnv    = "VEYRA_TEST_DOCKER_IMAGE"
)

// TestStartAgainstDockerEngine checks against a real Docker Engine that the
// assignment reaches the container's standard input and nothing else: the
// container runs `sh -c cat`, which echoes standard input to its log.
func TestStartAgainstDockerEngine(t *testing.T) {
	endpoint, image := os.Getenv(testEngineEndpointEnv), os.Getenv(testEngineImageEnv)
	if endpoint == "" || image == "" {
		t.Skipf("%s and %s not set; skipping the real Docker Engine test", testEngineEndpointEnv, testEngineImageEnv)
	}
	cfg := testConfig(endpoint)
	cfg.Image = image
	cfg.Network = "bridge"
	cfg.NamePrefix = "veyra-engine-test-"
	cfg.ServerArgs = []string{"sh", "-c", "cat"}
	cfg.StopTimeout = time.Second
	a, err := New(cfg)
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	ctx := context.Background()
	const matchID = "engine-test"
	_ = a.Remove(ctx, matchID)
	t.Cleanup(func() { _ = a.Remove(context.Background(), matchID) })

	assignment := `{"schemaVersion":1,"matchId":"engine-test","serverCredential":"vms_engine_test"}` + "\n"
	if err := a.Start(ctx, match.ServerSpec{MatchID: matchID, HostPort: 7789, Assignment: []byte(assignment)}); err != nil {
		t.Fatalf("Start: %v", err)
	}

	// cat exits once standard input closes, so the container stops.
	deadline := time.Now().Add(30 * time.Second)
	for {
		s, err := a.Status(ctx, matchID)
		if err != nil {
			t.Fatalf("Status: %v", err)
		}
		if s.Exited {
			if s.ExitCode != 0 {
				t.Fatalf("cat exited with %d", s.ExitCode)
			}
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("the container never saw the end of its standard input")
		}
		time.Sleep(200 * time.Millisecond)
	}

	logs := containerStdout(t, a, cfg.NamePrefix+matchID)
	if logs != assignment {
		t.Fatalf("the container read %q from standard input, want %q", logs, assignment)
	}

	// Nothing secret in what Docker records about the container.
	req, _ := http.NewRequestWithContext(ctx, http.MethodGet, a.base+"/containers/"+cfg.NamePrefix+matchID+"/json", nil)
	resp, err := a.client.Do(req)
	if err != nil {
		t.Fatalf("inspect: %v", err)
	}
	defer resp.Body.Close()
	inspect, _ := io.ReadAll(resp.Body)
	if bytes.Contains(inspect, []byte("vms_engine_test")) {
		t.Fatal("docker inspect shows the credential")
	}
}

// containerStdout reads a stopped container's standard output, removing
// the Engine's stream framing (an 8-byte header before each chunk).
func containerStdout(t *testing.T, a *Allocator, name string) string {
	t.Helper()
	resp, err := a.client.Get(a.base + "/containers/" + name + "/logs?stdout=1&stderr=0")
	if err != nil {
		t.Fatalf("logs: %v", err)
	}
	defer resp.Body.Close()
	var out strings.Builder
	header := make([]byte, 8)
	for {
		if _, err := io.ReadFull(resp.Body, header); err != nil {
			break
		}
		size := binary.BigEndian.Uint32(header[4:])
		chunk := make([]byte, size)
		if _, err := io.ReadFull(resp.Body, chunk); err != nil {
			t.Fatalf("logs frame: %v", err)
		}
		out.Write(chunk)
	}
	return out.String()
}
