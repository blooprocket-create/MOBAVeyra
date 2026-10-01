-- Draft Pick (ADR-041): a select of turns, with bans; trades between locked
-- teammates; and the phase each select is in, its turn, and the timing it
-- opened with.

ALTER TABLE selection.sessions DROP CONSTRAINT sessions_kind_check;
ALTER TABLE selection.sessions ADD CONSTRAINT sessions_kind_check CHECK (kind IN ('practice', 'casual', 'custom', 'draft'));

ALTER TABLE selection.sessions
    ADD COLUMN phase     text    NOT NULL DEFAULT 'picking' CHECK (phase IN ('banning', 'picking', 'final')),
    ADD COLUMN turn      integer NOT NULL DEFAULT 0 CHECK (turn >= 0),
    ADD COLUMN turn_done integer NOT NULL DEFAULT 0 CHECK (turn_done >= 0),
    ADD COLUMN timing    jsonb   NOT NULL DEFAULT '{}'::jsonb;

ALTER TABLE selection.seats ADD COLUMN ban_hover text CHECK (ban_hover ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$');

CREATE TABLE selection.bans (
    session_id  uuid    NOT NULL REFERENCES selection.sessions(id) ON DELETE CASCADE,
    ban_order   integer NOT NULL CHECK (ban_order >= 0),
    side        text    NOT NULL CHECK (side IN ('A', 'B')),
    account_id  uuid    NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    vanguard_id text    NOT NULL CHECK (vanguard_id ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    PRIMARY KEY (session_id, ban_order),
    UNIQUE (session_id, vanguard_id)
);

-- One standing offer per player (ADR-041 §7.5).
CREATE TABLE selection.trades (
    session_id   uuid NOT NULL REFERENCES selection.sessions(id) ON DELETE CASCADE,
    from_account uuid NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    to_account   uuid NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    PRIMARY KEY (session_id, from_account),
    CHECK (from_account <> to_account)
);
