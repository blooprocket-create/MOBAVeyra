// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraCombatTuningSubsystem.h"

#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraCombatTuning* GCombatTuningTestOverride = nullptr;
}

void UVeyraCombatTuningSubsystem::SetTestOverride(const FVeyraCombatTuning* Override)
{
	check(IsInGameThread());
	GCombatTuningTestOverride = Override;
}
#endif

void UVeyraCombatTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraCombatTuning& UVeyraCombatTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GCombatTuningTestOverride)
	{
		return *GCombatTuningTestOverride;
	}
#endif
	const UVeyraCombatTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraCombatTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Combat tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

VeyraTuning::FErrors UVeyraCombatTuningSubsystem::Reload()
{
	// As VeyraTuning::LoadDomain, with the domain's own checks before the hash is recorded.
	FVeyraCombatTuning Loaded;
	VeyraTuning::FDomainFiles Files;
	VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(Domain, Files);
	if (Errors.IsEmpty())
	{
		Errors = VeyraTuning::ValidateAndBind(Files.DocumentText, Files.SchemaText, FVeyraCombatTuning::SchemaVersion, Loaded);
	}
	if (Errors.IsEmpty())
	{
		Errors = VeyraCombatTuningRules::Validate(Loaded);
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Files.DocumentHash;
		VeyraTuning::RecordLoadedDomain(Domain, Files.DocumentHash);
	}
	return Errors;
}
