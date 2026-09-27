-- Onboarding and Vanguard entitlements (ADR-010 §6). The first-time tutorial is
-- stubbed as a starter choice: an account that has chosen its starter has an
-- onboarding row, and owns the starter.

CREATE SCHEMA account;

CREATE TABLE account.onboarding (
    account_id          uuid        PRIMARY KEY REFERENCES identity.accounts(id) ON DELETE CASCADE,
    starter_vanguard_id text        NOT NULL CHECK (starter_vanguard_id ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    completed_at        timestamptz NOT NULL
);

CREATE TABLE account.entitlements (
    account_id  uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    vanguard_id text        NOT NULL CHECK (vanguard_id ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    source      text        NOT NULL CHECK (source IN ('starter')),
    granted_at  timestamptz NOT NULL,
    PRIMARY KEY (account_id, vanguard_id)
);
