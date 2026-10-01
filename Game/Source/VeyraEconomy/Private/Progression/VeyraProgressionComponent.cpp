// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Progression/VeyraProgressionComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Progression/VeyraProgressionRules.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraEconomyLog.h"

UVeyraProgressionComponent::UVeyraProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Ranks.Init(0, UE_ARRAY_COUNT(VeyraAbilitySlots::All));
}

void UVeyraProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraProgressionComponent, Level, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraProgressionComponent, Ranks, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraProgressionComponent, RankShape, Params);

	FDoRepLifetimeParams OwnerParams = Params;
	OwnerParams.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraProgressionComponent, Experience, OwnerParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraProgressionComponent, UnspentSkillPoints, OwnerParams);
}

void UVeyraProgressionComponent::Initialize(const FVeyraStatGrowth& InGrowth, double InBaseAttackSpeed, const FVeyraRankShape* Shape)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	const FVeyraProgressionTuning& Tuning = UVeyraProgressionTuningSubsystem::Get();
	Growth = InGrowth;
	BaseAttackSpeed = InBaseAttackSpeed;
	RankShape = Shape ? *Shape : VeyraProgression::StandardShape(Tuning);
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraProgressionComponent, RankShape, this);
	Ranks.Init(0, UE_ARRAY_COUNT(VeyraAbilitySlots::All));
	// An innate R is learnt from the start (ADR-031 §2).
	for (int32 Index = 0; Index < Ranks.Num(); ++Index)
	{
		Ranks[Index] = VeyraProgression::StartingRank(static_cast<EVeyraAbilitySlot>(Index), RankShape);
	}
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraProgressionComponent, Ranks, this);
	SetLevel(1);
	SetExperience(0.0);
	SetUnspentSkillPoints(VeyraProgression::SkillPointsEarned(1, Tuning));
}

int32 UVeyraProgressionComponent::AddExperience(double Amount)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	if (!IsInitialized() || !FMath::IsFinite(Amount) || !(Amount > 0.0))
	{
		return 0;
	}
	const FVeyraProgressionTuning& Tuning = UVeyraProgressionTuningSubsystem::Get();
	int32 LevelsGained = 0;
	const VeyraProgression::FExperienceState After = VeyraProgression::AddExperience({ Level, Experience }, Amount, Tuning, LevelsGained);
	SetExperience(After.Experience);
	if (LevelsGained == 0)
	{
		return 0;
	}

	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	FVeyraStatBlock PerLevel;
	PerLevel.MaxHealth = Growth.MaxHealth;
	PerLevel.HealthRegen = Growth.HealthRegen;
	PerLevel.MaxResource = Growth.MaxResource;
	PerLevel.ResourceRegen = Growth.ResourceRegen;
	PerLevel.Armor = Growth.Armor;
	PerLevel.MagicResist = Growth.MagicResist;
	PerLevel.PhysicalPower = Growth.PhysicalPower;
	PerLevel.MagicPower = Growth.MagicPower;
	PerLevel.AttackSpeed = BaseAttackSpeed * Growth.AttackSpeedFraction;
	for (int32 Gained = 0; Gained < LevelsGained; ++Gained)
	{
		if (!AbilitySystem || !VeyraCombat::GrowBaseStats(*AbilitySystem, PerLevel))
		{
			UE_LOG(LogVeyraEconomy, Error, TEXT("%s levelled up without stat growth; see the errors above."), *GetNameSafe(GetOwner()));
		}
		SetLevel(Level + 1);
		SetUnspentSkillPoints(UnspentSkillPoints + Tuning.SkillPointsPerLevel);
		OnLevelGained.Broadcast(Level);
	}
	return LevelsGained;
}

EVeyraRankRefusal UVeyraProgressionComponent::AllocateRank(EVeyraAbilitySlot Slot)
{
	check(GetOwner() && GetOwner()->HasAuthority());
	const EVeyraRankRefusal Refusal = CheckRankUp(Slot);
	if (Refusal != EVeyraRankRefusal::None)
	{
		return Refusal;
	}
	const int32 Index = static_cast<int32>(Slot);
	++Ranks[Index];
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraProgressionComponent, Ranks, this);
	SetUnspentSkillPoints(UnspentSkillPoints - 1);
	return EVeyraRankRefusal::None;
}

EVeyraRankRefusal UVeyraProgressionComponent::CheckRankUp(EVeyraAbilitySlot Slot) const
{
	if (!IsInitialized())
	{
		return EVeyraRankRefusal::NotInitialized;
	}
	const int32 Index = static_cast<int32>(Slot);
	if (!Ranks.IsValidIndex(Index))
	{
		// Only the kit's slots take ranks: an item's Active has none (ADR-012 §1).
		return EVeyraRankRefusal::MaxRank;
	}
	return VeyraProgression::CheckRankUp(Slot, Ranks[Index], Level, UnspentSkillPoints, UVeyraProgressionTuningSubsystem::Get(), RankShape);
}

int32 UVeyraProgressionComponent::GetMaxRank(EVeyraAbilitySlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	return Ranks.IsValidIndex(Index) ? VeyraProgression::MaxRank(Slot, UVeyraProgressionTuningSubsystem::Get(), RankShape) : 0;
}

int32 UVeyraProgressionComponent::GetRank(EVeyraAbilitySlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	return Ranks.IsValidIndex(Index) ? Ranks[Index] : 0;
}

void UVeyraProgressionComponent::SetLevel(int32 NewLevel)
{
	Level = NewLevel;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraProgressionComponent, Level, this);
}

void UVeyraProgressionComponent::SetExperience(double NewExperience)
{
	Experience = NewExperience;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraProgressionComponent, Experience, this);
}

void UVeyraProgressionComponent::SetUnspentSkillPoints(int32 NewPoints)
{
	UnspentSkillPoints = NewPoints;
	MARK_PROPERTY_DIRTY_FROM_NAME(UVeyraProgressionComponent, UnspentSkillPoints, this);
}
