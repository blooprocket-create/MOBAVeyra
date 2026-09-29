-- Victory (ADR-011 §13): a standard match ends when a side destroys the other's
-- Prime Well, and that is the one end with a winner. Standard rules only is
-- checked by the service, the vocabulary and the pairing here. No server
-- reported a winner before this migration, so every recorded result holds.

ALTER TABLE match.results DROP CONSTRAINT results_end_reason_check;
ALTER TABLE match.results ADD CONSTRAINT results_end_reason_check
    CHECK (end_reason IN ('developer_request', 'abandoned', 'host_ended', 'prime_well_destroyed'));
ALTER TABLE match.results ADD CONSTRAINT results_victory_winner_check
    CHECK ((end_reason = 'prime_well_destroyed') = (winner IS NOT NULL));
