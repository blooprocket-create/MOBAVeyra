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

const FVeyraHitChainTuning* UVeyraVanguardsTuningSubsystem::FindHitChain(const FVeyraContentId& Passive)
{
	return Get().HitChain.Find(Passive);
}

const FVeyraGatheringLightTuning* UVeyraVanguardsTuningSubsystem::FindGatheringLight(const FVeyraContentId& Passive)
{
	return Get().GatheringLight.Find(Passive);
}

const FVeyraBreachTuning* UVeyraVanguardsTuningSubsystem::FindBreach(const FVeyraContentId& Passive)
{
	return Get().Breach.Find(Passive);
}

const FVeyraMovingTargetTuning* UVeyraVanguardsTuningSubsystem::FindMovingTarget(const FVeyraContentId& Passive)
{
	return Get().MovingTarget.Find(Passive);
}

const FVeyraCadenceTuning* UVeyraVanguardsTuningSubsystem::FindCadence(const FVeyraContentId& Passive)
{
	return Get().Cadence.Find(Passive);
}

const FVeyraMarkProcTuning* UVeyraVanguardsTuningSubsystem::FindMarkProc(const FVeyraContentId& Passive)
{
	return Get().MarkProc.Find(Passive);
}

const FVeyraHauntTuning* UVeyraVanguardsTuningSubsystem::FindHaunt(const FVeyraContentId& Passive)
{
	return Get().Haunt.Find(Passive);
}

const FVeyraCampRewardTuning* UVeyraVanguardsTuningSubsystem::FindCampReward(const FVeyraContentId& Passive)
{
	return Get().CampReward.Find(Passive);
}

const FVeyraMomentumTuning* UVeyraVanguardsTuningSubsystem::FindMomentum(const FVeyraContentId& Passive)
{
	return Get().Momentum.Find(Passive);
}

const FVeyraWildDominionTuning* UVeyraVanguardsTuningSubsystem::FindWildDominion(const FVeyraContentId& Passive)
{
	return Get().WildDominion.Find(Passive);
}

const FVeyraKitStatusesTuning* UVeyraVanguardsTuningSubsystem::FindKitStatuses(const FVeyraContentId& Passive)
{
	return Get().KitStatuses.Find(Passive);
}

const FVeyraAttackStrideTuning* UVeyraVanguardsTuningSubsystem::FindAttackStride(const FVeyraContentId& Passive)
{
	return Get().AttackStride.Find(Passive);
}

const FVeyraSlipstreamTuning* UVeyraVanguardsTuningSubsystem::FindSlipstream(const FVeyraContentId& Passive)
{
	return Get().Slipstream.Find(Passive);
}

const FVeyraReclaimTuning* UVeyraVanguardsTuningSubsystem::FindReclaim(const FVeyraContentId& Passive)
{
	return Get().Reclaim.Find(Passive);
}

const FVeyraQuarryTuning* UVeyraVanguardsTuningSubsystem::FindQuarry(const FVeyraContentId& Passive)
{
	return Get().Quarry.Find(Passive);
}

const FVeyraDisciplinesTuning* UVeyraVanguardsTuningSubsystem::FindDisciplines(const FVeyraContentId& Passive)
{
	return Get().Disciplines.Find(Passive);
}

const FVeyraStressTemperTuning* UVeyraVanguardsTuningSubsystem::FindStressTemper(const FVeyraContentId& Passive)
{
	return Get().StressTemper.Find(Passive);
}

const FVeyraChargerTuning* UVeyraVanguardsTuningSubsystem::FindCharger(const FVeyraContentId& Passive)
{
	return Get().Charger.Find(Passive);
}

const FVeyraAccordTuning* UVeyraVanguardsTuningSubsystem::FindAccord(const FVeyraContentId& Passive)
{
	return Get().Accord.Find(Passive);
}

const FVeyraMistTrailTuning* UVeyraVanguardsTuningSubsystem::FindMistTrail(const FVeyraContentId& Passive)
{
	return Get().MistTrail.Find(Passive);
}

const FVeyraAllHandsTuning* UVeyraVanguardsTuningSubsystem::FindAllHands(const FVeyraContentId& Passive)
{
	return Get().AllHands.Find(Passive);
}

const FVeyraUnreturnedTuning* UVeyraVanguardsTuningSubsystem::FindUnreturned(const FVeyraContentId& Passive)
{
	return Get().Unreturned.Find(Passive);
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
		Errors = VeyraVanguardRules::Validate(Loaded, UVeyraAbilitiesTuningSubsystem::Get(), Progression);
	}
	if (Errors.IsEmpty())
	{
		Tuning = Loaded;
		DocumentHash = Files.DocumentHash;
		VeyraTuning::RecordLoadedDomain(Domain, Files.DocumentHash);
	}
	return Errors;
}
