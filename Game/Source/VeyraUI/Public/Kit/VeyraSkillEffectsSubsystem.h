// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraSkillEffectsSubsystem.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
struct FVeyraAbilitiesTuning;
struct FVeyraAbilityEffects;
struct FVeyraCombatCue;

/** What an ability's effects read from Abilities.json (ADR-072 §4), so no reach or time is written twice. */
namespace VeyraSkillEffects
{
	/**
	 * How far Ability reaches from where it starts, for a channel's effect to run along: a rectangle's length or a
	 * circle's or sector's radius (its outermost zone), or a projectile's range; 0 for an ability with neither.
	 */
	VEYRAUI_API double ReachOf(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability);

	/** How long after Ability commits its area lands, in seconds; 0 for an ability whose area lands as it commits, or that has none. */
	VEYRAUI_API double LandingDelayOf(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability);

	/** Where a commit effect plays: at its caster (From), or at the ground point it was aimed at, its height and all. */
	VEYRAUI_API FVector CommitPlacement(const FVector& From, const FVector& Aimed, bool bAtTarget);
}

/** Where and when this machine last drew a cast's projectile (ADR-072 §4), to match the server's end of it. */
struct FVeyraSeenProjectile
{
	/** In the server's time. */
	double SeenAt = 0.0;
	FVector At = FVector::ZeroVector;
	/** Units a second. */
	double Speed = 0.0;
	FLinearColor Color = FLinearColor::White;
};

namespace VeyraSkillEffects
{
	/**
	 * Whether the server's end of a projectile at EndedAt, learnt at Now, is one this machine saw coming: drawn within
	 * WindowSeconds, and ending no farther from where it was drawn than it flies in that window (the end and the drawing
	 * may each run ahead of the other).
	 */
	VEYRAUI_API bool SawItEnd(const FVeyraSeenProjectile& Seen, const FVector& EndedAt, double Now, double WindowSeconds);
}

/**
 * Shows an ability's own effects, stage by stage, on every machine someone watches (ADR-072 §4): its windup's on its
 * caster's bones while the cast holds it, its channel's along the cast's direction, its commit's in place of the shared
 * flash, its projectiles' trail and where they end, and where a delayed area lands. Its recipes are the kit
 * presentation settings' AbilityEffects. It reads only the cues and the cast state every client receives, and decides
 * nothing: an impact shows where a flight ended or an area landed, never whether it hit.
 */
UCLASS()
class VEYRAUI_API UVeyraSkillEffectsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Ends the stages whose casts no longer hold them and lands the areas due. Its tick calls this; tests call it directly. */
	void Refresh();

	/** Shows a cue's stage: a windup begins, or a cast commits and its area will land. The cue subsystem calls this. */
	void NoteCue(const FVeyraCombatCue& Cue);

	/** Ability's own commit effect, its scale and whether it plays where the cast was aimed; null for the shared flash. */
	UNiagaraSystem* CommitEffectOf(const FVeyraContentId& Ability, float& OutScale, bool& bOutAtTarget) const;

	/** Ability's own trail for its projectiles and its scale; null for the shared trail. */
	UNiagaraSystem* TravelEffectOf(const FVeyraContentId& Ability, float& OutScale) const;

	/** This machine draws a projectile of Ability's cast CastId at At, flying at Speed, in Color, now. */
	void NoteProjectileDrawn(const FVeyraContentId& Ability, int32 CastId, const FVector& At, double Speed, const FLinearColor& Color);

	/** Whether Caster's windup or channel stages show now (begun by a cue, or by seeing it already held). */
	bool IsShowing(const AActor& Caster) const { return Showing.Contains(&Caster); }

	/** The windup effects pouring off Caster now, one per bone; empty while none does. */
	TArray<UNiagaraComponent*> FindWindupEffects(const AActor& Caster) const;

	/** The channel effect running from Caster now, or null. */
	UNiagaraComponent* FindChannelEffect(const AActor& Caster) const;

	/** How many delayed areas are still to land. */
	int32 GetPendingLandings() const { return Landings.Num(); }

private:
	const FVeyraAbilityEffects* EffectsOf(const FVeyraContentId& Ability) const;
	UNiagaraSystem* Loaded(const FVeyraAbilityEffects& Effects, const TCHAR* Stage) const;
	UNiagaraComponent* PlayAt(UNiagaraSystem& System, const FVector& Where, const FRotator& Facing, float Scale, const FLinearColor& Color);
	void BeginWindup(const AActor& Caster, const FVeyraContentId& Ability);
	void RefreshChannel(const AActor& Caster, const FVeyraContentId& Ability, const FVector& Direction);
	void EndStages(const AActor& Caster);

	/** Shows the stages of casts this machine first sees already held: a caster seen mid-windup or mid-channel. */
	void SeedHeldCasts();

	/** Plays an impact where the server ended a projectile, if this machine saw it coming. */
	void NoteProjectileEnd(const FVeyraCombatCue& Cue);

	bool bReady = false;
	FDelegateHandle CueHandle;

	/** Every stage's system, loaded with the settings, by ability ID and stage name ("Windup", "Commit"...). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UNiagaraSystem>> Systems;

	/** What each caster shows now: the cast it shows it for, its windup's effects and its channel's. */
	struct FShowing
	{
		FVeyraContentId Ability;
		TArray<TWeakObjectPtr<UNiagaraComponent>> Windup;
		TWeakObjectPtr<UNiagaraComponent> Channel;
	};
	TMap<TWeakObjectPtr<const AActor>, FShowing> Showing;

	/** Fades out everything Shown pours: its windup's effects and its channel's. */
	static void FadeOut(FShowing& Shown);

	/** The delayed areas still to land: where, when (in the server's time), what and in whose colour. */
	struct FLanding
	{
		FVeyraContentId Ability;
		FVector Where = FVector::ZeroVector;
		double At = 0.0;
		FLinearColor Color = FLinearColor::White;
	};
	TArray<FLanding> Landings;

	/** The cast projectiles drawn lately, by ability and cast, until their end comes or their window passes. */
	TMap<TPair<FName, int32>, FVeyraSeenProjectile> Seen;
};
