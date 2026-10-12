// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Kit/VeyraKitOrders.h"

#include "Brain/VeyraBotAbilities.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

namespace VeyraKitOrders
{
	bool IsPassingRejection(EVeyraCastRejection Rejection)
	{
		switch (Rejection)
		{
		case EVeyraCastRejection::CasterDead:
		case EVeyraCastRejection::CrowdControlled:
		case EVeyraCastRejection::Busy:
		case EVeyraCastRejection::InsufficientResource:
		case EVeyraCastRejection::TargetDead:
		case EVeyraCastRejection::OutOfRange:
		case EVeyraCastRejection::Paused:
		// A companion forms or reforms beside its owner a moment later (ADR-034 §3).
		case EVeyraCastRejection::NoCompanion:
		// The status that holds the cast back ends with its time, as a ride's lock does (ADR-035 §2).
		case EVeyraCastRejection::HeldBack:
			return true;
		default:
			return false;
		}
	}

	AActor* FindNearestEnemyBody(const AVeyraPlayerController& Controller, const AVeyraGameState& GameState, const FVector& From)
	{
		const AVeyraPlayerState* Self = Controller.GetPlayerState<AVeyraPlayerState>();
		AActor* Nearest = nullptr;
		double NearestDistance = TNumericLimits<double>::Max();
		for (const APlayerState* Participant : GameState.PlayerArray)
		{
			const AVeyraPlayerState* Candidate = Cast<AVeyraPlayerState>(Participant);
			APawn* Body = Candidate ? Candidate->GetPawn() : nullptr;
			if (!Self || !Body || Candidate->GetVeyraTeam() == Self->GetVeyraTeam() || !VeyraTargeting::IsAlive(Body))
			{
				continue;
			}
			const double Distance = FVector::Dist2D(From, Body->GetActorLocation());
			if (Distance < NearestDistance)
			{
				Nearest = Body;
				NearestDistance = Distance;
			}
		}
		return Nearest;
	}

	bool IsForAlly(const FVeyraContentId& Ability)
	{
		const TOptional<FVeyraBotAbilityProfile> Profile = VeyraBotAbilities::ProfileOf(Ability, 0.0);
		return Profile.IsSet() && Profile->Targeting == EVeyraBotTargeting::Self && Profile->AllyReach > 0.0;
	}

	double ReachOf(const FVeyraContentId& Ability)
	{
		const TOptional<FVeyraBotAbilityProfile> Profile = VeyraBotAbilities::ProfileOf(Ability, 0.0);
		return Profile.IsSet() ? Profile->Reach : 0.0;
	}

	FVeyraCastTarget TargetAt(AActor& Aim)
	{
		FVeyraCastTarget Target;
		Target.Actor = &Aim;
		Target.bHasLocation = true;
		Target.Location = Aim.GetActorLocation();
		return Target;
	}

	double ReadyAtOf(const AVeyraPlayerState& Own, const FVeyraContentId& Ability)
	{
		const UVeyraCooldownComponent* Cooldowns = Own.FindComponentByClass<UVeyraCooldownComponent>();
		const UVeyraAbilityLoadoutComponent* Loadout = Own.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		if (!Cooldowns || !Loadout)
		{
			return 0.0;
		}
		// Its own cooldown, or a shared one it starts, as a dash a mode grants may: whichever is due later.
		const FVeyraContentId Id = Loadout->CooldownIdOf(Ability);
		const AGameStateBase* Clock = Own.GetWorld() ? Own.GetWorld()->GetGameState() : nullptr;
		const double Now = Clock ? Clock->GetServerWorldTimeSeconds() : 0.0;
		const double Shared = Cooldowns->GetSharedRemainingSeconds(Id, Now);
		return FMath::Max(Cooldowns->GetReadyAt(Id), Shared > 0.0 ? Now + Shared : 0.0);
	}
}
