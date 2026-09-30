// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/RandomStream.h"

/**
 * One independent source of chance, drawn as a shuffled bag (ADR-023 §10): the author's
 * pseudo-random rule for crits, and for any other chance-based check that wants it.
 *
 * A bag holds Draws uniform values, one from each equal slice of [0, 1), in shuffled order. A check
 * takes the next and succeeds when it is below the check's chance at that moment; an empty bag is
 * refilled and shuffled. So over a full bag a constant chance p succeeds floor(p × Draws) or
 * ceil(p × Draws) times, and exactly p × Draws on average, while which draws succeed stays random:
 * long lucky and unlucky streaks are cut short. Each draw is uniform on its own, so a chance that
 * changes between draws is honoured with no bias; 0 never succeeds and 1 always does. One draw a bag
 * is an ordinary independent roll.
 *
 * Each bag has its own stream, seeded when it is made, so one bag's draws never change another's.
 */
class VEYRACOMBAT_API FVeyraOutcomeBag
{
public:
	explicit FVeyraOutcomeBag(int32 Seed);

	/** The next value in [0, 1), refilling the bag with Draws values (at least 1) when it is empty. */
	double Next(int32 Draws);

	/** Whether the next value falls below Chance. */
	bool Check(double Chance, int32 Draws)
	{
		return Next(Draws) < Chance;
	}

	int32 Remaining() const { return Values.Num(); }

private:
	FRandomStream Stream;

	/** What is left of the bag, drawn from the back. */
	TArray<double> Values;
};
