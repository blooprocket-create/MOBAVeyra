-- Champion select (ADR-010 §8): a session per select, a seat per player, and
-- the one active select each account may be in. A select creates at most one
-- match, which records the select it came from.

CREATE SCHEMA selection;

CREATE TABLE selection.sessions (
    id              uuid        PRIMARY KEY,
    kind            text        NOT NULL CHECK (kind IN ('practice')),
    mode            text        NOT NULL,
    host_account_id uuid        REFERENCES identity.accounts(id) ON DELETE CASCADE,
    state           text        NOT NULL CHECK (state IN ('picking', 'starting', 'started', 'cancelled')),
    created_at      timestamptz NOT NULL,
    deadline        timestamptz NOT NULL,
    starting_at     timestamptz,
    ended_at        timestamptz,
    match_id        uuid,
    cancel_reason   text        CHECK (cancel_reason IN ('timed_out', 'allocation_failed', 'starting_timed_out')),
    CHECK ((state = 'started') = (match_id IS NOT NULL)),
    CHECK ((state = 'cancelled') = (cancel_reason IS NOT NULL)),
    CHECK ((state IN ('started', 'cancelled')) = (ended_at IS NOT NULL)),
    CHECK (state = 'picking' OR state = 'cancelled' OR starting_at IS NOT NULL)
);

CREATE TABLE selection.seats (
    session_id   uuid        NOT NULL REFERENCES selection.sessions(id) ON DELETE CASCADE,
    account_id   uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    display_name text        NOT NULL,
    side         text        NOT NULL CHECK (side IN ('A', 'B')),
    seat_order   integer     NOT NULL,
    hover        text        CHECK (hover ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    locked       text        CHECK (locked ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    locked_at    timestamptz,
    PRIMARY KEY (session_id, account_id),
    CHECK ((locked IS NULL) = (locked_at IS NULL))
);

-- An account is in at most one active select. Rows go when the select ends.
CREATE TABLE selection.active_seats (
    account_id uuid PRIMARY KEY REFERENCES identity.accounts(id) ON DELETE CASCADE,
    session_id uuid NOT NULL REFERENCES selection.sessions(id) ON DELETE CASCADE
);

ALTER TABLE match.matches ADD COLUMN select_id uuid CONSTRAINT matches_select_id_key UNIQUE;
