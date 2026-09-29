// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraWorldTuningSubsystem.h"

#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraWorldTuning* GWorldTuningTestOverride = nullptr;
}

void UVeyraWorldTuningSubsystem::SetTestOverride(const FVeyraWorldTuning* Override)
{
	check(IsInGameThread());
	GWorldTuningTestOverride = Override;
}
#endif

void UVeyraWorldTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraWorldTuning& UVeyraWorldTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GWorldTuningTestOverride)
	{
		return *GWorldTuningTestOverride;
	}
#endif
	const UVeyraWorldTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraWorldTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("World tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

VeyraTuning::FErrors UVeyraWorldTuningSubsystem::Reload()
{
	// As VeyraTuning::LoadDomain, with the domain's own checks before the hash is recorded.
	FVeyraWorldTuning Loaded;
	VeyraTuning::FDomainFiles Files;
	VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(Domain, Files);
	if (Errors.IsEmpty())
	{
		Errors = VeyraTuning::ValidateAndBind(Files.DocumentText, Files.SchemaText, FVeyraWorldTuning::SchemaVersion, Loaded);
	}
	if (Errors.IsEmpty())
	{
		Errors = VeyraWorld::Validate(Loaded);
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Files.DocumentHash;
		VeyraTuning::RecordLoadedDomain(Domain, Files.DocumentHash);
	}
	return Errors;
}
