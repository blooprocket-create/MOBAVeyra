// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraVisionSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Delivery/VeyraDelayedArea.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Gate/VeyraFogGate.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Rewards/VeyraRewardSubsystem.h"
#include "Rules/VeyraVisionRules.h"
#include "GameFramework/GameStateBase.h"
#include "Targeting/VeyraParticipantData.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraVisionLog.h"
#include "Wards/VeyraWard.h"

namespace
{
	/** The side Observer looks from: a controller's is its player's. */
	EVeyraTeam SideOf(const UObject& Observer)
	{
		if (const AController* Controller = Cast<AController>(&Observer))
		{
			return VeyraTeams::TeamOf(Controller->PlayerState.Get());
		}
		return VeyraTeams::TeamOf(&Observer);
	}

	/** The body Observer looks from: its own, a participant's Vanguard, or a controller's player's Vanguard. */
	const AActor* ObserverBody(const UObject& Observer)
	{
		if (const AController* Controller = Cast<AController>(&Observer))
		{
			return Controller->PlayerState ? Controller->PlayerState->GetPawn() : nullptr;
		}
		if (const APlayerState* Participant = Cast<APlayerState>(&Observer))
		{
			return Participant->GetPawn();
		}
		return Cast<AActor>(&Observer);
	}

	bool IsVanguardBody(const AActor& Actor)
	{
		return VeyraUnits::IsVanguard(&Actor) && Actor.IsA<APawn>();
	}

	bool IsSide(EVeyraTeam Team)
	{
		return Team == EVeyraTeam::A || Team == EVeyraTeam::B;
	}

	/**
	 * The unit whose body Actor is: itself for a pawn, and its Vanguard for a participant's PlayerState,
	 * which is a unit for combat but sits nowhere on the battleground.
	 */
	const AActor* BodyOf(const AActor& Actor)
	{
		if (const APlayerState* Participant = Cast<APlayerState>(&Actor))
		{
			return Participant->GetPawn();
		}
		return &Actor;
	}

	/** How far Unit sees, or 0 when it gives no vision (wildlife, objectives, anything dead, anything with no body). */
	double SightOf(const AActor& Unit, const FVeyraSightTuning& Sight)
	{
		const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(&Unit);
		if (!Kind.IsSet() || !Unit.IsA<APawn>() || !VeyraTargeting::IsAlive(&Unit))
		{
			return 0.0;
		}
		switch (Kind.GetValue())
		{
		case EVeyraUnitKind::Vanguard:
			return Sight.Vanguard;
		case EVeyraUnitKind::Fluxborn:
			return Sight.Fluxborn;
		case EVeyraUnitKind::Structure:
			return Sight.Structure;
		case EVeyraUnitKind::Ward:
			return Sight.Ward;
		case EVeyraUnitKind::Wildlife:
		case EVeyraUnitKind::Objective:
			return 0.0;
		}
		return 0.0;
	}
}

UVeyraVisionSubsystem::UVeyraVisionSubsystem() = default;

UVeyraVisionSubsystem::~UVeyraVisionSubsystem() = default;

bool UVeyraVisionSubsystem::IsGated(const AActor& Unit)
{
	if (Unit.IsA<AVeyraProjectile>() || Unit.IsA<AVeyraDelayedArea>())
	{
		return true;
	}
	// A unit's body, which moves about the battleground; a PlayerState replicates to everyone (ADR-016 §3).
	const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(&Unit);
	return Kind.IsSet() && Unit.IsA<APawn>()
		&& (Kind.GetValue() == EVeyraUnitKind::Vanguard || Kind.GetValue() == EVeyraUnitKind::Fluxborn || Kind.GetValue() == EVeyraUnitKind::Wildlife
			|| Kind.GetValue() == EVeyraUnitKind::Ward);
}

bool UVeyraVisionSubsystem::IsInvisible(const AActor& Unit)
{
	// Only True Sight reveals an Invisible unit, and Sweeper alone grants it (Vision Bible §5).
	return VeyraUnits::IsWard(&Unit);
}

void UVeyraVisionSubsystem::Start()
{
	UWorld* World = GetWorld();
	if (bStarted || !World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	bStarted = true;
	if (UVeyraVisibilityRegistry* Registry = World->GetSubsystem<UVeyraVisibilityRegistry>())
	{
		Registry->Register(*this);
	}
	Gate = MakeShared<FVeyraFogGate>();
	Gate->Start(*World);
	SpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UVeyraVisionSubsystem::OnActorSpawned));
	if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
	{
		DeathHandle = Events->OnDeath.AddUObject(this, &UVeyraVisionSubsystem::OnDeath);
	}
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &UVeyraVisionSubsystem::UpdateNow), UVeyraVisionTuningSubsystem::Get().Update.UpdateSeconds,
		/*bLoop*/ true);
	UpdateNow();
}

void UVeyraVisionSubsystem::Stop()
{
	if (!bStarted)
	{
		return;
	}
	bStarted = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Timer);
		World->RemoveOnActorSpawnedHandler(SpawnedHandle);
		if (UVeyraCombatEventSubsystem* Events = World->GetSubsystem<UVeyraCombatEventSubsystem>())
		{
			Events->OnDeath.Remove(DeathHandle);
		}
		if (UVeyraVisibilityRegistry* Registry = World->GetSubsystem<UVeyraVisibilityRegistry>())
		{
			Registry->Unregister(*this);
		}
	}
	Gate.Reset();
	Seen.Reset();
	Known.Reset();
	Sources.Reset();
	Fogged.Reset();
	FogSightings.Reset();
	JoinedGroups.Reset();
}

void UVeyraVisionSubsystem::Deinitialize()
{
	Stop();
	Super::Deinitialize();
}

void UVeyraVisionSubsystem::OnActorSpawned(AActor* Actor)
{
	// Its own side receives it from its first send, when it replicates by then; if not, the next pass adds it.
	if (Actor && Gate && IsGated(*Actor))
	{
		const EVeyraTeam Team = VeyraTeams::TeamOf(Actor);
		if (IsSide(Team))
		{
			Gate->AddToSide(*Actor, Team);
		}
	}
}

AVeyraWard* UVeyraVisionSubsystem::PlaceWard(APlayerState& Placer, const FVector& Where)
{
	UWorld* World = GetWorld();
	const EVeyraTeam Team = VeyraTeams::TeamOf(&Placer);
	if (!World || World->GetNetMode() == NM_Client || !IsSide(Team))
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVeyraWard* Ward = World->SpawnActor<AVeyraWard>(Where, FRotator::ZeroRotator, Parameters);
	if (!Ward || !Ward->Place(Team, Placer))
	{
		if (Ward)
		{
			Ward->Destroy();
		}
		return nullptr;
	}
	// Its side receives it from its first send; it spawned before it had a side to join.
	if (Gate)
	{
		Gate->AddToSide(*Ward, Team);
	}
	UE_LOG(LogVeyraVision, Log, TEXT("%s places a ward at %s."), *Placer.GetPlayerName(), *Where.ToCompactString());
	return Ward;
}

void UVeyraVisionSubsystem::OnDeath(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	AVeyraWard* Ward = Victim ? Cast<AVeyraWard>(Victim->GetOwner()) : nullptr;
	if (!Ward)
	{
		return;
	}
	if (UVeyraRewardSubsystem* Rewards = GetWorld()->GetSubsystem<UVeyraRewardSubsystem>())
	{
		Rewards->RewardWardDestroyed(Death);
	}
	UE_LOG(LogVeyraVision, Log, TEXT("%s is destroyed."), *Ward->GetName());
	// After every other listener has heard of its death.
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Ward, [Ward]() { Ward->Destroy(); }));
}

void UVeyraVisionSubsystem::SetDenseFog(TArray<FVeyraFogCircle> Circles)
{
	Fog = MoveTemp(Circles);
	FogVolumes = VeyraVisionRules::ConnectVolumes(Fog);
	UE_LOG(LogVeyraVision, Log, TEXT("Dense Fog: %d circle(s) in %d volume(s)."), Fog.Num(), FogVolumes.IsEmpty() ? 0 : FMath::Max(FogVolumes) + 1);
	UpdateNow();
}

void UVeyraVisionSubsystem::UpdateNow()
{
	UWorld* World = GetWorld();
	if (!bStarted || !World)
	{
		return;
	}
	const FVeyraSightTuning& Sight = UVeyraVisionTuningSubsystem::Get().Sight;
	Sources.Reset();
	Known.Reset();
	TArray<const AActor*> Gated;
	// The Vanguards who look into fog: each side's living Vanguards, with their sight.
	TArray<TPair<const AActor*, double>> Lookouts;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor& Actor = **It;
		const EVeyraTeam Team = VeyraTeams::TeamOf(&Actor);
		if (IsSide(Team))
		{
			if (const double Radius = SightOf(Actor, Sight); Radius > 0.0)
			{
				Sources.Add(FVeyraSightSource{ Team, FVector2D(Actor.GetActorLocation()), Radius });
				if (IsVanguardBody(Actor))
				{
					Lookouts.Add({ &Actor, Radius });
				}
			}
		}
		if (IsGated(Actor))
		{
			Gated.Add(&Actor);
			Known.Add(&Actor);
			if (Gate && IsSide(Team))
			{
				Gate->AddToSide(Actor, Team);
			}
		}
	}

	// A Vanguard inside Dense Fog is hidden from all but the Vanguards inside the same volume, and a
	// teammate's sighting there is not shared (Vision Bible §2). Everything else is ordinary vision.
	Fogged.Reset();
	FogSightings.Reset();
	for (const AActor* Unit : Gated)
	{
		const int32 Volume = IsVanguardBody(*Unit) ? VeyraVisionRules::VolumeAt(Fog, FogVolumes, FVector2D(Unit->GetActorLocation())) : INDEX_NONE;
		if (Volume == INDEX_NONE)
		{
			continue;
		}
		Fogged.Add(Unit, Volume);
		for (const TPair<const AActor*, double>& Lookout : Lookouts)
		{
			const bool bInside = VeyraVisionRules::VolumeAt(Fog, FogVolumes, FVector2D(Lookout.Key->GetActorLocation())) == Volume;
			const bool bEnemy = VeyraTeams::TeamOf(Lookout.Key) != VeyraTeams::TeamOf(Unit);
			if (bInside && bEnemy && FVector2D::DistSquared(FVector2D(Lookout.Key->GetActorLocation()), FVector2D(Unit->GetActorLocation())) <= FMath::Square(Lookout.Value))
			{
				FogSightings.FindOrAdd(Lookout.Key).Add(Unit);
			}
		}
	}
	Seen.Reset();
	for (const EVeyraTeam Side : { EVeyraTeam::A, EVeyraTeam::B })
	{
		TSet<TWeakObjectPtr<const AActor>>& SideSeen = Seen.Add(Side);
		for (const AActor* Unit : Gated)
		{
			if (VeyraTeams::TeamOf(Unit) != Side && !Fogged.Contains(Unit) && !IsInvisible(*Unit) && VeyraVisionRules::IsSeenBy(Side, Sources, FVector2D(Unit->GetActorLocation())))
			{
				SideSeen.Add(Unit);
			}
		}
	}

	if (Gate && Gate->IsStarted())
	{
		// Each player receives what their side sees (G4 adds Dense Fog's own sightings).
		TSet<const APlayerController*> Present;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Controller = It->Get();
			const EVeyraTeam Team = Controller ? SideOf(*Controller) : EVeyraTeam::None;
			if (!IsSide(Team))
			{
				continue;
			}
			Present.Add(Controller);
			TSet<const AActor*> Receives;
			for (const TWeakObjectPtr<const AActor>& Unit : Seen.FindChecked(Team))
			{
				if (const AActor* Alive = Unit.Get())
				{
					Receives.Add(Alive);
				}
			}
			// And what the player's own Vanguard sees inside its fog, which reaches only them.
			const AActor* Body = ObserverBody(*Controller);
			if (const TSet<TWeakObjectPtr<const AActor>>* Sighted = Body ? FogSightings.Find(Body) : nullptr)
			{
				for (const TWeakObjectPtr<const AActor>& Unit : *Sighted)
				{
					if (const AActor* Alive = Unit.Get())
					{
						Receives.Add(Alive);
					}
				}
			}
			Gate->SyncPlayer(*Controller, Team, Receives);
		}
		Gate->ForgetPlayersExcept(Present);
	}
	UpdateParticipantData(*World);
}

void UVeyraVisionSubsystem::UpdateParticipantData(UWorld& World)
{
	const AGameStateBase* GameState = World.GetGameState();
	if (!GameState)
	{
		return;
	}
	TMap<APlayerController*, TSet<FName>> Wanted;
	for (FConstPlayerControllerIterator It = World.GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* Controller = It->Get(); Controller && IsSide(SideOf(*Controller)))
		{
			Wanted.Add(Controller);
		}
	}
	for (const APlayerState* Participant : GameState->PlayerArray)
	{
		const EVeyraTeam Team = Participant ? VeyraTeams::TeamOf(Participant) : EVeyraTeam::None;
		if (!IsSide(Team))
		{
			continue;
		}
		const FName Group = VeyraParticipantData::GroupOf(*Participant);
		const APawn* Body = Participant->GetPawn();
		for (TPair<APlayerController*, TSet<FName>>& Viewer : Wanted)
		{
			// A player's own data reaches them through the owner group.
			if (Viewer.Key->PlayerState == Participant)
			{
				continue;
			}
			const EVeyraTeam ViewerTeam = SideOf(*Viewer.Key);
			bool bSees = ViewerTeam == Team;
			if (!bSees && Body)
			{
				const TSet<TWeakObjectPtr<const AActor>>* SideSeen = Seen.Find(ViewerTeam);
				const AActor* ViewerBody = ObserverBody(*Viewer.Key);
				const TSet<TWeakObjectPtr<const AActor>>* Sighted = ViewerBody ? FogSightings.Find(ViewerBody) : nullptr;
				bSees = (SideSeen && SideSeen->Contains(Body)) || (Sighted && Sighted->Contains(Body));
			}
			if (bSees)
			{
				Viewer.Value.Add(Group);
			}
		}
	}
	for (TPair<APlayerController*, TSet<FName>>& Viewer : Wanted)
	{
		TSet<FName>& Joined = JoinedGroups.FindOrAdd(Viewer.Key);
		for (auto It = Joined.CreateIterator(); It; ++It)
		{
			if (!Viewer.Value.Contains(*It))
			{
				Viewer.Key->RemoveFromNetConditionGroup(*It);
				It.RemoveCurrent();
			}
		}
		for (const FName& Group : Viewer.Value)
		{
			if (!Joined.Contains(Group))
			{
				Viewer.Key->IncludeInNetConditionGroup(Group);
				Joined.Add(Group);
			}
		}
	}
	for (auto It = JoinedGroups.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

bool UVeyraVisionSubsystem::CanSee(const UObject& Observer, const AActor& Target) const
{
	const EVeyraTeam Side = SideOf(Observer);
	// Something on no side, such as a creature answering its attacker, sees what it fights.
	if (!IsSide(Side))
	{
		return true;
	}
	// An enemy Vanguard inside Dense Fog: only a Vanguard inside the same volume sees it (Vision Bible §2).
	const AActor* Body = BodyOf(Target);
	if (bStarted && Body && Fogged.Contains(Body) && VeyraTeams::TeamOf(Body) != Side)
	{
		const AActor* Looker = ObserverBody(Observer);
		const TSet<TWeakObjectPtr<const AActor>>* Sighted = Looker ? FogSightings.Find(Looker) : nullptr;
		return Sighted && Sighted->Contains(Body);
	}
	return IsVisibleToTeam(Side, Target);
}

bool UVeyraVisionSubsystem::IsVisibleToTeam(EVeyraTeam Team, const AActor& Target) const
{
	// A participant is seen where its Vanguard is.
	const AActor* Body = BodyOf(Target);
	if (!bStarted || !Body || !IsGated(*Body) || VeyraTeams::TeamOf(Body) == Team)
	{
		return true;
	}
	const TSet<TWeakObjectPtr<const AActor>>* SideSeen = Seen.Find(Team);
	if (SideSeen && SideSeen->Contains(Body))
	{
		return true;
	}
	if (IsInvisible(*Body))
	{
		return false;
	}
	// Spawned since the last pass: judged now, against that pass's sources.
	return !Known.Contains(Body) && VeyraVisionRules::IsSeenBy(Team, Sources, FVector2D(Body->GetActorLocation()));
}
