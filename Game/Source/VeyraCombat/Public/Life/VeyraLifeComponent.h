// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "VeyraLifeComponent.generated.h"

/** Whether a combatant is alive (Combat Bible §18). Not a Gameplay Tag: canon defines no Dead status. */
UENUM()
enum class EVeyraLifeState : uint8
{
	Alive,
	/** Death is final: Health reached 0 and nothing prevented it. */
	Dead,
};

/**
 * A combatant's life state. It lives beside the Ability System Component (on a Vanguard's
 * PlayerState), so it outlives each pawn. Only Combat changes it: VeyraCombat finalizes a death
 * and revives on respawn. Replicated to every client.
 */
UCLASS(ClassGroup = Combat)
class VEYRACOMBAT_API UVeyraLifeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraLifeComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsAlive() const { return State == EVeyraLifeState::Alive; }
	EVeyraLifeState GetState() const { return State; }

	/** Server only. Returns false if the state was already NewState. */
	bool SetState(EVeyraLifeState NewState);

private:
	UPROPERTY(Replicated)
	EVeyraLifeState State = EVeyraLifeState::Alive;
};
