// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraVisionSubsystem.h"

#include "Delivery/VeyraDelayedArea.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Gate/VeyraFogGate.h"
#include "Rules/VeyraVisionRules.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraVisionTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraVisionLog.h"

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
		&& (Kind.GetValue() == EVeyraUnitKind::Vanguard || Kind.GetValue() == EVeyraUnitKind::Fluxborn || Kind.GetValue() == EVeyraUnitKind::Wildlife);
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
		if (UVeyraVisibilityRegistry* Registry = World->GetSubsystem<UVeyraVisibilityRegistry>())
		{
			Registry->Unregister(*this);
		}
	}
	Gate.Reset();
	Seen.Reset();
	Known.Reset();
	Sources.Reset();
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
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor& Actor = **It;
		const EVeyraTeam Team = VeyraTeams::TeamOf(&Actor);
		if (IsSide(Team))
		{
			if (const double Radius = SightOf(Actor, Sight); Radius > 0.0)
			{
				Sources.Add(FVeyraSightSource{ Team, FVector2D(Actor.GetActorLocation()), Radius });
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

	Seen.Reset();
	for (const EVeyraTeam Side : { EVeyraTeam::A, EVeyraTeam::B })
	{
		TSet<TWeakObjectPtr<const AActor>>& SideSeen = Seen.Add(Side);
		for (const AActor* Unit : Gated)
		{
			if (VeyraTeams::TeamOf(Unit) != Side && VeyraVisionRules::IsSeenBy(Side, Sources, FVector2D(Unit->GetActorLocation())))
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
			Gate->SyncPlayer(*Controller, Team, Receives);
		}
		Gate->ForgetPlayersExcept(Present);
	}
}

bool UVeyraVisionSubsystem::CanSee(const UObject& Observer, const AActor& Target) const
{
	const EVeyraTeam Side = SideOf(Observer);
	// Something on no side, such as a creature answering its attacker, sees what it fights.
	return !IsSide(Side) || IsVisibleToTeam(Side, Target);
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
	// Spawned since the last pass: judged now, against that pass's sources.
	return !Known.Contains(Body) && VeyraVisionRules::IsSeenBy(Team, Sources, FVector2D(Body->GetActorLocation()));
}
