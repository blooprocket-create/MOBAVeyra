// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraShieldRewardSubsystem.generated.h"

class UAbilitySystemComponent;
struct FVeyraAbsorbedRewardTuning;
struct FVeyraDamageResolution;
struct FVeyraShieldGrant;

/**
 * Shields that reward what they absorb (ADR-027 §5), as Windward's second speed burst. It watches each
 * grant of a shield whose tuning names an absorbed reward, counts what that shield absorbs from the
 * damage Combat resolves, and once the count reaches the reward's share of the grant gives the holder
 * the reward's statuses, once, from the shield's provider. A new grant of the same shield from the same
 * provider to the same holder starts the count again; a watch ends with its reward or its shield's time.
 * Server only.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraShieldRewardSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Watches Grant, which Provider has just given Holder, for Reward. */
	void Watch(UAbilitySystemComponent& Provider, UAbilitySystemComponent& Holder, const FVeyraShieldGrant& Grant, const FVeyraAbsorbedRewardTuning& Reward);

	/** How many grants it watches. */
	int32 GetWatchCount() const { return Watches.Num(); }

private:
	void OnDamageResolved(const FVeyraDamageResolution& Resolution);

	struct FWatch
	{
		TWeakObjectPtr<UAbilitySystemComponent> Provider;
		TWeakObjectPtr<UAbilitySystemComponent> Holder;
		FVeyraContentId Id;
		/** What the grant must absorb for the reward. */
		double Needed = 0.0;
		double Absorbed = 0.0;
		/** When the shield runs out, in world time. */
		double Until = 0.0;
		TArray<FVeyraStatusSpec> Statuses;
	};
	TArray<FWatch> Watches;
	FDelegateHandle ResolvedHandle;
};
