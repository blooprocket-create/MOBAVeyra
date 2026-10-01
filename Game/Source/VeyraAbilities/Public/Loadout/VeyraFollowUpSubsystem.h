// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"

#include "VeyraFollowUpSubsystem.generated.h"

class UAbilitySystemComponent;
struct FVeyraMarkerEnd;

/**
 * Follow-ups beyond the cast that opened them (ADR-032 §5). One that arms opens a while after its cast
 * commits, as Shatterforge does once its wall has cooled. And a follow-up ends as the marker of the
 * ability that opened it ends, having nothing left to act on, as a placed shadow's swap or a wall's
 * detonation. Server only.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraFollowUpSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Opens FollowUp in Caster's Slot after Seconds of world time, so a pause holds it. */
	void OpenAfter(UAbilitySystemComponent& Caster, EVeyraAbilitySlot Slot, const FVeyraOverrideSpec& FollowUp, double Seconds);

	/** How many follow-ups wait to open. */
	int32 GetArmingCount() const { return Arming.Num(); }

private:
	void OnMarkerEnded(const FVeyraMarkerEnd& End);
	void Open(int32 Key);

	struct FArming
	{
		int32 Key = 0;
		TWeakObjectPtr<UAbilitySystemComponent> Caster;
		EVeyraAbilitySlot Slot = EVeyraAbilitySlot::Q;
		FVeyraOverrideSpec FollowUp;
		FTimerHandle Timer;
	};
	TArray<FArming> Arming;
	int32 NextKey = 1;
	FDelegateHandle MarkerEndHandle;
};
