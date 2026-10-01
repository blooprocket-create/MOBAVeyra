// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraAllHandsPassive.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Teams/VeyraTeam.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"
#include "VeyraVanguardsLog.h"

void UVeyraAllHandsPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		DealtHandle = Events->OnDamageDealt.AddUObject(this, &UVeyraAllHandsPassive::OnDamageDealt);
	}
}

void UVeyraAllHandsPassive::Stop()
{
	if (UVeyraCombatEventSubsystem* Events = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDamageDealt.Remove(DealtHandle);
	}
	DealtHandle.Reset();
	ReadyAt.Reset();
	Work = 0.0;
	Super::Stop();
}

void UVeyraAllHandsPassive::OnDamageDealt(const FVeyraDamageDealtEvent& Event)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const UAbilitySystemComponent* Source = Event.Source.Get();
	const UAbilitySystemComponent* Target = Event.Target.Get();
	const FVeyraAllHandsTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindAllHands(PassiveId);
	UWorld* World = GetWorld();
	if (!Owner || !Source || !Target || !Tuning || !World)
	{
		return;
	}
	// An allied Vanguard's damage, its owner's among them, on an enemy Vanguard (ADR-037 §6).
	const EVeyraTeam Side = VeyraTeams::TeamOf(Owner->GetOwner());
	const AActor* Struck = Target->GetAvatarActor();
	if (Side == EVeyraTeam::None || !VeyraUnits::IsVanguard(Source->GetAvatarActor()) || VeyraTeams::TeamOf(Source->GetOwner()) != Side
		|| !VeyraUnits::IsVanguard(Struck) || VeyraTeams::TeamOf(Target->GetOwner()) == Side)
	{
		return;
	}
	// No living companion, no Work: what it held goes with the companion.
	const UVeyraCompanionSubsystem* Keeper = World->GetSubsystem<UVeyraCompanionSubsystem>();
	AVeyraCompanion* Companion = Keeper ? Keeper->FindLiving(*Owner) : nullptr;
	if (!Companion)
	{
		Work = 0.0;
		return;
	}
	if (FVector::Dist2D(Struck->GetActorLocation(), Companion->GetActorLocation()) > Tuning->Radius)
	{
		return;
	}
	// Once per its cooldown for each contributor, so many hits, ticks and procs earn no more.
	const double Now = World->GetTimeSeconds();
	double& Ready = ReadyAt.FindOrAdd(Source, 0.0);
	if (Now < Ready)
	{
		return;
	}
	Ready = Now + Tuning->PerContributorSeconds;
	Work = FMath::Min(Work + Tuning->Work, Tuning->Threshold);
	if (Work < Tuning->Threshold)
	{
		return;
	}
	Work -= Tuning->Threshold;
	UAbilitySystemComponent& Repaired = *Companion->GetAbilitySystemComponent();
	const double Amount = Tuning->Repair + Tuning->RepairMaxHealthRatio * Repaired.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
	const double Restored = VeyraCombat::RestoreHealthFrom(*Owner, Repaired, Amount);
	++RepairCount;
	UE_LOG(LogVeyraVanguards, Verbose, TEXT("%s's All Hands repaired %s by %g."), *GetNameSafe(Owner->GetOwner()), *GetNameSafe(Companion), Restored);
}
