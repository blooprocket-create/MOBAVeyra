// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"

#include "VeyraCombatStateComponent.generated.h"

/**
 * A Vanguard's Combat State (Combat Bible §28): in combat from its last fight with an enemy
 * Vanguard until the tuned delay passes, and out of it at death. The server decides it on world
 * time, so a pause holds it; every machine sees it for presentation.
 */
UCLASS(ClassGroup = Combat)
class VEYRACOMBAT_API UVeyraCombatStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraCombatStateComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Behind the fog on a participant: its own, its teammates' and its observers' (ADR-016 §3). */
	virtual ELifetimeCondition GetReplicationCondition() const override;
	virtual void ReadyForReplication() override;

	/** Server only: the unit fought an enemy Vanguard now, so it stays in combat for the tuned delay. */
	void NoteCombat();

	/** Server only: the unit leaves Combat State at once, as at death. */
	void Clear();

	bool IsInCombat() const { return bInCombat; }

	/** Raised on every machine when IsInCombat changes, with its new value. */
	TMulticastDelegate<void(bool)> OnCombatStateChanged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnRep_InCombat();

	void SetInCombat(bool bNewInCombat);

	UPROPERTY(ReplicatedUsing = OnRep_InCombat)
	bool bInCombat = false;

	FTimerHandle OutOfCombatTimer;
};
