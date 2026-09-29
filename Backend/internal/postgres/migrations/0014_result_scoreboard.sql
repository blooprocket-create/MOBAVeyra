-- The result's scoreboard (ADR-017 §5): every player's statistics and final
-- equipment, humans and bots, as the match server reported them. The backend
-- checks its shape and computes nothing from it, so it is kept as a document.
-- Results recorded before this migration, and results from servers that send
-- no scoreboard, have none.

ALTER TABLE match.results ADD COLUMN players jsonb
    CHECK (players IS NULL OR jsonb_typeof(players) = 'array');
