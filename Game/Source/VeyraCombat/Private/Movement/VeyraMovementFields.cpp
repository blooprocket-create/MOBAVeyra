// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Movement/VeyraMovementFields.h"

#include "Math/UnrealMathUtility.h"

int32 UVeyraMovementFieldSubsystem::Add(const FVeyraMovementField& Field)
{
	const int32 Handle = NextHandle++;
	Fields.Add(Handle, Field);
	return Handle;
}

void UVeyraMovementFieldSubsystem::Remove(int32 Handle)
{
	Fields.Remove(Handle);
}

FVector UVeyraMovementFieldSubsystem::Bend(EVeyraTeam UnitSide, const FVector& Start, const FVector& End) const
{
	FVector Bent = End;
	if (UnitSide == EVeyraTeam::None)
	{
		return Bent;
	}
	for (const TPair<int32, FVeyraMovementField>& Entry : Fields)
	{
		const FVeyraMovementField& Field = Entry.Value;
		if (Field.Side == EVeyraTeam::None || Field.Side == UnitSide)
		{
			continue;
		}
		// Only a move that starts inside the field or crosses it bends.
		const FVector Flat(Field.Centre.X, Field.Centre.Y, Start.Z);
		const FVector Nearest = FMath::ClosestPointOnSegment(Flat, Start, FVector(End.X, End.Y, Start.Z));
		if (FVector::Dist2D(Nearest, Flat) > Field.Radius)
		{
			continue;
		}
		const FVector ToCentre = FVector(Field.Centre.X - Bent.X, Field.Centre.Y - Bent.Y, 0.0);
		Bent += ToCentre.GetSafeNormal() * FMath::Min(Field.Pull, ToCentre.Size());
	}
	return Bent;
}
