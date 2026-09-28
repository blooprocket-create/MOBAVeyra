// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Rules/VeyraTowerRules.h"

#include "VeyraStructureAttackComponent.generated.h"

class AVeyraStructure;

/**
 * A lane Spire's or base-defense tower's attack (Combat Bible §33, §55; Battleground Bible §19;
 * ADR-011 §8). On a world-time timer it reconsiders its target under VeyraTowerRules, Vanguard
 * priority included, and fires a homing shot at its cadence: Physical, a StructureAttack delivery,
 * never a crit, stronger with each consecutive shot at the same Vanguard. Each tower keeps its own
 * target and ramp. Server only; the battleground subsystem starts it.
 */
UCLASS(ClassGroup = World)
class VEYRAWORLD_API UVeyraStructureAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraStructureAttackComponent();

	/** Server: starts attacking, thinking on World.json's cadence. */
	void StartAttacking();

	/** Server: stops, as when its structure falls or the match ends. */
	void StopAttacking();

	/**
	 * Server: Attacker, an enemy Vanguard, damaged a defending Vanguard with both in range. The tower
	 * may give it priority at its next thought (Combat Bible §33).
	 */
	void NoteAggression(AActor& Attacker);

	/** Server: reconsiders its target and fires if its interval has passed. Its timer calls it; tests call it directly. */
	void Think();

	/** Whether Unit is within the tower's attack range, edge to edge. */
	bool IsInRange(const AActor& Unit) const;

	/** The living enemies it could shoot now. */
	TArray<FVeyraTowerCandidate> GatherCandidates() const;

	AActor* GetTarget() const { return Target.Get(); }
	bool HasPriority() const { return bPriority; }
	int32 GetRampStacks() const { return RampStacks; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Fire(AActor& Shot, bool bVanguard);
	AVeyraStructure* GetStructure() const;
	double Now() const;

	TWeakObjectPtr<AActor> Target;
	bool bPriority = false;
	TWeakObjectPtr<AActor> Claimant;
	TWeakObjectPtr<AActor> RampTarget;
	int32 RampStacks = 0;
	double NextShotAt = 0.0;
	FTimerHandle ThinkTimer;
};
