-- Party Chat, friend direct messages, champion-select team chat and post-match
-- chat (ADR-046). The chat service decides who reads each conversation, when
-- a message is sent and again when it is delivered; these tables keep what
-- was said and who joined which post-match chat.

CREATE SCHEMA chat;

-- seq orders every message. Sends take one advisory lock, so a later commit
-- always has a greater seq and a poll's cursor never passes a message that
-- committed late.
CREATE TABLE chat.messages (
    seq          bigserial   PRIMARY KEY,
    kind         text        NOT NULL CHECK (kind IN ('party', 'direct', 'select', 'postmatch')),
    conversation text        NOT NULL CHECK (conversation <> ''),
    sender_id    uuid        NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    sender_name  text        NOT NULL,
    recipient_id uuid        REFERENCES identity.accounts(id) ON DELETE CASCADE,
    body         text        NOT NULL CHECK (body <> ''),
    sent_at      timestamptz NOT NULL,
    client_id    text        NOT NULL CHECK (client_id ~ '^[A-Za-z0-9-]{8,64}$'),
    CHECK ((kind = 'direct') = (recipient_id IS NOT NULL)),
    -- A resend after a lost answer finds the first message instead of a second.
    UNIQUE (sender_id, client_id)
);

CREATE INDEX messages_conversation ON chat.messages (kind, conversation, seq);
CREATE INDEX messages_direct_sender ON chat.messages (sender_id, seq) WHERE kind = 'direct';
CREATE INDEX messages_direct_recipient ON chat.messages (recipient_id, seq) WHERE kind = 'direct';
-- The rate window counts a sender's recent messages.
CREATE INDEX messages_sender_time ON chat.messages (sender_id, sent_at);
-- Retention prunes by time.
CREATE INDEX messages_sent_at ON chat.messages (sent_at);

-- A participant's part in a match's post-match chat: the sequence of its
-- first message, which opts it in, and whether it left.
CREATE TABLE chat.postmatch_members (
    match_id   uuid    NOT NULL REFERENCES match.matches(id) ON DELETE CASCADE,
    account_id uuid    NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    join_seq   bigint  NOT NULL CHECK (join_seq >= 0),
    left_chat  boolean NOT NULL,
    PRIMARY KEY (match_id, account_id)
);

CREATE INDEX postmatch_members_open ON chat.postmatch_members (account_id) WHERE join_seq > 0 AND NOT left_chat;

CREATE TABLE chat.postmatch_mutes (
    match_id uuid NOT NULL REFERENCES match.matches(id) ON DELETE CASCADE,
    muter_id uuid NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    muted_id uuid NOT NULL REFERENCES identity.accounts(id) ON DELETE CASCADE,
    PRIMARY KEY (match_id, muter_id, muted_id),
    CHECK (muter_id <> muted_id)
);
