// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraAbilitiesTuning* GTestOverride = nullptr;
}

void UVeyraAbilitiesTuningSubsystem::SetTestOverride(const FVeyraAbilitiesTuning* Override)
{
	check(IsInGameThread());
	GTestOverride = Override;
}
#endif

void UVeyraAbilitiesTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraAbilitiesTuning& UVeyraAbilitiesTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GTestOverride)
	{
		return *GTestOverride;
	}
#endif
	const UVeyraAbilitiesTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraAbilitiesTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Abilities tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

const FVeyraTargetedDamageAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(const FVeyraContentId& Ability)
{
	return Get().TargetedDamage.Find(Ability);
}

bool UVeyraAbilitiesTuningSubsystem::Defines(const FVeyraContentId& Ability)
{
	return FindTargetedDamage(Ability) != nullptr;
}

VeyraTuning::FErrors UVeyraAbilitiesTuningSubsystem::Reload()
{
	FVeyraAbilitiesTuning Loaded;
	FBlake3Hash Hash;
	VeyraTuning::FErrors Errors = VeyraTuning::LoadDomain(Domain, FVeyraAbilitiesTuning::SchemaVersion, Loaded, Hash);
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Hash;
	}
	return Errors;
}
