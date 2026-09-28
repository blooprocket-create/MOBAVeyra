// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rewards/VeyraRewardSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Gold/VeyraGoldComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Rewards/VeyraRewardRules.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Units/VeyraUnit.h"
#include "VeyraEconomyLog.h"

void UVeyraRewardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraRewardSubsystem::OnDeath);
	}
}

void UVeyraRewardSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PassiveGoldTimer);
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDeath.Remove(DeathHandle);
		}
	}
	Super::Deinitialize();
}

void UVeyraRewardSubsystem::StartPassiveGold()
{
	const FVeyraPassiveGoldTuning& Passive = UVeyraEconomyTuningSubsystem::Get().PassiveGold;
	if (bStopped || !IsServer() || !(Passive.PerPayment > 0.0))
	{
		return;
	}
	// The first payment covers the first interval after the start, so the timer's first delay is never 0.
	GetWorld()->GetTimerManager().SetTimer(PassiveGoldTimer, this, &UVeyraRewardSubsystem::PayPassiveGold, static_cast<float>(Passive.IntervalSeconds),
		/*bLoop*/ true, static_cast<float>(Passive.StartSeconds + Passive.IntervalSeconds));
}

void UVeyraRewardSubsystem::PayPassiveGold()
{
	const double Payment = UVeyraEconomyTuningSubsystem::Get().PassiveGold.PerPayment;
	for (const FRecipient& Recipient : Recipients())
	{
		// Those on a side; a spectator plays no part.
		if (Recipient.Team != EVeyraTeam::None)
		{
			Recipient.Gold->Grant(Payment, EVeyraGoldReason::Passive);
		}
	}
}

void UVeyraRewardSubsystem::Stop()
{
	bStopped = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PassiveGoldTimer);
	}
}

void UVeyraRewardSubsystem::GrantStartingGold(APlayerState& Participant)
{
	if (UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>())
	{
		Gold->Grant(UVeyraEconomyTuningSubsystem::Get().Gold.Starting, EVeyraGoldReason::Starting);
	}
}

void UVeyraRewardSubsystem::OnDeath(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	// Kills are Vanguard terms (§5); World reports its own units' deaths with what only it knows.
	if (Victim && VeyraUnits::IsVanguard(Victim->GetOwner()))
	{
		RewardVanguardKill(Death);
	}
}

void UVeyraRewardSubsystem::RewardFluxbornDeath(const FVeyraDeathEvent& Death, const FVeyraContentId& Kind, double ActiveFlux)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	if (bStopped || !IsServer() || !Victim || !Death.Location.IsSet())
	{
		return;
	}
	const FVeyraEconomyTuning& Tuning = UVeyraEconomyTuningSubsystem::Get();
	// A Fluxborn strengthened by its team's Flux pays a little more (§4).
	const double Multiplier = VeyraRewards::FluxBonusMultiplier(ActiveFlux, Tuning.FluxBonus);
	const double Gold = Tuning.Gold.Fluxborn.FindRef(Kind) * Multiplier;
	const double Experience = Tuning.Experience.Fluxborn.FindRef(Kind) * Multiplier;
	const EVeyraTeam Allies = VeyraTeams::Opposing(VeyraTeams::TeamOf(Victim->GetOwner()));
	const FVector Where = Death.Location.GetValue();
	const TArray<FRecipient> All = Recipients();

	// The last hit's Gold goes to the Vanguard who landed it, wherever they stand; a Fluxborn's or a
	// structure's last hit leaves it unclaimed (§3.1).
	const FRecipient* LastHitter = FindRecipient(All, Death.Killer.Get());
	if (LastHitter && LastHitter->Team == Allies)
	{
		LastHitter->Gold->Grant(Gold, EVeyraGoldReason::LastHit);
	}
	else
	{
		LastHitter = nullptr;
	}

	TArray<const FRecipient*> Near;
	for (const FRecipient& Recipient : All)
	{
		if (Recipient.Team == Allies && Recipient.bAlive && IsNear(Recipient, Where))
		{
			Near.Add(&Recipient);
		}
	}
	// The other nearby allies share only when an allied Vanguard fought it recently (§3.2).
	const double Window = Tuning.Eligibility.FluxbornParticipationSeconds;
	const bool bFought = Death.Contributions.ContainsByPredicate([this, &All, Allies, Window, &Death](const FVeyraContribution& Contribution) {
		const FRecipient* Contributor = FindRecipient(All, Contribution.Contributor.Get());
		return Contributor && Contributor->Team == Allies && Death.DiedAtSeconds - Contribution.AtSeconds <= Window;
	});
	if (bFought)
	{
		for (const FRecipient* Recipient : Near)
		{
			if (Recipient != LastHitter)
			{
				Recipient->Gold->Grant(Gold * Tuning.Gold.ParticipationFraction, EVeyraGoldReason::Participation);
			}
		}
	}
	// Its XP is for the nearby living allies who can still gain it, whoever fought (§3.3).
	const TArray<const FRecipient*> Leveling = Near.FilterByPredicate([](const FRecipient* Recipient) { return CanGainExperience(*Recipient); });
	const double Share = VeyraRewards::FarmXpShare(Experience, Leveling.Num(), Tuning.Experience.SharedPoolFraction);
	for (const FRecipient* Recipient : Leveling)
	{
		GrantExperience(*Recipient, Share);
	}
}

void UVeyraRewardSubsystem::RewardVanguardKill(const FVeyraDeathEvent& Death)
{
	if (bStopped || !IsServer())
	{
		return;
	}
	const TArray<FRecipient> All = Recipients();
	const FRecipient* Victim = FindRecipient(All, Death.Victim.Get());
	const FRecipient* Killer = FindRecipient(All, Death.CreditedKiller.Get());
	// An Execution, with no enemy credited, pays nothing (§5.2).
	if (!Victim || !Killer || Killer->Team == Victim->Team)
	{
		return;
	}
	const FVeyraEconomyTuning& Tuning = UVeyraEconomyTuningSubsystem::Get();
	const double KillGold = Tuning.Gold.VanguardKill;
	Killer->Gold->Grant(KillGold, EVeyraGoldReason::Kill);
	if (!bFirstBloodTaken)
	{
		// The match's first enemy-credited kill; one per match (§5.2).
		bFirstBloodTaken = true;
		Killer->Gold->Grant(KillGold * Tuning.Gold.FirstBloodFraction, EVeyraGoldReason::FirstBlood);
	}
	TArray<const FRecipient*> Assisters;
	for (const TWeakObjectPtr<UAbilitySystemComponent>& Assister : Death.Assisters)
	{
		if (const FRecipient* Recipient = FindRecipient(All, Assister.Get()))
		{
			Assisters.Add(Recipient);
		}
	}
	const double AssistGold = VeyraRewards::AssistShare(KillGold, Assisters.Num(), Tuning.Gold.AssistPoolFraction);
	for (const FRecipient* Assister : Assisters)
	{
		Assister->Gold->Grant(AssistGold, EVeyraGoldReason::Assist);
	}

	// Kill XP (§6): the killer always takes part; assisters only while living and near the death.
	TArray<const FRecipient*> Participants = { Killer };
	for (const FRecipient* Assister : Assisters)
	{
		if (Assister->bAlive && Death.Location.IsSet() && IsNear(*Assister, Death.Location.GetValue()))
		{
			Participants.Add(Assister);
		}
	}
	const int32 VictimLevel = Victim->Progression->GetLevel();
	const double Pool = VeyraRewards::KillExperiencePool(VeyraRewards::KillExperience(VictimLevel, Tuning.Experience), Participants.Num(),
		VictimLevel > Killer->Progression->GetLevel(), Tuning.Experience);
	// Those at the cap count toward the pool but take none of it (§6).
	const TArray<const FRecipient*> Leveling = Participants.FilterByPredicate([](const FRecipient* Recipient) { return CanGainExperience(*Recipient); });
	for (const FRecipient* Recipient : Leveling)
	{
		GrantExperience(*Recipient, Pool / Leveling.Num());
	}
	UE_LOG(LogVeyraEconomy, Log, TEXT("%s killed %s with %d assist(s); %d share(s) of %.1f kill XP."), *GetNameSafe(Killer->AbilitySystem->GetOwner()),
		*GetNameSafe(Victim->AbilitySystem->GetOwner()), Assisters.Num(), Leveling.Num(), Pool);
}

void UVeyraRewardSubsystem::RewardStructureDestroyed(const FVeyraDeathEvent& Death, EVeyraTeam Team)
{
	if (bStopped || !IsServer())
	{
		return;
	}
	const FVeyraEconomyTuning& Tuning = UVeyraEconomyTuningSubsystem::Get();
	const EVeyraTeam Destroyers = VeyraTeams::Opposing(Team);
	const TArray<FRecipient> All = Recipients();
	// Its recent contributors split the pool, dead or away; the last hit earns nothing more (§8.1).
	TArray<const FRecipient*> Contributors;
	for (const FVeyraContribution& Contribution : Death.Contributions)
	{
		const FRecipient* Recipient = FindRecipient(All, Contribution.Contributor.Get());
		if (Recipient && Recipient->Team == Destroyers && Death.DiedAtSeconds - Contribution.AtSeconds <= Tuning.Eligibility.StructureContributionSeconds)
		{
			Contributors.AddUnique(Recipient);
		}
	}
	for (const FRecipient* Contributor : Contributors)
	{
		Contributor->Gold->Grant(Tuning.Gold.StructurePool / Contributors.Num(), EVeyraGoldReason::StructurePool);
	}
	// The first to fall in the match pays the whole destroying team, however it fell (§8.1).
	if (!bFirstStructureTaken)
	{
		bFirstStructureTaken = true;
		for (const FRecipient& Recipient : All)
		{
			if (Recipient.Team == Destroyers)
			{
				Recipient.Gold->Grant(Tuning.Gold.FirstStructureBonus, EVeyraGoldReason::FirstStructure);
			}
		}
	}
}

TArray<UVeyraRewardSubsystem::FRecipient> UVeyraRewardSubsystem::Recipients() const
{
	TArray<FRecipient> All;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return All;
	}
	for (TActorIterator<APlayerState> It(World); It; ++It)
	{
		APlayerState* Participant = *It;
		FRecipient Recipient;
		Recipient.AbilitySystem = Participant ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Participant) : nullptr;
		Recipient.Gold = Participant ? Participant->FindComponentByClass<UVeyraGoldComponent>() : nullptr;
		Recipient.Progression = Participant ? Participant->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
		if (!Recipient.AbilitySystem || !Recipient.Gold || !Recipient.Progression)
		{
			continue;
		}
		Recipient.Team = VeyraTeams::TeamOf(Participant);
		// Until it has a Vanguard the PlayerState is its own avatar, and stands nowhere.
		const AActor* Body = Recipient.AbilitySystem->GetAvatarActor();
		if (Body && Body != Participant)
		{
			Recipient.Location = Body->GetActorLocation();
		}
		Recipient.bAlive = VeyraTargeting::IsAlive(Participant);
		All.Add(Recipient);
	}
	return All;
}

const UVeyraRewardSubsystem::FRecipient* UVeyraRewardSubsystem::FindRecipient(TConstArrayView<FRecipient> All, const UAbilitySystemComponent* AbilitySystem) const
{
	return AbilitySystem ? All.FindByPredicate([AbilitySystem](const FRecipient& Recipient) { return Recipient.AbilitySystem == AbilitySystem; }) : nullptr;
}

bool UVeyraRewardSubsystem::IsNear(const FRecipient& Recipient, const FVector& Location)
{
	return Recipient.Location.IsSet() && FVector::Dist2D(Recipient.Location.GetValue(), Location) <= UVeyraEconomyTuningSubsystem::Get().Eligibility.Radius;
}

bool UVeyraRewardSubsystem::CanGainExperience(const FRecipient& Recipient)
{
	return Recipient.Progression->IsInitialized() && Recipient.Progression->GetLevel() < UVeyraProgressionTuningSubsystem::Get().MaxLevel;
}

void UVeyraRewardSubsystem::GrantExperience(const FRecipient& Recipient, double Amount) const
{
	if (Amount > 0.0)
	{
		Recipient.Progression->AddExperience(Amount);
	}
}

bool UVeyraRewardSubsystem::IsServer() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}
