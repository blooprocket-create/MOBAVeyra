// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraBattlegroundSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Battleground/VeyraBattlegroundMarker.h"
#include "Engine/World.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "Fluxborn/VeyraFluxbornController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Layout/VeyraLayout.h"
#include "Rules/VeyraStructureRules.h"
#include "Rules/VeyraWaveRules.h"
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

void UVeyraBattlegroundSubsystem::SpawnStructures(const FVeyraBattlegroundLayout& InLayout)
{
	UWorld* World = GetWorld();
	if (!World || !IsServer())
	{
		return;
	}
	Layout = InLayout;
	for (const FVeyraStructurePlacement& Placement : VeyraLayout::Structures(InLayout))
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

AVeyraStructure* UVeyraBattlegroundSubsystem::NextSiegeTarget(EVeyraTeam Defenders) const
{
	// StatusesOf skips null entries, so the index is taken over the live structures alike.
	TArray<AVeyraStructure*> Live;
	for (AVeyraStructure* Structure : Structures)
	{
		if (Structure)
		{
			Live.Add(Structure);
		}
	}
	const TOptional<int32> Next = VeyraStructureRules::NextToSiege(Defenders, StatusesOf(Structures));
	return Next.IsSet() ? Live[Next.GetValue()] : nullptr;
}

AVeyraFluxborn* UVeyraBattlegroundSubsystem::SpawnFluxborn(const FVeyraContentId& Kind, EVeyraTeam Team, EVeyraLane Lane)
{
	UWorld* World = GetWorld();
	const FVeyraLaneLayout* LaneLayout = Layout.IsSet() ? Layout->Lanes.FindByPredicate([Lane](const FVeyraLaneLayout& Each) { return Each.Lane == Lane; }) : nullptr;
	const FVeyraFluxbornDefinition* Definition = UVeyraWorldTuningSubsystem::Get().FindFluxborn(Kind);
	if (!World || !IsServer() || bStopped || !LaneLayout || !Definition || (Team != EVeyraTeam::A && Team != EVeyraTeam::B))
	{
		UE_LOG(LogVeyraWorld, Warning, TEXT("Refused to spawn a %s for team %s in the %s lane."), *Kind.ToString(), *UEnum::GetValueAsString(Team),
			*UEnum::GetValueAsString(Lane));
		return nullptr;
	}
	// It walks its lane toward the enemy base, then on to the enemy's Prime Well.
	TArray<FVector2D> Waypoints = VeyraLayout::Waypoints(*LaneLayout, Team);
	Waypoints.Add(VeyraLayout::ForTeam(VeyraLayout::ToVector(Layout->Base.PrimeWell), VeyraTeams::Opposing(Team)));
	// It spawns in front of its inhibitor, on the floor, facing up the lane.
	const FVector2D SpawnPoint = VeyraLayout::ForTeam(VeyraLayout::PointAlong(LaneLayout->Points, LaneLayout->FluxbornSpawnDistance), Team);
	const FVector Start(SpawnPoint, Definition->CapsuleHalfHeight);
	const FRotator Facing = FVector(Waypoints[1] - Waypoints[0], 0.0).Rotation();

	AVeyraFluxborn* Unit = World->SpawnActorDeferred<AVeyraFluxborn>(AVeyraFluxborn::StaticClass(), FTransform(Facing, Start), nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Unit)
	{
		return nullptr;
	}
	Unit->Configure(Kind, Team, Lane, MoveTemp(Waypoints));
	Unit->FinishSpawning(FTransform(Facing, Start));
	const FVeyraTeamFluxStrength& Strength = FluxOf(Team);
	if (!Unit->InitializeStats(Strength.HealthMultiplier, Strength.DamageMultiplier))
	{
		Unit->Destroy();
		return nullptr;
	}
	Fluxborn.Add(Unit);
	return Unit;
}

void UVeyraBattlegroundSubsystem::StartWaves()
{
	UWorld* World = GetWorld();
	if (!World || !IsServer() || bStopped || !Layout.IsSet() || bWavesStarted)
	{
		return;
	}
	bWavesStarted = true;
	WavesStartedAt = World->GetTimeSeconds();
	NextWave = 0;
	ScheduleWave();
}

void UVeyraBattlegroundSubsystem::ScheduleWave()
{
	UWorld* World = GetWorld();
	const double MatchSeconds = World->GetTimeSeconds() - WavesStartedAt;
	const double Delay = VeyraWaveRules::WaveTime(UVeyraWorldTuningSubsystem::Get().Waves, NextWave) - MatchSeconds;
	// A timer needs a delay above 0; a wave already due spawns at once.
	if (Delay <= 0.0)
	{
		OnWaveTimer();
		return;
	}
	World->GetTimerManager().SetTimer(WaveTimer, FTimerDelegate::CreateUObject(this, &UVeyraBattlegroundSubsystem::OnWaveTimer), static_cast<float>(Delay),
		/*bLoop*/ false);
}

void UVeyraBattlegroundSubsystem::OnWaveTimer()
{
	if (bStopped)
	{
		return;
	}
	SpawnWave(NextWave);
	ScheduleWave();
}

void UVeyraBattlegroundSubsystem::SpawnWave(int32 Index)
{
	UWorld* World = GetWorld();
	if (!World || !IsServer() || bStopped || !Layout.IsSet())
	{
		return;
	}
	const FVeyraWavesTuning& Waves = UVeyraWorldTuningSubsystem::Get().Waves;
	const bool bSiege = VeyraWaveRules::HasSiege(Waves, Index, VeyraWaveRules::WaveTime(Waves, Index));
	FTimerManager& Timers = World->GetTimerManager();
	FileTimers.RemoveAll([&Timers](const FTimerHandle& Handle) { return !Timers.IsTimerActive(Handle); });
	for (const FVeyraLaneLayout& Lane : Layout->Lanes)
	{
		for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
		{
			// A lane whose enemy inhibitor is down adds units to this team's waves in it (Battleground Bible §18).
			const TArray<FVeyraContentId> Units = VeyraWaveRules::Composition(Waves, bSiege, IsInhibitorDown(VeyraTeams::Opposing(Team), Lane.Lane));
			for (int32 Place = 0; Place < Units.Num(); ++Place)
			{
				const double Delay = Waves.UnitIntervalSeconds * Place;
				if (Delay <= 0.0)
				{
					SpawnFluxborn(Units[Place], Team, Lane.Lane);
					continue;
				}
				Timers.SetTimer(FileTimers.AddDefaulted_GetRef(),
					FTimerDelegate::CreateUObject(this, &UVeyraBattlegroundSubsystem::SpawnWaveUnit, Units[Place], Team, Lane.Lane),
					static_cast<float>(Delay), /*bLoop*/ false);
			}
		}
	}
	NextWave = FMath::Max(NextWave, Index + 1);
	UE_LOG(LogVeyraWorld, Log, TEXT("Wave %d spawned in %d lane(s)%s."), Index + 1, Layout->Lanes.Num(), bSiege ? TEXT(", with siege units") : TEXT(""));
}

void UVeyraBattlegroundSubsystem::SpawnWaveUnit(FVeyraContentId Kind, EVeyraTeam Team, EVeyraLane Lane)
{
	if (!bStopped)
	{
		SpawnFluxborn(Kind, Team, Lane);
	}
}

bool UVeyraBattlegroundSubsystem::IsInhibitorDown(EVeyraTeam Team, EVeyraLane Lane) const
{
	for (const AVeyraStructure* Structure : Structures)
	{
		if (Structure && Structure->GetVeyraTeam() == Team && Structure->GetStructureKind() == EVeyraStructureKind::Inhibitor
			&& Structure->GetLane() == TOptional<EVeyraLane>(Lane) && Structure->IsDestroyed())
		{
			return true;
		}
	}
	return false;
}

TArray<AVeyraFluxborn*> UVeyraBattlegroundSubsystem::GetFluxborn() const
{
	TArray<AVeyraFluxborn*> Living;
	for (const TWeakObjectPtr<AVeyraFluxborn>& Each : Fluxborn)
	{
		if (AVeyraFluxborn* Unit = Each.Get(); Unit && Unit->IsAlive())
		{
			Living.Add(Unit);
		}
	}
	return Living;
}

void UVeyraBattlegroundSubsystem::SetTeamFlux(EVeyraTeam Team, const FVeyraTeamFluxStrength& Flux)
{
	if (Team != EVeyraTeam::A && Team != EVeyraTeam::B)
	{
		return;
	}
	FluxOf(Team) = Flux;
	// Temporary Flux falls away from units already on the field too (Battleground Bible §4).
	for (AVeyraFluxborn* Unit : GetFluxborn())
	{
		if (Unit->GetVeyraTeam() == Team)
		{
			Unit->ApplyStrength(Flux.HealthMultiplier, Flux.DamageMultiplier);
		}
	}
}

FVeyraTeamFluxStrength UVeyraBattlegroundSubsystem::GetTeamFlux(EVeyraTeam Team) const
{
	return Team == EVeyraTeam::B ? FluxB : FluxA;
}

FVeyraTeamFluxStrength& UVeyraBattlegroundSubsystem::FluxOf(EVeyraTeam Team)
{
	return Team == EVeyraTeam::B ? FluxB : FluxA;
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
	// The Fluxborn stand where they are.
	for (AVeyraFluxborn* Unit : GetFluxborn())
	{
		if (AController* Controller = Unit->GetController())
		{
			Controller->UnPossess();
		}
	}
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(RegenerationTimer);
		Timers.ClearTimer(WaveTimer);
		for (FTimerHandle& File : FileTimers)
		{
			Timers.ClearTimer(File);
		}
		for (TPair<TWeakObjectPtr<AVeyraStructure>, FTimerHandle>& Rebuild : RebuildTimers)
		{
			Timers.ClearTimer(Rebuild.Value);
		}
	}
	RebuildTimers.Reset();
	FileTimers.Reset();
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
	if (AVeyraFluxborn* Unit = Victim ? Cast<AVeyraFluxborn>(Victim->GetOwner()) : nullptr; Unit && IsServer())
	{
		OnFluxbornDied(*Unit);
		return;
	}
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

void UVeyraBattlegroundSubsystem::OnFluxbornDied(AVeyraFluxborn& Unit)
{
	Fluxborn.Remove(&Unit);
	// Its body collapses where it fell, blocking nothing, and is removed after a moment (Battleground Bible §4).
	if (AController* Controller = Unit.GetController())
	{
		Controller->UnPossess();
		Controller->Destroy();
	}
	Unit.GetCharacterMovement()->DisableMovement();
	Unit.SetActorEnableCollision(false);
	Unit.SetLifeSpan(static_cast<float>(UVeyraWorldTuningSubsystem::Get().Fluxborn.CorpseSeconds));
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
	// The defender's Fluxborn near it may turn on the attacker (Battleground Bible §19).
	const double Response = UVeyraWorldTuningSubsystem::Get().Fluxborn.Ai.AggressionResponseRange;
	const EVeyraTeam Defenders = VeyraTeams::TeamOf(Defender);
	for (AVeyraFluxborn* Unit : GetFluxborn())
	{
		AVeyraFluxbornController* Controller = Cast<AVeyraFluxbornController>(Unit->GetController());
		if (Controller && Unit->GetVeyraTeam() == Defenders && VeyraTargeting::AreHostile(Unit, Attacker)
			&& VeyraTargeting::EdgeToEdgeDistance(*Unit, *Defender) <= Response)
		{
			Controller->NoteAggression(*Attacker);
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
