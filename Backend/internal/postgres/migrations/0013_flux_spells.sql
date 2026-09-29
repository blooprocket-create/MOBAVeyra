-- Each seat's and each participant's starting Flux Spells (Battleground Bible
-- §14; ADR-015 §5): two slots in slot order, an empty string for an empty
-- slot. A seat's spells follow its Vanguard's saved loadout until the player
-- edits them. Earlier seats and participants took no spells.

ALTER TABLE selection.seats
    ADD COLUMN flux_spells text[] NOT NULL DEFAULT ARRAY['', '']::text[]
        CHECK (cardinality(flux_spells) = 2),
    ADD COLUMN flux_spells_edited boolean NOT NULL DEFAULT false;

ALTER TABLE match.participants
    ADD COLUMN flux_spells text[] NOT NULL DEFAULT ARRAY['', '']::text[]
        CHECK (cardinality(flux_spells) = 2);

-- The saved loadout is the spells an account last took into a match with a
-- Vanguard (Pre-Game Client UX Bible 37).
CREATE INDEX participants_account_vanguard ON match.participants (account_id, vanguard_id);
