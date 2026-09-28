// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Bots/VeyraBotWanderComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "VeyraGameState.h"
#include "VeyraTeamStart.h"
#include "VeyraVanguardController.h"

UVeyraBotWanderComponent::UVeyraBotWanderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVeyraBotWanderComponent::BeginPlay()
{
	Super::BeginPlay();
	const float Interval = static_cast<float>(UVeyraMatchTuningSubsystem::Get().Bots.WanderIntervalSeconds);
	// A world timer: a pause holds the bots too.
	GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this] { Wander(); }), Interval, /*bLoop*/ true);
}

void UVeyraBotWanderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Timer);
	}
	Super::EndPlay(EndPlayReason);
}

TOptional<FVector> UVeyraBotWanderComponent::Wander()
{
	UWorld* World = GetWorld();
	const AVeyraGameState* GameState = World ? World->GetGameState<AVeyraGameState>() : nullptr;
	AVeyraVanguardController* Controller = Cast<AVeyraVanguardController>(GetOwner());
	if (!GameState || GameState->GetPhase() != EVeyraMatchPhase::Live || !Controller || !Controller->GetPawn())
	{
		return {};
	}

	// The middle of the map: midway between the sides' starts.
	FVector Sum = FVector::ZeroVector;
	int32 Starts = 0;
	for (TActorIterator<AVeyraTeamStart> It(World); It; ++It)
	{
		Sum += It->GetActorLocation();
		++Starts;
	}
	if (Starts == 0)
	{
		return {};
	}
	const FVector Middle = Sum / Starts;

	// A point spread evenly over the disc around it.
	const double Radius = UVeyraMatchTuningSubsystem::Get().Bots.WanderRadius * FMath::Sqrt(Random.GetFraction());
	const double Angle = Random.GetFraction() * UE_TWO_PI;
	const FVector Destination = Middle + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Radius;
	Controller->MoveToDestination(Destination);
	return Destination;
}
