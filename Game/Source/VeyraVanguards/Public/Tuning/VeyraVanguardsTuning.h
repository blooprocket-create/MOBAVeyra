// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Attacks/VeyraBasicAttackTypes.h"
#include "Content/VeyraContentId.h"
#include "Progression/VeyraProgressionTypes.h"
#include "Progression/VeyraProgressionTuning.h"
#include "Stats/VeyraStatBlock.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"

#include "VeyraVanguardsTuning.generated.h"

// The Vanguards domain's tuning, bound from Game/Tuning/Vanguards.json (ADR-006 §6, ADR-008 §2). The
// schema holds every range; a 0 here only means "not loaded".

/**
 * What a Vanguard spends to cast (Combat Bible §27; ADR-008 §2). Every family is spent, regenerated and
 * refunded through the same Resource attributes; Focus has no growth per level and its own colour on
 * the HUD (ADR-031 §1).
 */
UENUM()
enum class EVeyraResourceFamily : uint8
{
	Mana,
	Focus,
};

/** Who may play a Vanguard (ADR-010 §6). */
UENUM()
enum class EVeyraVanguardAvailability : uint8
{
	/** Released: players may own and pick it. */
	Playable,
	/** For tests and development servers only; Shipping match servers refuse it. */
	Developer,
};

/** A Vanguard's body in the world. */
USTRUCT()
struct FVeyraVanguardBodyTuning
{
	GENERATED_BODY()

	UPROPERTY()
	double CapsuleRadius = 0.0;

	/** Includes the capsule's rounded ends, so never less than the radius. */
	UPROPERTY()
	double CapsuleHalfHeight = 0.0;

	UPROPERTY()
	double TurnRateDegreesPerSecond = 0.0;
};

/** The abilities in a Vanguard's slots, by content ID; each at most one. */
USTRUCT()
struct FVeyraVanguardKitTuning
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVeyraContentId> Q;

	UPROPERTY()
	TArray<FVeyraContentId> W;

	UPROPERTY()
	TArray<FVeyraContentId> E;

	UPROPERTY()
	TArray<FVeyraContentId> R;
};

/** One Vanguard (ADR-008 §2). */
USTRUCT()
struct FVeyraVanguardDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	EVeyraVanguardAvailability Availability = EVeyraVanguardAvailability::Developer;

	UPROPERTY()
	EVeyraResourceFamily Resource = EVeyraResourceFamily::Mana;

	UPROPERTY()
	FVeyraVanguardBodyTuning Body;

	/** The stats at level 1. */
	UPROPERTY()
	FVeyraStatBlock BaseStats;

	/** What each later level adds (Economy & Progression Bible §9). */
	UPROPERTY()
	FVeyraStatGrowth Growth;

	UPROPERTY()
	FVeyraBasicAttackProfile BasicAttack;

	UPROPERTY()
	FVeyraVanguardKitTuning Abilities;

	/** At most one: a rank shape from the Progression tuning, a documented exception to the standard ranks (ADR-031 §2). */
	UPROPERTY()
	TArray<FVeyraContentId> RankShape;

	/** At most one passive, from one of the passive maps. */
	UPROPERTY()
	TArray<FVeyraContentId> Passive;
};

/**
 * A passive that shields its Vanguard whenever one of its abilities immobilizes an enemy Vanguard,
 * by a Stun or a displacement (Cairn's Deep Foundation, Character Bible §18).
 */
USTRUCT()
struct FVeyraDeepFoundationTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The shield each immobilized enemy Vanguard grants; merging bounds several into one. */
	UPROPERTY()
	FVeyraShieldTuning Shield;

	/** How long the same enemy Vanguard cannot grant the shield again, in seconds. */
	UPROPERTY()
	double LockoutSeconds = 0.0;
};

/**
 * A generic passive that builds up over the hit chain, consecutive basic attacks on one enemy
 * Vanguard (ADR-008 §5, ADR-009 §5): each attack in the chain applies Status again, so a stacking
 * status gains a stack per hit, and the chain ending takes it away (Qazharr's Sea Dog, Character
 * Bible §13).
 */
USTRUCT()
struct FVeyraHitChainTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** A status ID from Abilities.json's statuses map, put on the passive's Vanguard. */
	UPROPERTY()
	FVeyraContentId Status;
};

/**
 * Oriel's Gathering Light (Character Bible §20). Each damaging ability cast that hits an enemy
 * Vanguard adds one stack. At StacksToPrime it is primed, and the next such cast consumes the stacks
 * and sends a homing fragment at one struck Vanguard she can acquire. The stacks end at death
 * (ADR-008 §9).
 */
USTRUCT()
struct FVeyraGatheringLightTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Canon gives three (§20). */
	UPROPERTY()
	int32 StacksToPrime = 0;

	/** The fragment's damage, prepared when it launches (Combat Bible §50); one amount, since a passive has no ranks. */
	UPROPERTY()
	FVeyraDamageTuning FragmentDamage;

	/** How the fragment flies: a homing projectile, which terrain does not stop (ADR-008 §9). */
	UPROPERTY()
	FVeyraAttackProjectileTuning Fragment;
};

/**
 * Bryn's Breach (Character Bible §19). Every HitsToBreach-th consecutive basic attack on the same
 * enemy Vanguard, the hit chain, consumes Breach: the attack deals BonusDamage and offers Impact, one
 * explosion behind the target. An impact of higher priority, such as Breach Round's, replaces it.
 * Neither re-enters the hit pipeline (ADR-009 §5), and changing targets starts the count again.
 */
USTRUCT()
struct FVeyraBreachTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Canon gives the third hit (§19). */
	UPROPERTY()
	int32 HitsToBreach = 0;

	/** Added to the breaching attack's own damage; one amount, since a passive has no ranks. */
	UPROPERTY()
	FVeyraDamageTuning BonusDamage;

	/** The explosion behind the target; one amount for each damage component. */
	UPROPERTY()
	FVeyraSecondaryImpactTuning Impact;
};

/** Dead Reckoning: displacement banked toward an empowered attack on a Tracked target (Roster Bible §2). */
USTRUCT()
struct FVeyraDeadReckoningTuning
{
	GENERATED_BODY()

	/** Units banked before the next attack on a Tracked target spends them. */
	UPROPERTY()
	double ThresholdUnits = 0.0;

	/** The most that bank. */
	UPROPERTY()
	double CapUnits = 0.0;

	/** The empowered attack's bonus; one amount, since a passive has no ranks. */
	UPROPERTY()
	FVeyraDamageTuning Damage;

	/** And this much more Physical Power ratio for each StepUnits spent (Roster Bible §2's "per 100 units"). */
	UPROPERTY()
	double PhysicalPowerRatioPerStep = 0.0;

	/** The units each step of the ratio counts; above 0. */
	UPROPERTY()
	double StepUnits = 0.0;
};

/**
 * Kade's Moving Target (Roster Bible §2). An enemy Vanguard that Kade or an ally displaces becomes
 * Tracked: a status from Kade that lengthens his range against it, and his attacks on it deal bonus
 * damage. The displacement also banks toward Dead Reckoning. It reads Combat's OnDisplaced; no core
 * system names it. Its data is an entry in Vanguards.json's movingTarget map.
 */
USTRUCT()
struct FVeyraMovingTargetTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** From Abilities.json's statuses: a source-relative range, so only Kade's reach grows (ADR-018 §2). */
	UPROPERTY()
	FVeyraContentId TrackedStatus;

	/** Added to his attacks on a target he has Tracked; one amount. */
	UPROPERTY()
	FVeyraDamageTuning TrackedDamage;

	UPROPERTY()
	FVeyraDeadReckoningTuning DeadReckoning;
};

/** A Redlined form (Roster Bible §1): the slot, and the ability it runs at full Momentum. */
USTRUCT()
struct FVeyraRedlinedTuning
{
	GENERATED_BODY()

	/** Q, W or E. */
	UPROPERTY()
	EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;

	UPROPERTY()
	FVeyraContentId Ability;
};

/** Roadhouse (Roster Bible §1): the lunging attack that follows a Redlined cast. */
USTRUCT()
struct FVeyraRoadhouseTuning
{
	GENERATED_BODY()

	/** From Abilities.json's statuses, held on her while Roadhouse waits: its longer reach. */
	UPROPERTY()
	FVeyraContentId ReachStatus;

	/** Its bonus damage; one amount. */
	UPROPERTY()
	FVeyraDamageTuning Damage;

	/** And this share of the target's missing Health; at least 0. */
	UPROPERTY()
	double MissingHealthRatio = 0.0;

	/** And this share of her bonus Health; at least 0. */
	UPROPERTY()
	double BonusHealthRatio = 0.0;

	/** The lunge's units per second; above 0. */
	UPROPERTY()
	double LungeSpeed = 0.0;
};

/**
 * Raska's Redline (Roster Bible §1): Momentum, the stacks of a Counter status from her, builds with the
 * distance she moves herself, her attacks and her casts. At full, her next basic ability runs its
 * Redlined form and spends the meter; her next basic attack on an enemy Vanguard after that is Roadhouse.
 * Statuses may hold the meter full, as NO BRAKES does. Its data is an entry in Vanguards.json's
 * momentum map.
 */
USTRUCT()
struct FVeyraMomentumTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** From Abilities.json's statuses: a Stacking Counter whose stacks are Momentum and whose most stacks are full. */
	UPROPERTY()
	FVeyraContentId Meter;

	/** The units she moves herself for each point; above 0. */
	UPROPERTY()
	double UnitsPerPoint = 0.0;

	UPROPERTY()
	int32 PointsPerAttack = 0;

	UPROPERTY()
	int32 PointsPerCast = 0;

	/** How often her own movement is counted, in seconds; above 0. */
	UPROPERTY()
	double SampleSeconds = 0.0;

	/** The Redlined forms her slots hold at full. */
	UPROPERTY()
	TArray<FVeyraRedlinedTuning> Redlined;

	/** From Abilities.json's statuses: while she has any, the meter stays full. */
	UPROPERTY()
	TArray<FVeyraContentId> HoldFullStatuses;

	UPROPERTY()
	FVeyraRoadhouseTuning Roadhouse;
};

/**
 * Gorraveth's No Time to Bleed (Roster Bible §23): helping clear a whole jungle camp restores a share
 * of his Max Health and more, and gives statuses such as a burst of Movement Speed, once per cleared
 * camp; an enemy Vanguard takedown gives the same on its own cooldown. It reads World's
 * OnCampCleared and Combat's OnDeath. Its data is an entry in Vanguards.json's campReward map.
 */
USTRUCT()
struct FVeyraCampRewardTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Of his Max Health, restored; at least 0. */
	UPROPERTY()
	double HealthRatio = 0.0;

	/** And this much more; at least 0. */
	UPROPERTY()
	double HealthAmount = 0.0;

	/** From Abilities.json's statuses, put on him. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;

	/** Seconds between two rewards from takedowns; at least 0. */
	UPROPERTY()
	double TakedownCooldownSeconds = 0.0;
};

/**
 * Patch's Haunted Attachment (Roster Bible §5): an enemy Vanguard that damages Patch is Haunted for a
 * while, once per its own cooldown; a Haunted enemy that damages one of his allied Vanguards near him
 * instead is lashed by the spirit, and the Haunt is spent. It reads Combat's OnHostileDamage; no core
 * system names it. Its data is an entry in Vanguards.json's haunt map.
 */
USTRUCT()
struct FVeyraHauntTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** From Abilities.json's statuses, put on the enemy by the owner; its time is the Haunt's. */
	UPROPERTY()
	FVeyraContentId HauntStatus;

	/** How long before the same enemy can be Haunted again, in seconds; at least 0. */
	UPROPERTY()
	double PerEnemyCooldownSeconds = 0.0;

	/** Units between the owner's centre and the damaged ally's for the spirit to lash out; above 0. */
	UPROPERTY()
	double AllyRadius = 0.0;

	/** The lash's proc damage; one amount. */
	UPROPERTY()
	FVeyraDamageTuning Damage;

	/** From Abilities.json's statuses, put on the lashed enemy, such as a brief Slow. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;
};

/** Firing Line: at full Cadence, a spectral echo repeats each attack (Roster Bible §7). */
USTRUCT()
struct FVeyraFiringLineTuning
{
	GENERATED_BODY()

	/** Seconds after the attack's Commit before its echo fires. */
	UPROPERTY()
	double DelaySeconds = 0.0;

	/** The echo deals this fraction of the attack's own damage... */
	UPROPERTY()
	double AttackDamageFraction = 0.0;

	/** ...as this type, plus this amount and these ratios; one amount. It is proc damage with no On-Hit (Combat Bible §16). */
	UPROPERTY()
	FVeyraDamageTuning Damage;

	UPROPERTY()
	FVeyraAttackProjectileTuning Projectile;
};

/** The Last Volley's spectral rank: every Nth attack fires an area from Vera through her target (Roster Bible §7). */
USTRUCT()
struct FVeyraSpectralRankTuning
{
	GENERATED_BODY()

	UPROPERTY()
	int32 EveryAttacks = 0;

	/** From Vera toward the target: a rectangle runs through it. */
	UPROPERTY()
	FVeyraShape Shape;

	/** One amount each. */
	UPROPERTY()
	TArray<FVeyraDamageTuning> Damage;
};

/**
 * Vera's Cadence (Roster Bible §7). Each basic attack adds a stack of its Attack Speed status, which
 * decays one stack at a time once she stops (ADR-018 §2). At full stacks she is in Firing Line.
 * While her ultimate's status lasts, Cadence is full and cannot fall, and every Nth attack fires the
 * spectral rank. Its data is an entry in Vanguards.json's cadence map.
 */
USTRUCT()
struct FVeyraCadenceTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** From Abilities.json's statuses: a Stacking Attack Speed that decays one stack at a time. */
	UPROPERTY()
	FVeyraContentId Status;

	/** An attack on a target that has this status from Vera adds a second stack (Range Found's Ranged). */
	UPROPERTY()
	FVeyraContentId ExtraStackOn;

	/** While Vera has this status, her stacks decay this many times more slowly (Dig In). */
	UPROPERTY()
	FVeyraContentId SteadyStatus;

	UPROPERTY()
	double SteadyDecayMultiplier = 1.0;

	/** While Vera has this status, Cadence is full and cannot fall (The Last Volley). */
	UPROPERTY()
	FVeyraContentId FullStatus;

	UPROPERTY()
	FVeyraFiringLineTuning FiringLine;

	/** Fires while FullStatus lasts. */
	UPROPERTY()
	FVeyraSpectralRankTuning SpectralRank;
};

/** The first attack out of a Camouflage: bonus damage, and a full mark on a target not yet primed (Roster Bible §21). */
USTRUCT()
struct FVeyraEmergenceTuning
{
	GENERATED_BODY()

	/** The Camouflage it follows, from Abilities.json. */
	UPROPERTY()
	FVeyraContentId Status;

	/** How long after the Camouflage ends the first attack still counts. */
	UPROPERTY()
	double WindowSeconds = 0.0;

	/** One amount. */
	UPROPERTY()
	FVeyraDamageTuning BonusDamage;
};

/** After an ability commits, for a while each proc also sends a lesser bolt at a nearby enemy Vanguard (Grand Prank!). */
USTRUCT()
struct FVeyraProcBoltTuning
{
	GENERATED_BODY()

	/** The ability whose commit opens the window. */
	UPROPERTY()
	FVeyraContentId Ability;

	UPROPERTY()
	double WindowSeconds = 0.0;

	/** How near the proc's target the bolt's target stands, from the owner's reach of it. */
	UPROPERTY()
	double Radius = 0.0;

	/** One amount; proc damage that marks nothing and sends nothing. */
	UPROPERTY()
	FVeyraDamageTuning Damage;

	UPROPERTY()
	FVeyraAttackProjectileTuning Projectile;
};

/**
 * A mark-and-proc passive (ADR-008 §5; Roster Bible §21's Pocket Hex): each basic attack on an enemy
 * Vanguard adds a stack of the mark, from its owner; one that finds the mark at its cap spends it for
 * proc damage and adds none. Abilities add their own stacks through their statuses. Its data is an
 * entry in Vanguards.json's markProc map.
 */
USTRUCT()
struct FVeyraMarkProcTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** From Abilities.json's statuses: a Stacking mark, whose most stacks prime the proc. */
	UPROPERTY()
	FVeyraContentId Mark;

	/** One amount, and this much more for each Level past the first. */
	UPROPERTY()
	FVeyraDamageTuning ProcDamage;

	UPROPERTY()
	double ProcDamagePerLevel = 0.0;

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraEmergenceTuning> Emergence;

	/** At most one. */
	UPROPERTY()
	TArray<FVeyraProcBoltTuning> ProcBolts;
};

/**
 * Moro's Wild Dominion (Roster Bible §12; ADR-026 §5): while its owner stands on jungle terrain it holds
 * the passive's statuses, given again at each check; damage it deals to wildlife restores a share of
 * that damage as Health. No stacks and no jungle state. Its data is an entry in Vanguards.json's
 * wildDominion map.
 */
USTRUCT()
struct FVeyraWildDominionTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** From Abilities.json's statuses, put on the owner at each check it stands in the jungle; each outlasts a check. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;

	/** Seconds between checks; above 0. */
	UPROPERTY()
	double CheckSeconds = 0.0;

	/** Of the Health its damage takes from wildlife, the share it restores to the owner; from 0 to 1. */
	UPROPERTY()
	double WildlifeHealFraction = 0.0;
};

/**
 * A passive made wholly of the statuses its kit applies and the reactions to them (ADR-026 §1–§2), as
 * Korruk's Embedded: Splinters build to Fractured, which his abilities detonate. It names those
 * statuses, which must exist, for its description and checks, and runs nothing of its own. Its data
 * is an entry in Vanguards.json's kitStatuses map.
 */
USTRUCT()
struct FVeyraKitStatusesTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** From Abilities.json's statuses; at least one. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;
};

/**
 * Celandrine's Never Break Stride (Roster Bible §22; ADR-027 §1, §8): the share of her Movement Speed she
 * keeps through a basic attack's windup, and the statuses each primary basic attack that lands on an
 * enemy Vanguard gives her. Its data is an entry in Vanguards.json's attackStride map.
 */
USTRUCT()
struct FVeyraAttackStrideTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The share of her Movement Speed she keeps while a basic attack winds up; above 0, at most 1. */
	UPROPERTY()
	double WindupShare = 0.0;

	/** From Abilities.json's statuses: given her by each primary basic attack that lands on an enemy Vanguard. */
	UPROPERTY()
	TArray<FVeyraContentId> HitStatuses;
};

/**
 * Aurelisse's Slipstream (Roster Bible §24; ADR-027 §7): each ally-targeted buff she casts at an allied
 * Vanguard leaves a short current from her toward that ally, a lingering rectangle whose statuses speed
 * her and the allied Vanguards inside it. Its data is an entry in Vanguards.json's slipstream map.
 */
USTRUCT()
struct FVeyraSlipstreamTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The current's width; above 0. */
	UPROPERTY()
	double Width = 0.0;

	/** The longest current, from her toward the ally; above 0. */
	UPROPERTY()
	double MaxLength = 0.0;

	/** How long the current lasts, in seconds; above 0. */
	UPROPERTY()
	double DurationSeconds = 0.0;

	/** Seconds between its gifts of its statuses; above 0, at most its duration. */
	UPROPERTY()
	double PulseSeconds = 0.0;

	/** From Abilities.json's statuses: given her and the allied Vanguards inside at each pulse; each outlasts a pulse. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;
};

/**
 * Silt's Reclaim (Roster Bible §3; ADR-028 §4): each of its owner's basic attacks that lands on an enemy
 * Vanguard holding the owner's mark consumes the mark and heals the owner, once per lockout per target.
 * Its data is an entry in Vanguards.json's reclaim map.
 */
USTRUCT()
struct FVeyraReclaimTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** From Abilities.json's statuses: the mark its owner's damaging abilities apply. */
	UPROPERTY()
	FVeyraContentId Mark;

	/** The heal at Level 1, and what each Level after adds; at least 0. */
	UPROPERTY()
	double HealAmount = 0.0;

	UPROPERTY()
	double HealPerLevel = 0.0;

	/** Of its owner's Magic Power, added to the heal; at least 0. */
	UPROPERTY()
	double MagicPowerRatio = 0.0;

	/** Seconds before the same target can be reclaimed from again; above 0. */
	UPROPERTY()
	double LockoutSeconds = 0.0;
};

/**
 * One discipline's mark (ADR-031 §10), as Angeru's Veiled or Drawn: the other discipline's abilities apply
 * it through their own effects; the abilities in ConsumedBy spend it.
 */
USTRUCT()
struct FVeyraDisciplineMarkTuning
{
	GENERATED_BODY()

	/** From Abilities.json's statuses: the mark. */
	UPROPERTY()
	FVeyraContentId Status;

	/** The abilities whose hit on an enemy Vanguard holding the mark spends it. */
	UPROPERTY()
	TArray<FVeyraContentId> ConsumedBy;

	/** The extra strike as it is spent: its type, its amount at Level 1, what each Level adds, and its Physical Power ratio; 0 for none. */
	UPROPERTY()
	EVeyraDamageType DamageType = EVeyraDamageType::Physical;

	UPROPERTY()
	double DamageAmount = 0.0;

	UPROPERTY()
	double DamagePerLevel = 0.0;

	UPROPERTY()
	double PhysicalPowerRatio = 0.0;

	/** The fraction of the resistance its type meets that the strike ignores, from 0 to 1. */
	UPROPERTY()
	double Penetration = 0.0;

	/** The fraction of the spending ability's cost that comes back, from 0 to 1. */
	UPROPERTY()
	double ResourceRefund = 0.0;

	/** From Abilities.json's statuses: put on the owner as it is spent. */
	UPROPERTY()
	TArray<FVeyraContentId> CasterStatuses;
};

/** What one ability adds as it spends a mark (ADR-031 §10), as Severing Arc's stronger strike. */
USTRUCT()
struct FVeyraDisciplineBonusTuning
{
	GENERATED_BODY()

	UPROPERTY()
	FVeyraContentId Ability;

	/** Multiplies the extra strike; at least 0. */
	UPROPERTY()
	double DamageMultiplier = 1.0;

	/** The fraction of the ability's own remaining cooldown refunded, from 0 to 1. */
	UPROPERTY()
	double CooldownRefund = 0.0;
};

/**
 * Angeru's No Master (Roster Bible §15; ADR-031 §10): two disciplines' marks, each spent by the other's
 * abilities for a payoff, and a slot whose remaining cooldown each spending shortens. Its data is an
 * entry in Vanguards.json's disciplines map.
 */
USTRUCT()
struct FVeyraDisciplinesTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	TArray<FVeyraDisciplineMarkTuning> Marks;

	UPROPERTY()
	TArray<FVeyraDisciplineBonusTuning> Bonuses;

	/** The slot whose ability's remaining cooldown shortens by RefundSeconds as any mark is spent. */
	UPROPERTY()
	EVeyraAbilitySlot RefundSlot = EVeyraAbilitySlot::R;

	UPROPERTY()
	double RefundSeconds = 0.0;
};

/**
 * Tavi's You're It! (Roster Bible §6; ADR-030 §10): one enemy at a time holds its owner's mark. Its owner
 * moves faster while closing on the holder; its next basic attack on the holder spends the mark for bonus
 * magic damage and refunds CooldownRefund of Q, W and E's remaining cooldowns; and a kill of the holder
 * sends the mark to the nearest enemy Vanguard within JumpRadius. Its data is an entry in Vanguards.json's
 * quarry map.
 */
USTRUCT()
struct FVeyraQuarryTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** From Abilities.json's statuses: the mark its owner's abilities apply. */
	UPROPERTY()
	FVeyraContentId Mark;

	/** From Abilities.json's statuses: a MoveSpeed status its owner holds while closing on the holder. */
	UPROPERTY()
	FVeyraContentId ChaseStatus;

	UPROPERTY()
	double ChaseRange = 0.0;

	/** How far from straight at the holder its owner may be moving and still close on it. */
	UPROPERTY()
	double ChaseAngleDegrees = 0.0;

	UPROPERTY()
	double SampleSeconds = 0.0;

	/** The spent mark's magic damage at Level 1, what each Level adds, and its Magic Power ratio. */
	UPROPERTY()
	double DamageAmount = 0.0;

	UPROPERTY()
	double DamagePerLevel = 0.0;

	UPROPERTY()
	double MagicPowerRatio = 0.0;

	UPROPERTY()
	double CooldownRefund = 0.0;

	UPROPERTY()
	double JumpRadius = 0.0;
};

/** One of Unreturned's Health thresholds: below its fraction, its owner holds its statuses (ADR-028 §7). */
USTRUCT()
struct FVeyraUnreturnedThresholdTuning
{
	GENERATED_BODY()

	/** Of Max Health; above 0, at most 1. */
	UPROPERTY()
	double HealthFraction = 0.0;

	/** From Abilities.json's statuses; each outlasts a check. */
	UPROPERTY()
	TArray<FVeyraContentId> Statuses;
};

/**
 * Torr's Unreturned (Roster Bible §9; ADR-028 §7): out of Vanguard combat his core restores a share of
 * his missing Health each second, and below each Health threshold he holds its statuses, given again at
 * each check. Its data is an entry in Vanguards.json's unreturned map.
 */
USTRUCT()
struct FVeyraUnreturnedTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Seconds between checks; above 0. */
	UPROPERTY()
	double CheckSeconds = 0.0;

	/** Of missing Health, what it restores each second out of Vanguard combat; above 0, at most 1. */
	UPROPERTY()
	double RestoreFractionPerSecond = 0.0;

	UPROPERTY()
	TArray<FVeyraUnreturnedThresholdTuning> Thresholds;
};

USTRUCT()
struct FVeyraVanguardsTuning
{
	GENERATED_BODY()

	/** The Vanguards.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 15;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraVanguardDefinition> Vanguards;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraDeepFoundationTuning> DeepFoundation;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraHitChainTuning> HitChain;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraGatheringLightTuning> GatheringLight;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraBreachTuning> Breach;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraMovingTargetTuning> MovingTarget;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraCadenceTuning> Cadence;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraMarkProcTuning> MarkProc;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraHauntTuning> Haunt;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraCampRewardTuning> CampReward;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraMomentumTuning> Momentum;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraWildDominionTuning> WildDominion;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraKitStatusesTuning> KitStatuses;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraAttackStrideTuning> AttackStride;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraSlipstreamTuning> Slipstream;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraReclaimTuning> Reclaim;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraUnreturnedTuning> Unreturned;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraQuarryTuning> Quarry;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraDisciplinesTuning> Disciplines;
};

/** The Vanguards domain's rules for its tuning (ADR-008 §2, §5). */
namespace VeyraVanguardRules
{
	/**
	 * Problems with Tuning a schema cannot express, each a JSON pointer and a message: bodies, basic
	 * attacks, abilities the Abilities tuning does not define or whose rank lists do not suit their
	 * slot in the Vanguard's rank shape, rank shapes Progression does not define, passives no passive
	 * map defines, a passive ID in more than one map, and passive statuses the Abilities tuning does not
	 * define.
	 */
	VEYRAVANGUARDS_API TArray<FString> Validate(const FVeyraVanguardsTuning& Tuning, const FVeyraAbilitiesTuning& Abilities,
		const FVeyraProgressionTuning& Progression);

	/** The ranks Vanguard's kit takes: its rank shape from Progression, or the standard one (ADR-031 §2). */
	VEYRAVANGUARDS_API FVeyraRankShape RankShapeOf(const FVeyraVanguardDefinition& Vanguard, const FVeyraProgressionTuning& Progression);

	/** The Physical Power ratio Dead Reckoning adds for Banked units (Roster Bible §2): its ratio per step, by the steps banked. */
	VEYRAVANGUARDS_API double DeadReckoningRatio(const FVeyraDeadReckoningTuning& Reckoning, double Banked);
}
