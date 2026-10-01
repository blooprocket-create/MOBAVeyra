// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#include "Engine/Engine.h"
#include "Progression/VeyraProgressionRules.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	const FVeyraAbilitiesTuning* GTestOverride = nullptr;
}

void UVeyraAbilitiesTuningSubsystem::SetTestOverride(const FVeyraAbilitiesTuning* Override)
{
	check(IsInGameThread());
	GTestOverride = Override;
}
#endif

void UVeyraAbilitiesTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Rank lists are checked against the ranks Progression allows.
	Collection.InitializeDependency<UVeyraProgressionTuningSubsystem>();
	const VeyraTuning::FErrors Errors = Reload();
	if (!Errors.IsEmpty())
	{
		VeyraTuning::ReportLoadFailure(Domain, Errors);
	}
}

const FVeyraAbilitiesTuning& UVeyraAbilitiesTuningSubsystem::Get()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (GTestOverride)
	{
		return *GTestOverride;
	}
#endif
	const UVeyraAbilitiesTuningSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<UVeyraAbilitiesTuningSubsystem>() : nullptr;
	checkf(Subsystem && Subsystem->Tuning.IsSet(), TEXT("Abilities tuning is not loaded; see the tuning errors earlier in the log."));
	return Subsystem->Tuning.GetValue();
}

const FVeyraTargetedDamageAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(const FVeyraContentId& Ability)
{
	return Get().TargetedDamage.Find(Ability);
}

const FVeyraAreaAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindArea(const FVeyraContentId& Ability)
{
	return Get().Area.Find(Ability);
}

const FVeyraSelfBuffAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindSelfBuff(const FVeyraContentId& Ability)
{
	return Get().SelfBuff.Find(Ability);
}

const FVeyraSkillshotAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindSkillshot(const FVeyraContentId& Ability)
{
	return Get().Skillshot.Find(Ability);
}

const FVeyraDashAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindDash(const FVeyraContentId& Ability)
{
	return Get().Dash.Find(Ability);
}

const FVeyraEmpoweredAttackAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindEmpoweredAttack(const FVeyraContentId& Ability)
{
	return Get().EmpoweredAttack.Find(Ability);
}

const FVeyraVolleyAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindVolley(const FVeyraContentId& Ability)
{
	return Get().Volley.Find(Ability);
}

const FVeyraTetherAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindTether(const FVeyraContentId& Ability)
{
	return Get().Tether.Find(Ability);
}

const FVeyraAttachAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindAttach(const FVeyraContentId& Ability)
{
	return Get().Attach.Find(Ability);
}

const FVeyraRideAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindRide(const FVeyraContentId& Ability)
{
	return Get().Ride.Find(Ability);
}

const FVeyraAmbushAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindAmbush(const FVeyraContentId& Ability)
{
	return Get().Ambush.Find(Ability);
}

const FVeyraStanceAbilityTuning* UVeyraAbilitiesTuningSubsystem::FindStance(const FVeyraContentId& Ability)
{
	return Get().Stance.Find(Ability);
}

TOptional<FVeyraStatusSpec> UVeyraAbilitiesTuningSubsystem::FindStatus(const FVeyraContentId& Id, int32 SourceLevel)
{
	const FVeyraStatusTuning* Status = Get().Statuses.Find(Id);
	return Status ? TOptional<FVeyraStatusSpec>(VeyraAbilityRules::ToStatusSpec(Id, *Status, SourceLevel)) : TOptional<FVeyraStatusSpec>();
}

bool UVeyraAbilitiesTuningSubsystem::Defines(const FVeyraContentId& Ability)
{
	return VeyraAbilityRules::Defines(Get(), Ability);
}

VeyraTuning::FErrors UVeyraAbilitiesTuningSubsystem::Reload()
{
	// As VeyraTuning::LoadDomain, with the domain's own checks before the hash is recorded.
	FVeyraAbilitiesTuning Loaded;
	VeyraTuning::FDomainFiles Files;
	VeyraTuning::FErrors Errors = VeyraTuning::ReadDomainFiles(Domain, Files);
	if (Errors.IsEmpty())
	{
		Errors = VeyraTuning::ValidateAndBind(Files.DocumentText, Files.SchemaText, FVeyraAbilitiesTuning::SchemaVersion, Loaded);
	}
	if (Errors.IsEmpty())
	{
		// One value per rank for any count of ranks a rank shape gives a slot (ADR-031 §2).
		Errors = VeyraAbilityRules::Validate(Loaded, VeyraProgression::RankCounts(UVeyraProgressionTuningSubsystem::Get()));
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Files.DocumentHash;
		VeyraTuning::RecordLoadedDomain(Domain, Files.DocumentHash);
	}
	return Errors;
}
