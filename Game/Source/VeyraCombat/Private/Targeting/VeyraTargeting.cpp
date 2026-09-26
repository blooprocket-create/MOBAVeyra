// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Targeting/VeyraTargeting.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "Life/VeyraLifeComponent.h"
#include "Teams/VeyraTeam.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"

namespace VeyraTargeting
{
namespace
{
	/** The life state beside a unit's Ability System Component: a Vanguard's is on its PlayerState. */
	const UVeyraLifeComponent* FindLife(const AActor* Unit)
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Unit);
		const AActor* Owner = AbilitySystem ? AbilitySystem->GetOwner() : nullptr;
		return Owner ? Owner->FindComponentByClass<UVeyraLifeComponent>() : nullptr;
	}
}

bool IsAlive(const AActor* Unit)
{
	const UVeyraLifeComponent* Life = FindLife(Unit);
	return Life && Life->IsAlive();
}

bool AreHostile(const UObject* A, const UObject* B)
{
	return VeyraTeams::TeamOf(A) != VeyraTeams::TeamOf(B);
}

double EdgeToEdgeDistance(const AActor& A, const AActor& B)
{
	const double Centres = FVector::Dist2D(A.GetActorLocation(), B.GetActorLocation());
	return FMath::Max(0.0, Centres - A.GetSimpleCollisionRadius() - B.GetSimpleCollisionRadius());
}

bool IsWithinCastRange(const AActor& Caster, const AActor& Target, double CastRange)
{
	return EdgeToEdgeDistance(Caster, Target) <= CastRange + UVeyraCombatTuningSubsystem::Get().Targeting.ServerRangeTolerance;
}

EVeyraTargetValidity CheckEnemyTarget(const AActor& Caster, const AActor* Target, double CastRange)
{
	if (!Target || !FindLife(Target))
	{
		return EVeyraTargetValidity::NotACombatant;
	}
	if (Target == &Caster)
	{
		return EVeyraTargetValidity::Caster;
	}
	if (!IsAlive(Target))
	{
		return EVeyraTargetValidity::Dead;
	}
	if (!AreHostile(&Caster, Target))
	{
		return EVeyraTargetValidity::NotHostile;
	}
	return IsWithinCastRange(Caster, *Target, CastRange) ? EVeyraTargetValidity::Valid : EVeyraTargetValidity::OutOfRange;
}
}
