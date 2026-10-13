// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Movement/VeyraPathFollowingComponent.h"

#include "AI/Navigation/NavAgentInterface.h"
#include "NavigationData.h"

double UVeyraPathFollowingComponent::ArrivalDistance() const
{
	if (!Path.IsValid() || !NavMovementInterface.IsValid())
	{
		return 0.0;
	}
	// The engine's own reach test (HasReachedDestination): the acceptance radius, the goal's radius where the test takes
	// it, and the body's radius by its multiplier where the test takes that.
	float GoalRadius = 0.0f;
	if (bReachTestIncludesGoalRadius && DestinationActor.IsValid() && DestinationAgent)
	{
		FVector GoalOffset;
		float GoalHalfHeight = 0.0f;
		DestinationAgent->GetMoveGoalReachTest(GetOwner(), MoveOffset, GoalOffset, GoalRadius, GoalHalfHeight);
	}
	float AgentRadius = 0.0f;
	float AgentHalfHeight = 0.0f;
	NavMovementInterface->GetSimpleCollisionCylinder(AgentRadius, AgentHalfHeight);
	const double Acceptance = GetFinalAcceptanceRadius(*Path, OriginalMoveRequestGoalLocation);
	return Acceptance + GoalRadius + (bReachTestIncludesAgentRadius ? AgentRadius * MinAgentRadiusPct : 0.0f);
}

bool UVeyraPathFollowingComponent::GetCornersAhead(const FVector& From, TArray<FVector>& OutCorners) const
{
	OutCorners.Reset();
	if (GetStatus() != EPathFollowingStatus::Moving || !Path.IsValid() || !Path->IsValid())
	{
		return false;
	}
	const TArray<FNavPathPoint>& Points = Path->GetPathPoints();
	const int32 Next = static_cast<int32>(GetNextPathIndex());
	if (!Points.IsValidIndex(Next))
	{
		return false;
	}
	for (int32 Index = Next; Index < Points.Num(); ++Index)
	{
		OutCorners.Add(Points[Index].Location);
	}
	// A last leg toward a goal that moves heads for where the goal is now, as the path following steers it.
	if (!Path->IsPartial() && bMoveToGoalOnLastSegment && DestinationActor.IsValid())
	{
		OutCorners.Last() = DestinationAgent ? DestinationAgent->GetNavAgentLocation() + MoveOffset : DestinationActor->GetActorLocation();
	}
	// The body stops where it counts as arrived: that far short of the goal's place along its last leg.
	const FVector Goal = OutCorners.Last();
	const FVector LegStart = OutCorners.Num() > 1 ? OutCorners[OutCorners.Num() - 2] : From;
	const FVector2D Leg = FVector2D(Goal - LegStart);
	const double LegLength = Leg.Size();
	const double Short = FMath::Min(ArrivalDistance(), LegLength);
	if (LegLength > UE_KINDA_SMALL_NUMBER && Short > 0.0)
	{
		const FVector Stop = Goal - FVector(Leg / LegLength * Short, 0.0);
		OutCorners.Last() = FVector(Stop.X, Stop.Y, FMath::Lerp(LegStart.Z, Goal.Z, 1.0 - Short / LegLength));
	}
	return true;
}
