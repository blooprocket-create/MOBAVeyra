-- Matches: rosters, servers, results (ADR-007). A match's join key is erased
-- when it ends, and its server credential is stored only as a SHA-256 hash.

CREATE SCHEMA match;

CREATE TABLE match.matches (
    id                     uuid        PRIMARY KEY,
    mode                   text        NOT NULL,
    state                  text        NOT NULL CHECK (state IN ('allocating', 'ready', 'ended', 'failed')),
    created_at             timestamptz NOT NULL,
    ready_at               timestamptz,
    ended_at               timestamptz,
    join_key               bytea,
    server_credential_hash bytea       NOT NULL UNIQUE,
    host_port              integer     NOT NULL CHECK (host_port BETWEEN 1 AND 65535),
    server_removed_at      timestamptz,
    failure_reason         text,
    -- A finished match has an end time and no join key; an active one has a key.
    CHECK ((state IN ('ended', 'failed')) = (ended_at IS NOT NULL)),
    CHECK ((state IN ('ended', 'failed')) = (join_key IS NULL)),
    CHECK ((state = 'failed') = (failure_reason IS NOT NULL)),
    CHECK (state NOT IN ('ready', 'ended') OR ready_at IS NOT NULL),
    -- A server is removed only once its match is over.
    CHECK (server_removed_at IS NULL OR state IN ('ended', 'failed'))
);

-- A match holds its port until its server is removed.
CREATE UNIQUE INDEX matches_held_port_idx ON match.matches (host_port) WHERE server_removed_at IS NULL;

CREATE TABLE match.participants (
    match_id     uuid    NOT NULL REFERENCES match.matches(id) ON DELETE CASCADE,
    account_id   uuid    NOT NULL REFERENCES identity.accounts(id),
    -- The display name the server was given when the match started.
    display_name text    NOT NULL,
    side         text    NOT NULL CHECK (side IN ('A', 'B')),
    roster_order integer NOT NULL,
    PRIMARY KEY (match_id, account_id)
);
CREATE INDEX participants_account_idx ON match.participants (account_id);

-- An account has at most one active match. Rows go when the match ends.
CREATE TABLE match.active_assignments (
    account_id uuid PRIMARY KEY REFERENCES identity.accounts(id),
    match_id   uuid NOT NULL,
    FOREIGN KEY (match_id, account_id) REFERENCES match.participants (match_id, account_id) ON DELETE CASCADE
);

CREATE TABLE match.results (
    match_id         uuid             PRIMARY KEY REFERENCES match.matches(id) ON DELETE CASCADE,
    end_reason       text             NOT NULL CHECK (end_reason IN ('developer_request', 'abandoned')),
    winner           text             CHECK (winner IN ('A', 'B')),
    duration_seconds double precision NOT NULL CHECK (duration_seconds >= 0)
);

CREATE TABLE match.result_participants (
    match_id         uuid    NOT NULL REFERENCES match.results(match_id) ON DELETE CASCADE,
    account_id       uuid    NOT NULL,
    joined           boolean NOT NULL,
    connected_at_end boolean NOT NULL,
    PRIMARY KEY (match_id, account_id),
    FOREIGN KEY (match_id, account_id) REFERENCES match.participants (match_id, account_id) ON DELETE CASCADE,
    CHECK (joined OR NOT connected_at_end)
);
