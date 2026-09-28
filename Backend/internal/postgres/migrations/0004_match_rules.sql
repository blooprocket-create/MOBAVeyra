-- Match rules, the practice host and each participant's Vanguard (ADR-010 §7, §9).
-- Matches created before this migration are standard matches whose
-- participants carry no Vanguard.

ALTER TABLE match.matches
    ADD COLUMN rules text NOT NULL DEFAULT 'standard' CHECK (rules IN ('standard', 'practice')),
    ADD COLUMN host_account_id uuid REFERENCES identity.accounts(id),
    -- A practice match has a host; a standard match has none.
    ADD CONSTRAINT matches_practice_host_check CHECK ((rules = 'practice') = (host_account_id IS NOT NULL));
ALTER TABLE match.matches ALTER COLUMN rules DROP DEFAULT;

ALTER TABLE match.participants
    ADD COLUMN vanguard_id text CHECK (vanguard_id ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$');

-- Only a practice match's host can end it (host_ended); the match rule is
-- checked by the service, the vocabulary here.
ALTER TABLE match.results DROP CONSTRAINT results_end_reason_check;
ALTER TABLE match.results ADD CONSTRAINT results_end_reason_check
    CHECK (end_reason IN ('developer_request', 'abandoned', 'host_ended'));
