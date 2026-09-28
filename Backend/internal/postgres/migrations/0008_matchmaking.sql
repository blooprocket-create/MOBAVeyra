-- Matchmaking and Match Found (ADR-010 §10; Parties & Social Bible §2–3): a
-- queued party moves into a proposed match, which every player must accept,
-- and then into its champion select. Every status but idle locks the party.

ALTER TABLE party.parties DROP CONSTRAINT parties_status_check;
ALTER TABLE party.parties ADD CONSTRAINT parties_status_check CHECK (status IN ('idle', 'queued', 'found', 'selecting'));
-- When the leader pressed Find Match; a party returned to the queue keeps it.
ALTER TABLE party.parties ADD COLUMN queued_at timestamptz;
UPDATE party.parties SET queued_at = now() WHERE status <> 'idle';
ALTER TABLE party.parties ADD CONSTRAINT parties_queued_at_check CHECK ((status = 'idle') = (queued_at IS NULL));
CREATE INDEX parties_queue_idx ON party.parties (mode, queued_at) WHERE status = 'queued';

CREATE SCHEMA matchmaking;

CREATE TABLE matchmaking.found (
    id             uuid        PRIMARY KEY,
    mode           text        NOT NULL,
    state          text        NOT NULL CHECK (state IN ('pending', 'accepted', 'abandoned')),
    created_at     timestamptz NOT NULL,
    deadline       timestamptz NOT NULL,
    ended_at       timestamptz,
    select_id      uuid,
    abandon_reason text        CHECK (abandon_reason IN ('declined', 'timed_out', 'party_changed', 'select_failed')),
    CHECK ((state = 'pending') = (ended_at IS NULL)),
    CHECK ((state = 'accepted') = (select_id IS NOT NULL)),
    CHECK ((state = 'abandoned') = (abandon_reason IS NOT NULL))
);

-- A party may dissolve while its match is proposed, so these rows keep its ID
-- without a foreign key.
CREATE TABLE matchmaking.found_parties (
    found_id    uuid    NOT NULL REFERENCES matchmaking.found(id) ON DELETE CASCADE,
    party_id    uuid    NOT NULL,
    side        text    NOT NULL CHECK (side IN ('A', 'B')),
    party_order integer NOT NULL,
    PRIMARY KEY (found_id, party_id)
);

CREATE TABLE matchmaking.found_seats (
    found_id   uuid    NOT NULL REFERENCES matchmaking.found(id) ON DELETE CASCADE,
    account_id uuid    NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    party_id   uuid    NOT NULL,
    side       text    NOT NULL CHECK (side IN ('A', 'B')),
    seat_order integer NOT NULL,
    decision   text    CHECK (decision IN ('accepted', 'declined')),
    PRIMARY KEY (found_id, account_id)
);

-- An account is in at most one pending proposed match. Rows go when it ends.
CREATE TABLE matchmaking.pending_seats (
    account_id uuid PRIMARY KEY REFERENCES identity.accounts(id) ON DELETE CASCADE,
    found_id   uuid NOT NULL REFERENCES matchmaking.found(id) ON DELETE CASCADE
);
