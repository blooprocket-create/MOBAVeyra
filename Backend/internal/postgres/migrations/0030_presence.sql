-- Presence (Parties, Social & Matchmaking Bible §4–§5; ADR-061): when each
-- account was last seen, which its signed-in requests touch, and whether it
-- appears offline to friends outside its party. Kept in the database so a
-- restart or a second backend instance sees the same presence.

CREATE TABLE account.presence (
    account_id     uuid        PRIMARY KEY REFERENCES identity.accounts(id) ON DELETE CASCADE,
    seen_at        timestamptz,
    appear_offline boolean     NOT NULL DEFAULT false
);
