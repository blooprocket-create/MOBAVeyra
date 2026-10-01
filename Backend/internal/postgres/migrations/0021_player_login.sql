-- Player registration and sign-in through an external identity provider
-- (ADR-038). The provider proves who a player is; this table links its stable
-- user ID to the Veyra account, which owns everything else. Nothing the
-- provider holds (email, password) is stored here.

CREATE TABLE identity.provider_links (
    provider   text        NOT NULL CHECK (provider IN ('firebase')),
    subject    text        NOT NULL CHECK (length(subject) BETWEEN 1 AND 128),
    -- One provider identity per account, for now.
    account_id uuid        NOT NULL UNIQUE REFERENCES identity.accounts(id),
    created_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (provider, subject)
);

-- Display names are globally unique (Profiles & Identity Bible §4), and two
-- names that differ only in letter case are the same name (ADR-038 §4).
CREATE UNIQUE INDEX accounts_display_name_folded_key ON identity.accounts (lower(display_name));
