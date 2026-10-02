// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Abilities/VeyraGameplayAbility.h"
#include "Content/VeyraContentId.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraEchoSubsystem.generated.h"

class AVeyraEcho;
class UAbilitySystemComponent;

/** Why an Echo ended (ADR-050 §4). */
UENUM()
enum class EVeyraEchoEnd : uint8
{
	/** It repeated as many abilities as it may. */
	Spent,
	/** Its window ran out first. */
	Expired,
	/** Its holder formed another. */
	Replaced,
	/** Its holder died or left the battleground. */
	HolderGone,
};

/**
 * Keeps each Vanguard's Echo (ADR-050 §2–§5), on the server: one per holder. It forms an Echo where an Echo ability
 * sends it, hands a waiting Echo the repeat of its holder's next eligible cast, and ends it. An ended Echo stays
 * withdrawn, out of sight and dead at nobody's hand, until its holder forms another, so what it set going keeps its
 * source.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraEchoSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Server: Holder's Echo for Ability's manifest forms at Where, on the nearest ground, and waits for Holder's next
	 * eligible cast for its window (ADR-050 §4); any Echo Holder had ends first. Null if refused.
	 */
	AVeyraEcho* Manifest(UAbilitySystemComponent& Holder, const FVeyraContentId& Ability, const FVector& Where);

	/** Server: Holder's Echo while it stands, or null. */
	AVeyraEcho* FindStanding(const UAbilitySystemComponent& Holder) const;

	/**
	 * Server: the repeat Holder's waiting Echo takes of Holder's committed Cast (ADR-050 §5), when Cast's ability sits in
	 * one of the slots the Echo repeats: the same ability at the same rank, cast by the Echo from where it stands, aimed
	 * as the cast was and within its range, under a cast ID of its own. Unset when it takes none. Its last repeat ends it.
	 * The caller delivers the repeat, and only an ability whose delivery can be repeated asks.
	 */
	TOptional<FVeyraCast> TakeRepeat(UAbilitySystemComponent& Holder, const FVeyraCast& Cast);

	/** Server: Holder's standing Echo ends for Why. Nothing happens without one. */
	void End(const UAbilitySystemComponent& Holder, EVeyraEchoEnd Why);

	/** Server: an Echo ended, and why. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEchoEnded, AVeyraEcho& /*Echo*/, EVeyraEchoEnd /*Why*/);
	FOnEchoEnded OnEchoEnded;

	virtual void Deinitialize() override;

private:
	struct FKept
	{
		TWeakObjectPtr<UAbilitySystemComponent> Holder;
		TWeakObjectPtr<AVeyraEcho> Echo;
		int32 RepeatsLeft = 0;
		FTimerHandle Timer;
	};

	FKept* FindKept(const UAbilitySystemComponent& Holder);
	const FKept* FindKept(const UAbilitySystemComponent& Holder) const;

	/** Forms Holder's Echo for Ability at Where, ending and clearing away any Echo Holder had. Null if refused. */
	AVeyraEcho* Form(UAbilitySystemComponent& Holder, const FVeyraContentId& Ability, const FVector& Where, TOptional<double> Integrity);

	void EndKept(FKept& Entry, EVeyraEchoEnd Why);
	void OnWindowEnded(TWeakObjectPtr<UAbilitySystemComponent> Holder);

	TArray<FKept> Kept;
};
