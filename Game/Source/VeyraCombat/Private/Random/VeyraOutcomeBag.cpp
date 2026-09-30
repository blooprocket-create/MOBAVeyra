// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Random/VeyraOutcomeBag.h"

FVeyraOutcomeBag::FVeyraOutcomeBag(int32 Seed)
	: Stream(Seed)
{
}

double FVeyraOutcomeBag::Next(int32 Draws)
{
	if (Values.IsEmpty())
	{
		// One value from each equal slice of [0, 1), then a Fisher–Yates shuffle.
		const int32 Size = FMath::Max(Draws, 1);
		Values.Reserve(Size);
		for (int32 Slice = 0; Slice < Size; ++Slice)
		{
			Values.Add((Slice + static_cast<double>(Stream.GetFraction())) / Size);
		}
		for (int32 Last = Size - 1; Last > 0; --Last)
		{
			Values.Swap(Last, Stream.RandRange(0, Last));
		}
	}
	return Values.Pop(EAllowShrinking::No);
}
