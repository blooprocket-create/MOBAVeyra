// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Targeting/VeyraParticipantData.h"

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Net/Core/Misc/NetConditionGroupManager.h"
#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"
#include "Net/Subsystems/NetworkSubsystem.h"

namespace VeyraParticipantData
{
bool IsParticipantData(const UActorComponent& Component)
{
	const AActor* Owner = Component.GetOwner();
	return Owner && Owner->IsA<APlayerState>();
}

ELifetimeCondition ConditionFor(const UActorComponent& Component, ELifetimeCondition Otherwise)
{
	return IsParticipantData(Component) ? COND_NetGroup : Otherwise;
}

FName GroupOf(const AActor& Participant)
{
	// Server-side only, so the process's own object ID names it.
	return FName(*FString::Printf(TEXT("Veyra.Participant.%u"), Participant.GetUniqueID()));
}

void Gate(UObject& SubObject, const AActor& Participant)
{
	UWorld* World = Participant.GetWorld();
	UNetworkSubsystem* Network = World && World->GetNetMode() != NM_Client ? World->GetSubsystem<UNetworkSubsystem>() : nullptr;
	if (!Network)
	{
		return;
	}
	UE::Net::FNetConditionGroupManager& Groups = Network->GetNetConditionGroupManager();
	Groups.RegisterSubObjectInGroup(&SubObject, GroupOf(Participant));
	Groups.RegisterSubObjectInGroup(&SubObject, UE::Net::NetGroupOwner);
	// The replay driver records through the legacy path, which ignores Iris's filters (ADR-006 §5).
	Groups.RegisterSubObjectInGroup(&SubObject, UE::Net::NetGroupReplay);
	// Takes effect now for a subobject that already replicates; ApplyGates covers one that does not yet.
	UE::Net::FReplicationSystemUtil::UpdateSubObjectGroupMemberships(&SubObject, World);
}

void ApplyGates(const AActor& Participant)
{
	UWorld* World = Participant.GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	// An object in no group has nothing to apply, so every replicated component may be offered.
	for (UActorComponent* Component : Participant.GetComponents())
	{
		if (!Component || !Component->GetIsReplicated())
		{
			continue;
		}
		UE::Net::FReplicationSystemUtil::UpdateSubObjectGroupMemberships(Component, World);
		if (const UAbilitySystemComponent* Abilities = Cast<UAbilitySystemComponent>(Component))
		{
			for (UAttributeSet* Set : Abilities->GetSpawnedAttributes())
			{
				if (Set)
				{
					UE::Net::FReplicationSystemUtil::UpdateSubObjectGroupMemberships(Set, World);
				}
			}
		}
	}
}
}
