// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraStructureRules.h"

namespace VeyraStructureRules
{
namespace
{
	bool AnyStanding(TConstArrayView<FVeyraStructureStatus> All, TFunctionRef<bool(const FVeyraStructureStatus&)> Matches)
	{
		for (const FVeyraStructureStatus& Other : All)
		{
			if (!Other.bDestroyed && Matches(Other))
			{
				return true;
			}
		}
		return false;
	}

	bool AnyDestroyed(TConstArrayView<FVeyraStructureStatus> All, TFunctionRef<bool(const FVeyraStructureStatus&)> Matches)
	{
		for (const FVeyraStructureStatus& Other : All)
		{
			if (Other.bDestroyed && Matches(Other))
			{
				return true;
			}
		}
		return false;
	}
}

bool IsInvulnerable(const FVeyraStructureStatus& Structure, TConstArrayView<FVeyraStructureStatus> All)
{
	const EVeyraTeam Team = Structure.Team;
	const auto IsInhibitorOfTeam = [Team](const FVeyraStructureStatus& Other) { return Other.Team == Team && Other.Kind == EVeyraStructureKind::Inhibitor; };
	switch (Structure.Kind)
	{
	case EVeyraStructureKind::LaneSpire:
	case EVeyraStructureKind::Inhibitor:
		// Its lane's structures fall from the outer Spire inward.
		return AnyStanding(All, [&Structure](const FVeyraStructureStatus& Other) {
			return Other.Team == Structure.Team && Other.Lane == Structure.Lane && Other.Order < Structure.Order;
		});
	case EVeyraStructureKind::BaseTower:
		// Taking down any inhibitor opens the path to the base towers.
		return !AnyDestroyed(All, IsInhibitorOfTeam);
	case EVeyraStructureKind::PrimeWell:
	{
		const bool bTowersStand = AnyStanding(All, [Team](const FVeyraStructureStatus& Other) { return Other.Team == Team && Other.Kind == EVeyraStructureKind::BaseTower; });
		return bTowersStand || !AnyDestroyed(All, IsInhibitorOfTeam);
	}
	}
	return false;
}

bool PrimeWellRegenerates(EVeyraTeam Team, TConstArrayView<FVeyraStructureStatus> All)
{
	return !AnyDestroyed(All, [Team](const FVeyraStructureStatus& Other) { return Other.Team == Team && Other.Kind == EVeyraStructureKind::Inhibitor; });
}

bool Attacks(EVeyraStructureKind Kind)
{
	return Kind == EVeyraStructureKind::LaneSpire || Kind == EVeyraStructureKind::BaseTower;
}

bool HasBackdoorProtection(EVeyraStructureKind Kind)
{
	return Kind != EVeyraStructureKind::Inhibitor;
}

double NextBackdoorProtection(double Current, bool bAttackingFluxbornNear, double Max, double RampSeconds, double Seconds)
{
	// An attacking Fluxborn arriving removes it at once, with no ramp down.
	if (bAttackingFluxbornNear)
	{
		return 0.0;
	}
	const double Step = RampSeconds > 0.0 ? Max * FMath::Max(0.0, Seconds) / RampSeconds : Max;
	return FMath::Min(Max, FMath::Max(0.0, Current) + Step);
}

TOptional<int32> NextToSiege(EVeyraTeam Defenders, TConstArrayView<FVeyraStructureStatus> All)
{
	// Siege order, lowest first: the mid lane, the base towers, the Prime Well, then the other lanes.
	const auto Stage = [](const FVeyraStructureStatus& Structure) {
		if (Structure.Kind == EVeyraStructureKind::BaseTower)
		{
			return 1;
		}
		if (Structure.Kind == EVeyraStructureKind::PrimeWell)
		{
			return 2;
		}
		const EVeyraLane Lane = Structure.Lane.Get(EVeyraLane::Mid);
		return Lane == EVeyraLane::Mid ? 0 : Lane == EVeyraLane::Top ? 3 : 4;
	};
	TOptional<int32> Best;
	for (int32 Index = 0; Index < All.Num(); ++Index)
	{
		const FVeyraStructureStatus& Structure = All[Index];
		if (Structure.Team != Defenders || Structure.bDestroyed || IsInvulnerable(Structure, All))
		{
			continue;
		}
		const FVeyraStructureStatus* Current = Best.IsSet() ? &All[Best.GetValue()] : nullptr;
		if (!Current || Stage(Structure) < Stage(*Current) || (Stage(Structure) == Stage(*Current) && Structure.Order < Current->Order))
		{
			Best = Index;
		}
	}
	return Best;
}
}
