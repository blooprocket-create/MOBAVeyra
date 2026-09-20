> Repository Markdown edition: 25-Vanguard current design roster, expanded from the prior 20. Embedded concept art is omitted; consult the character-art directory for images actually uploaded to the repository.

# Veyra Initial Roster Character Bible

**Version:** 0.6  
**Kit consistency clarification (2026-09-20):** Cairn's passive/R shield attribution and displacement mitigation; Bryn's single empowered Breach explosion and Attack Speed overflow interaction; Oriel's valid-target-only homing fragment. No character lore, role or roster count changed.  
**Antagonist pass (2026-09-20):** Angeru, Marek, Gorraveth and Tavi rewritten as genuine antagonists. Origins, kits, roles, regions and roster count unchanged; in each case the change is the removal of an explicit moral safety rail rather than a new backstory.  
**Ride-state rulings (2026-09-20):** Raska's Momentum basis, Last Exit expiry behavior, and Last Exit overlap resolution recorded below. The generic ride-state rules they sit on live in **Combat Bible §56**; only the Raska-specific parts are here. No character lore, role or roster count changed.  
**Appearance pass (2026-09-20):** `**Visual language:**` paragraphs written for the fourteen Vanguards that had a surviving character sheet but no appearance description in canon (Silt, Relay, Patch, Tavi, Vera, Korruk, Torr, Marek, Neris, Moro, Qazharr, Varkesh, Sylra, Mavra), read off that existing art so the design survives regeneration. Raska, Kade and Angeru are deliberately excluded and still need a clean redesign; Vanguards 21–25 have no art yet. No kit, lore, role, region or roster count changed.  
**Appearance corrected from art (2026-09-20):** Raska's and Kade's `**Visual language:**` paragraphs were rewritten to match the author's regenerated artwork. The earlier paragraphs were my reconstruction from role and region and were simply wrong about how these two look; the art is authoritative and the text now follows it. No kit, lore, role, region or roster count changed.  
**Clean redesign (2026-09-20):** Raska, Kade and Angeru now carry `**Visual language:**` paragraphs, completing appearance coverage for all 25 Vanguards. These three are **new designs built from written canon alone** — role, region, kit, lore and personality — because their earlier sheets were withdrawn for third-party content and reconstructing that look would carry the reason for the withdrawal forward. Nothing was taken from the withdrawn art, and it must not be reintroduced. No kit, lore, role, region or roster count changed.  
**Current roster target:** 25 Vanguards (and growing as new designs are added)  
**Status:** Working character canon for the planned first-playable roster. All 25 Vanguards below belong to this design target; the number designed is not the number implemented/released.

## Roster principles

Veyra's first playable roster is intentionally **not** organized around mandatory top/jungle/mid/carry/support slots.

The game supplies a three-lane battlefield, an encompassing jungle, Flux Wells, wildlife, gold, XP, items, and opportunity costs. Players decide how five Vanguards distribute themselves.

- Five mid is legal.
- Two roamers is legal.
- Three solo lanes is legal.
- A team may choose not to run a traditional jungler.
- Balance should come from what a composition gives up, not from hard role restrictions.
- Gold/items strengthen Vanguards.
- Shared team Flux primarily strengthens Fluxborn/lane pressure and unlocks Flux Spell access rather than directly granting a generic champion stat ladder.
- Jungle wildlife has fixed camp identities and simple temporary traits; camps do not evolve during a match.
- Champion synergy should emerge naturally from mechanics rather than explicit named duo bonuses.
- Exact numerical values, ranges, cooldowns, ratios, and durations remain prototype tuning values.

## Roster overview

| # | Vanguard | Archetype | Damage | Origin | Signature |
|---|---|---|---|---|---|
| 1 | **Raska, The Redline** | Bruiser / Skirmisher | Physical | Iron March | Momentum fighter + Hound motorcycle |
| 2 | **Kade, Dead Reckoning** | Marksman / Utility Carry | Physical | Iron March | Geometry, prediction, allied displacement |
| 3 | **Silt, The Living Mire** | Control Mage | Magic | Buried Riverlands | Sandy marks, reclamation, anti-auto zones |
| 4 | **Relay, The Last Conductor** | Utility Vanguard | Utility | Ancient Flux Network | Stores and redistributes Charge |
| 5 | **Patch, The Last Hug** | Tank / Disruptor | Magic / Utility | Unknown | Possessed teddy that punishes enemies for ignoring him |
| 6 | **Tavi, The Playmate** | Assassin | Magic | Merrin | Tag, catch, hide-and-seek turned lethal |
| 7 | **Vera, The Last Volley** | Ranged Hypercarry | Magic | Merrin | Ramping cadence and firing-line play |
| 8 | **Korruk, The Splinterbeast** | Physical Burst Caster | Physical | Shatterdeep | Embed mineral spines, fracture, detonate |
| 9 | **Torr, The Unreturned** | Tank / Warden | Magic / Utility | Fluxborn | A Breaker that became permanent |
| 10 | **Marek, The Black Accord + Nix** | Warlock / Mobile Burst Caster | Magic | Drowned Cantons / Elsewhere | Fight from two positions at once |
| 11 | **Neris, The Tidebound** | Utility Caster / Enchanter | Magic | Drowned Cantons | Living-wave mobility, Calm/Storm states |
| 12 | **Moro, The Wildspark** | Jungler / Sorcerer | Magic | Wildwood | Simple predator mage stronger in jungle terrain |
| 13 | **Qazharr, The Harbor Wolf** | Fighter / Duelist | Physical | Drowned Cantons, adopted home | Sustained melee pressure and commitment |
| 14 | **Varkesh, The Forgeheart** | Battlemage / Warden | Magic | Ember Basin | Molten anti-mobility control |
| 15 | **Angeru, The Housebreaker** | Assassin / Skirmisher | Physical | Reed Provinces | Dual-stance Veil/Blade mastery, shadows, resets, flowing swordplay |
| 16 | **Sylra, The Mistwarden** | Utility / Controller | Magic | Drowned Cantons | Dense Fog control, presence information, regenerative Waymarks |
| 17 | **Mavra, The Spillwright** | Control Mage / DoT Mage | Magic | Iron March, Calder's Run | Contamination, persistent hazards, hard CC reactions |
| 18 | **Cairn, The River's Grasp** | Tank / Catcher / Warden | Physical / Utility | Buried Riverlands | Asymmetrical riverstone guardian; hook, stuns, max-Health shielding |
| 19 | **Bryn, The Harbor Gun** | Ranged Carry / Artillery Marksman | Physical | Drowned Cantons, adopted home | Slow heavy Flux-cannon attacks, Breach, non-targeting fog scout |
| 20 | **Oriel, The Waiting Light** | Poke / Burst Mage | Magic | Merrin | Stained-glass Echo; simple poke and exceptional late-game scaling |
| 21 | **Mimzi, The Pocket Hex** | Stealth / Ranged Proc Mage | Magic incl. basic attacks | Bramblehollow | Fennec Bramblekin; Hex procs and Camouflage |
| 22 | **Celandrine, The Wayrunner** | Mobile Ranged Carry / Kiter | Physical | Bramblehollow | Hare Bramblekin; attack while moving |
| 23 | **Gorraveth, The Slagmaw** | Fast-clear Melee Skirmisher | Physical | Ember Basin | Scarred reptilian hunter; camp momentum |
| 24 | **Aurelisse, The Open Sky** | Disengage Enchanter / Protector | Magic / Utility | Shatterdeep | Wind elemental; protection and directional peel |
| 25 | **Eudora Blackbridge, The Fieldwright** | Deployable Engineer / Siege | Physical / Utility | Iron March | Picket gun/bulwark machine |

---

# 1. Raska, The Redline

**Origin:** Iron March, Calder's Run  
**Role identity:** Late-game physical bruiser/skirmisher  
**Core fantasy:** Speed becomes violence.

Raska is a mechanic, fighter, courier, smuggler, and local folk hero whose motorcycle **Hound** is integral to her fighting style. She and Kade built Hound together: Raska handled the engine/chassis/Flux drive while Kade contributed stabilization, navigation, and inertial-control work.

Raska should begin relatively weak and become a monstrous late-game threat through items and Health scaling rather than having an oppressive natural early-game damage profile.

**Visual language:** Lean and hard-worked rather than bulky, with long windblown dark auburn hair streaked through with red. She wears a black crop top under an open red-and-oxblood riding jacket carrying a heavy armour plate over one shoulder, dark trousers layered with buckle straps and plated thigh and knee guards, and heavy buckled boots. One forearm is sheathed in a segmented mechanical bracer with an **orange Flux core burning at the wrist** — the coupling that keys her to Hound. Fingerless plated gloves, tattoo work across one upper arm, a pendant at the throat, and grease and road dust worked into her skin. **Hound is the other half of the silhouette**: a massive armoured machine in rust-red and gunmetal, plated bodywork over exposed pipework and cabling, enormous knobbled tyres, and orange Flux light running through its headlight grille, wheel hubs and engine channels. As Momentum builds, that orange climbs through both the bike and her bracer toward white. She and Hound both read as heavy, filthy and hard-used, **never** clean or showroom-fresh, and Hound is a brawler's machine built to hit things and keep going rather than a slim racing bike.

## Passive — Redline

Raska builds **Momentum** through movement, attacks, abilities, and especially high-speed riding on Hound.

At maximum Momentum, her next basic ability becomes **Redlined** and consumes the meter. Mounted actions are basic abilities for this purpose.

After consuming maximum Momentum, her next basic attack against an enemy Vanguard becomes **Roadhouse**: a lunging strike with bonus physical damage based partly on the target's missing Health and potentially bonus-Health scaling. She cannot basic-attack while mounted (Combat Bible §56), so Roadhouse is retained and spent after she leaves the ride state.

**Momentum accrues by distance travelled**, mounted or on foot, with attacks and abilities contributing separately. A stationary Raska builds none, including while mounted. Because ride Movement Speed is set rather than added (§56), she covers far more ground per second while riding and therefore builds Momentum proportionally faster — "especially high-speed riding" follows from the distance basis and must **not** be double-counted with an additional riding multiplier.

## Q — Breakneck

Short aggressive lunge/hook interaction.

A second movement through the target creates a more violent follow-through. Redlined behavior emphasizes heavy impact, armor cracking, and/or knock behavior.

## W — Countersteer

Directional defensive brace.

A successful block/mitigation window creates a counter opportunity. Redlined behavior upgrades the retaliation into stronger control, such as a stun.

## E — Kickstart

Hound arrives and Raska enters ride mode.

While mounted:

- very high movement speed;
- ghosting/less unit obstruction;
- wide turning behavior at high speed;
- strong Momentum generation.

Mounted actions include:

- **Sideswipe** — mounted offensive pass;
- **Powerslide** — mounted maneuver/control;
- **Bail Out** — Raska jumps free while Hound continues forward and can collide with/knock enemies.

## R — NO BRAKES

Raska enters an extreme Redline ride state.

Momentum remains full and her speed becomes enormous.

NO BRAKES grants **Unstoppable** for its duration under Combat Bible §9, Slows included; per §8, Suppression is unaffected by that immunity and remains the ordinary answer to it. The Unstoppable window ends when Last Exit fires.

Recast: **Last Exit** launches Raska and Hound separately. Raska crashes down with AoE knockup while Hound continues as a huge impact/knockback threat.

**The recast window is the full duration, and Last Exit fires automatically at expiry if she has not cast it.** The payoff is guaranteed; only its timing is hers to choose. Death before either pre-empts it: the ride ends and Hound separates per §56, but Raska's landing does not occur.

**An enemy caught by both halves takes both damage payloads but is displaced only once, by Raska's knockup.** Hound's knockback is suppressed against any target already caught centrally, so no target experiences both displacements and §9's displacement-replacement ordering never arises. Enemies struck by Hound but outside the central landing are knocked back normally.

**Identity guardrail:** Raska should feel like repeated high-speed passes and controlled recklessness, not "generic puncher with motorcycle skin."

---

# 2. Kade, Dead Reckoning

**Origin:** Iron March  
**Role identity:** Physical marksman / positional utility carry  
**Relationship:** Raska's boyfriend  
**Core fantasy:** Raska creates chaos; Kade understands geometry.

Kade is a former trajectory surveyor/engineer and precision shooter. He is calm, analytical, and comfortable with Raska rather than written as a melodramatic opposite.

He uses a long-barreled coil/precision rifle and turns displacement and positioning into damage opportunities.

**Visual language:** Sharp-featured and intent, with messy windswept brown hair and light stubble, and most readable **mid-sight** — one eye down the scope, the shot already worked out. His rifle is long and ornate: dark steel and brass fittings with **teal energy channels glowing the length of the body** and a large round optic with a teal lens. He wears a pale cream shirt with the sleeves pushed back, a dark harness and vest strapped with segmented chain-plate, articulated bracers, fingerless plated gloves and heavy belts; dark line-work tattooing runs down one forearm, and a long tattered crimson cloak trails behind him. **The teal of the weapon is the only cool note** in an otherwise crimson and pale-stone frame, which is what makes the barrel read first at any size. He is a marksman caught in the act of measuring a shot, **never** posed at rest, and the rifle is an ornate energy instrument rather than a plain military firearm.

## Passive — Moving Target

Enemies displaced by Kade **or his allies** become **Tracked**.

Kade's attacks against Tracked enemies gain increased effective range and bonus physical damage.

Repeated displacement can empower a future Dead Reckoning-style shot based partly on displacement distance.

This creates deep natural synergy with displacement-heavy teammates without explicit duo bonuses.

## Q — Throughline

Long precision/piercing shot used to establish or exploit firing geometry.

## W — Sightline

Creates a firing corridor.

Effects:

- ordinary area vision along the corridor, where the normal vision rules already permit it;
- attack-speed benefit for Kade;
- movement utility for allies using the route.

**Vision ruling (2026-09-20): Sightline grants no reveal.** It does not see into Dense Fog, does not expose Camouflaged or Invisible Vanguards, and does not grant remote targeted acquisition. The corridor's value is geometry and positioning, not information.

Sylra and Bryn are the roster's deliberate information specialists and both are capped at **presence** rather than position. A physical damage carry must not exceed them. The Vision Bible remains authoritative.

## E — Reposition

Recoil-assisted vault / positional correction tool.

Can interact with tethering or trajectory control rather than functioning as a generic free escape.

## R — Kill Corridor

Establishes a long firing lane and allows a limited series of powerful piercing shots.

Allied displacement of tracked targets can refresh or amplify his opportunities.

---

# 3. Silt, The Living Mire

**Origin:** Buried Riverlands, working regional name  
**Role identity:** Magic control mage  
**Nature:** Living sediment/earth consciousness  
**Core fantasy:** Everything returns to earth; Silt remembers where it went.

Silt formed from an ancient buried river system containing water, sand, clay, ash, ruins, remains, and accumulated memory. He is not a Fluxborn construct.

**Visual language:** A colossal mass of wet golden-ochre sediment, clay and riverbed debris that **towers over the ruins behind him**, hunched and forward-leaning on forelimbs that reach the ground in long tapering clawed digits. His surface is **layered and flaking** — overlapping plate-like sheets of dried sediment peeling away from a darker saturated interior beneath — and he is almost always caught mid-motion, flinging broad ribbons and sheets of wet material through the air and trailing rivulets where he drags. Warm sunlit ochre against pale stone. **He has no face** — no eyes, no mouth, no head shape anywhere on the mass; everything he expresses reads from posture, limb position and the direction the material is thrown. He carries two visible states: **depleted** after spending sediment (thinner limbs, crust cracked open, exposed internal structure) and **repaired** after Reclaim (bulked out, sealed, material flowing again). He is a living riverbed in motion and **never** resolves into a skull, a head or a face, and he is **not** dry stone.

## Passive — Reclaim

Damaging abilities coat enemy Vanguards in **Sandy**.

Basic-attacking a Sandy enemy consumes Sandy, tears material back into Silt, and heals him for a small amount scaling with Magic Power.

His body visibly repairs when he reclaims material.

## Q — Scattershot

Launch three balls of sand toward the target area.

The projectiles may spread, converge, or tag multiple targets. Hits apply Sandy.

The spell should not create triple healing from one target simply because multiple projectiles connect.

## W — Sink

Liquefies a target area.

After a delay:

- enemies near the center are stunned;
- enemies near the edge are slowed;
- affected Vanguards become Sandy.

## E — Sandstorm

Creates a persistent sandstorm area.

Enemies inside are **blinded**, causing basic attacks to miss, and become Sandy.

Sandy can be reapplied after being consumed, subject to an internal timing rule.

## R — Buried Alive

Creates a large churning sand/mud zone.

Enemies suffer slow + magic damage over time and become Sandy.

Consuming Sandy inside the zone triggers an eruption/damage interaction. Sandy can periodically reapply.

The end of the effect hardens/erupts for a final knockup and Sandy application.

---

# 4. Relay, The Last Conductor

**Origin:** Ancient Flux Network  
**Scale:** Roughly 3.8m machine — **under review (2026-09-20):** the approved hero art reads far larger, towering over the surrounding ruins. Art and this field disagree; the figure is a hitbox and readability decision and has not been changed here.  
**Role identity:** Utility/support-style Vanguard with open-role usage  
**Core fantasy:** An ancient mobile power station learning the modern world.

Relay was not built for war. R-01 was an ancient **Conductor**, a mobile power/maintenance machine used during the network-construction era.

Buried during the First Fracture, Relay was eventually uncovered and reactivated. His surviving directive is effectively: **restore network**.

Relay is intelligent, literal, and nurturing rather than a dumb comedy robot.

**Visual language:** A colossal machine of blocky bone-white slab plating over dark exposed mechanism, top-heavy and forward-leaning, **towering over the ruins around him**. The stencilled unit marking **R-01** is still legible on his shoulder plate. A single large ringed **amber optic** is set into his head, with smaller amber points at the chest and hand. His arms are enormous segmented manipulators ending in heavy articulated hands built for lifting and carrying, and a slender antenna mast rises from his back. **Moss and green growth have colonised him** — shoulders, joint housings, forearms and thighs — and a faded crimson expedition banner bearing a pale sigil is still tied to him; someone else left it there and he kept it. He is a maintenance and power machine that outlived the war around him, and the growth on him is old. The hands are manipulators and the mast is an antenna, **never** weapons; he carries no armament of any kind, and the moss is colonisation, not battle damage.

## Passive — Charger

Nearby allied or enemy Fluxborn deaths generate **Charge**.

Maximum Charge scales with champion level.

Jungle wildlife does **not** generate Charge.

Charge should be visible through Relay's body/battery presentation.

## Q — Rapid Discharge

Target self or ally.

Spend a percentage of current Charge to grant a finite number of **Overclocked Attacks**, increasing attack speed until the empowered attacks are consumed.

## W — Overcharge

For a short duration, Fluxborn deaths generate increased Charge.

## E — Magnetic Field

Deploy a field.

Allies inside gain slow resistance.

Enemy dashes/displacements inside the field bend or drag slightly toward its center.

This is movement manipulation, not merely another generic slow.

## R — Full Grid

Relay anchors and creates a temporary power network while current Charge drains.

Connected allies gain combat utility; allied basic attacks can reduce the cooldown of their next basic ability slightly.

Allied Fluxborn inside the network are overclocked with movement/attack-speed benefits.

Rapid Discharge becomes cheaper/more efficient while Full Grid is active.

---

# 5. Patch, The Last Hug

**Origin:** Unknown settlement near an old Flux route  
**Role identity:** Tank / disruptor  
**Form:** Tiny possessed teddy bear  
**Core fantasy:** "Don't let them get hurt."

Patch was once an ordinary child's toy near a disaster involving unstable/corrupted Flux infrastructure. The child vanished after a shelter event. Patch was later found upright.

His first known word: **"Safe?"**

The thing inside Patch remains mysterious. Patch is not written as evil; his supernatural violence is driven by an intensely protective attachment.

**Visual language:** A small stitched teddy bear standing upright, cream-tan felted fur worn thin and repaired all over with visible seams and patches. **Mismatched button eyes** — one dark, one pale — sewn on slightly crooked, with a stitched seam across the muzzle. A **red child's stitch heart marked with a pale cross** sits on his chest, a torn red scarf trails behind him, a small leather harness of straps crosses his body, and he carries a **lit lantern** in one paw whose warm amber flame is the only gentle light on him. Rearing behind and above him across the whole frame is an **enormous spectral bear of crimson energy** — burning eyes, open fanged maw, vast claws — towering over the ruins. His own face stays soft, kindly and harmless: plain stitched fabric and flat button eyes, **never** fangs, glowing eyes or a snarl. He is a toy that is still a toy, and every bit of the menace in the frame belongs to the spectral bear rather than to him.

## Passive — Haunted Attachment

The first time an enemy Vanguard damages Patch, that enemy becomes **Haunted** for a short period.

While Haunted, if they attack/damage a nearby ally instead of Patch, the spirit lashes out:

- small magic damage;
- brief slow.

Each enemy has an individual cooldown.

Gameplay sentence: **You started this with me. Finish it with me.**

## Q — Bear Hug

Short leap/grab/attach.

Patch heavily slows the target and is dragged with them.

Recast releases and throws Patch backward, creating a slight opposite-direction stumble on the target.

## W — Play Dead

Patch collapses into an ordinary-looking teddy.

While collapsed:

- immovable;
- heavily damage-resistant;
- cannot act.

Enemy attacks agitate the spirit. When the state ends, a fear pulse erupts around Patch, scaling up to a cap based on how much he was attacked.

## E — Don't Leave Me

Stitched arm / spectral thread tether.

If the target moves too far away, the tether triggers a pull-back effect once.

## R — The Thing Inside

A huge spectral monstrous bear manifests around the tiny toy.

Patch gains:

- large temporary Health;
- much larger collision/body presence;
- stronger zone control;
- stronger Bear Hug;
- ability to physically push/block enemies more effectively.

The manifestation eventually collapses back into the toy.

---

# 6. Tavi, The Playmate

**Origin:** Merrin  
**Nature:** Echo  
**Role identity:** Magic assassin  
**Core fantasy:** Children's games given lethal reality.

Tavi formed from generations of repeated children's games in Merrin.

She is not stupid, and she is not cruel. That is what makes her dangerous. Somewhere in the memories she formed from, a child was taught a rule, and Tavi learned it with everything else:

> Only play with people who are playing back.

**She no longer applies it.** Not because she rejected it — she has simply concluded that everyone is playing, they just have not realised yet. Running is a move. Hiding is a move. Begging is a move she has not seen often enough to recognise, so she treats it as part of the game and waits, delighted, for the next one.

Surrender is not a move. It cannot be, because the game does not have an ending where someone asks to stop.

She has never understood why some people do not get up afterwards. She is not indifferent to it; it upsets her, in the way a rule change upsets a child. Then she finds someone else, because the alternative is that the game is over, and the game has never been over.

**Visual language:** A young girl with cream-blonde twin tails streaked pink and clipped with bunny charms, caught mid-lunge with one hand thrown out toward the viewer and her whole face open in a wide, delighted grin. Her pupils are **pink and cross-shaped**. She wears a black jacket trimmed pink and hung with plush bunny charms, pink-and-white patterned shorts, bandaging on one forearm, wrist cuffs, knee pads and chunky black-and-pink boots with bunny faces on the toes. Her ball is pink, black and white with a **bunny face whose eyes are crosses**, trailing ribbons of pink light. **The cross is her mark and it is everywhere** — stitched across her shorts, drawn on her skin, painted on her ball, and sprayed as bunny graffiti across the ruins behind her. That is the It mark from her passive, left on the world the way a child chalks a wall. Everything is sunlit: blue sky, ivy, wildflowers, bright open daylight. Keep her brightly and evenly lit with her face fully visible and her expression pure uncomplicated delight — **never** shadowed, never framed as a threat, never posed menacingly. She is a child having a wonderful time, and the horror belongs entirely to what that means for whoever she is playing with.

## Passive — You're It!

Abilities mark a target as **It**.

Tavi gains movement speed toward It.

Her next basic attack against It consumes the mark for bonus magic damage and partial cooldown restoration.

A kill can transfer the game/mark to a nearby enemy Vanguard.

## Q — Catch!

Throw a magical ball projectile.

If it hits It, the ball can return. Catching the return reduces cooldown.

## W — Hide!

Tavi briefly enters **Invisibility** and leaves an illusion behind.

Recast or reveal creates burst behavior.

If It attacks the illusion, the illusion bursts and slows.

**Vision ruling (2026-09-20): this is Invisibility, not Camouflage, and the duration is deliberately short.** Under Camouflage an enemy at assassin range would see the real Tavi regardless, so the illusion would never function as the misdirection it exists to be.

Ordinary counterplay is unchanged: Sweeper and True Sight reveal her per the Vision Bible, attacking or casting an offensive ability breaks the stealth, stealth does not confer Untargetability, and targeted projectiles already in flight still connect.

This also separates her from **Mimzi**, whose Camouflage is a longer repositioning tool rather than a burst setup. The roster now carries both stealth grades intentionally, one each.

## E — Tag!

Dash through a target.

Interacts with You're It! and can gain a recast against the marked target.

## R — Ready or Not!

Tavi briefly disappears.

Recently damaged enemies become eligible **Playmates**.

Choose one and reappear beside them for heavy missing-Health magic damage.

A successful kill may permit one reduced-damage additional cast.

---

# 7. Vera, The Last Volley

**Origin:** Merrin  
**Nature:** Echo  
**Role identity:** Ranged magic hypercarry  
**Core fantasy:** Merrin remembering how it died.

Where Tavi embodies repeated childhood play, Vera embodies Merrin's final war: firing lines, siege commands, volleys, discipline, and death.

She understands death clearly and is emotionally detached rather than naive.

Her weapon identity is an ancient repeating warbow/arc-crossbow supported by spectral firing ranks.

**Visual language:** A disciplined military figure with white-silver hair tied back with a red ribbon, a dark layered Merrin officer's coat with red lining, and an ornate repeating arc-crossbow carried like a service weapon rather than a hunting bow. Her Echo nature shows at her edges, which thin and lose colour rather than staying solid. Behind and beside her stand **spectral blue Merrin soldiers in firing ranks** — they are part of her silhouette, not background decoration, and she reads as their commander. She is the memory of a war's last organised volley, **not** a lone ranger, hunter or duellist.

## Passive — Cadence

Basic attacks stack Attack Speed.

At maximum Cadence, Vera enters **Firing Line**.

While in Firing Line, each basic attack is repeated by a spectral firing echo for partial magic damage.

Stacks decay one at a time after she stops attacking.

## Q — Range Found

Deliberate long-range shot.

The target becomes **Ranged** and Vera gains increased basic-attack range specifically against that target.

Attacking the target refreshes Cadence.

## W — Dig In

Enter a stationary firing stance.

Vera cannot move, but gains:

- increased range;
- increased Attack Speed;
- resistance to slow/displacement;
- slower Cadence decay.

## E — Break the Line

Close-range cone spectral volley.

Deals magic damage and knocks enemies back.

Consumes Cadence, creating a defensive tradeoff.

## R — The Last Volley

Immediately reach maximum Cadence and prevent its loss for the duration.

Increase attack-speed ceiling.

Every third attack causes a spectral rank to fire through the target.

Dig In becomes faster/more effective during the ultimate.

---

# 8. Korruk, The Splinterbeast

**Origin:** Shatterdeep  
**Nature:** Native biological fauna  
**Role identity:** Physical burst caster / artillery beast  
**Core fantasy:** Embed spines, fracture the target, detonate it.

Korruk is a clever six-legged predator with mineralized skeletal armor, hollow spines, and pressure organs.

He is **not** an Echo, Flux mutation, construct, hidden god, or secretly humanoid intelligence.

Hunters/miners began harvesting his species' mineralized spines. Korruk tracks stolen material and destroys the industrial storage surrounding it.

**Visual language:** A low six-legged predator roughly 1.6m long, built close to the ground and clearly load-bearing on all six limbs. Pale bone-coloured mineral plating armours his back and flanks; **crimson crystalline spines** run along his spine and are visibly hollow — they read as something that can be fired and left in a target. His anatomy must work as a real animal's: no spider silhouette, no upright stance, no humanoid proportions. He is native Shatterdeep fauna and nothing else — **not** an Echo, a Flux mutation, a construct, or a hidden intelligence in a beast's body. Other spine colours are cosmetic variants of the same animal; crimson is the canon default.

## Passive — Embedded

Damaging abilities embed **Splinters** in enemy Vanguards.

At maximum Splinters, the target becomes **Fractured**.

A qualifying ability consumes Fractured for a major physical burst.

## Q — Spineburst

Cone/cluster launch of mineral spines.

Center hits apply additional Splinters.

## W — Pressure Mine

Spit a mineral mine to a target area.

After delay/proximity trigger it erupts for physical damage, slow, and Splinters.

Fractured interaction can cause immediate detonation/knockup behavior.

## E — Rupture

Concussive pressure pulse.

Low base damage on its own, but consumes embedded Splinters/Fractured state for very high physical burst.

## R — Shatterfield

Rain waves of mineral spines into a large area.

Repeated hits apply damage and Splinters.

The final pressure wave detonates Fractured targets.

---

# 9. Torr, The Unreturned

**Origin:** Ancient Fluxborn system  
**Role identity:** Mega tank / warden  
**Core fantasy:** A lane construct that became a person and refused to return.

Torr began as an ordinary **Breaker** Fluxborn.

A catastrophic Flux Well eruption flooded his core. His body expanded, routines became thoughts, and for the first time he became self-aware.

When later destroyed, his Flux did not return to the network. It reconstructed him instead.

Torr has a curious, newborn intelligence rather than a generic robot-revolution personality.

**Visual language:** A mass of rune-carved stone plates held in orbit around a blue-violet crystalline core by visible blue Flux light. He has **no fixed skeleton**: the plates separate, rotate and reassemble as he moves and strikes, and the gaps between them are the design, not a flaw in it. Moss and mineral staining mark the plates that spent the longest in the ground. However far his fragments scatter, they always return to him. He is held together by Flux, **not** welded or bolted — never draw him as a solid statue, a suit of armour, or a seamless rock body.

## Passive — Unreturned

Torr has poor normal regeneration.

After remaining out of Vanguard combat, his core reconstructs damaged outer armor and restores a percentage of missing Health over time.

As Health falls, outer shell breaks away and the core becomes increasingly exposed.

Health thresholds can grant tenacity/movement benefits representing a less encumbered, desperate core.

## Q — Battering Mass

Massive arm slam.

Deals magic damage and pushes enemies.

If the target collides with terrain, a Spire, or another Vanguard, they are stunned.

## W — Fortify

Lock armor plates into a frontal defensive state.

Gain major frontal damage reduction at the cost of movement.

When the state ends, plates release/burst and slow nearby enemies.

## E — Anchor

Deploy a Flux anchor.

Near the anchor:

- Torr cannot be displaced;
- allies gain displacement resistance;
- enemy movement abilities crossing the field are shortened/disrupted.

Recast can rip the anchor free for a small knockup.

## R — Overcapacity

Torr intentionally destabilizes his core.

He grows enormous and gains:

- temporary maximum Health;
- increased collision/body size;
- larger Q/E presence;
- displacement immunity;
- ability to physically block enemies.

Temporary Health drains during the state.

The ultimate ends with a powerful venting knockback.

---

# 10. Marek, The Black Accord + Nix

**Origin:** Drowned Cantons / The Elsewhere  
**Role identity:** Mobile magic burst caster / warlock  
**Core fantasy:** Fight from two positions at once.

Marek was a reckless but clever Pactcraft apprentice who broke Nix out of an old binding circle and offered a pact.

Nix bit him.

That was yes.

Their pact is mutual: Marek anchors Nix in Veyra, while Nix grants Marek access to Elsewhere magic.

**Nix was the first circle he opened. It was not the last.**

The Drowned Cantons' pactcraft tradition is built on binding — careful, conservative, generations deep — because a binding circle is not a prison for something dangerous so much as an agreement about where it may stand. Marek opens them to find out what is inside. Most of what he finds cannot be bargained with, and he does not always close what he opens.

He is not cruel and he does not kill people himself. He simply works, and in the places he has worked afterwards there are fewer people than before, and the Cantons' pactbinders arrive too late with the wrong charts. He regards this as the cost of learning something nobody else was willing to learn, and he regards the pactbinders as cowards for not having learned it first.

His affection for Nix is genuine. It has never once extended to anyone standing nearby.

Nix is a non-speaking skull-masked shadow creature with its own Health and battlefield presence. Nix can be killed/banished and later reform.

**Visual language:** Marek is a lean young man with dark curly hair and violet eyes, in a tattered black-and-violet coat, with green-black witchfire gathering in his hands. **Nix** is a quadrupedal shadow creature wearing a bone-white skull mask with violet eyes burning behind it, and has a larger horned true form it assumes when fully manifested. Marek's expression is delighted, curious and entirely unbothered — **not** snarling, not menacing, not posed as a villain. Whatever stands behind or beside him should read as far worse than he does; his calm interest in it is what makes the pair frightening.

## Passive — Bound Together

If Marek and Nix both damage the same enemy within a short window, trigger **Accord** for bonus magic damage.

Per-target cooldown.

## Q — Witchfire

Fire a black-green magical bolt.

If the target was recently damaged by Nix, trigger Accord and an enhanced/explosive interaction.

## W — Hunt

Nix leaps to a target area, damages/slows, remains there, and attacks independently.

Recast recalls Nix.

## E — Cross the Chain

Marek and Nix instantly swap positions.

Both departure positions erupt for damage.

If the interaction hits the same target from both sides, it can trigger Accord.

Successful Accord interactions can reduce the cooldown.

## R — Hell on a Leash

Nix assumes a larger true form.

Benefits may include:

- more Health;
- more movement speed;
- shorter Hunt cooldown;
- two Cross the Chain charges;
- stronger Accord.

A visible chain connects Marek and Nix; enemies crossing it take damage subject to a per-enemy internal cooldown.

If Nix is killed, the ultimate ends.

---

# 11. Neris, The Tidebound

**Origin:** Drowned Cantons  
**Nature:** Wakebound  
**Role identity:** Mobile support/utility caster  
**Core fantasy:** Ride the dangerous water and bring people home.

Neris was a rescue rider who drowned while saving a ship and returned changed, riding a living storm surge.

Her kit shifts between two sea states.

**Visual language:** A rescue rider with pale blue-white hair, a dark wide-brimmed hat, and a chained coat hung with working gear including a lit lantern. **She rides a living wave rather than standing on the ground**, and the water beneath her is always in motion. Her two sea states are visually distinct: **Calm** is a low steady swell, soft light and an open silhouette; **Storm** raises the water under her, lifts her higher, darkens the palette and turns the surge aggressive. She is a rescuer who drowned and came back still working, **not** a sea witch or a drowned ghost — the water is her mount and her tool, **not** a monster she commands.

## Passive — Sea State

**Calm Waters:** emphasizes healing/support.

**Storm Waters:** emphasizes damage/offense.

## Q — Breaking Wave

Ride a living wave forward with slight steering.

Recast/end causes a crash:

- damages enemies;
- heals allies.

Calm strengthens the healing side. Storm strengthens the damage side.

## W — Little Current

Create one targetable/killable **Waterling**.

Only one may exist at a time.

**Calm:** follows an ally, periodically heals, and grants slight movement speed.

**Storm:** hunts an enemy, dealing magic damage and slowing.

Recast redirects it.

## E — Change the Weather

Switch between Calm and Storm with a cooldown.

This should not become a zero-cost spam toggle.

## R — TIDEBREAKER

Create a huge steerable wave.

Direct impact:

- damages and knocks enemy Vanguards aside;
- burst-heals allies.

The cast locks the current Sea State and leaves a corresponding trail:

**Calm — Healing Wake:** allied movement/regen utility.

**Storm — Riptide:** enemy damage-over-time + slow.

---

# 12. Moro, The Wildspark

**Origin:** Wildwood / Verdant Depths  
**Nature:** Native horned/antler-like jungle predator with Wildlight magic  
**Role identity:** Beginner-friendly magic jungler/sorcerer  
**Core fantasy:** A simple magical predator that is strongest in its own terrain.

Moro is genuinely animal, not a speaking humanoid hidden inside a beast design.

**Visual language:** A large quadruped jungle predator with a dark bark-and-fur body, branching antlers grown through with living wood, and gold-green **Wildlight** markings that run along his limbs and flanks and brighten visibly as he channels. His eyes are a steady yellow-gold. He is genuinely an animal: no armour, no saddle, no harness, no handler, no speaking humanoid hidden inside the beast design. He is intelligent and territorial, and that has to read through posture, attention and stillness rather than through anything worn or carried.

## Passive — Wild Dominion

While in **jungle terrain**, Moro gains increased percentage damage and Attack Speed.

Damaging jungle wildlife restores a small amount of Health.

No stacks or evolving jungle state are required.

Wildlight markings brighten while the passive is active.

## Q — Arc Bolt

Straightforward magic projectile.

Bonus effectiveness against jungle wildlife.

## W — Bursting Ground

Delayed target-area magical eruption.

Deals damage and slows.

Larger/more effective against jungle camps.

## E — Pounce

Leap to a target location.

Landing deals small AoE magic damage.

If an enemy Vanguard is hit, Moro gains a brief movement-speed boost.

## R — Wildstorm

Roar and create a storm around Moro for several seconds.

Deals repeated magic damage and grants movement speed.

### Intended simple patterns

**Clear:** Q / W / basic attacks.  
**Gank:** E → W → Q.  
**Fight:** E → R.

---

# 13. Qazharr, The Harbor Wolf

**Origin:** Born on a distant unnamed coast; adopted Drowned Cantons as home  
**Role identity:** Simple sustained physical fighter/duelist  
**Core fantasy:** Get into the fight, stay there, and become harder to shake off.

Qazharr was once a pirate captain. He arrived in the Cantons intending a short stay, fell in love with the ports, people, food, drinking culture, and everyday life, and abandoned piracy to remain. Of his former crew, only his first mate and master gunner Bryn stayed with him; she later rebuilt her cannon to defend their adopted harbor.

He is foreign-born but locally rooted by choice.

He carries a large foreign boarding blade / oversized naval cleaver.

**Visual language:** A big, broad, weathered sailor with long dark curly hair, a full beard, and heavy tattoo work across his arms and chest, wearing a layered salvaged coat open over the chest and hung with everyday harbour hardware — rope, rings, earrings, buckles, working knots. He carries an oversized single-edged **boarding blade** over one shoulder. His default expression is a laugh. He should read as a harbour local who used to be a pirate, **not** as a pirate captain in costume: no tricorn, no pirate-flag iconography, no coat of office, nothing theatrical.

## Passive — Sea Dog

Consecutive basic attacks against the same enemy Vanguard progressively increase Attack Speed.

Switching targets or leaving combat causes the benefit to fall off.

## Q — Heavy Hand

Empower the next attack into a heavy cleave.

Deals bonus physical damage and briefly slows.

## W — Weather It

Gain a temporary shield and tenacity.

## E — Boarding Rush

Short forward charge.

Reaching an enemy creates a shoulder-check impact and brief movement-speed benefit.

This is primarily a way in, not a free escape.

## R — NO QUARTER

Readable wind-up into a huge two-handed sweeping arc.

Deals heavy physical damage and knocks enemies violently sideways in the direction of the swing.

If the strike hits an enemy Vanguard, Qazharr enters **No Quarter** for several seconds:

- substantial Attack Speed;
- movement speed while moving toward enemy Vanguards;
- tenacity;
- basic attacks cleave for reduced secondary damage.

Takedowns extend the state slightly up to a cap.

Gameplay sentence: **big swing → go crazy.**

---

# 14. Varkesh, The Forgeheart

**Origin:** Ember Basin  
**Nature:** Forgeheart / molten-metal elemental consciousness  
**Role identity:** Medium-range magic battlemage/warden, anti-mobility  
**Core fantasy:** Sudden movement creates stress; Varkesh makes that stress harden around you.

Varkesh is not a transformed human, a construct, or a Fluxborn.

Forgehearts form through centuries of repeated molten alloy, heat, pressure, smelting, and forging until elemental consciousness emerges.

Varkesh is old and respected and has worked with Iron March engineers for decades.

**Visual language:** A roughly 3m figure — close to twice a person's height — built from dark interlocking iron plates over a molten interior, with a bright orange-white core spiral set into the chest. The seams between plates glow, and movement sheds sparks and molten fragments. A heavy cloth drape hangs at the waist. **The metal is his body, not armour worn over something else**: never draw a person, pilot, or face inside the shell. He is a Forgeheart — an elemental consciousness that formed in the Ember Basin crucibles — **not** a golem, a construct, a Fluxborn, or a transformed human.

## Passive — Stress Temper

Damaging abilities coat enemy Vanguards in **Heated Metal**.

If a Heated enemy dashes or blinks, the coating rapidly cools at the endpoint:

- magic damage;
- brief root.

The coating is consumed and uses a per-target cooldown.

## Q — Slagshot

Launch molten material to a target area.

Small AoE magic damage + Heated Metal application.

## W — Tempered Shell

Cool Varkesh's outer body into black metal.

Gain a substantial shield and resistance to slows/control while the shield holds.

When the shield breaks/expires, heat bursts outward for minor damage.

## E — Molten Ground

Create a broad medium-range strip of molten terrain.

Enemies are slowed and entering the area applies Heated Metal.

Dashing across it creates a natural Stress Temper punishment.

## R — Forge Divide

Launch a huge rolling wall/wave of molten metal.

Deals heavy magic damage, slightly pushes enemies away from the centerline, and applies Heated Metal.

At maximum distance, the wave cools into a temporary solid black-iron wall that blocks both teams.

### Recast — Shatterforge

After a short arming delay, Varkesh may detonate the cooled wall.

The wall explodes for heavy magic damage in the surrounding area, slows enemies, and applies Heated Metal.

The decision is meaningful: preserve terrain control or destroy it for immediate combat impact.

---


# 15. Angeru, The Housebreaker

**Origin:** Reed Provinces, working regional name  
**Role identity:** Extreme-difficulty physical assassin / skirmisher  
**Resource:** Focus  
**Affiliation:** None  
**Core fantasy:** Master two rival martial traditions, combine them in ways neither school permits, and use each discipline to expose the other's weaknesses.

Angeru was born in a small village caught between the influence of two dominant martial institutions: the **Blade House** and the **Veil House**. Both house names remain working names.

When a violent criminal gang began extorting settlements in the region, Angeru's village appealed to both houses for protection.

The Blade House refused to intervene because the settlement lay outside the territory it had formally sworn to protect, and intervention risked escalating its rivalry with the Veil House.

The Veil House already knew about the gang, but refused to act because doing so would expose agents involved in a larger covert operation.

Both houses had reasons.

The gang returned. The village resisted. Angeru's family died and the village was destroyed.

What stayed with him was not merely hatred of the criminals. It was the realization that both houses possessed the power to help and had built systems that allowed them to justify doing nothing.

Angeru eventually entered the Blade House and learned its sword discipline, formal combat doctrine, movement, and dueling philosophy. Later, under another identity, he penetrated the Veil House and learned concealment, assassination, misdirection, shadow techniques, and covert movement.

The traditions insist that their disciplines are incompatible.

Angeru proved otherwise.

When both houses discovered what he had done, he rejected them completely. He now uses their own teachings against the institutions themselves, seeking to dismantle their leadership, political influence, monopoly on martial knowledge, and the social order that allowed his village to disappear between their obligations.

He began by targeting leadership. That is no longer where he stops.

Angeru has concluded that an institution has no existence apart from the people who carry it: the masters who teach the doctrine, the students who will inherit it, the retired swordsmen on house pensions, the families who took house money and asked no questions, and everyone who was told what was happening in his region and decided it was not their territory. There was no council that voted to let his village die. There was a thousand small permissions, and he is collecting them.

He keeps a list. The list grows, because anyone who shelters a name on it has demonstrated which side of the question they are on.

He is not indiscriminate and he takes no pleasure in it. He is **methodical**, which is worse, and he will explain his reasoning to anyone who asks — including the person he has come for, who is usually still trying to argue when he finishes.

> **"A technique does not know which house invented it."**

He says it as a principle of swordsmanship. He means it as a statement about guilt.

**Visual language:** He wears the equipment of both houses at once, mixed and deliberately stripped of its marks. From the **Blade House**, structured formal pieces — a fitted layered cuirass, disciplined lines, the long single-edged sword carried correctly at the hip. From the **Veil House**, soft dark wrapping — forearm and shin bindings, a hood down around his shoulders, cloth cut to move silently, and the thin throwing needles of Shadow Needle. **Every house insignia has been cut out of the cloth, leaving clean empty patches where the marks were**, and that detail is the character: he took both disciplines and belongs to neither. His face is uncovered — concealment is a tool he uses, not an identity he keeps, and he intends to be recognised. When **False Body** is active, a featureless dark double stands apart from him holding the same posture. His bearing is composed, unhurried and almost courteous. Keep him calmly and evenly lit, standing straight, looking directly at whoever he has come for — he is **not** snarling, **not** crouched, **not** posed as a threat, and **not** a masked operative. The menace is that he is perfectly calm and will explain his reasoning while he works.

## Passive — No Master

Angeru uses **Focus** instead of Mana.

Focus regenerates naturally. Cleanly weaving between his two disciplines restores additional efficiency and allows expert players to remain active longer than players who simply dump six cooldowns.

Veil abilities apply **Veiled** to enemy Vanguards.

Damaging a Veiled enemy with a Blade ability consumes the mark and triggers **Execution**:

- the triggering Blade ability deals additional physical damage;
- the strike partially bypasses Armor.

Blade abilities apply **Drawn** to enemy Vanguards.

Damaging a Drawn enemy with a Veil ability consumes the mark and triggers **Vanish**:

- part of the Veil ability's Focus cost is refunded;
- Angeru gains a brief burst of Movement Speed.

Consuming either cross-discipline mark also slightly reduces the remaining cooldown of **Forsake the Schools**.

The passive rewards transitions without forcing Angeru to alternate stances when remaining in one discipline is tactically correct.

## Veil Stance

Veil is the deceptive half of the kit: shadow placement, ranged setup, rapid repositioning, target switching, and reset-driven mobility.

### Q — Shadow Needle

Throw a fast piercing blade in a line.

Deals physical damage and applies **Veiled** to enemy Vanguards.

If **False Body** is active, the shadow simultaneously throws its own Shadow Needle toward the same target point.

A Vanguard struck by both takes additional physical damage, but does not simply receive two full copies of the spell.

### W — False Body

Send a shadow to a target location.

The shadow persists for several seconds and mimics Shadow Needle.

Recast while in Veil Stance to instantly exchange positions with the shadow.

False Body persists when Angeru switches into Blade Stance, but its swap recast becomes unavailable until he returns to Veil Stance.

The shadow is simultaneously an offensive angle, escape route, bluff, and delayed return point.

### E — Black Step

Blink to an enemy unit or the active False Body.

Blinking to an enemy causes an immediate physical slash and applies **Veiled** to enemy Vanguards.

If Black Step consumes **Drawn**, it receives a substantial partial cooldown refund.

Enemy Vanguard takedowns fully reset Black Step.

## Blade Stance

Blade is the direct sword-fighting half of the kit: flowing movement through combatants, sustained melee pressure, sweeping attacks, and deliberate execution strikes.

### Q — Flowing Cut

Advance a short distance in the aimed direction and immediately perform a fast sword sweep.

Deals physical damage and applies **Drawn**.

Flowing Cut has a relatively low cooldown and may be cast during **Passing Step**, allowing fluid dash-slash sequences.

### W — Severing Arc

Perform a deliberate wide sword strike with significant reach.

The outer portion of the blade is a **sweet spot** and deals increased physical damage.

If Severing Arc consumes **Veiled**, **Execution** substantially amplifies the strike.

This is one of Angeru's primary finishing tools.

### E — Passing Step

Dash through a target enemy and emerge on the opposite side.

Passing Step may target:

- enemy Vanguards;
- Fluxborn;
- jungle wildlife.

Enemy Vanguards struck take light physical damage and become **Drawn**.

Each individual target receives a short internal lockout before it may be targeted by Passing Step again.

Enemy units function as movement infrastructure, allowing advanced players to create routes through waves, camps, and team fights.

## R — Forsake the Schools

Switch between **Veil Stance** and **Blade Stance**.

Forsake the Schools is available from the beginning of the match. Angeru does not have a traditional ultimate ability.

Both stances maintain completely independent basic-ability cooldowns.

Focus is shared between the two stances.

The switch has a short cooldown and very brief transition animation. Consuming Veiled or Drawn reduces its remaining cooldown.

False Body remains in the world through stance changes.

## Mastery curve

At low mastery, the obvious pattern is:

**Veil chooses the fight → Blade finishes it.**

At higher mastery, Blade can initiate, Veil can disengage, shadows can be left behind during extended Blade sequences, Passing Step turns units into movement paths, and the player begins tracking two full cooldown sets simultaneously.

The intended endpoint is not one solved combo. A master Angeru player improvises from:

**six ability cooldowns + Focus + marks + shadow position + stance timing + individual dash-target lockouts + enemy cooldowns + escape routes.**

His extreme difficulty is intentional. His power budget should reward execution, planning, and battlefield awareness rather than raw button count.



# 16. Sylra, The Mistwarden

**Origin:** Drowned Cantons  
**Role identity:** Magic utility / controller  
**Damage:** Magic  
**Core fantasy:** Guide allies through uncertainty, deny clean information, and turn Dense Fog into controlled territory.

Sylra was raised among the harbor pilots, rescuers, chartkeepers, and fog-guides of the Drowned Cantons.

The Cantons' seas are difficult even in clear weather. Black reefs, narrow cliff channels, violent currents, sudden storms, and heavy coastal fog can turn a familiar route lethal within minutes. Long before every harbor had reliable modern instruments, local pilots developed a practical magical tradition around tuned bells, sea-glass, resonance, moisture, and wayfinding.

The tradition is neither Pactcraft nor Wakebound magic and does not rely on Flux. It is a local craft built from generations of surviving places where sight cannot be trusted.

Sylra learned its central lesson early:

> **You do not need perfect sight to know where the safe path is.**

Her family worked the harbor routes, guiding fishing boats, merchant traffic, rescue crews, and travelers through the Cantons' worst visibility.

During a severe whiteout, a merchant convoy ignored a harbor closure rather than lose time and contracts. Several vessels struck the black reefs beyond the safe channel. Rescue craft entered after them and quickly began losing visual contact with one another.

Sylra went into the fog carrying tuned bells, lanterns, and waymarks.

She placed resonant markers between wrecks, shaped narrow banks of controlled mist to establish recognizable routes, and used sound to track movement through the white without pretending she could see what she could not.

Dozens followed those signals back toward shore.

Not everyone returned.

The disaster changed Sylra's relationship with her craft. She became one of the Cantons' most capable fog-guides, increasingly called into conditions where ordinary visibility, charts, or signaling failed.

She is protective without being sentimental about danger. Fog is not evil. Storms are not malicious. Uncertainty is simply a condition that must be respected.

What she despises is sending people into danger while pretending uncertainty does not exist.

Sylra knows **Neris** through the Cantons' rescue culture. Their methods differ naturally: Neris can ride directly into danger on living water, while Sylra creates the route that allows everyone else to move through it.

She knows **Marek** by reputation and finds his approach to risk exhausting.

**Qazharr** has followed Cantons harbor bells more times than he will admit.

The renewed activation of ancient Flux infrastructure has recently begun producing unusual pressure changes and artificial fog around several old routes. When Sylra learned that Dense Fog within the Meridian Crucible obeyed similarly rigid visibility behavior, she chose to attune.

She wants to know whether the ancient network merely learned to reproduce a natural phenomenon the Cantons understand well — or whether the Cantons have been living beside remnants of something older than they realize.

**Visual language:** A hooded harbour pilot in layered grey-blue storm cloth, with dark markings across the eyes and a heavy chained coat hung with working navigational gear — tuned bells, sea-glass, keys, small weights — and a large lit lantern carried at the hip. Her silhouette is deliberately soft-edged and wrapped, difficult to resolve cleanly at distance; that ambiguity is the character, not a readability failure, and her lantern is what fixes her position for allies. She is a pilot and a guide, **not** a mystic or a cultist: every object on her is functional wayfinding equipment, **not** occult ornament.

## Passive — Follow the Bell

When Sylra enters Dense Fog, she leaves behind a short-lived **Mist Trail** that is readable to her allies.

Allied Vanguards moving along the trail gain a brief movement-speed benefit.

If an ally follows the trail into the same Dense Fog volume, that ally also gains a temporary protective shield.

Follow the Bell improves allied movement through uncertain terrain without granting additional direct vision.

## Q — Harbor Bell

Sylra sends a resonant magical pulse in a target direction.

The first enemy Vanguard struck takes magic damage and becomes **Sounded** for a short duration.

Sounded does **not** reveal the target.

If the Sounded Vanguard is inside Dense Fog, the fog periodically emits a general **presence ping** to Sylra's team indicating that the marked enemy is somewhere inside that fog volume.

If a Sounded target enters Dense Fog while the mark remains active, presence pings begin.

The effect never reveals the exact position, model, or targetability of an enemy to observers outside the fog.

## W — Lay the Mist

Create a temporary patch of **true Dense Fog** at the target location.

The created fog follows the battleground's normal Dense Fog rules:

- enemy Vanguards inside cannot be directly seen by observers outside;
- allied vision does not transmit through the fog boundary;
- wards inside the fog provide presence information rather than exact champion vision.

Lay the Mist can cover an engage, obscure a retreat, split sightlines, hide an objective approach, or force enemies to physically enter a dangerous area if they want certainty.

## E — Waymark

Place a magical navigation beacon at a target location.

Outside Dense Fog, the Waymark provides local ward-style vision.

Inside Dense Fog, it obeys the normal fog-ward rules:

- it does not reveal exact enemy positions to allies outside the fog;
- it pings when an enemy Vanguard enters the fog;
- if an enemy Vanguard is already present when the Waymark is placed, it immediately reports enemy presence.

Allied Vanguards within the Waymark's vicinity gain a **regenerative shield**.

The shield builds while an ally remains near the beacon. If damaged or broken, it can begin rebuilding while the ally continues holding the area, subject to a tunable regeneration delay.

Waymark is intended to create a temporary **safe harbor** around objectives, sieges, defenses, and fallback positions.

## R — Through the White

Sylra releases a massive bank of controlled mist, creating a long corridor or broad advancing zone of Dense Fog for several seconds.

The ultimate follows all normal Dense Fog visibility rules.

Enemies can see the fog itself and presence sensors may warn that Vanguards are inside it, but observers outside cannot directly see or target hidden Vanguards solely because an ally or ward is present within the fog.

Through the White is used to cover:

- team advances;
- retreats;
- Flux Well rotations;
- objective entrances;
- sieges;
- large-scale fight repositioning.

It is not team invisibility.

It is battlefield-scale uncertainty.

**Gameplay sentence:** **Make the route. Make the enemy guess. Give your team the confidence to go first.**

---


# 17. Mavra, The Spillwright

**Origin:** Calder's Run, Iron March  
**Role identity:** Magic control mage / damage-over-time mage  
**Damage:** Magic  
**Core fantasy:** Turn the battlefield into a hazardous worksite, contaminate anyone caught in it, and lock them down while the reaction finishes.

Mavra was born in Calder's Run and has never had much desire to leave it.

The settlement is full of mechanics, haulers, welders, refinery crews, salvage workers, tunnel runners, and people who understand that warning labels usually exist because somebody nearby already discovered what happens when you ignore them.

Mavra grew up fascinated by the materials that kept the March functioning: coolant slurry, binding resin, industrial solvents, sealants, mineral catalysts, refinery byproducts, and the strange compounds required to maintain old Flux infrastructure.

By the time she was old enough to work, she had already been banned from two storage sheds.

She eventually became a **spillwright** — part industrial chemist, part containment specialist, part practical mage.

A spillwright is who Calder's Run calls when a freight tank ruptures, a refinery line begins leaking something nobody recognizes, an old conduit contaminates a worksite, or several individually manageable substances have begun reacting with one another in extremely unmanageable ways.

Mavra practices **reactive material sorcery**.

She combines industrial chemistry with deliberate spellwork, manipulating how reagents cure, corrode, vaporize, crystallize, bind, and destabilize.

To an academic mage, the work can look crude.

Mavra does not care.

Her mixtures work.

The important part of her craft is rarely the first substance. It is understanding what happens when several conditions overlap.

A resin that is merely sticky can become stone-hard after exposure to a catalyst. A vapor that is irritating can become violently unstable when saturated with another reagent. A manageable contaminant can become dangerous after enough magical residue accumulates in tissue.

Mavra's rule is simple:

> **Everything is safe right up until it reacts with something else.**

She knew **Raska** before the Redline became a Calder's Run folk hero.

Raska was the teenager arriving with burned gloves, damaged components, and explanations that became less convincing every time she said the word "technically."

Mavra was often the person fixing the chemical, thermal, or material consequences of those decisions.

Their friendship is built on mutual competence, relentless insults, and complete trust.

Mavra also respects **Kade**, partly because Kade labels things.

Raska does not.

During the construction and later modification of **Hound**, Mavra helped source and formulate several of the motorcycle's coolants, sealants, and heat-resistant compounds. She maintains that some of Raska's subsequent modifications violate every operating condition those materials were designed around.

Raska regards this as evidence that Mavra made them correctly.

Mavra became locally famous after the **South Sump Failure**.

An aging refinery manifold ruptured beneath a freight-residential quarter of Calder's Run. The initial mechanical failure was survivable. The chain reaction afterward was far more dangerous.

Several industrial compounds entered the drainage system simultaneously, producing toxic vapor, reactive slurry, and rapidly growing crystalline blockages through evacuation routes.

Mavra entered the affected district with a respirator, improvised reagents, and a small work crew.

Rather than chase the spreading contamination, she predicted where each reaction would propagate and deliberately caused smaller controlled failures ahead of it.

She hardened streets into barriers.

She collapsed vapor pockets before they reached occupied structures.

She redirected reactive runoff.

Finally, she converted tons of leaking slurry into a solid containment mass around the failed manifold itself.

Most of the district survived.

The refinery owners later attempted to call the event an unavoidable equipment failure.

Mavra produced the maintenance records showing that replacement of the manifold had been postponed repeatedly.

Calder's Run liked her even more after that.

Mavra has little patience for anyone who treats danger as somebody else's problem.

Machines fail. Chemicals react. Flux behaves strangely. Those facts do not offend her.

Carelessness does.

The reactivation of ancient network infrastructure across Veyra has therefore become an enormous professional concern. Modern workers are increasingly encountering old systems built from materials and energetic principles nobody fully understands.

To Mavra, the Meridian Crucible is not merely an arena.

It is the largest functioning hazardous-material site she has ever been allowed to inspect.

Her reason for attuning was straightforward:

> **Someone should probably know what the hell is leaking.**

**Visual language:** An Iron March salvage chemist with dark red-brown hair pushed back under a red band, goggles worn at the throat, and a heavy layered coat with red lining marked with a hazard sigil. Belts of canisters, sealed bottles and reagent hardware cross her body, and she carries a large pressurised dispenser rig that glows orange at its seams. Her hands and coat carry grime, burn marks and chemical staining from actual work. Her equipment is **industrial**, not arcane — labelled containers, valves, straps, gauges and pressure fittings, **not** a witch's potion belt — and she should read as someone who fixes hazardous sites for a living, **not** as an alchemist or a poisoner.

## Passive — Hazard Exposure

Mavra's damaging abilities apply **Contaminated**, dealing magic damage over time.

Additional applications refresh Contaminated and build **Exposure** up to a cap.

At maximum Exposure, an enemy Vanguard becomes **Unstable**.

Unstable represents a target whose accumulated contaminants are ready to react with Mavra's control abilities.

Triggering an Unstable interaction does **not** remove Contaminated. The damage-over-time effect continues.

Mavra's core combat loop is:

**Contaminate → destabilize → immobilize → keep cooking them.**

## Q — Caustic Line

Project a narrow stream of corrosive reagent across the ground.

Enemies struck take magic damage and become Contaminated.

The reagent remains briefly as hazardous residue.

Enemies standing in or crossing the residue continue taking magic damage and have Contaminated refreshed.

Caustic Line is Mavra's primary zoning, lane-control, and setup ability.

## W — Flash Cure

Throw a mass of reactive binding resin into a target area.

After a short delay, the resin rapidly hardens.

Enemies caught take magic damage and are **rooted**.

If an affected enemy Vanguard is **Unstable**, the contaminants violently react with the curing resin and the root becomes a **stun** instead.

Flash Cure provides reliable hard control while rewarding prior contamination setup.

## E — Pressure Leak

Rupture a pressurized canister at a target location, creating a persistent cloud of reactive chemical vapor.

Enemies inside:

- take magic damage over time;
- are slowed;
- progressively accumulate Contaminated.

At the end of the effect, the remaining pressure ruptures upward and briefly **knocks up** enemies still inside.

The impending rupture must be readable.

The enemy knows it is coming.

Mavra's job is to make leaving difficult.

## R — CODE BLACK

Designate a large area as a catastrophic containment failure and scatter volatile reaction vessels throughout it.

The vessels rupture sequentially.

Enemies remaining in the disaster zone:

- repeatedly take magic damage;
- accumulate Contaminated much more rapidly;
- suffer heavy movement restriction from the hazardous area.

During CODE BLACK, **Flash Cure** reacts much more quickly when cast inside the zone.

Unstable enemy Vanguards caught by major reaction pulses are stunned.

When the containment sequence ends, the remaining compounds collapse toward the center in a violent pressure reaction, pulling enemies slightly inward before a final concussive knockdown.

CODE BLACK is not intended as a giant instant burst spell.

It creates a place the enemy realizes they should have left several seconds ago.

**Gameplay sentence:** **The longer you stay in Mavra's workspace, the less likely you are to leave it.**

---

# Relationship and world anchors

## Raska + Kade

Established romantic couple with a comfortable working relationship.

Hound is a shared engineering achievement even if Raska calls it "my bike."

## Iron March + Ember Basin

Established allied regions with practical, longstanding industrial ties.

Raska, Kade, and Mavra all come from the Iron March. Raska and Mavra specifically help establish Calder's Run as a lived-in community rather than a single-character origin point.

## Drowned Cantons

Marek, Neris, Qazharr, Sylra, and Bryn connect to the Cantons from different cultural angles. Marek is the only one the Cantons would now describe as a problem of their own making: the tradition trained him, and he is using it to undo the thing it exists to do. Sylra adds the Cantons' harbor-pilot and fog-guidance tradition without reducing regional magic to Pactcraft or Wakebound power. Bryn was Qazharr's first mate and the only crewmate to stay when he left piracy; her defensive harbor artillery is Flux engineering, distinct from Sylra's Bellcraft.

## Merrin

Tavi and Vera are both Echoes, but their identities emerge from different repeated experiences:

- Tavi = Merrin remembering how it lived — and never learning that it stopped.
- Vera = Merrin remembering how it died.
- Oriel = Merrin remembering the light of homecoming: the everyday hope that someone will return.

## Silt + Cairn

Both arise independently from the Buried Riverlands' living landscape. Silt represents shifting accumulated sediment and Cairn the immovable stone foundations. Neither is Fluxborn; their interaction creates natural control synergy, not an exclusive pair bonus.

## Relay + Torr

Relay remembers Fluxborn primarily as workers and network tools from the old era.

Torr is a self-aware Fluxborn who became permanent.

Their interaction should eventually explore what an ancient maintenance machine thinks of a construct that became a person.


## Reed Provinces — Angeru

The **Blade House** and **Veil House** are rival martial institutions whose political and cultural influence reaches beyond simple combat schools.

Angeru mastered both disciplines and now works to destroy the houses — which he no longer distinguishes from the people who make them up. He is not indiscriminate; he is methodical, and his list keeps growing. Future characters from either house should be able to present a credible opposing case, and the strongest version of that case is someone on his list who genuinely did nothing.

The regional name **Reed Provinces** and both house names remain provisional until the wider culture and geography are developed further.

## Patch

The entity inside Patch should remain mysterious. Do not rush to explain it as Flux, an Echo, or an Elsewhere creature simply for taxonomy convenience.

# First-playable roster goal

The current 25-Vanguard roster should collectively demonstrate that Veyra supports:

- humans;
- elementals;
- Echoes;
- Fluxborn;
- ancient machines;
- possessed objects;
- Pactcraft and Elsewhere creatures;
- Wakebound;
- native non-humanoid fauna;
- living landscapes;
- Wildlight magic;
- biological animal-humanoid Bramblekin from early Flux experimentation;
- wind-elemental rescue traditions and distinct native reptilian Basin peoples;
- engineered deployable machinery and direct ally protection.

The roster should make the setting feel larger than any one explanation and should create multiple viable composition shapes without enforced role assignments.

---

# Vanguards 18–20

Cairn, Bryn, and Oriel remain full roster members alongside Vanguards 21–25. The current first-playable design target is **25 Vanguards** and can continue growing. None is an automatically post-launch tier or mandatory role. Exact numbers remain prototype tuning values.

# 18. Cairn, The River's Grasp

**Origin:** Buried Riverlands (regional name remains provisional)  
**Nature:** Riverstone / ancient-foundation elemental; not a Fluxborn construct  
**Role identity:** High-Health tank / catcher / warden  
**Damage:** Low physical impact damage; control and survivability matter more than damage  
**Core fantasy:** Catch. Hold. Protect. The river carried the world onward; Cairn stayed to hold its foundations.

Cairn emerged from the compacted riverbed, fallen bridge foundations, buried stonework, and ancient iron of the same river system that gave rise to Silt. Where Silt is shifting sediment and a memory of everything the river carried, Cairn is its stubborn, solid foundation: he remembers what it buried. The disturbance of old Flux infrastructure awakened him; Flux did **not** create him. His aim is to keep the surviving foundations of the Buried Riverlands from being destroyed again.

**Visual language:** A gigantic, visibly **asymmetrical** mass of irregular riverstone, collapsed masonry, exposed moss, embedded weathered iron, and broken chains. His outsized hooked arm / chain apparatus dominates his silhouette; the other arm is shorter and built for support and ground slams. Avoid mirrored rock plates, uniform limbs, a smooth humanoid armor suit, or a symmetrical hook. His weight and silhouette should read clearly from gameplay camera distance. Ancient bridge-ruin motifs tie him to the Buried Riverlands rather than a nautical harbor.

**Personality:** Ancient, patient, protective, and difficult to provoke. His connection to Silt is shared homeland and a contrasting relationship to change, not a sibling bond or an exclusive duo mechanic.

## Passive — Deep Foundation

Cairn has increased maximum Health. When he **successfully immobilizes** an enemy Vanguard (including by a **Pull** or **Stun**), he gains a temporary **Deep Foundation** shield based on his maximum Health. **Iron Grasp (Q) triggers this shield when its pull lands**, even though the hook does not stun. Slows alone do not trigger it. Repeated immobilizations of the **same** Vanguard within a short per-target lockout do not grant another passive shield; simultaneous control types on one target count as a single passive trigger, not separate payouts. Deep Foundation shield contributions from different eligible enemies are combined into one bounded shield, not uncapped independent shields. The passive shield and the R shield can coexist, but their **combined Cairn-generated shielding cannot exceed a configurable cap**; the Combat Bible's shield absorption and reapplication rules still apply.

## Q — Iron Grasp

Extend the giant hooked arm on its embedded chain in a telegraphed straight-line skillshot. Catch and pull the first enemy Vanguard struck toward Cairn, dealing **minor physical damage**. The hook itself does **not** stun; **its successful Pull does count as one Deep Foundation immobilization**, subject to that target's passive lockout. The windup, travel, collision, terrain blocking, range, and miss recovery should give opponents a meaningful chance to evade or intercept the engage.

## W — Crushing Hold

Briefly raise both arms, then slam the ground in a frontal area. Enemy Vanguards in the central impact are briefly **stunned**; those in the outer area are **slowed**. Damage is intentionally low. Q can set up W, but the pull is not an unavoidable guaranteed stun: W has a readable windup and can also be used to protect allies from divers.

## E — Immovable

Plant both feet and reinforce Cairn's body with layers of riverstone for a short duration. He gains substantial damage reduction and **reduces the distance he is forcibly moved by valid pulls, pushes, and knockbacks by a configurable amount** while nearby allied Vanguards receive a smaller damage-reduction benefit. This changes **displacement distance**, not whether the displacement/control lands: he is **not immune** to Pull or Knockback, does not gain Unstoppable, and can still be interrupted or affected by other valid crowd control under ordinary Combat rules. Terrain and legal-position checks still apply. Cairn moves markedly slower during the effect and can end it early. He remains susceptible to damage and normal targeting; this is **not** blanket crowd-control immunity or Untargetability.

## R — Burden of the Depths

Cairn anchors in place and drives his arms into the ground. After a clearly indicated delay, stone and buried iron erupt in an area around him. Enemies in the central zone are **stunned and pulled a short distance inward**; enemies in the outer zone are slowed. He gains **one R-shield contribution per distinct enemy Vanguard caught centrally**, based on his maximum Health, **up to an R-specific cap**. A centrally caught target's stun and pull are **one control event for passive purposes**, and **R does not grant an additional Deep Foundation passive shield for that same central target on this cast**; its R-shield contribution is the payout instead. A previous Q/W passive shield can coexist with the R shield, subject to their **shared configurable total Cairn-generated shielding cap**. Outer-zone slows do not trigger Deep Foundation. Cairn cannot move during the initial channel, and enemies have time to escape the central area before it erupts.

**Play pattern:** Land a deliberate Q, step into W range, brace with E as teammates follow, and use R to punish enemy groups during Flux Well fights. Very high Health, reliable close-area control, and ally protection are offset by low damage, very low mobility, telegraphed actions, and dependence on landing the hook. Sustained damage, percentage-Health damage, kiting, and coordinated disengagement remain counterplay.

**Roster interactions and guardrails:** Silt's persistent control areas help Cairn hold targets, while Cairn provides a frontline for Silt's casting; no named duo buff. Do not grant Cairn health-to-damage scaling merely because he stacks Health. Apply the Combat Bible's ordinary control, displacement, shield, and hit-validation rules.

---

# 19. Bryn, The Harbor Gun

**Origin:** Born beyond the Cantons on a distant, still-unnamed coast; adopted home: Drowned Cantons  
**Role identity:** Slow-firing, heavy-damage physical ranged carry / artillery marksman  
**Weapon:** Mournwake, a custom Flux cannon rebuilt from salvaged naval artillery  
**Core fantasy:** One shot counts. Position carefully, choose a target, and punish it with enormous individual hits.

Bryn was Qazharr's **first mate and master gunner** during his pirating years. When he abandoned piracy to make a home among the Drowned Cantons, the rest of the crew went their own ways; Bryn was the **only crewmate who stayed**. She chose the people of the Cantons as much as she chose her old captain, then rebuilt Mournwake with local engineering to defend the harbor. She is a practical, weathered veteran who challenges Qazharr's impulsiveness while trusting him completely. Neither Vanguard needs an exclusive duo bonus.

**Visual language:** A compact, visibly veteran gunner with short salt-and-pepper hair, a weathered/scarred face, a worn dark-teal naval coat, functional reinforcement gear, ammunition pouches, and a giant **asymmetrically carried** brass-and-iron cannon. Exposed blue-lit Flux chambers, stabilizing braces, and mechanical recoil hardware make the cannon immediately recognizable in silhouette and at in-game scale. She is a fighter with a giant engineered gun, **not** a fast-firing minigun operator.

## Passive — Every Shot Counts

Mournwake has a deliberately **slow firing cadence** and heavy individual basic-attack damage. Attack Speed helps chamber the next round but cannot reduce her attack interval below a defined data-driven floor. **Her personal attack-interval floor does not create a second Attack Speed overflow threshold:** ordinary Combat Bible overflow basic-attack damage applies only when her underlying uncapped Attack Speed exceeds the game's normal **2.5 attacks-per-second reference**, not merely when she reaches Mournwake's slower firing floor. Consecutive basic attacks on the **same enemy Vanguard** build Breach. The **third** hit consumes Breach for additional physical damage and **one** small explosive impact **behind that target**; changing targets resets the previous target's Breach. A Breach-consuming Q **upgrades/replaces** this explosion rather than adding a second one. Basic attacks remain Bryn's main damage source and interact with applicable item effects. Breach must not become an uncapped ramp or cause inappropriate recursive item/proc triggers.

## Q — Breach Round

Load a specialized armor-piercing shell to **empower the next basic attack**. It deals bonus physical damage, applies configurable percentage Armor Penetration under the Combat Bible's normal mitigation order, and creates **one** narrow explosion behind the target. **If this attack is the third consecutive hit and consumes Breach, its enhanced Q explosion replaces the passive's normal behind-target explosion**: one impact, one secondary explosion, with the Breach bonus to the direct hit applied once. This is **not two overlapping explosions**, and neither explosion recursively triggers Breach or basic-attack On-Hit effects. The enhanced explosion becomes wider and stronger. The ability may reset the chambering **animation** for responsiveness, but **never resets the actual attack cooldown or bypasses her minimum interval between attacks**.

## W — Sounding Flare

Fire an illumination shell into a target area. The initial burst briefly slows enemies it validly hits, while the flare provides temporary ordinary area vision where the **normal vision rules** permit it. When its scan overlaps **Dense Fog**, it may report that an enemy Vanguard is **present in that fog zone**—not the enemy's model, outline, exact coordinates, or direct targetability to outside observers. It does **not** thin, remove, or convert Dense Fog; grant remote targeted acquisition; bypass terrain or stealth rules; or create brush (Veyra has no traditional brush). A Vanguard must personally enter the **same fog volume** for normal direct confirmation and targeted acquisition, subject to other visibility states. Non-targeted shots aimed into fog can still collide and hit under the existing Combat/Vision rules. Sounding Flare is a champion ability, **not** an extra ward or a replacement for the dedicated vision-tool slot. The Vision Bible remains authoritative.

## E — Kickback

Fire a compressed Flux charge forward to propel Bryn a short distance **backward**, opposite the blast. The blast deals minor physical damage and briefly slows enemies in front of her. The recoil does not cross walls or reset her basic-attack timer. It can create distance from divers or allow directional repositioning, but it must not erase her vulnerability to close-range commitment.

## R — Last Broadside

Plant Mournwake's braces and channel a large Flux charge while stationary. After a pronounced, visible charge, launch **one** long-range artillery shell into a chosen area. Its delayed detonation deals massive physical damage centrally and less damage toward the outer edge; central enemies take additional damage based on missing Health. The marked impact area and delayed arrival permit escape. The ultimate goes on cooldown once fired even if it misses. It is an artillery payoff, not a permanent extension of Bryn's basic-attack range.

**Play pattern:** Deliberately land basic attacks on one target, leverage the third-hit Breach and Q, scout or slow with W, use E sparingly against divers, and place R on groups trapped by team control. She has high individual shot and burst damage, useful artillery reach, low survivability, low mobility, and a long downtime between missed shots. Her sustained output against tanks remains viable through deliberate hits and item scaling; never compensate for slow attacks by letting W bypass Dense Fog or letting Q fire without the attack-interval floor.

**Relationships:** Qazharr is her former captain and long-time friend; he creates close-range chaos while she supplies disciplined supporting fire. Sylra's fog-guidance tradition and Bryn's non-targeting presence scan have complementary uses without turning Dense Fog into shared remote sight. Her cannon uses Flux engineering, not Bellcraft, Pactcraft, or Wakebound magic.

---

# 20. Oriel, The Waiting Light

**Origin:** Merrin  
**Nature:** Memory-born stained-glass elemental / Echo; not a ghost, ordinary human, Fluxborn, or Flux-powered construct  
**Role identity:** Easy-to-learn poke / burst mage with heavy late-game Magic Power scaling  
**Damage:** Magic  
**Core fantasy:** Broken windows remember the warmth of home. Poke, scale, and illuminate.

Oriel emerged when Merrin's lingering, repeated memories of homecoming and lit windows resonated through the city's shattered glass and twisted window frames. She remembers **the promise of returning home**, not the life of one specific dead resident. When she awakened, she gathered the city's fragments into a living body and eventually left Merrin to help protect the everyday lives that other cities still have. This is a distinct strand of Merrin's memory alongside Tavi's childhood/play and Vera's siege/war; it does not overwrite their personalities or explain all elemental life through Flux.

**Visual language:** An elegant but unmistakably **nonhuman, asymmetrical** figure built from irregular stained-glass panes, fractured lead/metal framing, floating shards, and a warm luminous core. Fragment arrangements and limbs should never appear like ordinary human skin under glass armor. Gold, pale blue, violet, and jewel-colored refractions intensify visibly with spellcasting. She hovers slightly above the ground, reading as fragile, ancient, and powerful rather than heavily armored.

## Passive — Gathering Light

Each **damaging ability cast that hits an enemy Vanguard** adds **one** Gathering Light stack, irrespective of number of enemies hit. At three stacks, the passive is primed: the **next damaging ability that hits at least one enemy Vanguard Oriel can legally acquire as a target at fragment launch** consumes the stacks and sends an extra homing glass fragment toward **one such validly acquired enemy Vanguard struck by that ability**, dealing bonus **Magic-Power-scaling magic damage**. **A blind, non-targeted Q/W/R hit into Dense Fog may deal its normal damage but cannot use that hit to remotely acquire a hidden enemy for the fragment.** If a cast hits only enemies she cannot legally acquire (including enemies inside Dense Fog while she is outside), the primed passive **remains primed**; it generates no fragment and does not rebuild an extra stack from that cast. If several enemies were hit, a legally acquirable struck enemy may be chosen without targeting the hidden ones. The fragment grants **no vision or reveal** and follows ordinary targeted-projectile rules *after* a valid launch; it never creates an exception to the Vision Bible's Dense Fog, stealth or targetability rules. The consuming cast does not immediately rebuild another stack. The passive is automatic: no stance, extra resource bar, or target-mark bookkeeping for the player.

## Q — Splinter Lance

Launch one razor-sharp glass shard in a straight line. It damages the **first enemy hit**, shattering on impact. This is Oriel's primary repeatable poke spell, with a cooldown that improves through ability ranks and a high Magic Power ratio. A miss produces neither a passive stack nor passive damage.

## W — Shattered Sky

Summon fragments over a target area; after a clearly visible delay they fall, dealing magic damage and briefly slowing enemies caught within the area. This is her primary area poke and wave-clear tool. The targeting indicator gives enemies room to dodge; its slow can help her land Q without becoming a hard crowd-control combo.

## E — Mirror Veil

Surround Oriel in rotating glass panels to gain a temporary **Magic-Power-scaling self-shield** and a brief Movement Speed burst. It deals no damage and adds no hard crowd control, dash, terrain bypass, or remote vision. This is a simple timing-and-positioning survival button rather than a reliable escape from committed divers.

## R — Final Radiance

Gather floating shards into an overhead prism, then channel a huge, **telegraphed straight-line beam** of refracted light. Oriel remains stationary for the channel; all valid enemies in its path take massive magic damage with a particularly high Magic Power ratio. The beam has **no** built-in stun, execute, displacement, reset, or extra debuff. Its payoff is positioning and damage, not an unavoidable crowd-control chain. Ordinary line of sight, Dense Fog target-acquisition limits, and non-targeted spell hit validation continue to apply as defined by the Combat/Vision Bibles.

**Play pattern and scaling:** Q frequently for lane poke; W to pressure groups and waves; E to stay alive; R when opponents group or allies have set up a clear shot. Oriel scales sharply through ability ranks, Magic Power, and cooldown access, **not** through infinite passive stacks or additional range/defensive tools. Her simple kit rewards accuracy, cooldown discipline, and position. She has strong late-game damage and area poke but remains fragile, slow to escape, and punishable when Q or R misses.

**Roster interactions and guardrails:** Cairn can set up Final Radiance with his control, but no forced lane or duo buff. Tavi and Vera are fellow Merrin Echoes with distinct formative memories: play, war, and the light of homecoming. Glass/light magic and Echo consciousness are not in-match Team Flux; do not introduce automatic personal stat bonuses from accumulating Team Flux.


---

# Vanguards 21–25

The following five are full members of the current planned roster. Existing combat, targeting, Dense Fog, jungle, Flux, and open-composition rules remain authoritative.

# 21. Mimzi, The Pocket Hex

**Origin:** Thistlewick, Bramblehollow. **Species:** Tiny adult fennec-fox Bramblekin; biological animal-humanoid rather than a human, Fluxborn, or Echo. **Identity:** Stealth-oriented ranged magic proc carry/mage. **Weapon:** The Tinkertwins, two floating brass rings with tuned Flux crystals. **Primary damage:** Magic, including her basic attacks.

**Visual:** Oversized ears, fluffy tail, short muzzle, paw-like hands with opposable thumbs, digitigrade feet, enormous teal hood, long scarf, trinket satchel, and small floating gold-and-blue rings firing star-like bolts. Her whole anatomy must read as animal-like, not a human with fox ears. Her tiny visual silhouette does not confer a hidden hitbox advantage.

**Lore:** Mimzi was raised in a Flux-device repair family in Thistlewick and grew into a brilliant, mischievous trinket-maker. The Tinkertwins started as remote retrieval devices for her parents' high shelves. Exploring the ancient **Root Vaults** beneath Bramblehollow, she found surviving records that reduced her people's ancestors to experimental subject numbers. An attempt to reactivate a sealed machine almost brought the chamber down, but she escaped with salvaged components and an obsession with understanding lost Flux engineering. She left the valley to search for other old machines. Mimzi is an adult who uses technology she personally built; Bramblekin as a whole are not inherently magical or Fluxborn.

### Passive — Pocket Hex

Basic attacks deal **magic damage**, scaling primarily with Magic Power. A successful basic attack against an enemy Vanguard normally applies **one Hex stack**; a damaging basic ability applies **two**, to a two-stack preparatory cap. Hitting a target that **already has two Hex stacks** with a basic attack consumes them for extra Magic-Power-scaling magic damage. That attack does not also reapply a stack; proc damage does not recursively trigger Hex or stack it. Stacks expire after a short lapse. Three ordinary basic attacks can trigger the proc without spells.

### Q — Twinklebolt

Fire one straight-line magic projectile, damaging the first enemy hit. A Vanguard hit gains two Hex stacks, priming the next basic attack.

### W — Winkbomb

Toss a miniature trinket into a target area. After a visible delay it explodes for magic area damage and a brief **slow**, applying two Hex stacks to enemy Vanguards hit. Handles wave clear and short-lived area control. It does not create brush or change Dense Fog.

### E — Now You See Me!

Enter **Camouflage** for a short duration and gain Movement Speed. Normal close-proximity detection, Sweeper, and other valid reveals still work; starting an attack or offensive cast breaks Camouflage. The **first basic attack after emerging** deals bonus magic damage and applies two Hex stacks instead of one **only if the target was not already primed**; on an already primed target, it consumes the existing two stacks for one normal proc without simultaneously re-priming. E is not Invisibility, Untargetability, a dash, or immunity to launched projectiles. It never overrides Dense Fog acquisition rules.

### R — Grand Prank!

Throw both rings into a chosen area for a readable magic explosion and two Hex stacks on each enemy Vanguard hit. For a short empowered window, each **actual Pocket Hex proc from Mimzi's basic attack** sends one lower-damage magic bolt to a **nearby legally acquirable enemy Vanguard**. Secondary bolts cannot apply Hex, generate further bolts, recursively trigger Pocket Hex, acquire unseen Vanguards from outside Dense Fog, or grant vision. Damage depends on following up with successful attacks.

**Play/counterplay:** Prime with Q/W, trigger with magic attacks, then use Camouflage to reposition. Fragile, low hard control, vulnerable to reveal and to enemies who deny attack access. Different from Vera's stationary attack-cadence scaling and Tavi's magic assassin pattern.

---

# 22. Celandrine, The Wayrunner

**Origin:** Bramblehollow, particularly the Thistlewick courier routes. **Species:** Adult hare-lineage Bramblekin, visibly animal-humanoid. **Identity:** Mobile, sustained **physical** ranged carry/kiter. **Weapon:** Twin compact spring-loaded bolt launchers worn along her forearms.

**Visual:** Longer-legged and more athletic than Mimzi, with powerful hare hind legs and feet, swept-back ears, short tail, weatherproof courier jacket, scarf, running wraps, map-and-letter satchel, and twin mechanical springbows. An experienced, fast-talking adult professional, competitive and impatient with delays, not another miniature mage.

**Lore:** A trusted wayrunner, Celandrine carries urgent messages and supplies between root-level communities, tree-canopy homes, remote bridges, streams, and distant Bramblehollow settlements. She learned to shoot while running to survive dangerous routes. As new Flux disturbances damaged bridges and isolated villages, she set out beyond the valley to map safer alternatives and follow clues toward the Meridian Crucible. She knows Mimzi and frequently delivers warnings not to touch unstable machinery.

### Passive — Never Break Stride

Celandrine may **move during her basic-attack windup**, at reduced Movement Speed instead of completely stopping. Each successful **primary basic attack against an enemy Vanguard** grants a brief, non-stacking Movement Speed burst, refreshed by further eligible hits; attacking wildlife, Fluxborn, structures, or deployables gives no such bonus. Normal attack intervals, acquisition, range, hit validation, Crit, and On-Hit rules still apply.

### Q — Doubletime

The next **three ordinary basic attacks** have shorter windups and bonus physical damage. No extra attack events, attack-timer bypass, cloned proc events, or altered Attack Speed cap.

### W — Briar Scatter

Scatter sharpened briar pods into a nearby area. Impact causes minor physical damage and leaves a visible short-lived patch that **slows** enemies crossing it. This does not create brush or Dense Fog and does not immobilize enemies.

### E — Sidebound

A single short directional leap for dodging, chasing, or retreating. **One charge, no attacking during the leap, no attack-timer reset, no crossing impassable terrain.** Using it aggressively creates a real defensive opening.

### R — OPEN ROAD!

For a short duration, gain Movement Speed and remove Never Break Stride's windup movement penalty, allowing attacks while moving at full allowed speed. Basic attacks pierce a short, narrow line behind their **legally acquired primary target**, dealing reduced **physical** damage to secondary collisions. Secondary impacts do not independently trigger Crit, On-Hit, item procs, or extra basic attacks. The effect grants no vision or direct target acquisition across Dense Fog.

**Play/counterplay:** Keep firing and moving rather than standing in a firing stance; sustained physical damage and spacing are her strengths. Limited individual shot impact and range, one dodge, and vulnerability to hard CC distinguish her from Kade, Vera, Bryn, and Mimzi.

---

# 23. Gorraveth, The Slagmaw

**Origin:** Ember Basin. **Nature:** Native biological reptilian humanoid, **not** a Forgeheart, Fluxborn, or transformed human; “Drakari” is a provisional lineage name used on visual exploration. **Identity:** Mobile melee physical skirmisher; rapid jungle-camp clear and early map rotations. **Weapons:** Two oversized hooked slag cleavers repurposed from mining tools.

**Visual:** Huge scarred, hunched reptilian silhouette with powerful digitigrade legs, muscular arms, a heavy tail, a chipped horn and broken horn stump, one clouded eye, torn frill, badly scarred snarl, and mismatched scorched mining armor. Thick **viscous black industrial slag** clings to the cleavers and armor gaps. Flesh and injuries remain visibly biological. Rude, vicious, impatient, and terrifying. He still despises officials who knowingly send workers into lethal conditions — but that contempt stopped translating into protecting anyone a long time ago.

**Lore:** As a tracker and retrieval specialist, Gorraveth was sent with a crew into a supposedly safe Ember Basin mine to recover valuable machinery. A second collapse trapped them by molten industrial waste. He dragged the others toward safety but was sealed below. Weeks after he was declared dead, he emerged through an old vent badly burned and missing a horn. He brought the salvaged equipment to the official who had signed the false safety report. The official did not survive the conversation, and Gorraveth made no particular effort to be elsewhere afterwards.

He came out of that mine without illusions. He had dragged four people toward an exit and been sealed under the Basin for his trouble, and nothing above ground had come looking. Since then he takes contracts, delivers exactly what was agreed, and does not care what is standing nearby when he does it. He does not lie, he does not renege, and he does not help anyone who is not paying — a settlement that cannot meet his price is simply a settlement he walks past, and he will tell them so to their faces.

Renewed ancient Flux infrastructure has displaced dangerous creatures beneath the Basin; he tracked the disturbances toward the Crucible because whatever is waking down there is going to be worth a great deal to someone. Varkesh respects his hard-won survival skills and is quietly appalled at what came back out of that vent.

### Passive — No Time to Bleed

Helping finish a **whole jungle wildlife camp** gives bounded Health restoration and a temporary Movement Speed burst. **Once per cleared camp**, not per individual creature or last hit. Enemy Vanguard takedowns can grant a similar effect on a separate cooldown. No camp evolution, extra Team Flux, or requirement to occupy a designated jungle role.

### Q — Rip Through

Short directional lunge that drags both cleavers through enemies for **physical area damage**, with additional controlled effectiveness against jungle wildlife. Stops at impassable terrain; it is not a wall-crossing dash.

### W — Furnace Rake

Perform two wide, visible physical sweeps while moving at reduced speed. The second is particularly effective against wildlife. Wildlife hits restore bounded Health **up to a cap per cast**, not unlimited healing per creature.

### E — Ravine Bound

Leap to a **valid visible landing location**, causing minor physical area damage. Cross only **short designated traversable terrain gaps**, not arbitrary walls; provide neither hidden-target detection nor remote Dense Fog acquisition. Its meaningful cooldown leaves Gorraveth exposed if used to start a fight.

### R — TEAR THEM APART

Enter a short hunting frenzy, enhancing physical basic attacks against enemy Vanguards. Q/W hits on an enemy Vanguard give a brief Movement Speed boost **toward that target**, refreshed rather than stacked. Takedowns extend the remaining duration slightly, up to a cap; they do **not** reset E or provide unstoppable status, invulnerability, or an execute.

**Play/counterplay:** Clear with W/Q, rotate using one passive reward per camp, and enter skirmishes from useful angles. Poor range, no reliable hard CC, and susceptibility to kiting or coordinated focus keep him distinct from Moro's magic jungle-terrain predator. No role or lane lock.

---

# 24. Aurelisse, The Open Sky

**Origin:** High Shatterdeep canyon crossings. **Nature:** Naturally emergent wind-and-mineral-dust elemental consciousness; neither Fluxborn nor a Merrin Echo. **Identity:** Low-damage magic/utility enchanter focused on **disengage, ally protection, and positioning**. **Focus:** Brass wind chimes, floating crystals, rescue cloth.

**Visual:** A tall, unmistakably nonhuman figure made of shifting translucent air, pale mineral dust, drifting crystals, and flowing cloth; eyes glimmer through a face only suggested by currents. Chimes ring as she casts, and her form may loosen into a compact cyclone. Gentle, attentive, quietly stubborn, and suddenly forceful when travelers are endangered.

**Lore:** Centuries of wind moving through resonant minerals in Shatterdeep's steep canyons produced Aurelisse's consciousness. Travelers in storms began hearing chimes just before a passable route opened; local communities hung their own chimes along bridges and cliff paths, and she learned to take a recognizable form to guide people directly. New Flux-related disturbances have begun damaging crossings and changing storm patterns. She followed their source toward the Meridian Crucible to protect travelers. Her natural elemental origin is unrelated to Merrin's Echoes or Bramblehollow's biological experiments.

### Passive — Slipstream

When Aurelisse casts an **ally-targeted ability** on an allied Vanguard, create a short-lived, narrow visible current between her and that ally. Allied Vanguards moving **along it toward the protected ally** gain a small Movement Speed benefit. Limited width, duration, and distance; no automatic escape, global speed, or extra vision.

### Q — Crosswind

Send a broad directional gust that deals **minor magic damage** and pushes affected enemies a short distance **sideways relative to the gust's path**. Angling it correctly peels a diver; angling it poorly pushes enemies out of allied attacks. Normal legal displacement and control mitigation apply.

### W — Rising Current

Create a visibly marked updraft at a target position. After a short warning it briefly **knocks up** enemies caught inside, then leaves a short-lived current that gives allies passing through a Movement Speed bonus. An avoidable interruption, not a long lockdown.

### E — Windward

Shield **Aurelisse or one allied Vanguard**, granting that recipient a short Movement Speed burst. If the shield absorbs a meaningful amount of damage before expiry, the recipient gets **one additional brief speed burst**, not another shield. An allied target creates Slipstream.

### R — ROOM TO BREATHE

Target herself or a **nearby ally**; a circular gust pushes nearby valid enemies **outward once**. For a short period, a wind field follows that ally, granting allies inside a modest limited shield and a Movement Speed bonus **when moving outward from the field's center**. It neither repeatedly pushes nor prevents enemy re-entry or grants invulnerability. Timing and positioning matter; it may accidentally push away a target allies were finishing.

**Play/counterplay:** Protect Vera, Bryn, or Oriel during committed attacks and support Celandrine's kiting. Q's qualifying displacement can create Kade's Tracked targets. Her limited damage, angle-dependent control, and lack of a large direct heal distinguish her from Neris, Sylra, and Cairn.

---

# 25. Eudora Blackbridge, The Fieldwright

**Origin:** Iron March freight and repair routes; **specific hometown not established**. **Nature:** Older human engineer with mechanical left-forearm prosthesis. **Identity:** Medium-range physical engineer, deployable/utility specialist, prepared-position siege support. **Machine:** **Picket**, a single four-legged modular rivet-cannon-and-bulwark platform; equipment, **not** an aware companion or Fluxborn.

**Visual:** Broad-shouldered older woman, short iron-gray hair, crooked healed nose, burned cheek, weathered red work coat, welding visor, industrial prosthetic arm, heavy boots, and tool harness. Picket has four heavy piston legs, a rotating rivet cannon, and a wide directional steel shield. Two visually distinct stationary modes: **Gun Platform OR Bulwark**, never simultaneous at full strength. Eudora is direct, practical, safety-minded, and deeply hostile to negligent employers.

**Lore:** Eudora repaired bridges, tunnels, engines, and freight infrastructure across the Iron March. Picket began as mobile cover for work crews endangered by falling debris or wildlife; after a night-shift attack she fitted a heavy rivet cannon, which saved the crew but damaged the bridge they had just repaired. Newly reactivated ancient Flux systems threaten more routes, so she traveled toward the Crucible to investigate. She knows Raska, Kade, and Mavra through engineering work; she respects Relay and Torr as people, not salvage. Do **not** silently assign her Calder's Run as a birthplace.

### Passive — All Hands

When an **allied Vanguard**, including Eudora, damages an enemy Vanguard within **Picket's operating area**, Picket earns bounded **Work**, with a per-contributor internal cooldown so multi-hit spells, repeated DoT ticks, and recursive procs cannot farm unlimited Work. At a threshold, Work is consumed to repair **Picket's Health** by a capped amount, never above its maximum. No active Picket, no Work generation; the passive does not heal allies.

### Q — Drive Rivet

Launch a physical linear rivet, damaging the **first enemy hit**. A struck enemy Vanguard is briefly **Designated**, prompting Picket in Gun Platform mode to prioritize them for its next **otherwise legal** shot if within normal range and visible/acquirable under ordinary rules. Designation grants no reveal, through-wall range, or remote Dense Fog target acquisition. Without Picket, Q remains a normal skillshot.

### W — Set the Picket

Deploy **one** Picket at a nearby valid position after a readable setup. It begins in Gun Platform mode, making slow **physical** shots at legally targetable enemies, with controlled bonus damage against enemy Fluxborn. Picket has finite Health, lifetime, and attack range and can be destroyed. Eudora cannot place a second Picket while one is active; packing/redeploying requires a valid procedure and W availability. It has no independent magical vision or fog-targeting exception; the server owns targeting and damage.

### E — Raise the Bulwark

Switch the **existing** Picket into Bulwark mode, stopping the gun and raising a **directional frontal shield**. Allied Vanguards and allied Fluxborn behind the covered side gain **partial capacity-limited protection** against applicable incoming ranged attacks/projectiles; a bounded portion of intercepted damage is transferred to Picket's finite Health. Attacks from uncovered angles, melee attacks, and ground effects bypass the shield. Recast after the mode-switch cooldown to return to Gun Platform. No invulnerability or full gunfire and full defense simultaneously.

### R — MOVE THE LINE!

Unanchor the **existing operational Picket** and order it to follow a nearby selected allied Vanguard at limited speed for a short time. While mobile it provides **reduced gunfire plus reduced frontal directional protection**, facing the direction chosen at cast time; nearby allied Fluxborn gain a small temporary shield while moving with it. When the effect ends, Picket anchors at a valid location and returns to its preceding stationary mode; if destroyed the ultimate ends immediately. No second machine, permanent objective-control point, bonus Team Flux, or remote Dense Fog vision.

**Play/counterplay:** Coordinate around one protected position, switch between firepower and cover, and use R to advance a lane wave or contest a spawned objective. Flanks, destroying Picket, and rapid chases counter her. Eudora complements Relay's Fluxborn enhancements, Cairn/Aurelisse's protection, and stationary Vera/Bryn damage without exclusive named-pair bonuses.

---

## New roster relationship and system anchors

- **Bramblehollow:** Mimzi and Celandrine are distinct adult fox- and hare-lineage Bramblekin. Their people are independent biological descendants of early living-subject Flux experiments, not summoned Fluxborn or universally magical beings. Thistlewick is their living settlement and the Root Vaults are abandoned labs beneath the valley.
- **Ember Basin:** Gorraveth is a living reptilian native with severe mine injuries, **not** another Forgeheart like Varkesh. No automatic duo effect. Varkesh's disapproval is no longer about manners.
- **Shatterdeep:** Aurelisse's naturally formed wind consciousness and cliff-rescue tradition differ from Korruk's evolved predatory fauna and from the Bramblekin's experimental history.
- **Iron March:** Eudora knows Raska, Kade, and Mavra through engineering, but her hometown is not fixed. She treats Relay and Torr as conscious beings.
- **Team synergies:** Qualifying allied displacement from Aurelisse can activate Kade's existing Tracked passive; Aurelisse and Eudora provide opportunities for stationary carries to attack safely; Celandrine can exploit teammate-created space; Mimzi and Gorraveth add alternate approach angles. These are natural kit interactions, **not mandatory lanes or named duo buffs**.
- **System boundaries:** Mimzi uses normal Camouflage; her homing bolts and Eudora's Picket require valid direct acquisition. Celandrine's pierce is not separate basic attacks. Neither Gorraveth's jungle efficiency nor Eudora's machine alters the map's Flux Wells, jungle camp evolution, or team resource rules.