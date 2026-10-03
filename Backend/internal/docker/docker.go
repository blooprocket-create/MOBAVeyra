// Package docker is the local match-server allocator (ADR-007 §11). It talks
// to the Docker Engine API over the Docker socket (or TCP) with net/http only,
// and starts one container per match. A server's assignment, which holds its
// credential, is written to the container's standard input and nowhere else:
// not its environment, its command line or its filesystem.
//
// The Docker socket controls the Docker host, so this allocator is for a
// developer machine only. A hosted fleet uses its own allocator.
package docker

import (
	"bufio"
	"bytes"
	"context"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"net"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"time"

	"github.com/blooprocket-create/MOBAVeyra/Backend/internal/match"
)

// AssignmentSwitch tells the match server to read its assignment from
// standard input (ADR-007 §5). It names the channel; it is not a secret.
const AssignmentSwitch = "-VeyraAssignment=stdin"

// Labels mark the containers this allocator manages.
const (
	labelManaged = "veyra.managed"
	labelMatchID = "veyra.match-id"
)

// A kept log's folder is readable by the host's user; the Engine's log frames
// carry an 8-byte header.
const (
	logDirectoryMode = 0o755
	frameHeaderSize  = 8
)

// matchIDPattern is what a kept log's file is named after: a match ID.
var matchIDPattern = regexp.MustCompile(`^[A-Za-z0-9][A-Za-z0-9-]{0,63}$`)

// Config is the allocator's validated configuration.
type Config struct {
	// Endpoint is unix:///path/to/docker.sock or tcp://host:port.
	Endpoint       string
	APIVersion     string
	RequestTimeout time.Duration
	Image          string
	Network        string
	NamePrefix     string
	ContainerPort  int
	HostIP         string
	ServerArgs     []string
	StopTimeout    time.Duration
	// LogDirectory, when set, is where each match server's log is saved, as
	// <match ID>.log, before its container is removed (ADR-062 §4).
	LogDirectory string
	// Log reports what could not be saved; nil means the default logger.
	Log *slog.Logger
}

// Allocator implements match.Allocator on the Docker Engine API.
type Allocator struct {
	cfg    Config
	client *http.Client
	base   string
}

var _ match.Allocator = (*Allocator)(nil)

// New builds an Allocator for the configured endpoint.
func New(cfg Config) (*Allocator, error) {
	u, err := url.Parse(cfg.Endpoint)
	if err != nil {
		return nil, fmt.Errorf("docker endpoint: %w", err)
	}
	transport := &http.Transport{}
	var base string
	switch u.Scheme {
	case "unix":
		socket := u.Path
		transport.DialContext = func(ctx context.Context, _, _ string) (net.Conn, error) {
			var d net.Dialer
			return d.DialContext(ctx, "unix", socket)
		}
		// The host part is ignored when dialling a socket.
		base = "http://docker"
	case "tcp":
		base = "http://" + u.Host
	default:
		return nil, fmt.Errorf("docker endpoint must be unix:// or tcp://, got %q", cfg.Endpoint)
	}
	return &Allocator{cfg: cfg, client: &http.Client{Transport: transport}, base: base + "/v" + cfg.APIVersion}, nil
}

// containerName is a match's container. Addressing servers by a name derived
// from the match ID means the backend always knows where a match's server is,
// even if it stopped between creating the container and recording that.
func (a *Allocator) containerName(matchID string) string { return a.cfg.NamePrefix + matchID }

// Start creates the match's container, attaches to its standard input, starts
// it and writes the assignment, then closes standard input.
func (a *Allocator) Start(ctx context.Context, spec match.ServerSpec) error {
	ctx, cancel := context.WithTimeout(ctx, a.cfg.RequestTimeout)
	defer cancel()
	name := a.containerName(spec.MatchID)
	port := fmt.Sprintf("%d/udp", a.cfg.ContainerPort)
	create := map[string]any{
		"Image":        a.cfg.Image,
		"Cmd":          serverCommand(spec.Map, a.cfg.ServerArgs),
		"AttachStdin":  true,
		"OpenStdin":    true,
		"StdinOnce":    true,
		"Tty":          false,
		"Labels":       map[string]string{labelManaged: "true", labelMatchID: spec.MatchID},
		"ExposedPorts": map[string]any{port: map[string]any{}},
		"HostConfig": map[string]any{
			"PortBindings": map[string]any{port: []map[string]string{{"HostIp": a.cfg.HostIP, "HostPort": strconv.Itoa(spec.HostPort)}}},
			"NetworkMode":  a.cfg.Network,
			"Init":         true,
			"CapDrop":      []string{"ALL"},
			"SecurityOpt":  []string{"no-new-privileges"},
		},
	}
	if err := a.call(ctx, http.MethodPost, "/containers/create?name="+url.QueryEscape(name), create, http.StatusCreated); err != nil {
		return fmt.Errorf("create %s: %w", name, err)
	}
	stdin, err := a.attachStdin(ctx, name)
	if err != nil {
		return fmt.Errorf("attach %s: %w", name, err)
	}
	defer stdin.Close()
	if err := a.call(ctx, http.MethodPost, "/containers/"+name+"/start", nil, http.StatusNoContent); err != nil {
		return fmt.Errorf("start %s: %w", name, err)
	}
	if _, err := stdin.Write(spec.Assignment); err != nil {
		return fmt.Errorf("write the assignment to %s: %w", name, err)
	}
	// Closing standard input gives the server end of input (StdinOnce).
	if err := stdin.Close(); err != nil {
		return fmt.Errorf("close %s's standard input: %w", name, err)
	}
	return nil
}

// serverCommand is the match server's command line: the map it loads, then the
// configured arguments, then the switch that has it read its assignment. An
// image whose command needs no map (the engine test's) is given none.
func serverCommand(mapPath string, serverArgs []string) []string {
	var command []string
	if mapPath != "" {
		command = append(command, mapPath)
	}
	command = append(command, serverArgs...)
	return append(command, AssignmentSwitch)
}

// attachStdin attaches to a container's standard input. The Engine upgrades
// the request to a raw stream; Go hands the upgraded connection back as the
// response body.
func (a *Allocator) attachStdin(ctx context.Context, name string) (io.WriteCloser, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, a.base+"/containers/"+name+"/attach?stream=1&stdin=1", nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Connection", "Upgrade")
	req.Header.Set("Upgrade", "tcp")
	resp, err := a.client.Do(req)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode != http.StatusSwitchingProtocols {
		defer resp.Body.Close()
		return nil, apiError(resp)
	}
	stream, ok := resp.Body.(io.ReadWriteCloser)
	if !ok {
		resp.Body.Close()
		return nil, errors.New("the upgraded attach stream is not writable")
	}
	return stream, nil
}

// Status reports on a match's container.
func (a *Allocator) Status(ctx context.Context, matchID string) (match.ServerStatus, error) {
	ctx, cancel := context.WithTimeout(ctx, a.cfg.RequestTimeout)
	defer cancel()
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, a.base+"/containers/"+a.containerName(matchID)+"/json", nil)
	if err != nil {
		return match.ServerStatus{}, err
	}
	resp, err := a.client.Do(req)
	if err != nil {
		return match.ServerStatus{}, err
	}
	defer resp.Body.Close()
	if resp.StatusCode == http.StatusNotFound {
		return match.ServerStatus{Missing: true}, nil
	}
	if resp.StatusCode != http.StatusOK {
		return match.ServerStatus{}, apiError(resp)
	}
	var inspect struct {
		State struct {
			Status   string
			Running  bool
			ExitCode int
		}
	}
	if err := json.NewDecoder(resp.Body).Decode(&inspect); err != nil {
		return match.ServerStatus{}, err
	}
	switch {
	case inspect.State.Running:
		return match.ServerStatus{Running: true}, nil
	case inspect.State.Status == "exited" || inspect.State.Status == "dead":
		return match.ServerStatus{Exited: true, ExitCode: inspect.State.ExitCode}, nil
	default:
		// Created but not yet started, or restarting: neither running nor gone.
		return match.ServerStatus{}, nil
	}
}

// Remove stops a match's container, giving it StopTimeout to exit, and
// deletes it. A container that does not exist is already removed.
func (a *Allocator) Remove(ctx context.Context, matchID string) error {
	name := a.containerName(matchID)
	stopCtx, cancel := context.WithTimeout(ctx, a.cfg.RequestTimeout+a.cfg.StopTimeout)
	defer cancel()
	stopSeconds := strconv.Itoa(int(a.cfg.StopTimeout / time.Second))
	err := a.call(stopCtx, http.MethodPost, "/containers/"+name+"/stop?t="+stopSeconds, nil, http.StatusNoContent, http.StatusNotModified, http.StatusNotFound)
	if err != nil {
		return fmt.Errorf("stop %s: %w", name, err)
	}
	// Its log goes with the container, so it is kept first. A log that cannot be
	// kept never keeps the container: the reaper would try again forever.
	if a.cfg.LogDirectory != "" {
		if err := a.keepLog(ctx, name, matchID); err != nil {
			a.logger().Warn("could not keep a match server's log", "match", matchID, "err", err)
		}
	}
	deleteCtx, cancelDelete := context.WithTimeout(ctx, a.cfg.RequestTimeout)
	defer cancelDelete()
	if err := a.call(deleteCtx, http.MethodDelete, "/containers/"+name+"?force=1", nil, http.StatusNoContent, http.StatusNotFound); err != nil {
		return fmt.Errorf("delete %s: %w", name, err)
	}
	return nil
}

func (a *Allocator) logger() *slog.Logger {
	if a.cfg.Log != nil {
		return a.cfg.Log
	}
	return slog.Default()
}

// keepLog saves a container's output to LogDirectory/<match ID>.log. A
// container that is already gone has no log to keep.
func (a *Allocator) keepLog(ctx context.Context, name, matchID string) error {
	// Match IDs are the backend's own UUIDs; anything else never names a file.
	if !matchIDPattern.MatchString(matchID) {
		return fmt.Errorf("match ID %q is not a file name", matchID)
	}
	logCtx, cancel := context.WithTimeout(ctx, a.cfg.RequestTimeout)
	defer cancel()
	req, err := http.NewRequestWithContext(logCtx, http.MethodGet, a.base+"/containers/"+name+"/logs?stdout=1&stderr=1", nil)
	if err != nil {
		return err
	}
	resp, err := a.client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode == http.StatusNotFound {
		return nil
	}
	if resp.StatusCode != http.StatusOK {
		return apiError(resp)
	}
	if err := os.MkdirAll(a.cfg.LogDirectory, logDirectoryMode); err != nil {
		return err
	}
	file, err := os.Create(filepath.Join(a.cfg.LogDirectory, matchID+".log"))
	if err != nil {
		return err
	}
	if err := demultiplex(file, resp.Body); err != nil {
		file.Close()
		return err
	}
	return file.Close()
}

// demultiplex copies a container's log stream to w. Without a TTY the Engine
// frames each chunk with an 8-byte header: the stream (0 stdin, 1 stdout,
// 2 stderr), three zero bytes, then the chunk's length, big-endian. A stream
// that does not start with such a header is copied as it is.
func demultiplex(w io.Writer, r io.Reader) error {
	reader := bufio.NewReader(r)
	for {
		header, err := reader.Peek(frameHeaderSize)
		if errors.Is(err, io.EOF) && len(header) == 0 {
			return nil
		}
		if err != nil && !errors.Is(err, io.EOF) {
			return err
		}
		if len(header) < frameHeaderSize || header[0] > 2 || header[1] != 0 || header[2] != 0 || header[3] != 0 {
			_, err := io.Copy(w, reader)
			return err
		}
		size := int64(binary.BigEndian.Uint32(header[4:frameHeaderSize]))
		if _, err := reader.Discard(frameHeaderSize); err != nil {
			return err
		}
		if _, err := io.CopyN(w, reader, size); err != nil {
			return err
		}
	}
}

// call makes one API request and checks its status.
func (a *Allocator) call(ctx context.Context, method, path string, body any, ok ...int) error {
	var reader io.Reader
	if body != nil {
		raw, err := json.Marshal(body)
		if err != nil {
			return err
		}
		reader = bytes.NewReader(raw)
	}
	req, err := http.NewRequestWithContext(ctx, method, a.base+path, reader)
	if err != nil {
		return err
	}
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	resp, err := a.client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	for _, code := range ok {
		if resp.StatusCode == code {
			_, _ = io.Copy(io.Discard, resp.Body)
			return nil
		}
	}
	return apiError(resp)
}

// apiError turns an Engine error response into an error.
func apiError(resp *http.Response) error {
	var body struct {
		Message string `json:"message"`
	}
	raw, _ := io.ReadAll(io.LimitReader(resp.Body, 4096))
	if json.Unmarshal(raw, &body) == nil && body.Message != "" {
		return fmt.Errorf("docker: %s: %s", resp.Status, body.Message)
	}
	return fmt.Errorf("docker: %s", resp.Status)
}
