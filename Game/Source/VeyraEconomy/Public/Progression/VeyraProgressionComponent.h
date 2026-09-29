// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Progression/VeyraProgressionTypes.h"
#include "Slots/VeyraAbilitySlot.h"

#include "VeyraProgressionComponent.generated.h"

/**
 * One unit's in-match progression (Economy & Progression Bible §1, §9, §16): its level and XP, its
 * unspent skill points and its ability ranks. It sits beside the unit's Ability System Component, on
 * the PlayerState, so progression survives death. The server changes it; level and ranks replicate to
 * everyone, XP and unspent points to the owner.
 */
UCLASS()
class VEYRAECONOMY_API UVeyraProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraProgressionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Server only: starts the unit at level 1 with that level's skill points and no ranks. Growth is
	 * what each later level adds to its base stats; BaseAttackSpeed is its level-1 Attack Speed, which
	 * Attack Speed growth is a fraction of. Call once per match.
	 */
	void Initialize(const FVeyraStatGrowth& Growth, double BaseAttackSpeed);

	/**
	 * Server only: adds Amount XP (Economy & Progression §9). Each level gained raises the base stats
	 * by the growth and grants its skill points. Returns the number of levels gained.
	 */
	int32 AddExperience(double Amount);

	/** Server only: spends one skill point raising Slot by one rank (§1, §9). Points are never refunded. */
	EVeyraRankRefusal AllocateRank(EVeyraAbilitySlot Slot);

	bool IsInitialized() const { return Level > 0; }
	int32 GetLevel() const { return Level; }

	/**
	 * Server only: the unit's level-1 Attack Speed, which Attack Speed growth and bonus Attack Speed
	 * are fractions of, so both add rather than compound (ADR-012 §6).
	 */
	double GetBaseAttackSpeed() const { return BaseAttackSpeed; }
	int32 GetRank(EVeyraAbilitySlot Slot) const;

	/** Owner and server only: XP towards the next level, with full fractional precision (§1). */
	double GetExperience() const { return Experience; }

	/** Owner and server only. */
	int32 GetUnspentSkillPoints() const { return UnspentSkillPoints; }

	/** Server only: raised after each level gained, with the new level. */
	TMulticastDelegate<void(int32 /*NewLevel*/)> OnLevelGained;

private:
	void SetLevel(int32 NewLevel);
	void SetExperience(double NewExperience);
	void SetUnspentSkillPoints(int32 NewPoints);

	/** 0 until Initialize. */
	UPROPERTY(Replicated)
	int32 Level = 0;

	/** One rank per slot, in EVeyraAbilitySlot order. */
	UPROPERTY(Replicated)
	TArray<int32> Ranks;

	UPROPERTY(Replicated)
	double Experience = 0.0;

	UPROPERTY(Replicated)
	int32 UnspentSkillPoints = 0;

	FVeyraStatGrowth Growth;
	double BaseAttackSpeed = 0.0;
};
