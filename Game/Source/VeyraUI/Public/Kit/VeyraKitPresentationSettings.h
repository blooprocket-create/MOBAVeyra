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

/**
 * How an ability's cast shows beyond its body's clip (ADR-072 §4), stage by stage; a stage left unset shows what the
 * shared presentation does. Every stage is tinted by its caster's side.
 */
USTRUCT()
struct FVeyraAbilityEffects
{
	GENERATED_BODY()

	/** An ability ID from Abilities.json, compared ignoring case. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FName Ability;

	/** Looping, on WindupBones of its caster's body, from its windup until nothing holds the cast. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Windup;

	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	TArray<FName> WindupBones;

	/** Looping, from its caster along the cast's direction while it channels, its length the ability's reach. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Channel;

	/** One-shot, as it commits, in place of the shared flash: at its caster, or where it was aimed if bCommitAtTarget. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Commit;

	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	bool bCommitAtTarget = false;

	/** Looping, on each projectile of it in flight, in place of the shared trail. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FVeyraSkillEffectStage Travel;

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
	 * How long after this machine last drew a cast's projectile its server end may still show an impact there, in
	 * seconds (ADR-072 §4): the end and the projectile's removal arrive in either order, a round trip apart. A projectile
	 * that only left sight has no end, and one that ends farther than it could have flown since it was last drawn shows
	 * none, so an impact never shows what the viewer did not see heading there.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Casts", meta = (ClampMin = "0"))
	float ProjectileEndWindowSeconds = 0.0f;

	/** The user parameter a channel's effect takes its length by, in units: how far its ability reaches (ADR-072 §4). */
	UPROPERTY(Config, EditAnywhere, Category = "Casts")
	FName ChannelLengthParameter;

	/** The effects for Ability, if it has its own; null for the shared presentation's. */
	const FVeyraAbilityEffects* EffectsOf(FName Ability) const;

	/** Whether Status is drawn as a strand. */
	bool IsStrand(FName Status) const;

	/** The mark drawn for Status, if any. */
	const FVeyraStatusMark* MarkOf(FName Status) const;
};
