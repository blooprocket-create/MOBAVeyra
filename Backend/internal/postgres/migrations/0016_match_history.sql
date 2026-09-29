-- Match History (Pre-Game Client UX Bible 51, 64, 67; ADR-017 §6) lists a
-- player's completed matches newest first, in pages that continue from the
-- last match shown. participants_account_idx finds the player's matches; this
-- orders and pages them.

CREATE INDEX matches_ended_idx ON match.matches (ended_at DESC, id DESC) WHERE state = 'ended';
