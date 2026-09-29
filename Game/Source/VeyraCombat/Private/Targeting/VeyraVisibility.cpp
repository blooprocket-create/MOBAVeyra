// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Targeting/VeyraVisibility.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UVeyraVisibilityRegistry::Register(IVeyraVisibility& InVisibility)
{
	checkf(!Visibility || Visibility == &InVisibility, TEXT("A world has one vision."));
	Visibility = &InVisibility;
}

void UVeyraVisibilityRegistry::Unregister(const IVeyraVisibility& InVisibility)
{
	if (Visibility == &InVisibility)
	{
		Visibility = nullptr;
	}
}

namespace VeyraVisibility
{
const IVeyraVisibility* Find(const UWorld* World)
{
	const UVeyraVisibilityRegistry* Registry = World ? World->GetSubsystem<UVeyraVisibilityRegistry>() : nullptr;
	return Registry ? Registry->Get() : nullptr;
}

bool CanSee(const UObject& Observer, const AActor& Target)
{
	const IVeyraVisibility* Visibility = Find(Target.GetWorld());
	return !Visibility || Visibility->CanSee(Observer, Target);
}

bool IsVisibleToTeam(EVeyraTeam Team, const AActor& Target)
{
	const IVeyraVisibility* Visibility = Find(Target.GetWorld());
	return !Visibility || Visibility->IsVisibleToTeam(Team, Target);
}
}
