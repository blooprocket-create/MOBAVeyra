// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rewards/VeyraEconomyTuningSubsystem.h"

#include "Engine/Engine.h"
#include "Rewards/VeyraRewardRules.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraEconomyTuning* GEconomyTuningTestOverride = nullptr;
}

void UVeyraEconomyTuningSubsystem::SetTestOverride(const FVeyraEconomyTuning* Override)
{
	check(IsInGameThread());
	GEconomyTuningTestOverride = Override;
}
#endif

void UVeyraEconomyTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraEconomyTuning& UVeyraEconomyTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GEconomyTuningTestOverride)
	{
		return *GEconomyTuningTestOverride;
	}
#endif
	const UVeyraEconomyTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraEconomyTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Economy tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

VeyraTuning::FErrors UVeyraEconomyTuningSubsystem::Reload()
{
	// As VeyraTuning::LoadDomain, with the domain's own checks before the hash is recorded.
	FVeyraEconomyTuning Loaded;
	VeyraTuning::FDomainFiles Files;
	VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(Domain, Files);
	if (Errors.IsEmpty())
	{
		Errors = VeyraTuning::ValidateAndBind(Files.DocumentText, Files.SchemaText, FVeyraEconomyTuning::SchemaVersion, Loaded);
	}
	if (Errors.IsEmpty())
	{
		Errors = VeyraRewards::Validate(Loaded);
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Files.DocumentHash;
		VeyraTuning::RecordLoadedDomain(Domain, Files.DocumentHash);
	}
	return Errors;
}
