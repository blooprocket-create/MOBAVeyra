# Item readability revision

Author feedback requested clearer equipment identity and separation at 32px.
Twelve individual built-in image_gen outputs replace their matching Sources/
entries. The other 52 sources retain their previous hashes. prompts.json stores
the exact prompts and image-reference roles. before_<id>.png preserves each
superseded source; <id>.png is the retained new source.

comparison.jpg shows each revision at 176px, 32px and 48px grayscale, alongside
the previous source at 48px. review_sheet.py reproduces it. The parent
export_icons.py reproduces all shipping RGB256 icons and full-set review sheets.
Import these twelve IDs using BuildIconArt.ps1 -Kind Items -Ids with their LFS
locks, then Test.ps1 -Filter Veyra.UI.ItemArt. apply_revision.py records and applies
the initial replacement; do not rerun it as the normal export step.

Item Bible sections 4–12 own the mechanics; Art Direction's inventory illustration
section owns the visual direction. No rules, tuning or gameplay implementation
changed. Final art approval and measured production-performance sign-off remain
pending. Engine/ holds before/after actual shop captures from the same Practice
smoke flow; no controlled frame-time comparison is claimed.
