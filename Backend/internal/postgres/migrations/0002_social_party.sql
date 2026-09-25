-- Social graph and parties (Parties & Social Bible §1–2, §5–6).

CREATE SCHEMA social;

CREATE TABLE social.friend_requests (
    from_id    uuid        NOT NULL REFERENCES identity.accounts(id),
    to_id      uuid        NOT NULL REFERENCES identity.accounts(id),
    created_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (from_id, to_id),
    CHECK (from_id <> to_id)
);
CREATE INDEX friend_requests_to_idx ON social.friend_requests (to_id);

-- One row per friendship, stored with account_a < account_b.
CREATE TABLE social.friendships (
    account_a  uuid        NOT NULL REFERENCES identity.accounts(id),
    account_b  uuid        NOT NULL REFERENCES identity.accounts(id),
    created_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (account_a, account_b),
    CHECK (account_a < account_b)
);
CREATE INDEX friendships_b_idx ON social.friendships (account_b);

CREATE TABLE social.blocks (
    blocker_id uuid        NOT NULL REFERENCES identity.accounts(id),
    blocked_id uuid        NOT NULL REFERENCES identity.accounts(id),
    created_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (blocker_id, blocked_id),
    CHECK (blocker_id <> blocked_id)
);
CREATE INDEX blocks_blocked_idx ON social.blocks (blocked_id);

CREATE SCHEMA party;

CREATE TABLE party.parties (
    id         uuid        PRIMARY KEY,
    leader_id  uuid        NOT NULL REFERENCES identity.accounts(id),
    mode       text,
    privacy    text        NOT NULL CHECK (privacy IN ('private', 'public')),
    status     text        NOT NULL CHECK (status IN ('idle', 'queued')),
    created_at timestamptz NOT NULL DEFAULT now()
);

-- An account belongs to at most one party.
CREATE TABLE party.members (
    party_id   uuid        NOT NULL REFERENCES party.parties(id) ON DELETE CASCADE,
    account_id uuid        NOT NULL UNIQUE REFERENCES identity.accounts(id),
    ready      boolean     NOT NULL,
    joined_at  timestamptz NOT NULL,
    PRIMARY KEY (party_id, account_id)
);

CREATE TABLE party.invites (
    id         uuid        PRIMARY KEY,
    party_id   uuid        NOT NULL REFERENCES party.parties(id) ON DELETE CASCADE,
    inviter_id uuid        NOT NULL REFERENCES identity.accounts(id),
    invitee_id uuid        NOT NULL REFERENCES identity.accounts(id),
    created_at timestamptz NOT NULL,
    expires_at timestamptz NOT NULL,
    UNIQUE (party_id, invitee_id)
);
CREATE INDEX invites_invitee_idx ON party.invites (invitee_id);
