// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraAbilitySystemComponent.h"

#include "AttributeSet.h"
#include "Targeting/VeyraParticipantData.h"

void UVeyraAbilitySystemComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	// The engine registered each set for every connection; a participant's go behind the fog instead.
	if (!IsUsingRegisteredSubObjectList() || !VeyraParticipantData::IsParticipantData(*this) || !GetOwner()->HasAuthority())
	{
		return;
	}
	for (UAttributeSet* Set : GetSpawnedAttributes())
	{
		if (Set)
		{
			RemoveReplicatedSubObject(Set);
			AddReplicatedSubObject(Set, COND_NetGroup);
			VeyraParticipantData::Gate(*Set, *GetOwner());
		}
	}
}
