// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "VeyraKitPresentationSettings.generated.h"

class UNiagaraSystem;

/** A status drawn as a strand from the unit that applied it to the unit that holds it, as a tether (ADR-071 §2). */
USTRUCT()
struct FVeyraStatusStrand
{
	GENERATED_BODY()

	/** A status ID from Abilities.json, compared ignoring case. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FName Status;
};

/** A status shown by an effect on the unit that holds it while it lasts, as a haunting mark (ADR-071 §4). */
USTRUCT()
struct FVeyraStatusMark
{
	GENERATED_BODY()

	/** A status ID from Abilities.json, compared ignoring case. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FName Status;

	/** The effect it pours, in the side colour of the unit that applied it. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	TSoftObjectPtr<UNiagaraSystem> Effect;

	/** The effect's user scale. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit", meta = (ClampMin = "0"))
	float Scale = 0.0f;

	/** How high over the holder's feet it pours, as a share of the holder's drawn height. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit", meta = (ClampMin = "0"))
	float HeightShare = 0.0f;
};

/** One stage of a skill's effects (ADR-072 §4): its system, unset for none, and the system's user scale. */
USTRUCT()
struct FVeyraSkillEffectStage
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	TSoftObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(Config, EditAnywhere, Category = "Kit", meta = (ClampMin = "0"))
	float Scale = 0.0f;

	bool IsSet() const { return !Effect.IsNull(); }
};

/** A Vanguard's own basic attack in flight (ADR-072 §9), in place of the shared one. */
USTRUCT()
struct FVeyraVanguardAttack
{
	GENERATED_BODY()

	/** A Vanguard ID from Vanguards.json, compared ignoring case. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FName Vanguard;

	/** Looping, on each of its basic attacks' projectiles in flight, turned to face the flight. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Travel;
};

/**
 * How an ability's cast shows beyond its body's clip (ADR-072 §4), stage by stage; a stage left unset shows what the
 * shared presentation does. Every stage is tinted by its caster's side.
 */
/** How much of a cast's telegraph shows beside its own effects (ADR-075 §1). */
UENUM()
enum class EVeyraTelegraphShow : uint8
{
	/** The outline and its shaded fill. */
	Full,
	/** The outline alone, under an effect that shows the area: a big threat's safety net. */
	Outline,
	/** None: the ability's own effects show its area at its true shape. */
	None,
};

USTRUCT()
struct FVeyraAbilityEffects
{
	GENERATED_BODY()

	/** An ability ID from Abilities.json, compared ignoring case. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FName Ability;

	/**
	 * How much of its telegraph shows to every player beside its own effects (ADR-075 §1). It is kept whole wherever no
	 * effect of its shows that area at its true shape, whatever this says: a windup's, a channel's or a delayed area's
	 * needs Area (or a delayed area's Mark), a lingering area's needs Linger, a projectile's lane needs Travel. The local
	 * player's own indicator always shows, and Show Skill Outlines brings every telegraph back.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	EVeyraTelegraphShow Telegraph = EVeyraTelegraphShow::Full;

	/** Looping, on WindupBones of its caster's body, from its windup until nothing holds the cast. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Windup;

	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	TArray<FName> WindupBones;

	/**
	 * Looping, from its caster along the cast's direction while it channels, its length the ability's reach: from
	 * ChannelBone of its caster's body (the hands it pours from), or its middle when None.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Channel;

	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FName ChannelBone;

	/** One-shot, as it commits, in place of the shared flash: at its caster, or where it was aimed if bCommitAtTarget. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Commit;

	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	bool bCommitAtTarget = false;

	/** Looping, on each projectile of it in flight, in place of the shared trail, turned to face its flight. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Travel;

	/** Whether its projectiles draw no grey-box body: what flies is its Travel effect, or its tether's bone (ADR-072 §4). */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	bool bHideProjectileBody = false;

	/**
	 * A projectile that is part of its caster's body (ADR-072 §4), as a hook on its arm: TetherBone of the caster's body
	 * flies with each projectile of it, drawn where the projectile is, and when the flight ends (where the server ended
	 * it) it returns to TetherFromBone at the speed the ability pulls at, or flies at when it pulls nothing. Tether, a
	 * looping effect, runs from TetherFromBone to it the while. None for a projectile apart from its caster.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FName TetherBone;

	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FName TetherFromBone;

	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Tether;

	/** Looping, where a delayed area of it was aimed, from its commit until it lands: the mark it makes there. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Mark;

	/**
	 * Looping, flying from ArcBone of its caster's body to where a delayed area of it was aimed, from its commit until
	 * it lands, rising ArcHeight units over its middle: something thrown there.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Arc;

	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FName ArcBone;

	UPROPERTY(Config, EditAnywhere, Category = "Kit", meta = (ClampMin = "0"))
	float ArcHeight = 0.0f;

	/** Looping, over each lingering area of it while it lasts, its length and width the area's shape. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Linger;

	/**
	 * Looping on the ground over each area its cast lands in, at its true shape (ADR-075 §3): from its windup through its
	 * channel, and over each delayed area of it until it lands. Its shape (circle, sector or rectangle), radius, half-arc,
	 * length and width come from Abilities.json through the cast's own telegraph view, it is turned the cast's way, and its
	 * landing runs from 0 to 1 as the telegraph's fill would.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Area;

	/** Whether an effect of it shows the area a telegraph of Source would (ADR-075 §1). */
	bool ShowsArea(bool bCast, bool bDelayed, bool bLingering, bool bLane) const
	{
		return (bCast && Area.IsSet()) || (bDelayed && (Area.IsSet() || Mark.IsSet())) || (bLingering && Linger.IsSet()) || (bLane && Travel.IsSet());
	}

	/** One-shot, where a projectile of it ends, or where a delayed area of it lands. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Impact;
};

/**
 * How Vanguards' kits show beyond their bodies (ADR-071): tethers as strands, auras and end payloads as rings on the
 * ground, statuses as marks and casts as their own effects. Presentation, not tuning: every reach comes from
 * Abilities.json; this says only how it looks. Stored in Config/DefaultGame.ini; nothing shows while Validate finds
 * problems.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Veyra Kit Presentation"))
class VEYRAUI_API UVeyraKitPresentationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Every problem with these settings, as "Field: message"; empty when the kit presentation can use them. */
	TArray<FString> Validate() const;

	/** The strand drawn for each status in StatusStrands (ADR-071 §2). */
	UPROPERTY(Config, EditAnywhere, Category = "Strands")
	TArray<FVeyraStatusStrand> StatusStrands;

	/** Its thickness, in units, and how high over both units' feet it runs, as a share of each one's drawn height. */
	UPROPERTY(Config, EditAnywhere, Category = "Strands", meta = (ClampMin = "0"))
	float StrandThickness = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Strands", meta = (ClampMin = "0", ClampMax = "1"))
	float StrandHeightShare = 0.0f;

	/** The beads flowing along it from its holder to its source, each crossing it in StrandFlowSeconds, as StrandEffect. */
	UPROPERTY(Config, EditAnywhere, Category = "Strands", meta = (ClampMin = "0"))
	int32 StrandBeads = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Strands", meta = (ClampMin = "0"))
	float StrandFlowSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Strands")
	TSoftObjectPtr<UNiagaraSystem> StrandEffect;

	/**
	 * An aura's ring on the ground while it lasts (ADR-071 §3), AuraThickness thick in its caster's side colour, with a
	 * second ring sweeping out across it every AuraPulseSeconds so it reads as alive.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Auras", meta = (ClampMin = "0"))
	float AuraThickness = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Auras", meta = (ClampMin = "0"))
	float AuraPulseSeconds = 0.0f;

	/** An end payload's ring, spreading from its caster to its full reach over BurstSeconds as BurstEffect plays (ADR-071 §3). */
	UPROPERTY(Config, EditAnywhere, Category = "Bursts", meta = (ClampMin = "0"))
	float BurstSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Bursts", meta = (ClampMin = "0"))
	float BurstThickness = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Bursts")
	TSoftObjectPtr<UNiagaraSystem> BurstEffect;

	/** The statuses shown as marks on the units that hold them (ADR-071 §4). */
	UPROPERTY(Config, EditAnywhere, Category = "Marks")
	TArray<FVeyraStatusMark> StatusMarks;

	/** The abilities whose casts show their own effects, stage by stage (ADR-072 §4). */
	UPROPERTY(Config, EditAnywhere, Category = "Casts")
	TArray<FVeyraAbilityEffects> AbilityEffects;

	/**
	 * How a ranged basic attack shows in flight (ADR-072 §9), a Vanguard's, a minion's or a structure's: drawn in place of
	 * the grey-box body and its trail, in the attacker's side colour. Unset keeps the grey-box body.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Attacks")
	FVeyraSkillEffectStage AttackTravel;

	/** Vanguards whose basic attacks show their own effect in flight, in place of AttackTravel. */
	UPROPERTY(Config, EditAnywhere, Category = "Attacks")
	TArray<FVeyraVanguardAttack> VanguardAttacks;

	/** The Vanguard's own attack in flight; null for one with none. */
	const FVeyraVanguardAttack* VanguardAttackOf(FName Vanguard) const;

	/**
	 * How long after this machine last drew a cast's projectile its server end may still show an impact there, in
	 * seconds (ADR-072 §4): the end and the projectile's removal arrive in either order, a round trip apart. A projectile
	 * that only left sight has no end, and one that ends farther than it could have flown since it was last drawn shows
	 * none, so an impact never shows what the viewer did not see heading there.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Casts", meta = (ClampMin = "0"))
	float ProjectileEndWindowSeconds = 0.0f;

	/**
	 * The user parameters an effect that runs along something takes its length and width by, in units (ADR-072 §4): a
	 * channel's reach, a tether's span, a lingering area's shape.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Casts")
	FName LengthParameter;

	UPROPERTY(Config, EditAnywhere, Category = "Casts")
	FName WidthParameter;

	/**
	 * The user parameters an Area effect takes its shape by (ADR-075 §3), beside LengthParameter and WidthParameter: which
	 * shape (0 a circle, 1 a sector, 2 a rectangle from its origin along its direction), its radius in units, its half-arc
	 * in degrees (180 for a circle), and how far it has landed, from 0 to 1.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Casts")
	FName ShapeParameter;

	UPROPERTY(Config, EditAnywhere, Category = "Casts")
	FName RadiusParameter;

	UPROPERTY(Config, EditAnywhere, Category = "Casts")
	FName HalfArcParameter;

	UPROPERTY(Config, EditAnywhere, Category = "Casts")
	FName LandingParameter;

	/**
	 * The ground's slope under an Area effect, along its facing and across it (rise over run), which its particles take
	 * their height from, so it lies on the ground's best plane while its footprint seen from above stays its true shape
	 * (ADR-075 §3).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Casts")
	FName SlopeAlongParameter;

	UPROPERTY(Config, EditAnywhere, Category = "Casts")
	FName SlopeAcrossParameter;

	/** The effects for Ability, if it has its own; null for the shared presentation's. */
	const FVeyraAbilityEffects* EffectsOf(FName Ability) const;

	/** Whether Status is drawn as a strand. */
	bool IsStrand(FName Status) const;

	/** The mark drawn for Status, if any. */
	const FVeyraStatusMark* MarkOf(FName Status) const;
};
