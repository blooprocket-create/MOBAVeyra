# Veyra Vision & Reconnaissance Bible

**Version:** 0.1  
**Status:** Current working vision canon  
**Scope:** Fog of war, Dense Fog, stealth detection, player vision tools, ward lifetimes, and related reward rules.  
**Tuning:** All tool ranges, lifetimes, durations, charge-recharge intervals, cooldowns, linger times, detection radii, ward Gold rewards, and swap costs are data-driven until playtested.

This document consolidates the vision and ward decisions established during battleground and economy design. It is the primary reference for vision-tool implementation. The Battleground Bible owns the map and its Dense Fog locations; the Combat Bible owns general targetability, stealth, hit validation, and projectile/skillshot behavior. Where older documents are silent about the three vision tools, use this document. If a real contradiction is discovered, surface it rather than silently inventing a rule.

## 1. Foundational visibility

- Veyra has normal team-based fog of war and shared ordinary map vision.
- The battleground currently has **no traditional brush/bush concealment**.
- **Dense Fog** is a distinct concealment volume with stricter direct-vision and targeting rules than ordinary fog of war.
- Vision, detection, and targetability are distinct concepts. Knowing a hidden Vanguard's position does not necessarily grant permission to acquire it as a targeted-ability or basic-attack target.
- Server-authoritative gameplay validates detection, information sharing, targeting, ward placement and ward destruction. UI/VFX show those results without independently owning visibility rules.

## 2. Dense Fog

- A Vanguard outside a Dense Fog volume cannot directly see or acquire an enemy Vanguard inside it as a targeted attack/ability target.
- Allied Vanguard presence inside a fog volume does **not** transmit direct enemy-Vanguard vision to an ally outside. The observing Vanguard must personally enter the **same fog volume** to gain ordinary direct visual confirmation and targeted acquisition.
- An observing Vanguard inside the same Dense Fog volume may use normal direct targeting against enemies they can validly see, subject to other states such as stealth, Stasis, or genuine Untargetability.
- Enemy outlines from Sweeper are a deliberate **information-only exception** to Dense Fog concealment: they provide position information, **not** remote direct targeting from outside that fog.
- Skillshots, ground-targeted abilities, and other non-targeted effects can still be aimed into Dense Fog and hit valid hidden enemies when their gameplay geometry intersects.
- An enemy inside Dense Fog is **not literally in the Combat Bible's Untargetable state** merely because the observer is outside; the restriction is on the outside observer's direct targeting/acquisition. This matters for AoEs, existing DoTs, projectiles, and other combat interactions.

### Ability-created Dense Fog (ruled 2026-09-23)

- **Abilities may create Dense Fog at runtime.** A fog volume created during a match by an ability (currently Sylra's **Lay the Mist** and **Through the White**) is **the same construct** as a map-authored Dense Fog volume, not a lookalike visual effect. Every rule in this section and in §4–§6 applies to it unchanged: outside-observer concealment, the same-volume rule for direct vision and targeting, ward and Waymark presence behavior, Sweeper outlines and Quick Sight.
- The **Vision system owns** runtime fog volumes, as it owns authored ones (per [ADR-003](../ADR/ADR-003-owned-field-entities.md)). The creating ability supplies only placement, shape and lifetime from validated data; it does not implement its own concealment. When the volume expires, the fog's effects end at once, with no lingering concealment.
- The Battleground Bible still owns **where map-authored fog is placed**; abilities do not move or remove it.
- **Overlapping fog is one volume.** When an ability-created volume overlaps or touches another Dense Fog volume, authored or ability-created, the connected fog counts as **one fog volume** for the same-volume rule while they stay connected. A Vanguard inside any part of it can directly see and target enemies anywhere in it. When a created volume expires, the remaining fog splits back into its separate volumes at once.

## 3. One dedicated vision-tool slot

Every Vanguard has **one dedicated vision-tool slot**, separate from ordinary inventory and Flux Spell slots.

There are three current tool options:

1. **Persistent Ward** — places long-lived, enemy-invisible wards using regenerating charges.
2. **Sweeper** — actively detects stealth, including enemy wards and hidden units, in its detection area.
3. **Quick Sight** — grants immediate short-duration vision in an area without placing a lasting ward.

Each Vanguard starts a match with **Persistent Ward equipped for free and three charges ready**.

Vision tools may be purchased/swapped **only at the owner's fountain shop**. Each swap, including returning to a previously equipped tool, costs a small **flat, data-driven Gold amount**. Different tool choices do not have escalating or separate price schedules under current rules.

Changing tools affects future activations; it does not retroactively remove a ward already placed.

## 4. Persistent Ward

### Charges and recharge

- The equipped Persistent Ward tool carries a maximum of **3 charges** at a time.
- Each placement spends one charge and creates one independent ward.
- While on the battlefield with Persistent Ward equipped, its charges regenerate on a cooldown until the carried charge cap is reached. Exact recharge timing is data-driven.
- Returning to the owner's fountain immediately refills the carried charges to 3.
- Equipping or re-equipping Persistent Ward at the fountain immediately grants all 3 charges, even after previously swapping away.
- Respawning at the fountain refills Persistent Ward charges to 3 if it is equipped.

### Placed wards

- A successfully placed ward is invisible to the opposing team by default and grants ordinary team vision in its valid area outside Dense Fog.
- Each placed ward has its own lifetime and expires independently.
- A ward may be detected and destroyed before its natural expiration.
- **There is no cap on the total number of wards one Vanguard may have active on the map.** Three is a **carried-charge cap**, not an active-ward cap. If recharge and good placement timing allow more than three simultaneously active wards, those wards remain.
- Existing wards do not vanish when their owner recalls, dies, respawns, switches vision tools, or re-equips Persistent Ward.
- Wards remain until their individual lifetimes expire or enemies destroy them.
- A ward's own ordinary vision does not automatically reveal Invisible/Camouflaged Vanguards; specific detection must come from a rule that allows it.

### Wards inside Dense Fog

A Persistent Ward placed inside a Dense Fog volume is a **presence sensor**, not a remote enemy-Vanguard camera.

- It does not grant allies outside the fog an enemy Vanguard's model, exact coordinates, outline, or direct target acquisition.
- When an enemy Vanguard enters the ward's relevant fog coverage, the ward sends a **presence ping** to its team. *Relevant fog coverage* means the ward's own data-defined sensor area within that fog, **not the whole fog volume**.
- When placed while an enemy Vanguard is already in the relevant fog coverage, the ward pings immediately.
- The ping communicates that an enemy Vanguard is **present in that fog zone**, not their exact location.
- Ping cadence, persistence, presentation, and sensor coverage are data-driven.
- The ward does not override the requirement that an observing Vanguard enter the same fog volume to get direct enemy-Vanguard vision and targeting.

## 5. Sweeper

- Sweeper has **unlimited uses subject to its normal cooldown**; it does not consume ward charges or deploy a ward.
- When activated, Sweeper detects and reveals **all types of stealth** within its valid detection area, including enemy invisible wards, Invisible or Camouflaged Vanguards, stealthed companions, summons, and other stealthed units.
- Detected enemy wards are revealed to **the entire allied team** while validly detected, and any allied Vanguard may target and destroy them during that window.
- A surviving ward becomes invisible again after detection ends unless another effect is still revealing it.
- General combat visibility and targetability restrictions continue to apply to detected enemy units.
- **True Sight** is the name for this reveal effect: it exposes Camouflaged and Invisible units within its area (Combat Bible §11). **Sweeper is currently the only source of True Sight.** Any future source must be defined as True Sight explicitly; ordinary wards and vision never grant it.

### Sweeper versus Dense Fog

Sweeper can show the **outline and position** of an enemy Vanguard within its detection area even when that enemy is inside Dense Fog and the observer is outside.

- The outline is team-visible information, **not** direct enemy-Vanguard vision for targeted acquisition from outside the fog.
- Basic attacks and targeted abilities still cannot acquire that enemy from outside the fog.
- Allied Vanguards can aim skillshots, ground effects, and other non-targeted attacks at the outlined position; normal collision and hit rules apply.
- An observing Vanguard who enters the same Dense Fog volume can acquire the enemy normally if otherwise valid.
- The outline **lingers briefly** after Sweeper detection ends, then disappears. The brief linger is data-driven and conveys no ongoing tracking after expiration.
- Sweeper does not generally disable Dense Fog or turn it into ordinary team-shared vision.

## 6. Quick Sight

- Quick Sight has **unlimited uses subject to its normal cooldown**.
- Activation grants ordinary team vision in a chosen valid area for a short, data-driven duration.
- Quick Sight **does not place a ward** or create a persistent destructible ward object.
- Quick Sight does not automatically reveal Invisible/Camouflaged Vanguards or override general stealth rules.

### Quick Sight versus Dense Fog

When its area overlaps Dense Fog, Quick Sight acts as a **temporary presence sensor** for enemies in that fog, rather than granting remote direct vision.

- It can communicate enemy-Vanguard presence in the scanned fog zone during its effect.
- It grants **no outline, exact position, or direct target acquisition** of Vanguards inside the Dense Fog to observers outside.
- Entering the same fog volume remains necessary for direct visual confirmation and targeted acquisition.

## 7. Swapping, cooldowns, and death

- Vision-tool swaps happen at the owner's fountain shop and cost Gold each time.
- A tool swap **never deletes previously placed wards**.
- Sweeper and Quick Sight keep their individual remaining cooldowns when switched away from and later re-equipped; swapping cannot refresh a cooldown.
- Persistent Ward instead receives its full 3 carried charges immediately whenever equipped at the fountain.
- Sweeper and Quick Sight cooldowns continue ticking through death. They do **not** automatically reset on death or respawn.
- If Persistent Ward is equipped when its owner respawns at the fountain, it refills to 3 charges.
- Placed wards continue working through their owner's death, recall, and tool swaps until expiration or destruction.

## 8. Gold and XP for clearing wards

- Destroying an enemy ward grants its destroying Vanguard a **small, fixed, data-driven Gold reward**.
- Ward destruction grants **no XP**.
- There is **no proximity Gold** and **no team-shared Gold** from ward destruction.
- Sweeper can reveal wards for allies to destroy; the Vanguard who actually destroys the ward receives the reward.

## 9. Implementation boundaries and deliberate tuning gaps

- Keep ordinary shared map vision, Dense Fog concealment, presence-sensor events, Sweeper outline visibility, stealth reveal, and combat target acquisition as **separate explicit states/channels**. In particular, an outline must never be implemented by granting outside observers full targetability or blanket real Untargetability to the enemy.
- No UI element or player client may decide what an unseen enemy is doing or validate a hit.
- Existing placed wards are independent world entities; the owner's current tool equipment only controls what they can activate next.
- Tune separately: charge recharge, ward lifetime and vision area, placement range, ward durability/destroy rules, Sweeper area and active duration, Sweeper outline linger, Quick Sight area and lifetime, shop swap cost, ward destruction Gold, and presence ping cadence.
- This version intentionally does **not** invent additional vision-tool variants, ward ownership caps, remote fog-targeting exceptions, or a global stealth-immunity rule.

### Fog of war at the data boundary (ruled 2026-09-25)

- **Fog of war is enforced at the data boundary, per player.** The server sends an enemy unit or enemy-owned entity to a player's client only while it is visible to *that player*. Hidden enemy state is never sent to a client and then concealed there.
- **Shared vision and Dense Fog.** Ordinary shared team vision (§1) makes an enemy visible to every teammate. A Dense Fog sighting (§2) makes it visible only to observers inside the same fog volume; allies outside that volume do not receive it.
- **Channels carry only what they allow.** A presence ping (§4, §6) tells the team that an enemy Vanguard is present in a fog zone. A Sweeper outline (§5) carries outline and position while it lasts. Neither sends the hidden unit's full state.
- **The Vision system is the only writer** of who can see what: the team visibility groups and each observer's fog-volume membership. Other systems and clients read that result; they never widen it.
- The technical decision is recorded in [ADR-006](../ADR/ADR-006-unreal-project-scaffold.md) §5.
