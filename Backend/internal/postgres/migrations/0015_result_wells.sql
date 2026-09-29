-- The Flux Wells a match's sides secured, in order: each capture's site, side
-- and match-clock time (Match Statistics Bible §5; ADR-017 §5). The results'
-- team summary counts each capture once. Results recorded before this
-- migration, and results from servers that send none, have none.

ALTER TABLE match.results ADD COLUMN wells jsonb
    CHECK (wells IS NULL OR jsonb_typeof(wells) = 'array');
