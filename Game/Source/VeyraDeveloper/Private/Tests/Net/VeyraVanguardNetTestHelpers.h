// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "GameFramework/PlayerState.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "VeyraPlayerState.h"

// Helpers the Veyra.Net.Vanguards.* suites share: a duel between the first two clients' Vanguards,
// and what the server or a client sees of them.
namespace VeyraNetTests
{
	inline FVeyraContentId ContentId(const TCHAR* Text)
	{
		return FVeyraContentId::FromText(Text).GetValue();
	}

	/** On the server: the participant of the client at ClientIndex. */
	inline AVeyraPlayerState* ParticipantOf(const FBasePIENetworkComponentState& ServerState, int32 ClientIndex)
	{
		const AVeyraPlayerController* Controller = ServerControllerOf(ServerState, ClientIndex);
		return Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
	}

	inline double HealthLost(const AVeyraPlayerState* Participant)
	{
		const UAbilitySystemComponent& Unit = *Participant->GetAbilitySystemComponent();
		return Unit.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - Unit.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
	}

	inline const TArray<FVeyraShieldEntry>& ShieldsOf(const AVeyraPlayerState* Participant)
	{
		return Participant->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger().Shields;
	}

	/** The status with Id on the participant with PlayerId, as this machine sees it; null without it. */
	inline const FVeyraStatusEntry* SeenStatus(const UWorld* World, int32 PlayerId, const FVeyraContentId& Id)
	{
		for (const APlayerState* Participant : GameStateOf(World)->PlayerArray)
		{
			if (Participant && Participant->GetPlayerId() == PlayerId)
			{
				return Participant->FindComponentByClass<UVeyraStatusComponent>()->GetLedger().Entries.FindByPredicate(
					[&Id](const FVeyraStatusEntry& Entry) { return Entry.Id == Id; });
			}
		}
		return nullptr;
	}

	/** Whether this machine sees the participant with PlayerId carrying a status of Kind. */
	inline bool SeesStatus(const UWorld* World, int32 PlayerId, EVeyraStatusKind Kind)
	{
		for (const APlayerState* Participant : GameStateOf(World)->PlayerArray)
		{
			if (Participant && Participant->GetPlayerId() == PlayerId)
			{
				return Participant->FindComponentByClass<UVeyraStatusComponent>()->GetLedger().Entries.ContainsByPredicate(
					[Kind](const FVeyraStatusEntry& Entry) { return Entry.Kind == Kind; });
			}
		}
		return false;
	}

	/**
	 * A duel between the first two clients' Vanguards. On the server, Prepare raises both to a level
	 * with a rank in every ability and stands the second a distance from the first, toward the lane's
	 * centre; a client then casts at it.
	 */
	struct FVanguardDuel
	{
		int32 CasterId = INDEX_NONE;
		int32 TargetId = INDEX_NONE;
		FVector TargetPoint = FVector::ZeroVector;

		/** Server: returns false if either participant is not Vanguard or a rank is refused. */
		bool Prepare(FBasePIENetworkComponentState& ServerState, const FVeyraContentId& Vanguard, int32 Level, double Distance)
		{
			AVeyraPlayerState* Caster = ParticipantOf(ServerState, 0);
			AVeyraPlayerState* Target = ParticipantOf(ServerState, 1);
			if (!Caster || !Target || !Caster->GetPawn() || !Target->GetPawn() || Caster->GetVanguardId() != Vanguard || Target->GetVanguardId() != Vanguard)
			{
				return false;
			}
			const TArray<int32>& ToNextLevel = UVeyraProgressionTuningSubsystem::Get().Experience.ToNextLevel;
			int32 Experience = 0;
			for (int32 Step = 1; Step < Level; ++Step)
			{
				Experience += ToNextLevel[Step - 1];
			}
			for (AVeyraPlayerState* Participant : { Caster, Target })
			{
				UVeyraProgressionComponent* Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
				Progression->AddExperience(Experience);
				for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
				{
					if (Progression->AllocateRank(Slot) != EVeyraRankRefusal::None)
					{
						return false;
					}
				}
			}
			const FVector From = Caster->GetPawn()->GetActorLocation();
			TargetPoint = From + FVector(-From.X, 0.0, 0.0).GetSafeNormal() * Distance;
			Target->GetPawn()->SetActorLocation(TargetPoint);
			CasterId = Caster->GetPlayerId();
			TargetId = Target->GetPlayerId();
			return true;
		}

		/** Client: the first client casts Slot at the second Vanguard and its place. */
		void CastAtTheTarget(const UWorld* World, EVeyraAbilitySlot Slot) const
		{
			FVeyraCastTarget Target;
			Target.Actor = FindVanguard(World, TargetId);
			Target.bHasLocation = true;
			Target.Location = TargetPoint;
			LocalControllerOf(World)->IssueCastOrder(Slot, Target);
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
