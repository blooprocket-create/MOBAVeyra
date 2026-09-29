// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"

#include "VeyraVolleySubsystem.generated.h"

class UAbilitySystemComponent;
struct FVeyraDisplacementEvent;
struct FVeyraVolleyAbilityTuning;

/**
 * Each caster's open volley lane (ADR-018 §6), on the server. A lane holds its caster's slot to its
 * shot, keeps each shot within its angle, counts the shots left and those its bonus earns, and closes
 * when they or its time run out, ending the stance it gave.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraVolleySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Opens Caster's lane from Slot's Ability along Direction, replacing any lane it had. */
	void Open(UAbilitySystemComponent& Caster, EVeyraAbilitySlot Slot, const FVeyraContentId& Ability, const FVeyraVolleyAbilityTuning& Tuning,
		const FVector& Direction, int32 CasterLevel);

	/** Direction brought within Caster's lane, when Shot is its lane's shot; Direction itself otherwise. */
	FVector AimWithin(const UAbilitySystemComponent& Caster, const FVeyraContentId& Shot, const FVector& Direction) const;

	/** Caster fired Shot: one fewer is left, and the lane closes once none are. */
	void NoteShot(UAbilitySystemComponent& Caster, const FVeyraContentId& Shot);

	/** The shots Caster's lane has left; 0 with no lane open. */
	int32 GetShotsLeft(const UAbilitySystemComponent& Caster) const;

	/** Closes Caster's lane, if it has one: its slot holds its own ability again, and its stance ends. */
	void Close(UAbilitySystemComponent& Caster);

	virtual void Deinitialize() override;

private:
	struct FLane
	{
		TWeakObjectPtr<UAbilitySystemComponent> Caster;
		EVeyraAbilitySlot Slot = EVeyraAbilitySlot::R;
		FVeyraContentId Shot;
		FVector Direction = FVector::ForwardVector;
		double HalfAngleDegrees = 0.0;
		int32 ShotsLeft = 0;
		int32 BonusLeft = 0;
		FVeyraContentId BonusStatus;
		TArray<FVeyraContentId> CasterStatuses;
		FTimerHandle Timer;
	};

	FLane* Find(const UAbilitySystemComponent& Caster);
	const FLane* Find(const UAbilitySystemComponent& Caster) const;

	/** An ally's displacement of an enemy its caster marked earns a shot, up to the bonus's most. */
	void OnDisplaced(const FVeyraDisplacementEvent& Event);

	TArray<FLane> Lanes;
	FDelegateHandle DisplacedHandle;
};
