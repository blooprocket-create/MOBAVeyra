package match

import "context"

// ServerSpec is what an allocator needs to start one match server.
type ServerSpec struct {
	MatchID string
	// HostPort is the reserved port players connect to.
	HostPort int
	// Assignment is written to the server's standard input and nowhere else
	// (ADR-007 §5). It holds the server's credential.
	Assignment []byte
}

// ServerStatus is what an allocator knows about a match's server.
type ServerStatus struct {
	Running bool
	// Exited servers have stopped; ExitCode is their status.
	Exited   bool
	ExitCode int
	// Missing servers do not exist, or no longer do.
	Missing bool
}

// Allocator starts and removes match servers: local Docker now, a fleet
// later (ADR-005 H3). Servers are addressed by match ID.
type Allocator interface {
	// Start starts the match's server and hands it its assignment.
	Start(ctx context.Context, spec ServerSpec) error
	// Status reports on the match's server.
	Status(ctx context.Context, matchID string) (ServerStatus, error)
	// Remove stops the match's server if it runs and deletes it. Removing a
	// server that does not exist succeeds.
	Remove(ctx context.Context, matchID string) error
}
