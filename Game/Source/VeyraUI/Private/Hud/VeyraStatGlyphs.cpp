// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraStatGlyphs.h"

namespace VeyraStatGlyphs
{
namespace
{
	using FStroke = TArray<FVector2D>;

	/** The heater shield the defences share. */
	FStroke Shield()
	{
		return { { 8, 2 }, { 13, 4 }, { 13, 8 }, { 11.5, 11.2 }, { 8, 14 }, { 4.5, 11.2 }, { 3, 8 }, { 3, 4 }, { 8, 2 } };
	}

	/** An arrow through a wall, the penetrations'. */
	TArray<FStroke> Pierce()
	{
		return { { { 2, 8 }, { 12, 8 } }, { { 9, 5 }, { 12, 8 }, { 9, 11 } }, { { 14, 3 }, { 14, 13 } } };
	}
}

TArray<TArray<FVector2D>> Of(const FString& Stat)
{
	if (Stat == TEXT("physical_power"))
	{
		// A sword: its blade, its point's guard and its hilt.
		return { { { 3, 13 }, { 11, 5 } }, { { 9, 3 }, { 13, 3 }, { 13, 7 } }, { { 2, 12 }, { 4, 14 } } };
	}
	if (Stat == TEXT("magic_power"))
	{
		// A four-pointed star.
		return { { { 8, 2 }, { 9.6, 6.4 }, { 14, 8 }, { 9.6, 9.6 }, { 8, 14 }, { 6.4, 9.6 }, { 2, 8 }, { 6.4, 6.4 }, { 8, 2 } } };
	}
	if (Stat == TEXT("armor"))
	{
		return { Shield() };
	}
	if (Stat == TEXT("magic_resist"))
	{
		// The shield with a ward's mark across it.
		return { Shield(), { { 6, 7 }, { 8, 8.6 }, { 10, 7 } } };
	}
	if (Stat == TEXT("attack_speed"))
	{
		// Two chevrons.
		return { { { 3, 4 }, { 7, 8 }, { 3, 12 } }, { { 8, 4 }, { 12, 8 }, { 8, 12 } } };
	}
	if (Stat == TEXT("ability_haste"))
	{
		// An hourglass.
		return { { { 4, 2 }, { 12, 2 } }, { { 4, 14 }, { 12, 14 } }, { { 5, 2 }, { 11, 14 } }, { { 11, 2 }, { 5, 14 } } };
	}
	if (Stat == TEXT("crit_chance"))
	{
		// Crosshairs round a point.
		return { { { 8, 2 }, { 8, 5 } }, { { 8, 11 }, { 8, 14 } }, { { 2, 8 }, { 5, 8 } }, { { 11, 8 }, { 14, 8 } }, { { 7.4, 8 }, { 8.6, 8 } } };
	}
	if (Stat == TEXT("move_speed"))
	{
		// A boot.
		return { { { 5, 2 }, { 5, 10 }, { 12, 10 }, { 13, 13 }, { 3, 13 }, { 3, 2 }, { 5, 2 } } };
	}
	if (Stat == TEXT("health_regen"))
	{
		return { { { 8, 3 }, { 8, 13 } }, { { 3, 8 }, { 13, 8 } } };
	}
	if (Stat == TEXT("resource_regen"))
	{
		// A drop.
		return { { { 8, 2 }, { 11, 6 }, { 12, 9 }, { 11, 12 }, { 8, 13.5 }, { 5, 12 }, { 4, 9 }, { 5, 6 }, { 8, 2 } } };
	}
	if (Stat == TEXT("physical_penetration") || Stat == TEXT("magic_penetration"))
	{
		return Pierce();
	}
	if (Stat == TEXT("tenacity"))
	{
		return { { { 8, 2 }, { 8, 14 } }, { { 4, 5 }, { 12, 5 } }, { { 4, 11 }, { 12, 11 } } };
	}
	if (Stat == TEXT("attack_range"))
	{
		// A reach's arc, and its line out.
		FStroke Arc;
		constexpr int32 Steps = 8;
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const double Angle = UE_DOUBLE_PI + UE_DOUBLE_PI * Step / Steps;
			Arc.Add(FVector2D(8.0 + 6.0 * FMath::Cos(Angle), 9.0 + 6.0 * FMath::Sin(Angle)));
		}
		return { Arc, { { 8, 9 }, { 13, 4 } } };
	}
	return {};
}
}
