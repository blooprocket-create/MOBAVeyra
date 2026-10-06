// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Layout/VeyraWidthCurve.h"

TArray<FVeyraCurveSample> VeyraWidthCurve::Sample(TConstArrayView<FVector2D> Points, TConstArrayView<double> Widths, int32 SamplesPerSegment)
{
	TArray<FVeyraCurveSample> Samples;
	const int32 Count = Points.Num();
	if (Count < 2 || Widths.Num() != Count || SamplesPerSegment < 1)
	{
		return Samples;
	}
	const auto At = [&Points, Count](int32 Index) { return Points[FMath::Clamp(Index, 0, Count - 1)]; };
	for (int32 Index = 0; Index < Count - 1; ++Index)
	{
		const FVector2D From = At(Index);
		const FVector2D To = At(Index + 1);
		const FVector2D FromTangent = (At(Index + 1) - At(Index - 1)) / 2.0;
		const FVector2D ToTangent = (At(Index + 2) - At(Index)) / 2.0;
		for (int32 Step = 0; Step < SamplesPerSegment; ++Step)
		{
			const double T = static_cast<double>(Step) / SamplesPerSegment;
			Samples.Add({ FMath::CubicInterp(From, FromTangent, To, ToTangent, T), FMath::Lerp(Widths[Index], Widths[Index + 1], T) });
		}
	}
	Samples.Add({ At(Count - 1), Widths.Last() });
	return Samples;
}

double VeyraWidthCurve::SignedDistance(TConstArrayView<FVeyraCurveSample> Samples, const FVector2D& Point)
{
	double Nearest = TNumericLimits<double>::Max();
	for (int32 Index = 1; Index < Samples.Num(); ++Index)
	{
		const FVeyraCurveSample& A = Samples[Index - 1];
		const FVeyraCurveSample& B = Samples[Index];
		const FVector2D Delta = B.Point - A.Point;
		const double LengthSquared = Delta.SizeSquared();
		const double T = LengthSquared > UE_DOUBLE_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(Point - A.Point, Delta) / LengthSquared, 0.0, 1.0) : 0.0;
		Nearest = FMath::Min(Nearest, FVector2D::Distance(Point, A.Point + Delta * T) - FMath::Lerp(A.Width, B.Width, T) / 2.0);
	}
	return Nearest;
}
