// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraVanguardsTuningSubsystem.h"

#include "Engine/Engine.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraVanguardsTuning* GVanguardsTuningTestOverride = nullptr;
}

void UVeyraVanguardsTuningSubsystem::SetTestOverride(const FVeyraVanguardsTuning* Override)
{
	check(IsInGameThread());
	GVanguardsTuningTestOverride = Override;
}
#endif

void UVeyraVanguardsTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Kits name abilities, whose rank lists must suit the slots Progression defines.
	Collection.InitializeDependency<UVeyraAbilitiesTuningSubsystem>();
	Collection.InitializeDependency<UVeyraProgressionTuningSubsystem>();
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraVanguardsTuning& UVeyraVanguardsTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GVanguardsTuningTestOverride)
	{
		return *GVanguardsTuningTestOverride;
	}
#endif
	const UVeyraVanguardsTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraVanguardsTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Vanguards tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

const FVeyraVanguardDefinition* UVeyraVanguardsTuningSubsystem::FindVanguard(const FVeyraContentId& Vanguard)
{
	return Get().Vanguards.Find(Vanguard);
}

const FVeyraDeepFoundationTuning* UVeyraVanguardsTuningSubsystem::FindDeepFoundation(const FVeyraContentId& Passive)
{
	return Get().DeepFoundation.Find(Passive);
}

VeyraTuning::FErrors UVeyraVanguardsTuningSubsystem::Reload()
{
	// As VeyraTuning::LoadDomain, with the domain's own checks before the hash is recorded.
	FVeyraVanguardsTuning Loaded;
	VeyraTuning::FDomainFiles Files;
	VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(Domain, Files);
	if (Errors.IsEmpty())
	{
		Errors = VeyraTuning::ValidateAndBind(Files.DocumentText, Files.SchemaText, FVeyraVanguardsTuning::SchemaVersion, Loaded);
	}
	const UVeyraAbilitiesTuningSubsystem* Abilities = GEngine ? GEngine->GetEngineSubsystem<UVeyraAbilitiesTuningSubsystem>() : nullptr;
	if (Errors.IsEmpty() && (!Abilities || !Abilities->IsLoaded()))
	{
		Errors.Add(TEXT("Vanguards.json names abilities, and the Abilities tuning did not load"));
	}
	if (Errors.IsEmpty())
	{
		const FVeyraProgressionTuning& Progression = UVeyraProgressionTuningSubsystem::Get();
		Errors = VeyraVanguardRules::Validate(Loaded, UVeyraAbilitiesTuningSubsystem::Get(), Progression.BasicAbilityMaxRank, Progression.UltimateMaxRank);
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Files.DocumentHash;
		VeyraTuning::RecordLoadedDomain(Domain, Files.DocumentHash);
	}
	return Errors;
}
