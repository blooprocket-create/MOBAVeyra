-- Reports and commendation (ADR-047). A match's reports group into one case,
-- which this schema holds until a moderation service exists; each report
-- stays distinct. A report changes nothing else, and nothing reads a
-- commendation.

CREATE SCHEMA conduct;

CREATE TABLE conduct.cases (
    match_id  uuid        PRIMARY KEY REFERENCES match.matches(id) ON DELETE CASCADE,
    opened_at timestamptz NOT NULL
);

-- One report per reporter, reported player and match: a repeat finds the
-- first instead of flooding the case.
CREATE TABLE conduct.reports (
    match_id      uuid        NOT NULL REFERENCES conduct.cases(match_id) ON DELETE CASCADE,
    reporter_id   uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    reported_id   uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    reported_name text        NOT NULL,
    reason        text        NOT NULL CHECK (reason ~ '^[a-z][a-z_]{0,31}$'),
    details       text        NOT NULL,
    client_id     text        NOT NULL CHECK (client_id ~ '^[A-Za-z0-9-]{8,64}$'),
    created_at    timestamptz NOT NULL,
    PRIMARY KEY (match_id, reporter_id, reported_id),
    UNIQUE (reporter_id, client_id),
    CHECK (reporter_id <> reported_id)
);

CREATE TABLE conduct.commendations (
    match_id       uuid        NOT NULL REFERENCES match.matches(id) ON DELETE CASCADE,
    commender_id   uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    commended_id   uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    commended_name text        NOT NULL,
    created_at     timestamptz NOT NULL,
    PRIMARY KEY (match_id, commender_id),
    CHECK (commender_id <> commended_id)
);
