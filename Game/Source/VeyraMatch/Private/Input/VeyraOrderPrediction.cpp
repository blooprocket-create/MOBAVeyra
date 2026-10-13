// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Input/VeyraOrderPrediction.h"

TOptional<FVeyraMotion> VeyraOrderPrediction::Predict(const FVeyraOrderPredictionInputs& In)
{
	if (!In.bMayWalk || In.Goal.ContainsNaN() || In.From.ContainsNaN() || !FMath::IsFinite(In.Speed) || !FMath::IsFinite(In.StopShort))
	{
		return {};
	}
	FVeyraMotion Predicted;
	Predicted.StartTime = In.ServerNow;
	Predicted.Start = In.From;
	Predicted.Yaw = static_cast<float>(In.Yaw);
	Predicted.Speed = static_cast<float>(In.Speed);
	Predicted.OrderNumber = In.OrderNumber;
	Predicted.Kind = EVeyraMotionKind::Stand;

	const FVector2D Toward(In.Goal - In.From);
	const double Distance = Toward.Size();
	if (Distance <= In.StopShort)
	{
		// Already where the order would take it: an attack turns to its target; a move has nothing to show.
		if (!In.bFacesWithinReach)
		{
			return {};
		}
		if (Distance > UE_KINDA_SMALL_NUMBER)
		{
			Predicted.Yaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Toward.Y, Toward.X)));
		}
		return Predicted;
	}
	if (!(In.Speed > 0.0))
	{
		return {};
	}
	// Straight toward it, stopping short as the server will; the server's path takes over as it answers.
	const FVector2D Stop = FVector2D(In.From) + Toward / Distance * (Distance - In.StopShort);
	Predicted.Kind = EVeyraMotionKind::Walk;
	Predicted.Corners.Add(FVector(Stop, In.From.Z));
	return Predicted;
}
