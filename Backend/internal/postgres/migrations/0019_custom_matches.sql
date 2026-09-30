-- Custom matches (ADR-021 §2–§3): a custom lobby's champion select carries its
-- bots and its session rules into the match it creates. Custom matches have a
-- host, as practice does, and custom settings, which nothing else has. The
-- rules are the services'; the vocabulary and pairings are here.

ALTER TABLE match.matches DROP CONSTRAINT matches_rules_check;
ALTER TABLE match.matches ADD CONSTRAINT matches_rules_check CHECK (rules IN ('standard', 'practice', 'custom'));
ALTER TABLE match.matches DROP CONSTRAINT matches_practice_host_check;
ALTER TABLE match.matches ADD CONSTRAINT matches_host_check CHECK ((rules IN ('practice', 'custom')) = (host_account_id IS NOT NULL));
ALTER TABLE match.matches
    ADD COLUMN custom_victory_enabled boolean,
    -- NULL plays the game's own starting Gold.
    ADD COLUMN custom_starting_gold double precision CHECK (custom_starting_gold >= 0),
    ADD CONSTRAINT matches_custom_settings_check CHECK ((rules = 'custom') = (custom_victory_enabled IS NOT NULL)),
    ADD CONSTRAINT matches_custom_gold_check CHECK (custom_starting_gold IS NULL OR rules = 'custom');

ALTER TABLE selection.sessions DROP CONSTRAINT sessions_kind_check;
ALTER TABLE selection.sessions ADD CONSTRAINT sessions_kind_check CHECK (kind IN ('practice', 'casual', 'custom'));
ALTER TABLE selection.sessions
    -- The lobby that launched a custom select. No reference: the lobby closes
    -- when its match starts, and the select outlives it.
    ADD COLUMN lobby_id uuid,
    ADD COLUMN custom_victory_enabled boolean,
    ADD COLUMN custom_starting_gold double precision CHECK (custom_starting_gold >= 0),
    ADD CONSTRAINT sessions_custom_check CHECK ((kind = 'custom') = (lobby_id IS NOT NULL AND custom_victory_enabled IS NOT NULL)),
    ADD CONSTRAINT sessions_custom_gold_check CHECK (custom_starting_gold IS NULL OR kind = 'custom');

-- A custom select's bots, in each side's seat order, which sets their lanes.
CREATE TABLE selection.bots (
    session_id  uuid    NOT NULL REFERENCES selection.sessions(id) ON DELETE CASCADE,
    bot_order   integer NOT NULL,
    side        text    NOT NULL CHECK (side IN ('A', 'B')),
    vanguard_id text    NOT NULL CHECK (vanguard_id ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    difficulty  text    NOT NULL CHECK (difficulty IN ('beginner', 'intermediate')),
    PRIMARY KEY (session_id, bot_order)
);
