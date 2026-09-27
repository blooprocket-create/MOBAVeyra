// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraMatchTuningSubsystem.h"

#include "Engine/Engine.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"

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
	// Developer matches name Vanguards, which the Vanguards domain defines.
	Collection.InitializeDependency<UVeyraVanguardsTuningSubsystem>();
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
	const UVeyraVanguardsTuningSubsystem* Vanguards = GEngine ? GEngine->GetEngineSubsystem<UVeyraVanguardsTuningSubsystem>() : nullptr;
	if (Errors.IsEmpty() && (!Vanguards || !Vanguards->IsLoaded()))
	{
		Errors.Add(TEXT("Match.json names Vanguards, and the Vanguards tuning did not load"));
	}
	for (int32 Index = 0; Errors.IsEmpty() && Index < Loaded.DeveloperMatch.Vanguards.Num(); ++Index)
	{
		const FVeyraContentId& Vanguard = Loaded.DeveloperMatch.Vanguards[Index];
		if (!UVeyraVanguardsTuningSubsystem::FindVanguard(Vanguard))
		{
			Errors.Add(FString::Printf(TEXT("/developerMatch/vanguards/%d: names \"%s\", which Vanguards.json does not define"), Index, *Vanguard.ToString()));
		}
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Hash;
	}
	return Errors;
}
