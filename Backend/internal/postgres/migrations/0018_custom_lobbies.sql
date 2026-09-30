-- Invite-only custom lobbies (ADR-021 §1; Custom Matches Bible §1–§4): the
-- host, the humans and bots in their seats, the session's rules and pending
-- invitations. The rules themselves are the lobby package's; the vocabulary is
-- here.

CREATE SCHEMA lobby;

CREATE TABLE lobby.lobbies (
    id              uuid        PRIMARY KEY,
    host_id         uuid        NOT NULL REFERENCES identity.accounts(id),
    status          text        NOT NULL CHECK (status IN ('open', 'selecting')),
    victory_enabled boolean     NOT NULL,
    victory_chosen  boolean     NOT NULL,
    -- NULL plays the game's own starting Gold.
    starting_gold   double precision CHECK (starting_gold IS NULL OR starting_gold >= 0),
    created_at      timestamptz NOT NULL DEFAULT now()
);

-- An account is in at most one lobby, and a seat holds at most one human.
CREATE TABLE lobby.members (
    lobby_id   uuid        NOT NULL REFERENCES lobby.lobbies(id) ON DELETE CASCADE,
    account_id uuid        NOT NULL UNIQUE REFERENCES identity.accounts(id),
    joined_at  timestamptz NOT NULL,
    side       text        NOT NULL CHECK (side IN ('A', 'B')),
    seat       integer     NOT NULL CHECK (seat >= 0),
    PRIMARY KEY (lobby_id, account_id),
    UNIQUE (lobby_id, side, seat)
);

CREATE TABLE lobby.bots (
    lobby_id    uuid    NOT NULL REFERENCES lobby.lobbies(id) ON DELETE CASCADE,
    side        text    NOT NULL CHECK (side IN ('A', 'B')),
    seat        integer NOT NULL CHECK (seat >= 0),
    vanguard_id text    NOT NULL CHECK (vanguard_id ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    difficulty  text    NOT NULL CHECK (difficulty IN ('beginner', 'intermediate')),
    PRIMARY KEY (lobby_id, side, seat)
);

CREATE TABLE lobby.invites (
    id         uuid        PRIMARY KEY,
    lobby_id   uuid        NOT NULL REFERENCES lobby.lobbies(id) ON DELETE CASCADE,
    inviter_id uuid        NOT NULL REFERENCES identity.accounts(id),
    invitee_id uuid        NOT NULL REFERENCES identity.accounts(id),
    created_at timestamptz NOT NULL,
    expires_at timestamptz NOT NULL,
    UNIQUE (lobby_id, invitee_id)
);
CREATE INDEX invites_invitee_idx ON lobby.invites (invitee_id);
