// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraFluxTuningSubsystem.h"

#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraFluxTuning* GFluxTuningTestOverride = nullptr;
}

void UVeyraFluxTuningSubsystem::SetTestOverride(const FVeyraFluxTuning* Override)
{
	check(IsInGameThread());
	GFluxTuningTestOverride = Override;
}
#endif

void UVeyraFluxTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraFluxTuning& UVeyraFluxTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GFluxTuningTestOverride)
	{
		return *GFluxTuningTestOverride;
	}
#endif
	const UVeyraFluxTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraFluxTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Flux tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

VeyraTuning::FErrors UVeyraFluxTuningSubsystem::Reload()
{
	// As VeyraTuning::LoadDomain, with the domain's own checks before the hash is recorded.
	FVeyraFluxTuning Loaded;
	VeyraTuning::FDomainFiles Files;
	VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(Domain, Files);
	if (Errors.IsEmpty())
	{
		Errors = VeyraTuning::ValidateAndBind(Files.DocumentText, Files.SchemaText, FVeyraFluxTuning::SchemaVersion, Loaded);
	}
	if (Errors.IsEmpty())
	{
		Errors = VeyraFlux::Validate(Loaded);
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Files.DocumentHash;
		VeyraTuning::RecordLoadedDomain(Domain, Files.DocumentHash);
	}
	return Errors;
}
