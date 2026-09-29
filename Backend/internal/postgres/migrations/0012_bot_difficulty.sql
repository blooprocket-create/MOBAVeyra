-- Each bot's difficulty (Custom Matches Bible §3; ADR-013 §6): how it plays,
-- never the rules. Every bot recorded before this migration wandered, which
-- is closest to Beginner.

ALTER TABLE match.bots ADD COLUMN difficulty text NOT NULL DEFAULT 'beginner'
    CHECK (difficulty IN ('beginner', 'intermediate'));
ALTER TABLE match.bots ALTER COLUMN difficulty DROP DEFAULT;
