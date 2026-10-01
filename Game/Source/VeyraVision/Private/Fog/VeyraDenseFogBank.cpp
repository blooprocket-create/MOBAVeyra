// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Fog/VeyraDenseFogBank.h"

#include "Components/SceneComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

AVeyraDenseFogBank::AVeyraDenseFogBank()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// Where fog lies is map knowledge: every player receives it (Vision Bible §2).
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AVeyraDenseFogBank::SetCircles(TConstArrayView<FVeyraFogCircle> Circles)
{
	Centres.Reset(Circles.Num());
	for (const FVeyraFogCircle& Circle : Circles)
	{
		Centres.Add(FVector2f(Circle.Center));
	}
	Radius = Circles.IsEmpty() ? 0.0f : static_cast<float>(Circles[0].Radius);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraDenseFogBank, Centres, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraDenseFogBank, Radius, this);
}

TArray<FVeyraFogCircle> AVeyraDenseFogBank::GetCircles() const
{
	TArray<FVeyraFogCircle> Circles;
	for (const FVector2f& Centre : Centres)
	{
		Circles.Add(FVeyraFogCircle{ FVector2D(Centre), Radius });
	}
	return Circles;
}

void AVeyraDenseFogBank::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraDenseFogBank, Centres, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraDenseFogBank, Radius, Params);
}
