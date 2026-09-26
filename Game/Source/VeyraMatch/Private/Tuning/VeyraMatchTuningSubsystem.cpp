// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraMatchTuningSubsystem.h"

#include "Engine/Engine.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraMatchTuning* GTestOverride = nullptr;
}

void UVeyraMatchTuningSubsystem::SetTestOverride(const FVeyraMatchTuning* Override)
{
	check(IsInGameThread());
	GTestOverride = Override;
}
#endif

void UVeyraMatchTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The developer loadout names abilities, which the Abilities domain defines.
	Collection.InitializeDependency<UVeyraAbilitiesTuningSubsystem>();
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraMatchTuning& UVeyraMatchTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GTestOverride)
	{
		return *GTestOverride;
	}
#endif
	const UVeyraMatchTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraMatchTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Match tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

VeyraTuning::FErrors UVeyraMatchTuningSubsystem::Reload()
{
	FVeyraMatchTuning Loaded;
	FBlake3Hash Hash;
	VeyraTuning::FErrors Errors = VeyraTuning::LoadDomain(Domain, FVeyraMatchTuning::SchemaVersion, Loaded, Hash);
	if (Errors.IsEmpty() && Loaded.DeveloperLoadout.CapsuleHalfHeight < Loaded.DeveloperLoadout.CapsuleRadius)
	{
		// A capsule's half height includes its hemispherical ends, so it can never be shorter than its radius.
		Errors.Add(TEXT("/developerLoadout/capsuleHalfHeight: must be at least capsuleRadius"));
	}
	const UVeyraAbilitiesTuningSubsystem* Abilities = GEngine ? GEngine->GetEngineSubsystem<UVeyraAbilitiesTuningSubsystem>() : nullptr;
	if (Errors.IsEmpty() && (!Abilities || !Abilities->IsLoaded() || !UVeyraAbilitiesTuningSubsystem::Defines(Loaded.DeveloperLoadout.AbilityQ)))
	{
		Errors.Add(FString::Printf(TEXT("/developerLoadout/abilityQ: names \"%s\", which Abilities.json does not define"),
			*Loaded.DeveloperLoadout.AbilityQ.ToString()));
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Hash;
	}
	return Errors;
}
