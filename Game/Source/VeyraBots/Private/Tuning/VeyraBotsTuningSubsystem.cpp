// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraBotsTuningSubsystem.h"

#include "Engine/Engine.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraBotsTuning* GBotsTuningTestOverride = nullptr;
}

void UVeyraBotsTuningSubsystem::SetTestOverride(const FVeyraBotsTuning* Override)
{
	check(IsInGameThread());
	GBotsTuningTestOverride = Override;
}
#endif

void UVeyraBotsTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Builds name items, every released Vanguard needs its entry, and each ability named one a bot can aim.
	Collection.InitializeDependency<UVeyraAbilitiesTuningSubsystem>();
	Collection.InitializeDependency<UVeyraItemsTuningSubsystem>();
	Collection.InitializeDependency<UVeyraVanguardsTuningSubsystem>();
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraBotsTuning& UVeyraBotsTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GBotsTuningTestOverride)
	{
		return *GBotsTuningTestOverride;
	}
#endif
	const UVeyraBotsTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraBotsTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Bots tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

const FVeyraBotDifficultyTuning& UVeyraBotsTuningSubsystem::GetDifficulty(EVeyraBotDifficulty Difficulty)
{
	const FVeyraBotDifficultiesTuning& Difficulties = Get().Difficulties;
	return Difficulty == EVeyraBotDifficulty::Intermediate ? Difficulties.Intermediate : Difficulties.Beginner;
}

const FVeyraBotVanguardTuning* UVeyraBotsTuningSubsystem::FindVanguard(const FVeyraContentId& Vanguard)
{
	return Get().Vanguards.Find(Vanguard);
}

VeyraTuning::FErrors UVeyraBotsTuningSubsystem::Reload()
{
	// As VeyraTuning::LoadDomain, with the domain's own checks before the hash is recorded.
	FVeyraBotsTuning Loaded;
	VeyraTuning::FDomainFiles Files;
	VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(Domain, Files);
	if (Errors.IsEmpty())
	{
		Errors = VeyraTuning::ValidateAndBind(Files.DocumentText, Files.SchemaText, FVeyraBotsTuning::SchemaVersion, Loaded);
	}
	const UVeyraItemsTuningSubsystem* Items = GEngine ? GEngine->GetEngineSubsystem<UVeyraItemsTuningSubsystem>() : nullptr;
	const UVeyraVanguardsTuningSubsystem* Vanguards = GEngine ? GEngine->GetEngineSubsystem<UVeyraVanguardsTuningSubsystem>() : nullptr;
	if (Errors.IsEmpty() && (!Items || !Items->IsLoaded() || !Vanguards || !Vanguards->IsLoaded()))
	{
		Errors.Add(TEXT("Bots.json names items and Vanguards, and the Items or Vanguards tuning did not load"));
	}
	if (Errors.IsEmpty())
	{
		Errors = VeyraBots::Validate(Loaded);
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Files.DocumentHash;
		VeyraTuning::RecordLoadedDomain(Domain, Files.DocumentHash);
	}
	return Errors;
}
