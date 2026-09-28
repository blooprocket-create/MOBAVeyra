-- A block between two players of a proposed match or a Casual Select, placed
-- after it was formed, stops it before its match exists (Parties & Social
-- Bible §6). The reason names no block, so no player learns of another's.

ALTER TABLE matchmaking.found DROP CONSTRAINT found_abandon_reason_check;
ALTER TABLE matchmaking.found ADD CONSTRAINT found_abandon_reason_check
    CHECK (abandon_reason IN ('declined', 'timed_out', 'party_changed', 'select_failed', 'no_longer_matched'));

ALTER TABLE selection.sessions DROP CONSTRAINT sessions_cancel_reason_check;
ALTER TABLE selection.sessions ADD CONSTRAINT sessions_cancel_reason_check
    CHECK (cancel_reason IN ('timed_out', 'allocation_failed', 'starting_timed_out', 'left', 'presence_lost', 'no_longer_matched'));
