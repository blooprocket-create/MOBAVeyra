-- Queue-dodge restrictions (Match Flow Bible §2; ADR-060): when each account's
-- restriction for leaving a matchmade champion select ends. Personal, and no
-- moderation sanction; an ended restriction is simply in the past.

CREATE TABLE account.queue_restrictions (
    account_id       uuid        PRIMARY KEY REFERENCES identity.accounts(id) ON DELETE CASCADE,
    restricted_until timestamptz NOT NULL
);