// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Wells/VeyraFluxWellSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Layout/VeyraLayout.h"
#include "Terrain/VeyraSurfacePlacement.h"
#include "Rewards/VeyraRewardSubsystem.h"
#include "Rules/VeyraFluxWellRules.h"
#include "Targeting/VeyraTargeting.h"
#include "TimerManager.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraWorldLog.h"
#include "Wells/VeyraFluxWell.h"

void UVeyraFluxWellSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UVeyraBattlegroundSubsystem>();
	if (UVeyraCombatEventSubsystem* Events = Collection.InitializeDependency<UVeyraCombatEventSubsystem>())
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraFluxWellSubsystem::OnDeath);
		HostileDamageHandle = Events->OnHostileDamage.AddUObject(this, &UVeyraFluxWellSubsystem::OnHostileDamage);
	}
}

void UVeyraFluxWellSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Timer : OpenTimers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
		World->GetTimerManager().ClearTimer(PresenceTimer);
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDeath.Remove(DeathHandle);
			Events->OnHostileDamage.Remove(HostileDamageHandle);
		}
	}
	Wells.Reset();
	Super::Deinitialize();
}

void UVeyraFluxWellSubsystem::Start()
{
	UWorld* World = GetWorld();
	const UVeyraBattlegroundSubsystem* Battleground = World ? World->GetSubsystem<UVeyraBattlegroundSubsystem>() : nullptr;
	// The Wells stand on the battleground's river, so a world without one has none.
	if (!World || !IsServer() || bStarted || bStopped || !Battleground || !Battleground->GetLayout())
	{
		return;
	}
	bStarted = true;
	const FVeyraFluxWellsTuning& Tuning = UVeyraWorldTuningSubsystem::Get().FluxWells;
	const double Now = World->GetTimeSeconds();
	for (int32 Site = 0; Site < Tuning.Sites.Num(); ++Site)
	{
		FVector Location;
		if (!VeyraSurfacePlacement::Resolve(*World, VeyraLayout::ToVector(Tuning.Sites[Site]), Tuning.CapsuleHalfHeight,
			UVeyraWorldTuningSubsystem::Get().Layout.Surface, Location))
		{
			UE_LOG(LogVeyraWorld, Error, TEXT("Flux Well site %d has no playable surface."), Site);
			continue;
		}
		const FTransform Where(FRotator::ZeroRotator, Location);
		AVeyraFluxWell* Well = World->SpawnActorDeferred<AVeyraFluxWell>(AVeyraFluxWell::StaticClass(), Where, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Well)
		{
			continue;
		}
		Well->Configure(Site);
		Well->FinishSpawning(Where);
		if (!Well->InitializeStats())
		{
			Well->Destroy();
			continue;
		}
		const int32 Index = Wells.Add(Well);
		LastWorkedAt.Add(Now);
		OpenTimers.AddDefaulted();
		// Closed until its opening time on the match clock (§6).
		const double OpenSeconds = Tuning.Timing.OpenSeconds;
		Well->SetState(EVeyraFluxWellState::Closed, Now + OpenSeconds);
		if (OpenSeconds <= 0.0)
		{
			Open(Index);
		}
		else
		{
			World->GetTimerManager().SetTimer(OpenTimers[Index], FTimerDelegate::CreateWeakLambda(this, [this, Index] { Open(Index); }),
				static_cast<float>(OpenSeconds), /*bLoop*/ false);
		}
	}
	World->GetTimerManager().SetTimer(PresenceTimer, this, &UVeyraFluxWellSubsystem::OnPresenceTimer, static_cast<float>(Tuning.Presence.TickSeconds), /*bLoop*/ true);
	UE_LOG(LogVeyraWorld, Log, TEXT("The Flux Wells stand: %d, opening in %g s."), Wells.Num(), Tuning.Timing.OpenSeconds);
}

void UVeyraFluxWellSubsystem::Stop()
{
	bStopped = true;
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Timer : OpenTimers)
		{
			World->GetTimerManager().ClearTimer(Timer);
		}
		World->GetTimerManager().ClearTimer(PresenceTimer);
	}
}

void UVeyraFluxWellSubsystem::Open(int32 Index)
{
	AVeyraFluxWell* Well = Wells.IsValidIndex(Index) ? Wells[Index].Get() : nullptr;
	if (!Well || bStopped || !IsServer())
	{
		return;
	}
	// Whole again, however it was left (§6: a secured Well returns after its cycle).
	UAbilitySystemComponent& AbilitySystem = *Well->GetAbilitySystemComponent();
	if (!Well->IsStanding())
	{
		VeyraCombat::Revive(AbilitySystem);
	}
	VeyraCombat::RestoreHealth(AbilitySystem, AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()));
	Well->SetState(EVeyraFluxWellState::Open, 0.0);
	LastWorkedAt[Index] = GetWorld()->GetTimeSeconds();
	UE_LOG(LogVeyraWorld, Log, TEXT("Flux Well %d is open."), Well->GetSite());
}

void UVeyraFluxWellSubsystem::OnPresenceTimer()
{
	UpdatePresence(UVeyraWorldTuningSubsystem::Get().FluxWells.Presence.TickSeconds);
}

void UVeyraFluxWellSubsystem::UpdatePresence(double Seconds)
{
	if (bStopped || !IsServer())
	{
		return;
	}
	const FVeyraFluxWellsTuning& Tuning = UVeyraWorldTuningSubsystem::Get().FluxWells;
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < Wells.Num(); ++Index)
	{
		AVeyraFluxWell* Well = Wells[Index].Get();
		if (!Well || Well->GetState() != EVeyraFluxWellState::Open || !Well->IsStanding())
		{
			continue;
		}
		const TArray<AActor*> TeamA = PresentVanguards(*Well, EVeyraTeam::A);
		const TArray<AActor*> TeamB = PresentVanguards(*Well, EVeyraTeam::B);
		if (!TeamA.IsEmpty() || !TeamB.IsEmpty())
		{
			LastWorkedAt[Index] = Now;
		}
		UAbilitySystemComponent& Target = *Well->GetAbilitySystemComponent();
		const FVeyraPresenceDrain Drain = VeyraFluxWellRules::PresenceDrain(TeamA.Num(), TeamB.Num(), Tuning.Presence);
		if (Drain.Side != EVeyraTeam::None && Drain.PerSecond > 0.0)
		{
			// Dealt in the name of the nearest drainer, so a presence tick can land the last hit (ADR-014 §4).
			const TArray<AActor*>& Drainers = Drain.Side == EVeyraTeam::A ? TeamA : TeamB;
			if (UAbilitySystemComponent* Source = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Drainers[0]))
			{
				FVeyraRawDamageEvent Damage;
				Damage.Components.Add({ EVeyraDamageType::TrueDamage, Drain.PerSecond * Seconds });
				Damage.Delivery = EVeyraDamageDelivery::Presence;
				VeyraCombat::DealDamage(*Source, Target, Damage);
			}
			continue;
		}
		// Left alone a while, it heals (ADR-014 §9); contested to a stall, it does not.
		if (TeamA.IsEmpty() && TeamB.IsEmpty() && VeyraFluxWellRules::Regenerates(Now, LastWorkedAt[Index], Tuning.Regeneration.IdleSeconds))
		{
			VeyraCombat::RestoreHealth(Target, Target.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) * Tuning.Regeneration.FractionPerSecond * Seconds);
		}
	}
}

void UVeyraFluxWellSubsystem::OnHostileDamage(const FVeyraHostileDamageEvent& Event)
{
	const UAbilitySystemComponent* Target = Event.Target.Get();
	const AVeyraFluxWell* Well = Target ? Cast<AVeyraFluxWell>(Target->GetOwner()) : nullptr;
	const int32 Index = Well ? Wells.IndexOfByKey(Well) : INDEX_NONE;
	if (LastWorkedAt.IsValidIndex(Index))
	{
		LastWorkedAt[Index] = GetWorld()->GetTimeSeconds();
	}
}

void UVeyraFluxWellSubsystem::OnDeath(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	AVeyraFluxWell* Well = Victim ? Cast<AVeyraFluxWell>(Victim->GetOwner()) : nullptr;
	const int32 Index = Well ? Wells.IndexOfByKey(Well) : INDEX_NONE;
	if (!Well || Index == INDEX_NONE || !IsServer() || Well->GetState() != EVeyraFluxWellState::Open)
	{
		return;
	}
	// Secured by the side of its last hit, by damage or by presence (§6), then its cycle.
	UAbilitySystemComponent* Securer = Death.CreditedKiller.Get() ? Death.CreditedKiller.Get() : Death.Killer.Get();
	const EVeyraTeam Team = Securer ? VeyraTeams::TeamOf(Securer->GetOwner()) : EVeyraTeam::None;
	const double RespawnSeconds = UVeyraWorldTuningSubsystem::Get().FluxWells.Timing.RespawnSeconds;
	Well->SetState(EVeyraFluxWellState::Respawning, GetWorld()->GetTimeSeconds() + RespawnSeconds);
	if (!bStopped)
	{
		GetWorld()->GetTimerManager().SetTimer(OpenTimers[Index], FTimerDelegate::CreateWeakLambda(this, [this, Index] { Open(Index); }),
			static_cast<float>(RespawnSeconds), /*bLoop*/ false);
	}
	if (Team == EVeyraTeam::None)
	{
		UE_LOG(LogVeyraWorld, Warning, TEXT("Flux Well %d fell to no side."), Well->GetSite());
		return;
	}
	// Its Gold pool for the securing side's Vanguards there at this moment, and whoever landed the hit (Economy Bible §8.2).
	TArray<UAbilitySystemComponent*> Capturers;
	for (AActor* Vanguard : PresentVanguards(*Well, Team))
	{
		if (UAbilitySystemComponent* Capturer = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Vanguard))
		{
			Capturers.AddUnique(Capturer);
		}
	}
	if (Securer && VeyraUnits::IsVanguard(Securer->GetOwner()))
	{
		Capturers.AddUnique(Securer);
	}
	if (UVeyraRewardSubsystem* Rewards = GetWorld()->GetSubsystem<UVeyraRewardSubsystem>())
	{
		Rewards->RewardFluxWellSecured(Capturers);
	}
	UE_LOG(LogVeyraWorld, Log, TEXT("Flux Well %d was secured by %s."), Well->GetSite(), *UEnum::GetValueAsString(Team));
	FVeyraFluxWellSecuredEvent Event;
	Event.Site = Well->GetSite();
	Event.Team = Team;
	for (UAbilitySystemComponent* Capturer : Capturers)
	{
		Event.Capturers.Add(Capturer);
	}
	if (Securer && VeyraUnits::IsVanguard(Securer->GetOwner()))
	{
		Event.FinalHitter = Securer;
	}
	OnFluxWellSecured.Broadcast(Event);
}

TArray<AActor*> UVeyraFluxWellSubsystem::PresentVanguards(const AVeyraFluxWell& Well, EVeyraTeam Side) const
{
	TArray<AActor*> Present;
	const double Radius = UVeyraWorldTuningSubsystem::Get().FluxWells.Radius;
	const FVector Where = Well.GetActorLocation();
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = *It;
		if (VeyraUnits::IsVanguard(Pawn) && VeyraTeams::TeamOf(Pawn) == Side && VeyraTargeting::IsAlive(Pawn) && FVector::Dist2D(Pawn->GetActorLocation(), Where) <= Radius)
		{
			Present.Add(Pawn);
		}
	}
	Present.Sort([&Where](const AActor& A, const AActor& B) { return FVector::DistSquared2D(A.GetActorLocation(), Where) < FVector::DistSquared2D(B.GetActorLocation(), Where); });
	return Present;
}

bool UVeyraFluxWellSubsystem::IsServer() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}
