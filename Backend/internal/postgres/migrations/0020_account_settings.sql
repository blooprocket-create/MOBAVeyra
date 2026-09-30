-- Each account's settings document (ADR-024 §1): the account-level player
-- settings the client syncs across machines, as text by setting ID. The
-- backend keeps the latest revision only and never reads what a value means.

CREATE TABLE account.settings (
    account_id uuid        PRIMARY KEY REFERENCES identity.accounts(id) ON DELETE CASCADE,
    revision   bigint      NOT NULL CHECK (revision > 0),
    doc        jsonb       NOT NULL CHECK (jsonb_typeof(doc) = 'object'),
    updated_at timestamptz NOT NULL
);
