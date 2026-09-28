// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraBattlegroundSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Battleground/VeyraBattlegroundMarker.h"
#include "Engine/World.h"
#include "Layout/VeyraLayout.h"
#include "Rules/VeyraStructureRules.h"
#include "Structures/VeyraStructure.h"
#include "Structures/VeyraStructureAttackComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraCombatVerbs.h"
#include "VeyraWorldLog.h"

namespace
{
	TArray<FVeyraStructureStatus> StatusesOf(TConstArrayView<TObjectPtr<AVeyraStructure>> Structures)
	{
		TArray<FVeyraStructureStatus> Statuses;
		for (const AVeyraStructure* Structure : Structures)
		{
			if (Structure)
			{
				Statuses.Add({ Structure->GetStructureKind(), Structure->GetVeyraTeam(), Structure->GetLane(), Structure->GetOrder(), Structure->IsDestroyed() });
			}
		}
		return Statuses;
	}

	FString Describe(const AVeyraStructure& Structure)
	{
		const TOptional<EVeyraLane> Lane = Structure.GetLane();
		return FString::Printf(TEXT("team %s's %s%s #%d"), *UEnum::GetValueAsString(Structure.GetVeyraTeam()),
			*UEnum::GetValueAsString(Structure.GetStructureKind()),
			Lane.IsSet() ? *FString::Printf(TEXT(" (%s)"), *UEnum::GetValueAsString(Lane.GetValue())) : TEXT(""), Structure.GetOrder());
	}
}

void UVeyraBattlegroundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>();
	if (Events)
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraBattlegroundSubsystem::OnDeath);
		HostileDamageHandle = Events->OnHostileDamage.AddUObject(this, &UVeyraBattlegroundSubsystem::OnHostileDamage);
	}
}

void UVeyraBattlegroundSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDeath.Remove(DeathHandle);
			Events->OnHostileDamage.Remove(HostileDamageHandle);
		}
	}
	Stop();
	Super::Deinitialize();
}

void UVeyraBattlegroundSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (IsServer() && AVeyraBattlegroundMarker::IsBattleground(InWorld))
	{
		SpawnStructures(UVeyraWorldTuningSubsystem::Get().Layout);
	}
}

void UVeyraBattlegroundSubsystem::SpawnStructures(const FVeyraBattlegroundLayout& Layout)
{
	UWorld* World = GetWorld();
	if (!World || !IsServer())
	{
		return;
	}
	for (const FVeyraStructurePlacement& Placement : VeyraLayout::Structures(Layout))
	{
		AVeyraStructure* Structure = World->SpawnActorDeferred<AVeyraStructure>(AVeyraStructure::StaticClass(), FTransform::Identity, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Structure->Configure(Placement);
		// Its capsule stands on the floor.
		const FVector Location(Placement.Location.X, Placement.Location.Y, Structure->GetTuning().CapsuleHalfHeight);
		Structure->FinishSpawning(FTransform(Location));
		Structure->InitializeStats();
		Structures.Add(Structure);
		if (UVeyraStructureAttackComponent* Attack = Structure->GetAttack())
		{
			Attack->StartAttacking();
		}
	}
	RefreshInvulnerability();
	const float Tick = static_cast<float>(UVeyraCombatTuningSubsystem::Get().Regeneration.TickSeconds);
	World->GetTimerManager().SetTimer(RegenerationTimer, FTimerDelegate::CreateUObject(this, &UVeyraBattlegroundSubsystem::OnRegenerationTimer), Tick, /*bLoop*/ true);
	UE_LOG(LogVeyraWorld, Log, TEXT("Spawned the battleground's %d structures."), Structures.Num());
}

AVeyraStructure* UVeyraBattlegroundSubsystem::FindStructure(EVeyraTeam Team, EVeyraStructureKind Kind, TOptional<EVeyraLane> Lane, int32 Order) const
{
	for (AVeyraStructure* Structure : Structures)
	{
		if (Structure && Structure->GetVeyraTeam() == Team && Structure->GetStructureKind() == Kind && Structure->GetLane() == Lane
			&& Structure->GetOrder() == Order)
		{
			return Structure;
		}
	}
	return nullptr;
}

void UVeyraBattlegroundSubsystem::Stop()
{
	bStopped = true;
	for (AVeyraStructure* Structure : Structures)
	{
		if (UVeyraStructureAttackComponent* Attack = Structure ? Structure->GetAttack() : nullptr)
		{
			Attack->StopAttacking();
		}
	}
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(RegenerationTimer);
		for (TPair<TWeakObjectPtr<AVeyraStructure>, FTimerHandle>& Rebuild : RebuildTimers)
		{
			Timers.ClearTimer(Rebuild.Value);
		}
	}
	RebuildTimers.Reset();
}

void UVeyraBattlegroundSubsystem::RefreshInvulnerability()
{
	const TArray<FVeyraStructureStatus> Statuses = StatusesOf(Structures);
	for (AVeyraStructure* Structure : Structures)
	{
		if (Structure)
		{
			const FVeyraStructureStatus Status{ Structure->GetStructureKind(), Structure->GetVeyraTeam(), Structure->GetLane(), Structure->GetOrder(), Structure->IsDestroyed() };
			Structure->SetInvulnerable(VeyraStructureRules::IsInvulnerable(Status, Statuses));
		}
	}
}

void UVeyraBattlegroundSubsystem::RegeneratePrimeWells(double Seconds)
{
	if (bStopped || !(Seconds > 0.0))
	{
		return;
	}
	const double Fraction = UVeyraWorldTuningSubsystem::Get().PrimeWell.RegenerationFractionPerSecond;
	const TArray<FVeyraStructureStatus> Statuses = StatusesOf(Structures);
	for (AVeyraStructure* Structure : Structures)
	{
		if (!Structure || Structure->GetStructureKind() != EVeyraStructureKind::PrimeWell || Structure->IsDestroyed()
			|| !VeyraStructureRules::PrimeWellRegenerates(Structure->GetVeyraTeam(), Statuses))
		{
			continue;
		}
		UAbilitySystemComponent& Well = *Structure->GetAbilitySystemComponent();
		if (VeyraCombat::GetMissingHealth(Well) > 0.0)
		{
			VeyraCombat::RestoreHealth(Well, Well.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) * Fraction * Seconds);
		}
	}
}

void UVeyraBattlegroundSubsystem::OnDeath(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	AVeyraStructure* Structure = Victim ? Cast<AVeyraStructure>(Victim->GetOwner()) : nullptr;
	if (!Structure || !IsServer() || !Structures.Contains(Structure))
	{
		return;
	}
	UE_LOG(LogVeyraWorld, Log, TEXT("%s was destroyed."), *Describe(*Structure));
	RefreshInvulnerability();

	if (Structure->GetStructureKind() == EVeyraStructureKind::Inhibitor && !bStopped)
	{
		// It reconstructs after its own timer (Battleground Bible §10), on world time, so a pause holds it.
		const double RebuildSeconds = UVeyraWorldTuningSubsystem::Get().Inhibitor.RebuildSeconds;
		UWorld* World = GetWorld();
		Structure->SetRebuildsAt(World->GetTimeSeconds() + RebuildSeconds);
		FTimerHandle& Timer = RebuildTimers.FindOrAdd(Structure);
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &UVeyraBattlegroundSubsystem::RebuildInhibitor,
			TWeakObjectPtr<AVeyraStructure>(Structure)), static_cast<float>(RebuildSeconds), /*bLoop*/ false);
	}

	FVeyraStructureDestroyedEvent Event;
	Event.Structure = Structure;
	Event.Kind = Structure->GetStructureKind();
	Event.Team = Structure->GetVeyraTeam();
	Event.Lane = Structure->GetLane();
	Event.Death = Death;
	OnStructureDestroyed.Broadcast(Event);
}

void UVeyraBattlegroundSubsystem::OnHostileDamage(const FVeyraHostileDamageEvent& Event)
{
	const UAbilitySystemComponent* Source = Event.Source.Get();
	const UAbilitySystemComponent* Target = Event.Target.Get();
	AActor* Attacker = Source ? Source->GetAvatarActor() : nullptr;
	const AActor* Defender = Target ? Target->GetAvatarActor() : nullptr;
	if (bStopped || !IsServer() || !Attacker || !Defender || !VeyraUnits::IsVanguard(Attacker) || !VeyraUnits::IsVanguard(Defender))
	{
		return;
	}
	for (AVeyraStructure* Structure : Structures)
	{
		UVeyraStructureAttackComponent* Attack = Structure ? Structure->GetAttack() : nullptr;
		if (Attack && !Structure->IsDestroyed() && Structure->GetVeyraTeam() == VeyraTeams::TeamOf(Defender)
			&& VeyraTargeting::AreHostile(Structure, Attacker) && Attack->IsInRange(*Attacker) && Attack->IsInRange(*Defender))
		{
			Attack->NoteAggression(*Attacker);
		}
	}
}

void UVeyraBattlegroundSubsystem::RebuildInhibitor(TWeakObjectPtr<AVeyraStructure> Inhibitor)
{
	AVeyraStructure* Structure = Inhibitor.Get();
	RebuildTimers.Remove(Inhibitor);
	if (!Structure || bStopped || !Structure->Rebuild())
	{
		return;
	}
	UE_LOG(LogVeyraWorld, Log, TEXT("%s reconstructed."), *Describe(*Structure));
	RefreshInvulnerability();
}

void UVeyraBattlegroundSubsystem::OnRegenerationTimer()
{
	RegeneratePrimeWells(UVeyraCombatTuningSubsystem::Get().Regeneration.TickSeconds);
}

bool UVeyraBattlegroundSubsystem::IsServer() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}
