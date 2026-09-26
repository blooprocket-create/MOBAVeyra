// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraCombatTuningSubsystem.h"

#include "Engine/Engine.h"

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
	const UVeyraCombatTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraCombatTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Combat tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

VeyraTuning::FErrors UVeyraCombatTuningSubsystem::Reload()
{
	FVeyraCombatTuning Loaded;
	FBlake3Hash Hash;
	VeyraTuning::FErrors Errors = VeyraTuning::LoadDomain(Domain, FVeyraCombatTuning::SchemaVersion, Loaded, Hash);
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Hash;
	}
	return Errors;
}
