// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraCombatRollSubsystem.h"

#include "AbilitySystemComponent.h"
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

double UVeyraCombatRollSubsystem::Draw(const UWorld* World, const UAbilitySystemComponent& Unit, const FVeyraContentId& Channel, int32 Draws)
{
	UVeyraCombatRollSubsystem* Rolls = World ? World->GetSubsystem<UVeyraCombatRollSubsystem>() : nullptr;
	if (!Rolls)
	{
		return static_cast<double>(FMath::FRand());
	}
	const TPair<FObjectKey, FVeyraContentId> Key(FObjectKey(&Unit), Channel);
	FVeyraOutcomeBag* Bag = Rolls->Bags.Find(Key);
	if (!Bag)
	{
		// A new bag is a moment to let go of the bags of units that are gone.
		for (auto It = Rolls->Bags.CreateIterator(); It; ++It)
		{
			if (!It.Key().Key.ResolveObjectPtr())
			{
				It.RemoveCurrent();
			}
		}
		Bag = &Rolls->Bags.Emplace(Key, FVeyraOutcomeBag(static_cast<int32>(Rolls->Stream.GetUnsignedInt())));
	}
	return Bag->Next(Draws);
}

void UVeyraCombatRollSubsystem::SetSeed(int32 Seed)
{
	Stream.Initialize(Seed);
	Bags.Reset();
}
