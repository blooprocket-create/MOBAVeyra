// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Statistics/VeyraMatchStatisticsSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Statistics/VeyraScoreComponent.h"
#include "Structures/VeyraStructure.h"
#include "Teams/VeyraTeam.h"
#include "Units/VeyraUnit.h"
#include "VeyraMatchLog.h"
#include "VeyraPlayerState.h"
#include "VeyraVisionSubsystem.h"
#include "Wards/VeyraWard.h"
#include "Wells/VeyraFluxWellSubsystem.h"

namespace VeyraStatisticsService
{
bool AreEnemies(const UObject* A, const UObject* B)
{
	const EVeyraTeam Side = VeyraTeams::TeamOf(A);
	return Side != EVeyraTeam::None && VeyraTeams::TeamOf(B) == VeyraTeams::Opposing(Side);
}

void AddByType(FVeyraDamageByType& Damage, EVeyraDamageType Type, double Amount)
{
	switch (Type)
	{
	case EVeyraDamageType::Physical:
		Damage.Physical += Amount;
		break;
	case EVeyraDamageType::Magic:
		Damage.Magic += Amount;
		break;
	case EVeyraDamageType::TrueDamage:
		Damage.TrueDamage += Amount;
		break;
	}
}

/** Whether Unit is an enemy tower, whose damage counts apart (Match Statistics Bible §2): a lane Spire or base tower. */
bool IsTower(const AActor* Unit)
{
	const AVeyraStructure* Structure = Cast<AVeyraStructure>(Unit);
	return Structure
		&& (Structure->GetStructureKind() == EVeyraStructureKind::LaneSpire || Structure->GetStructureKind() == EVeyraStructureKind::BaseTower);
}

/** The crowd control the statistics count (ADR-017 §9.3): the kinds that exist. */
bool IsCountedCrowdControl(EVeyraStatusKind Kind)
{
	return Kind == EVeyraStatusKind::Stun || Kind == EVeyraStatusKind::Slow;
}

bool IsServer(const UWorld* World)
{
	return World && World->GetNetMode() != NM_Client;
}
}

void UVeyraMatchStatisticsSubsystem::Start()
{
	UWorld* World = GetWorld();
	if (bRecording || bStopped || !VeyraStatisticsService::IsServer(World))
	{
		return;
	}
	bRecording = true;
	if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraMatchStatisticsSubsystem::OnDeath);
		DamageHandle = Events->OnDamageResolved.AddUObject(this, &UVeyraMatchStatisticsSubsystem::OnDamageResolved);
		HealingHandle = Events->OnHealthRestored.AddUObject(this, &UVeyraMatchStatisticsSubsystem::OnHealthRestored);
		StatusHandle = Events->OnStatusApplied.AddUObject(this, &UVeyraMatchStatisticsSubsystem::OnStatusApplied);
	}
	if (UVeyraVisionSubsystem* Vision = World->GetSubsystem<UVeyraVisionSubsystem>())
	{
		WardHandle = Vision->OnWardPlaced.AddUObject(this, &UVeyraMatchStatisticsSubsystem::OnWardPlaced);
	}
	if (UVeyraFluxWellSubsystem* Wells = World->GetSubsystem<UVeyraFluxWellSubsystem>())
	{
		WellHandle = Wells->OnFluxWellSecured.AddUObject(this, &UVeyraMatchStatisticsSubsystem::OnFluxWellSecured);
	}
}

void UVeyraMatchStatisticsSubsystem::Stop()
{
	if (!bRecording)
	{
		return;
	}
	bRecording = false;
	bStopped = true;
	UWorld* World = GetWorld();
	StoppedAt = World ? World->GetTimeSeconds() : 0.0;
	if (UVeyraCombatEventSubsystem* Events = World ? World->GetSubsystem<UVeyraCombatEventSubsystem>() : nullptr)
	{
		Events->OnDeath.Remove(DeathHandle);
		Events->OnDamageResolved.Remove(DamageHandle);
		Events->OnHealthRestored.Remove(HealingHandle);
		Events->OnStatusApplied.Remove(StatusHandle);
	}
	if (UVeyraVisionSubsystem* Vision = World ? World->GetSubsystem<UVeyraVisionSubsystem>() : nullptr)
	{
		Vision->OnWardPlaced.Remove(WardHandle);
	}
	if (UVeyraFluxWellSubsystem* Wells = World ? World->GetSubsystem<UVeyraFluxWellSubsystem>() : nullptr)
	{
		Wells->OnFluxWellSecured.Remove(WellHandle);
	}
	for (FRecord& Record : Records)
	{
		if (UVeyraGoldComponent* Gold = Record.Participant.IsValid() ? Record.Participant->FindComponentByClass<UVeyraGoldComponent>() : nullptr)
		{
			Gold->OnGoldGranted.Remove(Record.GoldHandle);
		}
		// Game/Scripts/Smoke.ps1 reads these lines.
		const TOptional<FVeyraPlayerStatistics> Final = Record.Participant.IsValid() ? Snapshot(*Record.Participant) : TOptional<FVeyraPlayerStatistics>();
		UE_CLOG(Final.IsSet(), LogVeyraMatch, Display,
			TEXT("Statistics for %s: %d/%d/%d, level %d, %d minion and %d jungle last hits, %.0f damage to Vanguards, %.0f to towers, %.0f Gold earned, %d ward(s) placed."),
			*Record.Participant->GetPlayerName(), Final->Kills, Final->Deaths, Final->Assists, Final->Level, Final->MinionKills, Final->JungleKills,
			Final->VanguardDamage, Final->TowerDamage, Final->GoldEarned, Final->WardsPlaced);
	}
}

void UVeyraMatchStatisticsSubsystem::AddParticipant(AVeyraPlayerState& Participant)
{
	if (!VeyraStatisticsService::IsServer(GetWorld()) || Find(&Participant))
	{
		return;
	}
	FRecord& Record = Records.AddDefaulted_GetRef();
	Record.Participant = &Participant;
	Record.Unit = Participant.GetAbilitySystemComponent();
	if (UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>())
	{
		Record.GoldHandle = Gold->OnGoldGranted.AddUObject(this, &UVeyraMatchStatisticsSubsystem::OnGoldGranted, TWeakObjectPtr<AVeyraPlayerState>(&Participant));
	}
}

TOptional<FVeyraPlayerStatistics> UVeyraMatchStatisticsSubsystem::Snapshot(const AVeyraPlayerState& Participant) const
{
	const FRecord* Record = Find(Participant);
	if (!Record)
	{
		return {};
	}
	FVeyraPlayerStatistics Statistics = Record->Statistics;
	// Earned Gold is the sum of its sources, so the two always reconcile (§7).
	Statistics.GoldEarned = Statistics.GoldBySource.Total();

	// Each source's crowd control on each target counts once where its applications overlap (§4).
	const double Until = RecordedUntil();
	for (const TPair<TPair<FObjectKey, uint8>, TArray<FVeyraSpan>>& Entry : Record->CrowdControl)
	{
		TArray<FVeyraSpan> Spans = Entry.Value;
		for (FVeyraSpan& Span : Spans)
		{
			Span.End = FMath::Min(Span.End, Until);
		}
		const double Seconds = VeyraStatisticsRules::UnionSeconds(MoveTemp(Spans));
		(static_cast<EVeyraStatusKind>(Entry.Key.Value) == EVeyraStatusKind::Stun ? Statistics.CrowdControl.Stun : Statistics.CrowdControl.Slow) += Seconds;
	}

	// Its level and equipment as they are now (§8).
	if (const UVeyraProgressionComponent* Progression = Participant.FindComponentByClass<UVeyraProgressionComponent>())
	{
		Statistics.Level = Progression->GetLevel();
	}
	Statistics.Items.Reset();
	if (const UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>())
	{
		for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
		{
			Statistics.Items.Add(Slot.IsEmpty() ? FVeyraContentId() : Slot.Item);
		}
	}
	Statistics.FluxSpells.Reset();
	const UVeyraAbilityLoadoutComponent* Loadout = Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
	for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Spells)
	{
		const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Slot) : nullptr;
		Statistics.FluxSpells.Add(Entry ? Entry->Ability : FVeyraContentId());
	}
	return Statistics;
}

void UVeyraMatchStatisticsSubsystem::Deinitialize()
{
	Stop();
	Records.Reset();
	Super::Deinitialize();
}

UVeyraMatchStatisticsSubsystem::FRecord* UVeyraMatchStatisticsSubsystem::Find(const UAbilitySystemComponent* Unit)
{
	return Unit ? Records.FindByPredicate([Unit](const FRecord& Record) { return Record.Unit.Get() == Unit; }) : nullptr;
}

UVeyraMatchStatisticsSubsystem::FRecord* UVeyraMatchStatisticsSubsystem::Find(const APlayerState* Participant)
{
	return Participant ? Records.FindByPredicate([Participant](const FRecord& Record) { return Record.Participant.Get() == Participant; }) : nullptr;
}

const UVeyraMatchStatisticsSubsystem::FRecord* UVeyraMatchStatisticsSubsystem::Find(const AVeyraPlayerState& Participant) const
{
	return Records.FindByPredicate([&Participant](const FRecord& Record) { return Record.Participant.Get() == &Participant; });
}

void UVeyraMatchStatisticsSubsystem::OnDeath(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	const AActor* VictimUnit = Victim ? Victim->GetOwner() : nullptr;
	if (!VictimUnit)
	{
		return;
	}
	if (VeyraUnits::IsVanguard(VictimUnit))
	{
		if (FRecord* Died = Find(Victim))
		{
			++Died->Statistics.Deaths;
			Publish(*Died);
		}
		// An Execution credits no one (Combat Bible §18).
		if (FRecord* Killer = Find(Death.CreditedKiller.Get()))
		{
			++Killer->Statistics.Kills;
			Publish(*Killer);
		}
		for (const TWeakObjectPtr<UAbilitySystemComponent>& Assister : Death.Assisters)
		{
			if (FRecord* Assisted = Find(Assister.Get()))
			{
				++Assisted->Statistics.Assists;
				Publish(*Assisted);
			}
		}
		EndCrowdControlOn(*Victim, Death.DiedAtSeconds);
		return;
	}
	const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(VictimUnit);
	if (Kind == EVeyraUnitKind::Fluxborn)
	{
		// A Fluxborn's last hit belongs to whoever landed it, only if a Vanguard did (§2).
		FRecord* LastHitter = Find(Death.Killer.Get());
		if (LastHitter && VeyraStatisticsService::AreEnemies(LastHitter->Participant.Get(), VictimUnit))
		{
			++LastHitter->Statistics.MinionKills;
			Publish(*LastHitter);
		}
	}
	else if (Kind == EVeyraUnitKind::Wildlife)
	{
		if (FRecord* Hunter = Find(Death.CreditedKiller.Get()))
		{
			++Hunter->Statistics.JungleKills;
			Publish(*Hunter);
		}
	}
	else if (Kind == EVeyraUnitKind::Ward)
	{
		if (FRecord* Destroyer = Find(Death.CreditedKiller.Get()))
		{
			++Destroyer->Statistics.WardsDestroyed;
		}
	}
}

void UVeyraMatchStatisticsSubsystem::OnDamageResolved(const FVeyraDamageResolution& Event)
{
	const UAbilitySystemComponent* Target = Event.Target.Get();
	const AActor* TargetUnit = Target ? Target->GetOwner() : nullptr;
	// A ward counts hits, not damage (ADR-016 §6).
	if (!TargetUnit || VeyraUnits::IsWard(TargetUnit))
	{
		return;
	}
	// Shielding belongs to each shield's provider, for what it actually absorbed (§3).
	for (const FVeyraShieldShare& Share : Event.Shields)
	{
		if (FRecord* Provider = Find(Share.Provider.Get()))
		{
			Provider->Statistics.DamageShielded += Share.Absorbed;
		}
	}
	// Damage is Health actually removed: what shields took is not also damage (§3).
	if (!(Event.HealthLost > 0.0))
	{
		return;
	}
	if (FRecord* Taker = Find(Target))
	{
		VeyraStatisticsService::AddByType(Taker->Statistics.DamageTaken, Event.Type, Event.HealthLost);
	}
	FRecord* Dealer = Find(Event.Source.Get());
	if (!Dealer || Event.Source.Get() == Target)
	{
		return;
	}
	FVeyraPlayerStatistics& Statistics = Dealer->Statistics;
	VeyraStatisticsService::AddByType(Statistics.DamageDealt, Event.Type, Event.HealthLost);
	const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(TargetUnit);
	if (Kind == EVeyraUnitKind::Objective)
	{
		// A Flux Well belongs to no side (ADR-014 §4).
		Statistics.WellDamage += Event.HealthLost;
	}
	else if (VeyraStatisticsService::AreEnemies(Dealer->Participant.Get(), TargetUnit))
	{
		if (Kind == EVeyraUnitKind::Vanguard)
		{
			Statistics.VanguardDamage += Event.HealthLost;
		}
		else if (VeyraStatisticsService::IsTower(TargetUnit))
		{
			Statistics.TowerDamage += Event.HealthLost;
		}
	}
}

void UVeyraMatchStatisticsSubsystem::OnHealthRestored(const FVeyraHealthRestored& Event)
{
	FRecord* Healer = Find(Event.Provider.Get());
	const UAbilitySystemComponent* Target = Event.Target.Get();
	if (!Healer || !Target)
	{
		return;
	}
	if (Target == Healer->Unit.Get())
	{
		Healer->Statistics.SelfHealing += Event.Restored;
	}
	else if (const FRecord* Healed = Find(Target); Healed && VeyraTeams::TeamOf(Healed->Participant.Get()) == VeyraTeams::TeamOf(Healer->Participant.Get()))
	{
		Healer->Statistics.TeammateHealing += Event.Restored;
	}
}

void UVeyraMatchStatisticsSubsystem::OnStatusApplied(const FVeyraStatusApplied& Event)
{
	FRecord* Source = Find(Event.Source.Get());
	const UAbilitySystemComponent* Target = Event.Target.Get();
	const AActor* TargetUnit = Target ? Target->GetOwner() : nullptr;
	// On enemy Vanguards only (§4).
	if (!Source || !TargetUnit || !VeyraStatisticsService::IsCountedCrowdControl(Event.Kind) || !VeyraUnits::IsVanguard(TargetUnit)
		|| !VeyraStatisticsService::AreEnemies(Source->Participant.Get(), TargetUnit))
	{
		return;
	}
	Source->CrowdControl.FindOrAdd({ FObjectKey(Target), static_cast<uint8>(Event.Kind) }).Add({ Event.StartsAt, Event.EndsAt });
}

void UVeyraMatchStatisticsSubsystem::OnWardPlaced(const AVeyraWard& /*Ward*/, APlayerState& Placer)
{
	if (FRecord* Record = Find(&Placer))
	{
		++Record->Statistics.WardsPlaced;
	}
}

void UVeyraMatchStatisticsSubsystem::OnFluxWellSecured(const FVeyraFluxWellSecuredEvent& Event)
{
	// Secured with participation: each Vanguard of the securing side at the Well (§5).
	for (const TWeakObjectPtr<UAbilitySystemComponent>& Capturer : Event.Capturers)
	{
		if (FRecord* Record = Find(Capturer.Get()))
		{
			++Record->Statistics.WellsSecured;
		}
	}
	if (FRecord* FinalHitter = Find(Event.FinalHitter.Get()))
	{
		++FinalHitter->Statistics.WellFinalHits;
	}
}

void UVeyraMatchStatisticsSubsystem::OnGoldGranted(double Amount, EVeyraGoldReason Reason, TWeakObjectPtr<AVeyraPlayerState> Participant)
{
	FRecord* Record = bRecording ? Find(Participant.Get()) : nullptr;
	if (Record)
	{
		VeyraStatisticsRules::AddEarned(Record->Statistics.GoldBySource, Reason, Amount);
	}
}

void UVeyraMatchStatisticsSubsystem::Publish(const FRecord& Record)
{
	UVeyraScoreComponent* Score = Record.Participant.IsValid() ? Record.Participant->FindComponentByClass<UVeyraScoreComponent>() : nullptr;
	if (!Score)
	{
		return;
	}
	const FVeyraPlayerStatistics& Statistics = Record.Statistics;
	FVeyraScore Public;
	Public.Kills = Statistics.Kills;
	Public.Deaths = Statistics.Deaths;
	Public.Assists = Statistics.Assists;
	Public.MinionKills = Statistics.MinionKills;
	Public.JungleKills = Statistics.JungleKills;
	Score->SetScore(Public);
}

void UVeyraMatchStatisticsSubsystem::EndCrowdControlOn(const UAbilitySystemComponent& Target, double At)
{
	const FObjectKey TargetKey(&Target);
	for (FRecord& Record : Records)
	{
		for (TPair<TPair<FObjectKey, uint8>, TArray<FVeyraSpan>>& Entry : Record.CrowdControl)
		{
			if (Entry.Key.Key != TargetKey)
			{
				continue;
			}
			for (FVeyraSpan& Span : Entry.Value)
			{
				Span.End = FMath::Min(Span.End, At);
			}
		}
	}
}

double UVeyraMatchStatisticsSubsystem::RecordedUntil() const
{
	if (bStopped)
	{
		return StoppedAt;
	}
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}
