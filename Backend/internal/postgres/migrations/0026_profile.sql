-- Player profiles (ADR-048): each account's official icon and background, the
-- permanently owned Vanguard it features, and whether its Match History is
-- shared. An account without a row shows the catalog's defaults and shares
-- nothing. Levels, ownership and Mastery stay with progression.

CREATE SCHEMA profile;

CREATE TABLE profile.appearances (
    account_id         uuid        PRIMARY KEY REFERENCES identity.accounts(id) ON DELETE CASCADE,
    icon               text        NOT NULL CHECK (icon ~ '^[a-z][a-z0-9_]{0,63}$'),
    background         text        NOT NULL CHECK (background ~ '^[a-z][a-z0-9_]{0,63}$'),
    -- Null when the account features no Vanguard (UX-71).
    featured_vanguard  text        CHECK (featured_vanguard ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    show_match_history boolean     NOT NULL DEFAULT false,
    updated_at         timestamptz NOT NULL DEFAULT now()
);
