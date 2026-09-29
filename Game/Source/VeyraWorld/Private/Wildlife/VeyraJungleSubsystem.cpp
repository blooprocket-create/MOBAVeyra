// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Wildlife/VeyraJungleSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Layout/VeyraLayout.h"
#include "Rewards/VeyraRewardSubsystem.h"
#include "Rules/VeyraWildlifeRules.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraWorldLog.h"
#include "Wildlife/VeyraWildlife.h"
#include "Wildlife/VeyraWildlifeController.h"

void UVeyraJungleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UVeyraBattlegroundSubsystem>();
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraJungleSubsystem::OnDeath);
		HostileDamageHandle = Events->OnHostileDamage.AddUObject(this, &UVeyraJungleSubsystem::OnHostileDamage);
	}
}

void UVeyraJungleSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		for (FCamp& Camp : Camps)
		{
			World->GetTimerManager().ClearTimer(Camp.Timer);
		}
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDeath.Remove(DeathHandle);
			Events->OnHostileDamage.Remove(HostileDamageHandle);
		}
	}
	Camps.Reset();
	Super::Deinitialize();
}

void UVeyraJungleSubsystem::Start()
{
	const UWorld* World = GetWorld();
	const UVeyraBattlegroundSubsystem* Battleground = World ? World->GetSubsystem<UVeyraBattlegroundSubsystem>() : nullptr;
	// The camps stand in the battleground's jungle, so a world without one has none.
	if (!IsServer() || bStarted || bStopped || !Battleground || !Battleground->GetLayout())
	{
		return;
	}
	bStarted = true;
	// Team A's camps as World.json places them, then Team B's, their mirror.
	const FVeyraWildlifeTuning& Wildlife = UVeyraWorldTuningSubsystem::Get().Wildlife;
	for (const EVeyraTeam Half : { EVeyraTeam::A, EVeyraTeam::B })
	{
		for (int32 TuningIndex = 0; TuningIndex < Wildlife.Camps.Num(); ++TuningIndex)
		{
			FCamp& Camp = Camps.AddDefaulted_GetRef();
			Camp.TuningIndex = TuningIndex;
			Camp.Half = Half;
			Camp.Center = VeyraLayout::ForTeam(VeyraLayout::ToVector(Wildlife.Camps[TuningIndex].Center), Half);
		}
	}
	for (int32 Index = 0; Index < Camps.Num(); ++Index)
	{
		ScheduleSpawn(Index, Wildlife.Camps[Camps[Index].TuningIndex].SpawnSeconds);
	}
	UE_LOG(LogVeyraWorld, Log, TEXT("The jungle starts: %d camp(s)."), Camps.Num());
}

void UVeyraJungleSubsystem::Stop()
{
	bStopped = true;
	if (UWorld* World = GetWorld())
	{
		for (FCamp& Camp : Camps)
		{
			World->GetTimerManager().ClearTimer(Camp.Timer);
			Camp.SpawnsAt = 0.0;
		}
	}
}

int32 UVeyraJungleSubsystem::SpawnCamp(int32 Index)
{
	UWorld* World = GetWorld();
	if (!World || !IsServer() || bStopped || !Camps.IsValidIndex(Index))
	{
		return 0;
	}
	FCamp& Camp = Camps[Index];
	Camp.Creatures.RemoveAll([](const TWeakObjectPtr<AVeyraWildlife>& Creature) { return !Creature.IsValid() || !Creature->IsAlive(); });
	if (!Camp.Creatures.IsEmpty())
	{
		return 0;
	}
	const FVeyraWorldTuning& Tuning = UVeyraWorldTuningSubsystem::Get();
	const FVeyraCampTuning& CampTuning = Tuning.Wildlife.Camps[Camp.TuningIndex];
	const FVeyraWildlifeSpecies* Species = Tuning.FindSpecies(CampTuning.Species);
	if (!Species)
	{
		UE_LOG(LogVeyraWorld, Error, TEXT("Camp %d's species %s is unknown; World.json defines none."), Index, *CampTuning.Species.ToString());
		return 0;
	}
	World->GetTimerManager().ClearTimer(Camp.Timer);
	Camp.SpawnsAt = 0.0;
	Camp.Contributors.Reset();
	int32 Spawned = 0;
	for (const FVector2D& Spot : VeyraWildlifeRules::Positions(Camp.Center, CampTuning.Count, CampTuning.Spacing))
	{
		// On the floor, facing out from the camp's heart.
		const FVector Home(Spot, Species->CapsuleHalfHeight);
		const FRotator Facing = (Spot - Camp.Center).IsNearlyZero() ? FRotator::ZeroRotator : FVector(Spot - Camp.Center, 0.0).Rotation();
		AVeyraWildlife* Creature = World->SpawnActorDeferred<AVeyraWildlife>(AVeyraWildlife::StaticClass(), FTransform(Facing, Home), nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (!Creature)
		{
			continue;
		}
		// Each keeps to its own spot, but a pack shares its camp's leash (ADR-014 §2).
		Creature->Configure(CampTuning.Species, Index, Home, Camp.Center, CampTuning.LeashRadius);
		Creature->FinishSpawning(FTransform(Facing, Home));
		if (!Creature->InitializeStats())
		{
			Creature->Destroy();
			continue;
		}
		Camp.Creatures.Add(Creature);
		++Spawned;
	}
	UE_LOG(LogVeyraWorld, Log, TEXT("Camp %d, %s on %s's half, spawned %d creature(s)."), Index, *CampTuning.Species.ToString(), *UEnum::GetValueAsString(Camp.Half),
		Spawned);
	return Spawned;
}

TArray<FVeyraCampState> UVeyraJungleSubsystem::GetCamps() const
{
	TArray<FVeyraCampState> States;
	const FVeyraWildlifeTuning& Wildlife = UVeyraWorldTuningSubsystem::Get().Wildlife;
	for (int32 Index = 0; Index < Camps.Num(); ++Index)
	{
		const FCamp& Camp = Camps[Index];
		FVeyraCampState& State = States.AddDefaulted_GetRef();
		State.Index = Index;
		State.Half = Camp.Half;
		State.Species = Wildlife.Camps.IsValidIndex(Camp.TuningIndex) ? Wildlife.Camps[Camp.TuningIndex].Species : FVeyraContentId();
		State.Center = Camp.Center;
		State.Alive = GetCreatures(Index).Num();
		State.SpawnsAt = Camp.SpawnsAt;
	}
	return States;
}

TArray<AVeyraWildlife*> UVeyraJungleSubsystem::GetCreatures(int32 Index) const
{
	TArray<AVeyraWildlife*> Living;
	if (Camps.IsValidIndex(Index))
	{
		for (const TWeakObjectPtr<AVeyraWildlife>& Creature : Camps[Index].Creatures)
		{
			if (Creature.IsValid() && Creature->IsAlive())
			{
				Living.Add(Creature.Get());
			}
		}
	}
	return Living;
}

void UVeyraJungleSubsystem::OnHostileDamage(const FVeyraHostileDamageEvent& Event)
{
	const UAbilitySystemComponent* Source = Event.Source.Get();
	const UAbilitySystemComponent* Target = Event.Target.Get();
	const AVeyraWildlife* Hurt = Target ? Cast<AVeyraWildlife>(Target->GetOwner()) : nullptr;
	AActor* Attacker = Source ? Source->GetAvatarActor() : nullptr;
	if (!IsServer() || !Hurt || !Attacker)
	{
		return;
	}
	// A Vanguard that hurt it helps clear its camp (ADR-018 §3).
	if (Camps.IsValidIndex(Hurt->GetCamp()) && VeyraUnits::IsVanguard(Source->GetOwner()))
	{
		Camps[Hurt->GetCamp()].Contributors.AddUnique(Event.Source);
	}
	// The whole camp answers, as League's camps do.
	for (AVeyraWildlife* Creature : GetCreatures(Hurt->GetCamp()))
	{
		if (AVeyraWildlifeController* Controller = Cast<AVeyraWildlifeController>(Creature->GetController()))
		{
			Controller->NoteAggression(*Attacker);
		}
	}
}

void UVeyraJungleSubsystem::OnDeath(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	if (AVeyraWildlife* Creature = Victim ? Cast<AVeyraWildlife>(Victim->GetOwner()) : nullptr; Creature && IsServer())
	{
		OnCreatureDied(*Creature, Death);
	}
}

void UVeyraJungleSubsystem::OnCreatureDied(AVeyraWildlife& Creature, const FVeyraDeathEvent& Death)
{
	const FVeyraWorldTuning& Tuning = UVeyraWorldTuningSubsystem::Get();
	// Its body collapses where it fell, blocking nothing, and is removed after a moment.
	if (AController* Controller = Creature.GetController())
	{
		Controller->UnPossess();
		Controller->Destroy();
	}
	Creature.GetCharacterMovement()->DisableMovement();
	Creature.SetActorEnableCollision(false);
	Creature.SetLifeSpan(static_cast<float>(Tuning.Wildlife.Ai.CorpseSeconds));
	// Each creature pays at its own death (Economy & Progression Bible §7).
	if (UVeyraRewardSubsystem* Rewards = GetWorld()->GetSubsystem<UVeyraRewardSubsystem>())
	{
		Rewards->RewardWildlifeDeath(Death, Creature.GetSpecies());
	}

	const int32 Index = Creature.GetCamp();
	if (!Camps.IsValidIndex(Index) || !GetCreatures(Index).IsEmpty())
	{
		return;
	}
	// The camp is clear: its trait goes to the Vanguard credited with its last kill (§8), and it
	// respawns on its own timer (§17).
	UAbilitySystemComponent* Killer = Death.CreditedKiller.Get();
	const FVeyraWildlifeSpecies* Species = Tuning.FindSpecies(Creature.GetSpecies());
	if (Killer && Species && VeyraUnits::IsVanguard(Killer->GetOwner()))
	{
		for (const FVeyraContentId& Trait : Species->Traits)
		{
			if (const TOptional<FVeyraStatusSpec> Status = UVeyraAbilitiesTuningSubsystem::FindStatus(Trait))
			{
				VeyraCombat::ApplyStatus(*Killer, *Killer, Status.GetValue());
			}
		}
	}
	FCamp& Camp = Camps[Index];
	UE_LOG(LogVeyraWorld, Log, TEXT("Camp %d, %s on %s's half, was cleared%s."), Index, *Creature.GetSpecies().ToString(), *UEnum::GetValueAsString(Camp.Half),
		Killer ? *FString::Printf(TEXT(" by %s"), *GetNameSafe(Killer->GetOwner())) : TEXT(""));
	// Once per clear, with who helped (ADR-018 §3).
	OnCampCleared.Broadcast(FVeyraCampCleared{ Index, Creature.GetSpecies(), Camp.Contributors });
	Camp.Contributors.Reset();
	if (!bStopped)
	{
		ScheduleSpawn(Index, Tuning.Wildlife.Camps[Camp.TuningIndex].RespawnSeconds);
	}
}

void UVeyraJungleSubsystem::ScheduleSpawn(int32 Index, double Seconds)
{
	UWorld* World = GetWorld();
	if (!World || !Camps.IsValidIndex(Index))
	{
		return;
	}
	// The timer manager ignores a delay of 0, so a camp due now spawns now.
	if (Seconds <= 0.0)
	{
		SpawnCamp(Index);
		return;
	}
	FCamp& Camp = Camps[Index];
	Camp.SpawnsAt = World->GetTimeSeconds() + Seconds;
	World->GetTimerManager().SetTimer(Camp.Timer, FTimerDelegate::CreateWeakLambda(this, [this, Index] { SpawnCamp(Index); }), static_cast<float>(Seconds),
		/*bLoop*/ false);
}

bool UVeyraJungleSubsystem::IsServer() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}
