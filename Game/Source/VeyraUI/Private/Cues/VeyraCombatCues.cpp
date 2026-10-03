// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Cues/VeyraCombatCues.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Casting/VeyraCastStateComponent.h"
#include "GameFramework/Actor.h"
#include "Hud/VeyraHudModel.h"
#include "Targeting/VeyraTargeting.h"
#include "Units/VeyraUnit.h"

namespace VeyraCombatCues
{
namespace
{
	/** A component kept beside Unit's Ability System Component: on a Vanguard's participant, or on the unit itself. */
	template <typename TComponent>
	const TComponent* FindBesideAbilitySystem(const AActor& Unit)
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
		const AActor* Owner = AbilitySystem ? AbilitySystem->GetOwner() : nullptr;
		return Owner ? Owner->FindComponentByClass<TComponent>() : nullptr;
	}
}

TArray<FVeyraCombatCue> Between(const AActor& Unit, const FVeyraUnitSighting& Before, const FVeyraUnitSighting& Now)
{
	TArray<FVeyraCombatCue> Cues;
	const auto Add = [&Cues, &Unit](EVeyraCombatCueKind Kind) -> FVeyraCombatCue& {
		FVeyraCombatCue& Cue = Cues.AddDefaulted_GetRef();
		Cue.Kind = Kind;
		Cue.Unit = &Unit;
		Cue.Location = Unit.GetActorLocation();
		return Cue;
	};
	// Health and shields lost, unless the unit's Max Health fell with them, as a sold item takes it.
	const double Lost = Before.Vitality - Now.Vitality;
	if (Lost > UE_KINDA_SMALL_NUMBER && Before.bAlive && Now.MaxHealth >= Before.MaxHealth)
	{
		Add(EVeyraCombatCueKind::Hit).Amount = Lost;
	}
	if (Before.bAlive && !Now.bAlive)
	{
		Add(EVeyraCombatCueKind::Death);
		return Cues;
	}
	// A new phase, or the same phase of a new attack: each sets its own end.
	const bool bNewAttackPhase = Now.AttackPhase != Before.AttackPhase || Now.AttackPhaseEndsAt != Before.AttackPhaseEndsAt;
	if (bNewAttackPhase && Now.AttackPhase == EVeyraAttackPhase::Windup)
	{
		FVeyraCombatCue& Cue = Add(EVeyraCombatCueKind::AttackWindup);
		Cue.Target = Now.AttackTarget;
		Cue.EndsAt = Now.AttackPhaseEndsAt;
	}
	else if (bNewAttackPhase && Now.AttackPhase == EVeyraAttackPhase::Backswing)
	{
		Add(EVeyraCombatCueKind::AttackCommit).Target = Now.AttackTarget;
	}
	if (Now.CastPhase == EVeyraCastPhase::Windup && (Before.CastPhase != EVeyraCastPhase::Windup || Before.CastId != Now.CastId))
	{
		FVeyraCombatCue& Cue = Add(EVeyraCombatCueKind::CastWindup);
		Cue.Ability = Now.CastAbility;
		Cue.Location = Now.CastLocation;
	}
	if (Now.CommitSerial != Before.CommitSerial)
	{
		FVeyraCombatCue& Cue = Add(EVeyraCombatCueKind::CastCommit);
		Cue.Ability = Now.CommitAbility;
		Cue.Location = Now.CommitLocation;
	}
	return Cues;
}

TOptional<FVeyraUnitSighting> Sight(const AActor& Unit, EVeyraTeam Viewer)
{
	const TOptional<FVeyraHudVitals> Vitals = VeyraUnits::KindOf(&Unit) ? VeyraHud::VitalsOf(Unit, Viewer) : TOptional<FVeyraHudVitals>();
	if (!Vitals)
	{
		return {};
	}
	FVeyraUnitSighting Sighting;
	Sighting.bAlive = VeyraTargeting::IsAlive(&Unit);
	Sighting.Vitality = Vitals->Health + Vitals->Shield;
	Sighting.MaxHealth = Vitals->MaxHealth;
	if (const UVeyraBasicAttackComponent* Attacks = FindBesideAbilitySystem<UVeyraBasicAttackComponent>(Unit))
	{
		const FVeyraAttackState& Attack = Attacks->GetState();
		Sighting.AttackPhase = Attack.Phase;
		Sighting.AttackPhaseEndsAt = Attack.PhaseEndsAt;
		Sighting.AttackTarget = Attack.Target;
	}
	if (const UVeyraCastStateComponent* Casts = FindBesideAbilitySystem<UVeyraCastStateComponent>(Unit))
	{
		const FVeyraCastState& Cast = Casts->GetState();
		Sighting.CastPhase = Cast.Phase;
		Sighting.CastId = Cast.CastId;
		Sighting.CastAbility = Cast.Ability;
		Sighting.CastLocation = Cast.Location;
		const FVeyraCastCommit& Commit = Casts->GetLastCommit();
		Sighting.CommitSerial = Commit.Serial;
		Sighting.CommitAbility = Commit.Ability;
		Sighting.CommitLocation = Commit.Location;
	}
	return Sighting;
}
}
