// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Progression/VeyraProgressionTuningSubsystem.h"

#include "Engine/Engine.h"
#include "Progression/VeyraProgressionRules.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraProgressionTuning* GProgressionTuningTestOverride = nullptr;
}

void UVeyraProgressionTuningSubsystem::SetTestOverride(const FVeyraProgressionTuning* Override)
{
	check(IsInGameThread());
	GProgressionTuningTestOverride = Override;
}
#endif

void UVeyraProgressionTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraProgressionTuning& UVeyraProgressionTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GProgressionTuningTestOverride)
	{
		return *GProgressionTuningTestOverride;
	}
#endif
	const UVeyraProgressionTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraProgressionTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Progression tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

VeyraTuning::FErrors UVeyraProgressionTuningSubsystem::Reload()
{
	// As VeyraTuning::LoadDomain, with the domain's own checks before the hash is recorded.
	FVeyraProgressionTuning Loaded;
	VeyraTuning::FDomainFiles Files;
	VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(Domain, Files);
	if (Errors.IsEmpty())
	{
		Errors = VeyraTuning::ValidateAndBind(Files.DocumentText, Files.SchemaText, FVeyraProgressionTuning::SchemaVersion, Loaded);
	}
	if (Errors.IsEmpty())
	{
		Errors = VeyraProgression::Validate(Loaded);
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Files.DocumentHash;
		VeyraTuning::RecordLoadedDomain(Domain, Files.DocumentHash);
	}
	return Errors;
}
