// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"

#include "VeyraCompanionSubsystem.generated.h"

class AActor;
class AVeyraCompanion;
class UAbilitySystemComponent;
enum class EVeyraCompanionMode : uint8;
struct FVeyraCompanionTuning;
struct FVeyraDamageDealtEvent;
struct FVeyraDeathEvent;

/**
 * Keeps each owner's companion (ADR-034 §3). Server only. On a world-time timer for each, so a pause holds it:
 * - it forms the companion beside its owner once its owner has a living body;
 * - it banishes the companion as its owner dies, and as the companion itself is killed;
 * - it reforms the companion beside its living owner, at full Health, once its reform time has passed
 *   since it was killed: as its owner revives, or later;
 * - it keeps the companion's stats grown to its owner's Level, and its share of its owner's Magic Power;
 * - a deployed companion stands at its point, through its owner's death, until its time runs out or it is destroyed
 *   (ADR-037 §1).
 */
UCLASS()
class VEYRAABILITIES_API UVeyraCompanionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBanished, AVeyraCompanion&);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Server: Owner keeps a companion of Id from now on, one at most. It forms beside its owner once its
	 * owner has a living body. False for an unknown companion or an owner that keeps one already.
	 */
	bool Summon(UAbilitySystemComponent& Owner, const FVeyraContentId& Id);

	/**
	 * Server: Owner's summoned companion of Id, bound to Unit as Mode says (Escort or Hunt), for LifetimeSeconds
	 * (ADR-035 §5). It forms beside its owner's living body now. One of its summons already living is
	 * redirected instead, keeping its time. False for an unknown companion, an owner with no body, or an owner
	 * that keeps a companion for good.
	 */
	bool SummonFor(UAbilitySystemComponent& Owner, const FVeyraContentId& Id, EVeyraCompanionMode Mode, AActor& Unit, double LifetimeSeconds);

	/**
	 * Server: Owner's companion of Id deployed at Where, the nearest ground there, anchored and facing Facing, for
	 * LifetimeSeconds (ADR-037 §1). One already deployed and living moves there instead, keeping its Health, and its
	 * time starts again. False for an unknown companion, a time of none, or an owner that keeps a companion for good.
	 */
	bool Deploy(UAbilitySystemComponent& Owner, const FVeyraContentId& Id, const FVector& Where, const FVector& Facing, double LifetimeSeconds);

	/** Server: binds Owner's living summoned companion to Unit as Mode says, keeping its time. False without one. */
	bool Redirect(const UAbilitySystemComponent& Owner, EVeyraCompanionMode Mode, AActor& Unit);

	/** Server: Owner's companion, living or banished, or null before it forms. */
	AVeyraCompanion* Find(const UAbilitySystemComponent& Owner) const;

	/** Server: Owner's companion while it is on the battleground, or null. */
	AVeyraCompanion* FindLiving(const UAbilitySystemComponent& Owner) const;

	/** Server: keeps Owner's companion now, as its timer does. Tests call it. */
	void Keep(const UAbilitySystemComponent& Owner);

	/** A companion left the battleground: killed, or banished with its owner. */
	FOnBanished OnBanished;

private:
	struct FKept
	{
		TWeakObjectPtr<UAbilitySystemComponent> Owner;
		FVeyraContentId Id;
		TWeakObjectPtr<AVeyraCompanion> Companion;
		/** When a killed companion may reform, in world time. */
		double ReformsAt = 0.0;
		/** When a summoned companion goes for good, in world time; 0 for one kept for good (ADR-035 §5). */
		double EndsAt = 0.0;
		/** When an escort next helps its ally, in world time. */
		double NextPulseAt = 0.0;
		/** Whether it was deployed at a point, where it stands through its owner's death (ADR-037 §1). */
		bool bDeployed = false;
		FTimerHandle Timer;
	};

	FKept* FindKept(const UAbilitySystemComponent& Owner);
	const FKept* FindKept(const UAbilitySystemComponent& Owner) const;
	/** Thinks for Entry on a world-time timer, so a pause holds it. */
	void StartKeeping(FKept& Entry, const FVeyraCompanionTuning& Tuning);
	void Form(FKept& Entry, const AActor& OwnerBody);
	void FormAt(FKept& Entry, const FTransform& Where);
	void Banish(FKept& Entry);
	/** A summoned companion goes for good: banished, destroyed and no longer kept (ADR-035 §5). */
	void Dismiss(const UAbilitySystemComponent& Owner);
	/** An escort's heal and statuses for its ally, if near enough (ADR-035 §5). */
	void Pulse(FKept& Entry, AVeyraCompanion& Companion);
	void OnDeath(const FVeyraDeathEvent& Death);
	void OnDamageDealt(const FVeyraDamageDealtEvent& Dealt);

	/** Where a companion of Radius forms or reforms beside its owner's body. */
	FVector BesideOwner(const AActor& OwnerBody, double Radius) const;

	TArray<FKept> Kept;
	FDelegateHandle DeathHandle;
	FDelegateHandle DealtHandle;
};
