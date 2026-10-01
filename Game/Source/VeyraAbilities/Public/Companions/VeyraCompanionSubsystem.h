// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"

#include "VeyraCompanionSubsystem.generated.h"

class AActor;
class AVeyraCompanion;
class UAbilitySystemComponent;
struct FVeyraDeathEvent;

/**
 * Keeps each owner's companion (ADR-034 §3). Server only. On a world-time timer for each, so a pause holds it:
 * - it forms the companion beside its owner once its owner has a living body;
 * - it banishes the companion as its owner dies, and as the companion itself is killed;
 * - it reforms the companion beside its living owner, at full Health, once its reform time has passed
 *   since it was killed: as its owner revives, or later;
 * - it keeps the companion's stats grown to its owner's Level, and its share of its owner's Magic Power.
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
		FTimerHandle Timer;
	};

	FKept* FindKept(const UAbilitySystemComponent& Owner);
	const FKept* FindKept(const UAbilitySystemComponent& Owner) const;
	void Form(FKept& Entry, const AActor& OwnerBody);
	void Banish(FKept& Entry);
	void OnDeath(const FVeyraDeathEvent& Death);

	/** Where a companion of Radius forms or reforms beside its owner's body. */
	FVector BesideOwner(const AActor& OwnerBody, double Radius) const;

	TArray<FKept> Kept;
	FDelegateHandle DeathHandle;
};
