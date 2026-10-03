-- Favorite Vanguards (Pre-Game Client UX Bible 30; ADR-058 §5): marked while
-- browsing the Collection, offered as champion select's Favorites filter. A
-- favorite grants nothing; ownership stays in account.entitlements.

CREATE TABLE account.favorite_vanguards (
    account_id  uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    vanguard_id text        NOT NULL CHECK (vanguard_id ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    marked_at   timestamptz NOT NULL,
    PRIMARY KEY (account_id, vanguard_id)
);
