// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraCombatRollSubsystem.h"

#include "Engine/World.h"
#include "VeyraCombatLog.h"

void UVeyraCombatRollSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	SetSeed(FMath::Rand());
	if (InWorld.GetNetMode() != NM_Client)
	{
		UE_LOG(LogVeyraCombat, Log, TEXT("Combat rolls use seed %d."), GetSeed());
	}
}

double UVeyraCombatRollSubsystem::Roll(const UWorld* World)
{
	UVeyraCombatRollSubsystem* Rolls = World ? World->GetSubsystem<UVeyraCombatRollSubsystem>() : nullptr;
	return Rolls ? static_cast<double>(Rolls->Stream.GetFraction()) : static_cast<double>(FMath::FRand());
}

void UVeyraCombatRollSubsystem::SetSeed(int32 Seed)
{
	Stream.Initialize(Seed);
}
