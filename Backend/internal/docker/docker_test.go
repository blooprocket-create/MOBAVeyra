package docker

import (
	"context"
	"encoding/json"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// fakeEngine is a minimal Docker Engine API: enough of create, attach,
// start, inspect, stop and delete to check what the allocator sends.
type fakeEngine struct {
	mu         sync.Mutex
	calls      []string
	createBody map[string]any
	stdin      string
	stdinDone  chan struct{}
	containers map[string]string // name -> "created", "running" or "exited"
	exitCode   int
}

func newFakeEngine() *fakeEngine {
	return &fakeEngine{containers: map[string]string{}, stdinDone: make(chan struct{})}
}

func (f *fakeEngine) record(call string) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.calls = append(f.calls, call)
}

func (f *fakeEngine) handler(t *testing.T) http.Handler {
	mux := http.NewServeMux()
	mux.HandleFunc("POST /v1.44/containers/create", func(w http.ResponseWriter, r *http.Request) {
		f.record("create")
		var body map[string]any
		if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
			t.Errorf("create body: %v", err)
		}
		f.mu.Lock()
		f.createBody = body
		f.containers[r.URL.Query().Get("name")] = "created"
		f.mu.Unlock()
		w.WriteHeader(http.StatusCreated)
		_, _ = w.Write([]byte(`{"Id":"abc"}`))
	})
	mux.HandleFunc("POST /v1.44/containers/{name}/attach", func(w http.ResponseWriter, r *http.Request) {
		f.record("attach")
		if r.Header.Get("Upgrade") != "tcp" || r.URL.Query().Get("stdin") != "1" {
			t.Errorf("attach must upgrade and ask for stdin: %v %v", r.Header, r.URL.Query())
		}
		conn, buf, err := w.(http.Hijacker).Hijack()
		if err != nil {
			t.Errorf("hijack: %v", err)
			return
		}
		_, _ = buf.WriteString("HTTP/1.1 101 UPGRADED\r\nContent-Type: application/vnd.docker.raw-stream\r\nConnection: Upgrade\r\nUpgrade: tcp\r\n\r\n")
		_ = buf.Flush()
		go func() {
			defer conn.Close()
			data, _ := io.ReadAll(buf)
			f.mu.Lock()
			f.stdin = string(data)
			f.mu.Unlock()
			close(f.stdinDone)
		}()
	})
	mux.HandleFunc("POST /v1.44/containers/{name}/start", func(w http.ResponseWriter, r *http.Request) {
		f.record("start")
		f.mu.Lock()
		f.containers[r.PathValue("name")] = "running"
		f.mu.Unlock()
		w.WriteHeader(http.StatusNoContent)
	})
	mux.HandleFunc("GET /v1.44/containers/{name}/json", func(w http.ResponseWriter, r *http.Request) {
		f.mu.Lock()
		state, ok := f.containers[r.PathValue("name")]
		code := f.exitCode
		f.mu.Unlock()
		if !ok {
			w.WriteHeader(http.StatusNotFound)
			_, _ = w.Write([]byte(`{"message":"No such container"}`))
			return
		}
		_ = json.NewEncoder(w).Encode(map[string]any{"State": map[string]any{"Status": state, "Running": state == "running", "ExitCode": code}})
	})
	mux.HandleFunc("POST /v1.44/containers/{name}/stop", func(w http.ResponseWriter, r *http.Request) {
		f.record("stop t=" + r.URL.Query().Get("t"))
		f.mu.Lock()
		defer f.mu.Unlock()
		if _, ok := f.containers[r.PathValue("name")]; !ok {
			w.WriteHeader(http.StatusNotFound)
			return
		}
		f.containers[r.PathValue("name")] = "exited"
		w.WriteHeader(http.StatusNoContent)
	})
	mux.HandleFunc("DELETE /v1.44/containers/{name}", func(w http.ResponseWriter, r *http.Request) {
		f.record("delete force=" + r.URL.Query().Get("force"))
		f.mu.Lock()
		defer f.mu.Unlock()
		if _, ok := f.containers[r.PathValue("name")]; !ok {
			w.WriteHeader(http.StatusNotFound)
			return
		}
		delete(f.containers, r.PathValue("name"))
		w.WriteHeader(http.StatusNoContent)
	})
	return mux
}

// serveOnSocket serves the fake engine on a Unix socket, as Docker does.
func serveOnSocket(t *testing.T, f *fakeEngine) string {
	t.Helper()
	socket := filepath.Join(t.TempDir(), "docker.sock")
	listener, err := net.Listen("unix", socket)
	if err != nil {
		t.Fatalf("listen: %v", err)
	}
	srv := httptest.NewUnstartedServer(f.handler(t))
	srv.Listener = listener
	srv.Start()
	t.Cleanup(srv.Close)
	return "unix://" + socket
}

func testConfig(endpoint string) Config {
	return Config{
		Endpoint:       endpoint,
		APIVersion:     "1.44",
		RequestTimeout: 5 * time.Second,
		Image:          "veyra-match-server:local",
		Network:        "veyra_default",
		NamePrefix:     "veyra-match-",
		ContainerPort:  7777,
		HostIP:         "127.0.0.1",
		ServerArgs:     []string{"/Game/Map", "-port=7777"},
		StopTimeout:    10 * time.Second,
	}
}

const testAssignment = `{"schemaVersion":1,"matchId":"m-1","serverCredential":"vms_secret"}` + "\n"

func TestStartHandsTheAssignmentOnlyToStandardInput(t *testing.T) {
	engine := newFakeEngine()
	a, err := New(testConfig(serveOnSocket(t, engine)))
	if err != nil {
		t.Fatalf("New: %v", err)
	}
	if err := a.Start(context.Background(), match.ServerSpec{MatchID: "m-1", HostPort: 7780, Assignment: []byte(testAssignment)}); err != nil {
		t.Fatalf("Start: %v", err)
	}
	select {
	case <-engine.stdinDone:
	case <-time.After(5 * time.Second):
		t.Fatal("standard input was never closed")
	}

	engine.mu.Lock()
	defer engine.mu.Unlock()
	if engine.stdin != testAssignment {
		t.Fatalf("stdin got %q", engine.stdin)
	}
	if strings.Join(engine.calls[:3], ",") != "create,attach,start" {
		t.Fatalf("calls %v: must attach before starting", engine.calls)
	}
	raw, _ := json.Marshal(engine.createBody)
	if strings.Contains(string(raw), "vms_") || strings.Contains(string(raw), "schemaVersion") {
		t.Fatalf("the create request leaked the assignment: %s", raw)
	}
	if _, ok := engine.createBody["Env"]; ok {
		t.Fatal("the container must get no environment")
	}
	cmd := engine.createBody["Cmd"].([]any)
	if len(cmd) != 3 || cmd[2] != AssignmentSwitch {
		t.Fatalf("Cmd %v must end with %s", cmd, AssignmentSwitch)
	}
	if engine.createBody["OpenStdin"] != true || engine.createBody["StdinOnce"] != true {
		t.Fatal("standard input must be open once")
	}
	host := engine.createBody["HostConfig"].(map[string]any)
	binding := host["PortBindings"].(map[string]any)["7777/udp"].([]any)[0].(map[string]any)
	if binding["HostIp"] != "127.0.0.1" || binding["HostPort"] != "7780" || host["NetworkMode"] != "veyra_default" {
		t.Fatalf("wrong host config: %v", host)
	}
	if caps := host["CapDrop"].([]any); len(caps) != 1 || caps[0] != "ALL" {
		t.Fatalf("capabilities must be dropped: %v", caps)
	}
	labels := engine.createBody["Labels"].(map[string]any)
	if labels[labelMatchID] != "m-1" || labels[labelManaged] != "true" {
		t.Fatalf("labels %v", labels)
	}
}

func TestStatusAndRemove(t *testing.T) {
	engine := newFakeEngine()
	a, _ := New(testConfig(serveOnSocket(t, engine)))
	ctx := context.Background()

	if s, err := a.Status(ctx, "m-1"); err != nil || !s.Missing {
		t.Fatalf("no container: %+v %v", s, err)
	}
	if err := a.Remove(ctx, "m-1"); err != nil {
		t.Fatalf("removing a missing container must succeed: %v", err)
	}

	if err := a.Start(ctx, match.ServerSpec{MatchID: "m-1", HostPort: 7780, Assignment: []byte(testAssignment)}); err != nil {
		t.Fatalf("Start: %v", err)
	}
	<-engine.stdinDone
	if s, _ := a.Status(ctx, "m-1"); !s.Running {
		t.Fatalf("want running, got %+v", s)
	}
	engine.mu.Lock()
	engine.containers["veyra-match-m-1"] = "exited"
	engine.exitCode = 3
	engine.mu.Unlock()
	if s, _ := a.Status(ctx, "m-1"); !s.Exited || s.ExitCode != 3 {
		t.Fatalf("want exited 3, got %+v", s)
	}
	if err := a.Remove(ctx, "m-1"); err != nil {
		t.Fatalf("Remove: %v", err)
	}
	engine.mu.Lock()
	defer engine.mu.Unlock()
	if _, ok := engine.containers["veyra-match-m-1"]; ok {
		t.Fatal("the container was not deleted")
	}
	last := engine.calls[len(engine.calls)-2:]
	if last[0] != "stop t=10" || last[1] != "delete force=1" {
		t.Fatalf("calls %v: want a graceful stop, then a forced delete", engine.calls)
	}
}

func TestEngineErrorsAreReported(t *testing.T) {
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
		w.WriteHeader(http.StatusConflict)
		_, _ = w.Write([]byte(`{"message":"container name already in use"}`))
	}))
	t.Cleanup(srv.Close)
	a, _ := New(testConfig("tcp://" + srv.Listener.Addr().String()))
	err := a.Start(context.Background(), match.ServerSpec{MatchID: "m-1", HostPort: 7780, Assignment: []byte(testAssignment)})
	if err == nil || !strings.Contains(err.Error(), "already in use") {
		t.Fatalf("want the engine's message, got %v", err)
	}
}

func TestNewRejectsOtherSchemes(t *testing.T) {
	if _, err := New(testConfig("http://localhost:2375")); err == nil {
		t.Fatal("an http endpoint must be refused")
	}
}
