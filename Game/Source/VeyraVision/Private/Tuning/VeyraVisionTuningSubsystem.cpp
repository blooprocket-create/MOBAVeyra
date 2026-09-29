// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraVisionTuningSubsystem.h"

#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraVisionTuning* GVisionTuningTestOverride = nullptr;
}

void UVeyraVisionTuningSubsystem::SetTestOverride(const FVeyraVisionTuning* Override)
{
	check(IsInGameThread());
	GVisionTuningTestOverride = Override;
}
#endif

void UVeyraVisionTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraVisionTuning& UVeyraVisionTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GVisionTuningTestOverride)
	{
		return *GVisionTuningTestOverride;
	}
#endif
	const UVeyraVisionTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraVisionTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Vision tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

VeyraTuning::FErrors UVeyraVisionTuningSubsystem::Reload()
{
	// As VeyraTuning::LoadDomain, with the domain's own checks before the hash is recorded.
	FVeyraVisionTuning Loaded;
	VeyraTuning::FDomainFiles Files;
	VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(Domain, Files);
	if (Errors.IsEmpty())
	{
		Errors = VeyraTuning::ValidateAndBind(Files.DocumentText, Files.SchemaText, FVeyraVisionTuning::SchemaVersion, Loaded);
	}
	if (Errors.IsEmpty())
	{
		Errors = VeyraVision::Validate(Loaded);
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Files.DocumentHash;
		VeyraTuning::RecordLoadedDomain(Domain, Files.DocumentHash);
	}
	return Errors;
}
