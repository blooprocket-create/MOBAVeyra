-- Match flow (ADR-019 §5): a standard match may end by surrender, which the
-- other side wins, or by remake, which nobody wins; and each participant's
-- result records a personal loss for absence and the absence itself (Match
-- Flow Bible §6–§8). Standard rules only is checked by the service. Results
-- recorded before this migration had no absence, so the defaults hold them.

ALTER TABLE match.results DROP CONSTRAINT results_end_reason_check;
ALTER TABLE match.results ADD CONSTRAINT results_end_reason_check
    CHECK (end_reason IN ('developer_request', 'abandoned', 'host_ended', 'prime_well_destroyed', 'surrender', 'remake'));
ALTER TABLE match.results DROP CONSTRAINT results_victory_winner_check;
ALTER TABLE match.results ADD CONSTRAINT results_victory_winner_check
    CHECK ((end_reason IN ('prime_well_destroyed', 'surrender')) = (winner IS NOT NULL));

ALTER TABLE match.result_participants
    ADD COLUMN personal_loss boolean NOT NULL DEFAULT false,
    ADD COLUMN absent_seconds double precision NOT NULL DEFAULT 0 CHECK (absent_seconds >= 0),
    ADD CONSTRAINT result_participants_personal_loss_check CHECK (joined OR NOT personal_loss);
