-- Account progression, the account currencies, purchases and Vanguard Mastery
-- (ADR-045). Flux and Refined Flux here are persistent account currencies,
-- never in-match Team Flux. The balances are what the grants gave, less what
-- the purchases spent, plus development adjustments: every change is a row.

CREATE SCHEMA progression;

CREATE TABLE progression.accounts (
    account_id   uuid        PRIMARY KEY REFERENCES identity.accounts(id) ON DELETE CASCADE,
    level        integer     NOT NULL CHECK (level >= 1),
    level_xp     bigint      NOT NULL CHECK (level_xp >= 0),
    lifetime_xp  bigint      NOT NULL CHECK (lifetime_xp >= 0),
    flux         bigint      NOT NULL CHECK (flux >= 0),
    refined_flux bigint      NOT NULL CHECK (refined_flux >= 0),
    updated_at   timestamptz NOT NULL
);

CREATE TABLE progression.mastery (
    account_id      uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    vanguard_id     text        NOT NULL CHECK (vanguard_id ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    level           integer     NOT NULL CHECK (level >= 1),
    level_points    bigint      NOT NULL CHECK (level_points >= 0),
    lifetime_points bigint      NOT NULL CHECK (lifetime_points >= 0),
    updated_at      timestamptz NOT NULL,
    PRIMARY KEY (account_id, vanguard_id)
);

-- One row per match and account: a replayed result grants nothing twice.
CREATE TABLE progression.grants (
    match_id       uuid        NOT NULL REFERENCES match.matches(id) ON DELETE CASCADE,
    account_id     uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    reason         text        NOT NULL CHECK (reason IN ('', 'custom', 'not_matchmade', 'no_contest', 'not_completed', 'not_joined',
                                                         'personal_loss', 'coop_level')),
    account_xp     bigint      NOT NULL CHECK (account_xp >= 0),
    level_before   integer     NOT NULL CHECK (level_before >= 1),
    level_after    integer     NOT NULL CHECK (level_after >= level_before),
    flux           bigint      NOT NULL CHECK (flux >= 0),
    refined_flux   bigint      NOT NULL CHECK (refined_flux >= 0),
    vanguard_id    text        NOT NULL CHECK (vanguard_id = '' OR vanguard_id ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    mastery_points bigint      NOT NULL CHECK (mastery_points >= 0),
    mastery_before integer     NOT NULL CHECK (mastery_before >= 0),
    mastery_after  integer     NOT NULL CHECK (mastery_after >= mastery_before),
    granted_at     timestamptz NOT NULL,
    PRIMARY KEY (match_id, account_id)
);

-- One row per purchase ID the client generated: a repeated purchase returns
-- its first outcome.
CREATE TABLE progression.purchases (
    purchase_id  text        PRIMARY KEY CHECK (purchase_id ~ '^[A-Za-z0-9-]{8,64}$'),
    account_id   uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    vanguard_id  text        NOT NULL CHECK (vanguard_id ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    currency     text        NOT NULL CHECK (currency IN ('flux', 'refinedFlux')),
    price        bigint      NOT NULL CHECK (price > 0),
    purchased_at timestamptz NOT NULL
);
CREATE INDEX purchases_account ON progression.purchases (account_id);

-- Currency the development route granted (ADR-045 §6), so the balances stay
-- reconcilable with the ledgers.
CREATE TABLE progression.dev_adjustments (
    id           bigserial   PRIMARY KEY,
    account_id   uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    flux         bigint      NOT NULL CHECK (flux >= 0),
    refined_flux bigint      NOT NULL CHECK (refined_flux >= 0),
    granted_at   timestamptz NOT NULL
);

-- A Vanguard bought with an account currency is owned as a purchase.
ALTER TABLE account.entitlements DROP CONSTRAINT entitlements_source_check;
ALTER TABLE account.entitlements ADD CONSTRAINT entitlements_source_check CHECK (source IN ('starter', 'purchase'));
