-- AI participants (bots) in a match (ADR-010 §7). A practice match adds the
-- bots its settings list, so its player has targets. A bot is not an account:
-- it has no join ticket and no result; the server adds it when the match
-- starts.

CREATE TABLE match.bots (
    match_id    uuid    NOT NULL REFERENCES match.matches(id) ON DELETE CASCADE,
    bot_order   integer NOT NULL,
    side        text    NOT NULL CHECK (side IN ('A', 'B')),
    vanguard_id text    NOT NULL CHECK (vanguard_id ~ '^[a-z][a-z0-9]*(_[a-z0-9]+)*$'),
    PRIMARY KEY (match_id, bot_order)
);
