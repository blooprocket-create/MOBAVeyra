// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Passives/VeyraWildDominionPassive.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Layout/VeyraLayout.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"

void UVeyraWildDominionPassive::Start(UAbilitySystemComponent& Owner, const FVeyraContentId& InPassiveId)
{
	Super::Start(Owner, InPassiveId);
	UWorld* World = GetWorld();
	const FVeyraWildDominionTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindWildDominion(PassiveId);
	if (!World || !Tuning)
	{
		return;
	}
	// World time, so a pause holds it.
	World->GetTimerManager().SetTimer(CheckTimer, FTimerDelegate::CreateUObject(this, &UVeyraWildDominionPassive::Check), static_cast<float>(Tuning->CheckSeconds), /*bLoop*/ true);
	if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
	{
		ResolvedHandle = Events->OnDamageResolved.AddUObject(this, &UVeyraWildDominionPassive::OnDamageResolved);
	}
}

void UVeyraWildDominionPassive::Stop()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDamageResolved.Remove(ResolvedHandle);
		}
	}
	ResolvedHandle.Reset();
	Super::Stop();
}

void UVeyraWildDominionPassive::Check()
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraWildDominionTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindWildDominion(PassiveId);
	const UWorld* World = GetWorld();
	const AActor* Body = Owner ? Owner->GetAvatarActor() : nullptr;
	const UVeyraBattlegroundSubsystem* Battleground = World ? World->GetSubsystem<UVeyraBattlegroundSubsystem>() : nullptr;
	// Only a battleground has jungle.
	const FVeyraBattlegroundLayout* Layout = Battleground ? Battleground->GetLayout() : nullptr;
	if (!Owner || !Tuning || !Body || !Layout || !VeyraTargeting::IsAlive(Body))
	{
		return;
	}
	const FVector Location = Body->GetActorLocation();
	if (!VeyraLayout::IsJungle(*Layout, FVector2D(Location.X, Location.Y)))
	{
		return;
	}
	const UVeyraProgressionComponent* Progression = Owner->GetOwner()->FindComponentByClass<UVeyraProgressionComponent>();
	const int32 Level = Progression && Progression->IsInitialized() ? Progression->GetLevel() : 1;
	for (const FVeyraContentId& StatusId : Tuning->Statuses)
	{
		if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(StatusId, Level))
		{
			VeyraCombat::ApplyStatus(*Owner, *Owner, Status.GetValue());
		}
	}
}

void UVeyraWildDominionPassive::OnDamageResolved(const FVeyraDamageResolution& Resolution)
{
	UAbilitySystemComponent* Owner = OwnerAbilitySystem.Get();
	const FVeyraWildDominionTuning* Tuning = UVeyraVanguardsTuningSubsystem::FindWildDominion(PassiveId);
	const UAbilitySystemComponent* Target = Resolution.Target.Get();
	if (!Owner || !Tuning || Resolution.Source.Get() != Owner || !Target || !(Resolution.HealthLost > 0.0))
	{
		return;
	}
	// A share of the Health it took from wildlife, whatever the terrain (Roster Bible §12).
	const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(Target->GetAvatarActor());
	if (Kind.IsSet() && Kind.GetValue() == EVeyraUnitKind::Wildlife)
	{
		VeyraCombat::RestoreHealthFrom(*Owner, *Owner, Resolution.HealthLost * Tuning->WildlifeHealFraction);
	}
}
