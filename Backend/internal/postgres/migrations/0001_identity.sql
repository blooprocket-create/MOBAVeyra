-- Identity: accounts, Veyra sessions and launch codes (ADR-005).
-- Only SHA-256 hashes of tokens and codes are stored, never the plaintext.

CREATE SCHEMA identity;

CREATE TABLE identity.accounts (
    id           uuid        PRIMARY KEY DEFAULT gen_random_uuid(),
    display_name text        NOT NULL UNIQUE,
    dev_seeded   boolean     NOT NULL,
    created_at   timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE identity.sessions (
    token_hash    bytea       PRIMARY KEY,
    account_id    uuid        NOT NULL REFERENCES identity.accounts(id),
    kind          text        NOT NULL CHECK (kind IN ('launcher', 'game')),
    build_version text,
    created_at    timestamptz NOT NULL,
    expires_at    timestamptz NOT NULL,
    revoked_at    timestamptz,
    CHECK ((kind = 'game') = (build_version IS NOT NULL))
);

CREATE INDEX sessions_account_idx ON identity.sessions (account_id);

CREATE TABLE identity.launch_codes (
    code_hash     bytea       PRIMARY KEY,
    account_id    uuid        NOT NULL REFERENCES identity.accounts(id),
    build_version text        NOT NULL,
    created_at    timestamptz NOT NULL,
    expires_at    timestamptz NOT NULL,
    consumed_at   timestamptz
);
