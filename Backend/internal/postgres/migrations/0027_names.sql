-- Display-name changes, claims and forced renames (ADR-049). Existing accounts
-- count as logged in now, so no name becomes claimable merely by this change.

ALTER TABLE identity.accounts
    ADD COLUMN last_launcher_login_at timestamptz NOT NULL DEFAULT now(),
    ADD COLUMN free_rename_used       boolean     NOT NULL DEFAULT false,
    -- The last voluntary change; null before any.
    ADD COLUMN last_rename_at         timestamptz,
    -- Set when another account claimed the name: choose a new one, for free.
    ADD COLUMN rename_required        boolean     NOT NULL DEFAULT false;

-- What an account currency paid for besides a Vanguard, such as a name change,
-- so the balances stay explained (ADR-045 §6).
CREATE TABLE progression.spends (
    id         bigserial   PRIMARY KEY,
    account_id uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    reason     text        NOT NULL CHECK (reason ~ '^[a-z_]{1,32}$'),
    currency   text        NOT NULL CHECK (currency IN ('flux', 'refinedFlux')),
    amount     bigint      NOT NULL CHECK (amount > 0),
    spent_at   timestamptz NOT NULL
);
CREATE INDEX spends_account ON progression.spends (account_id, spent_at);