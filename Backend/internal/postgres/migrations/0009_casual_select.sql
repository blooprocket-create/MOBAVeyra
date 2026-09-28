-- Casual Select (ADR-010 §8, §10): the players of a match everyone accepted
-- pick together. A select remembers when each player's client last asked
-- about it, its presence, and who left it: a dodge's record, with no penalty
-- until Match Flow §2 sets a schedule.

ALTER TABLE selection.sessions DROP CONSTRAINT sessions_kind_check;
ALTER TABLE selection.sessions ADD CONSTRAINT sessions_kind_check CHECK (kind IN ('practice', 'casual'));
ALTER TABLE selection.sessions DROP CONSTRAINT sessions_cancel_reason_check;
ALTER TABLE selection.sessions ADD CONSTRAINT sessions_cancel_reason_check
    CHECK (cancel_reason IN ('timed_out', 'allocation_failed', 'starting_timed_out', 'left', 'presence_lost'));
ALTER TABLE selection.sessions ADD COLUMN left_by uuid REFERENCES identity.accounts(id) ON DELETE SET NULL;

ALTER TABLE selection.seats ADD COLUMN last_seen timestamptz;
