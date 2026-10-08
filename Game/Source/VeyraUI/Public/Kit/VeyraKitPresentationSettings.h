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

/** An ability whose cast shows its own effect in place of the shared cast flash (ADR-071 §4). */
USTRUCT()
struct FVeyraAbilityCastEffect
{
	GENERATED_BODY()

	/** An ability ID from Abilities.json, compared ignoring case. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	FName Ability;

	UPROPERTY(Config, EditAnywhere, Category = "Kit")
	TSoftObjectPtr<UNiagaraSystem> Effect;

	/** The effect's user scale. */
	UPROPERTY(Config, EditAnywhere, Category = "Kit", meta = (ClampMin = "0"))
	float Scale = 0.0f;
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

	/** The abilities whose casts show their own effects (ADR-071 §4). */
	UPROPERTY(Config, EditAnywhere, Category = "Casts")
	TArray<FVeyraAbilityCastEffect> AbilityCastEffects;

	/** The cast effect for Ability, if it has its own; null for the shared flash. */
	const FVeyraAbilityCastEffect* CastEffectOf(FName Ability) const;

	/** Whether Status is drawn as a strand. */
	bool IsStrand(FName Status) const;

	/** The mark drawn for Status, if any. */
	const FVeyraStatusMark* MarkOf(FName Status) const;
};
