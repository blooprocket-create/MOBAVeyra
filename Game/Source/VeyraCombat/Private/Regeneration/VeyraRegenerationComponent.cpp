// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Regeneration/VeyraRegenerationComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/World.h"
#include "Life/VeyraLifeComponent.h"
#include "TimerManager.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

UVeyraRegenerationComponent::UVeyraRegenerationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVeyraRegenerationComponent::BeginPlay()
{
	Super::BeginPlay();
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	TickSeconds = UVeyraCombatTuningSubsystem::Get().Regeneration.TickSeconds;
	GetWorld()->GetTimerManager().SetTimer(Timer, this, &UVeyraRegenerationComponent::OnTimer, static_cast<float>(TickSeconds), /*bLoop*/ true);
}

void UVeyraRegenerationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Timer);
	}
	Super::EndPlay(EndPlayReason);
}

void UVeyraRegenerationComponent::OnTimer()
{
	ApplyTick(TickSeconds);
}

void UVeyraRegenerationComponent::ApplyTick(double Seconds)
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	const UVeyraLifeComponent* Life = Owner ? Owner->FindComponentByClass<UVeyraLifeComponent>() : nullptr;
	if (!AbilitySystem || (Life && !Life->IsAlive()) || !(Seconds > 0.0))
	{
		return;
	}
	if (AbilitySystem->GetSet<UVeyraResourceSet>())
	{
		const double Regeneration = AbilitySystem->GetNumericAttribute(UVeyraResourceSet::GetResourceRegenAttribute());
		if (Regeneration > 0.0)
		{
			VeyraCombat::RestoreResource(*AbilitySystem, Regeneration * Seconds);
		}
	}
	if (AbilitySystem->GetSet<UVeyraVitalsSet>())
	{
		const double Regeneration = AbilitySystem->GetNumericAttribute(UVeyraVitalsSet::GetHealthRegenAttribute());
		if (Regeneration > 0.0 && VeyraCombat::GetMissingHealth(*AbilitySystem) > 0.0)
		{
			VeyraCombat::RestoreHealth(*AbilitySystem, Regeneration * Seconds);
		}
	}
}
