// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Targeting/VeyraTargeting.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "Life/VeyraLifeComponent.h"
#include "Teams/VeyraTeam.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "Targeting/VeyraVisibility.h"

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
	// The two sides are hostile to each other. Anything on no side is no one's enemy, save one explicit
	// category (Combat Bible §29; ADR-014 §1): a neutral unit, wildlife or an objective, is hostile to
	// what is on a side (Vanguards and what they cast), but never to lane Fluxborn or structures, and
	// never to another neutral unit.
	const EVeyraTeam TeamA = VeyraTeams::TeamOf(A);
	const EVeyraTeam TeamB = VeyraTeams::TeamOf(B);
	if (TeamA != EVeyraTeam::None && TeamB != EVeyraTeam::None)
	{
		return TeamA != TeamB;
	}
	const UObject* Sided = TeamA != EVeyraTeam::None ? A : TeamB != EVeyraTeam::None ? B : nullptr;
	const UObject* Other = Sided == A ? B : A;
	if (!Sided || !VeyraUnits::IsNeutral(Other))
	{
		return false;
	}
	const TOptional<EVeyraUnitKind> SidedKind = VeyraUnits::KindOf(Sided);
	return !SidedKind.IsSet() || (SidedKind.GetValue() != EVeyraUnitKind::Fluxborn && SidedKind.GetValue() != EVeyraUnitKind::Structure);
}

bool CanAcquire(const UObject* Acquirer, const AActor& Target)
{
	return !Acquirer || VeyraVisibility::CanSee(*Acquirer, Target);
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

EVeyraTargetValidity CheckEnemyTarget(const AActor& Caster, const AActor* Target, double CastRange, EVeyraStructureTargeting Structures)
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
	if (Structures == EVeyraStructureTargeting::Refuse && VeyraUnits::IsStructure(Target))
	{
		return EVeyraTargetValidity::Structure;
	}
	// Knowing where a unit is does not permit targeting it (Vision Bible §1).
	if (!CanAcquire(&Caster, *Target))
	{
		return EVeyraTargetValidity::NotVisible;
	}
	return IsWithinCastRange(Caster, *Target, CastRange) ? EVeyraTargetValidity::Valid : EVeyraTargetValidity::OutOfRange;
}
}
