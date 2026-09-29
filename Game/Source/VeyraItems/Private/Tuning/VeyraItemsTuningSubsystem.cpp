// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraItemsTuningSubsystem.h"

#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraItemsTuning* GItemsTuningTestOverride = nullptr;
}

void UVeyraItemsTuningSubsystem::SetTestOverride(const FVeyraItemsTuning* Override)
{
	check(IsInGameThread());
	GItemsTuningTestOverride = Override;
}
#endif

void UVeyraItemsTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraItemsTuning& UVeyraItemsTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GItemsTuningTestOverride)
	{
		return *GItemsTuningTestOverride;
	}
#endif
	const UVeyraItemsTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraItemsTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Items tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

const FVeyraItemDefinition* UVeyraItemsTuningSubsystem::FindItem(const FVeyraContentId& Id)
{
	return Get().Items.Find(Id);
}

VeyraTuning::FErrors UVeyraItemsTuningSubsystem::Reload()
{
	// As VeyraTuning::LoadDomain, with the domain's own checks before the hash is recorded.
	FVeyraItemsTuning Loaded;
	VeyraTuning::FDomainFiles Files;
	VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(Domain, Files);
	if (Errors.IsEmpty())
	{
		Errors = VeyraTuning::ValidateAndBind(Files.DocumentText, Files.SchemaText, FVeyraItemsTuning::SchemaVersion, Loaded);
	}
	if (Errors.IsEmpty())
	{
		Errors = VeyraItems::Validate(Loaded);
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Files.DocumentHash;
		VeyraTuning::RecordLoadedDomain(Domain, Files.DocumentHash);
	}
	return Errors;
}
