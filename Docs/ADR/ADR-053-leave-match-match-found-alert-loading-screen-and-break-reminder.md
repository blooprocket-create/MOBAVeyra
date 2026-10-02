# ADR-053: Leave Match, the Match Found alert, the loading screen and the break reminder

**Status:** Proposed. The author's standing instruction (2026-09-28) is to keep working unreviewed toward a viable game, choosing a provisional answer where canon is silent. §6 lists every such answer for the author to overturn. This ADR becomes Accepted when the author merges the pull request that adds it.
**Date:** 2026-10-02
**Related:**
- [Match Flow Bible](../Design/Veyra_Match_Flow_Bible_v0.1.md) §3, §5.2 (leaving a live match) and §13 (the loading screen).
- [Settings & Accessibility Bible](../Design/Veyra_Settings_Accessibility_Bible_v0.1.md) §11.1 (SET-76), §11.3 (SET-50, SET-71) and §11.6 (SET-111–120).
- [Client Platform Bible](../Design/Veyra_Client_Platform_Bible_v0.1.md) §1, §3 and §6; [Account, Collection & Mastery Bible](../Design/Veyra_Account_Collection_Mastery_Bible_v0.1.md) §2 (the break reminder).
- [ADR-010](ADR-010-play-flow.md): the client coordinator, Reconnect-only and the shell.
- [ADR-019](ADR-019-match-flow.md): disconnects, autopilot, rejoin and the personal loss.
- [ADR-024](ADR-024-player-settings.md): the settings registry; this ADR adds the first Audio settings.

## Context

The server side of match flow is built: disconnects start autopilot, rejoin returns the player to their Vanguard, and Reconnect-only keeps a player who left out of new queues. Four approved client pieces around it are missing:
- **Leave Match.** The in-match menu has no way to leave a live match on purpose.
- **The Match Found alert.** A client in the background gives no sign that a match was found.
- **The loading screen.** After travel, a client sees the battleground at 00:00 while the server still waits for players, with nothing to say so.
- **The break reminder.** Nothing suggests a break after long play.

## Decision

### 1. Leave Match

- The in-match menu offers **Leave Match** in a live match. It asks the coordinator through a new client intent, `LeaveMatch`, which the coordinator allows only in a match.
- **Confirmation (SET-76):** with `interface_confirm_leave_match` On (the default), Leave Match first asks **Stay in Match** or **Leave Match**, with Stay focused. The match does not pause.
- **What leaving does:** nothing new. The client leaves as a disconnect does: the server starts autopilot and counts toward the personal loss as for any disconnect (ADR-019), and the client enters Reconnect-only, from which the player can return to the same Vanguard (Match Flow §3, §5.2). The results screen says the player left.

### 2. The Match Found alert

- **Background attention (SET-50):** when Match Found arrives while the client's window is unfocused or minimized and `audio_background_match_notification` is On (the default), the client asks the OS for taskbar attention until the window is activated. It never takes focus itself, never accepts, and shows nothing about the party or players.
- **Match-ready sound (SET-71):** with `audio_match_ready_sound` On (the default), Match Found plays a one-time cue, in the foreground or the background. Off keeps the visual prompt and the background attention.
- These are the registry's first **Audio** settings.

### 3. The match loading screen

- **When:** from the client's arrival in the match until the server leaves its Loading phase. It covers the battleground and the HUD, and closes the moment loading completes: nothing on it delays match entry (Match Flow §13; SET-120).
- **Stages (SET-114):** plain labels with an indeterminate activity indicator and no percentages or time estimates. They are *Loading Match* until the client has its own Vanguard, then *Waiting for Players*. No player's device or connection details show.
- **Tips and lore (SET-116–120):**
  - Gameplay tips and lore facts are text-table rows, written from established mechanics, controls, world and Vanguard canon. A tip names an action, not a key, since keys are rebindable.
  - `interface_loading_tips`: **Both** (the default), **Tips Only**, **Lore Only** or **Off**. Essential status shows in every mode.
  - **Rotation:** the enabled entries in an order shuffled per match, so the starting entry varies. Each shows once before any repeats. Nothing tracks lifetime reading.
  - **Display time:** each automatically shown entry stays at least 8 seconds, longer for longer text.
  - **Previous and Next** are keyboard-accessible. Using either pauses automatic rotation for the rest of the loading screen.
- **Not yet:** required graphics preparation with Preparing/Complete/Failed and Retry (SET-111) waits for its own work. No loading audio cue is added (SET-113 rejected).

### 4. The break reminder

- `interface_play_reminder`: **Off**, **After 1 Hour**, **After 2 Hours** (the default) or **After 3 Hours**.
- **Counting play time:** the client counts continuous play from the start of the first match of a streak. A long enough gap without a match ends the streak.
- **Showing it:** after a match, once the streak reaches the chosen duration, the results screen and Home show a dismissible reminder. Dismissing it starts the count again. It never locks out play, forces a break or costs XP (Account Bible §2; Client Platform §3).

### 5. Ownership

| Piece | Owner |
|---|---|
| The Leave Match intent, the play streak and the reminder's state | VeyraServices: the client coordinator and pure rules beside it |
| The menu's Leave Match and its confirmation; the Match Found alert and cue; the loading screen; the reminder banner | VeyraUI |
| Tips and lore text | The text table |
| Rotation timings, the streak gap and the cue | Validated client presentation settings |
| Setting definitions and text | Settings.json and the text table |

### 6. Provisional answers where canon is open

1. **Leave Match is offered in every live match,** practice and custom included. Leaving a match with no other human ends it as abandoned, as any lone disconnect does ([ADR-007](ADR-007-match-join-contract.md)).
2. **No focus stealing.** Background attention is a taskbar flash until the player activates the window. The OS toast with Accept and Decline (SET-50) needs an application identity the installer does not register yet, and waits for it.
3. **The cue** is a short procedural tone, like champion select's turn cue, until audio assets exist.
4. **Loading stages:** *Loading Match*, then *Waiting for Players*. Before travel, the existing Match Starting and Connecting screens keep their place.
5. **Display time:** 8 seconds, plus 1 second for every 20 characters beyond 120.
6. **The break reminder** defaults to After 2 Hours. A streak ends after 20 minutes without a match.

## Out of scope

- Required graphics preparation and Retry (SET-111).
- Background audio modes (SET-49) and device fallback (SET-70).
- The OS notification's Accept and Decline (SET-50).
- Queue-dodge restrictions: the duration and escalation are open (Match Flow §17).
- Presence: an offline member cancelling the party's queue, and the post-match grace period (Parties §2, §4).
