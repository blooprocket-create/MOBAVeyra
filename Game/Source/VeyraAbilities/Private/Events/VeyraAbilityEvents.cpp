// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Events/VeyraAbilityEvents.h"

#include "Engine/World.h"

void UVeyraAbilityEventSubsystem::Announce(const UWorld* World, const FVeyraAbilityHit& Hit)
{
	if (UVeyraAbilityEventSubsystem* Events = World ? World->GetSubsystem<UVeyraAbilityEventSubsystem>() : nullptr)
	{
		Events->OnAbilityHit.Broadcast(Hit);
	}
}
