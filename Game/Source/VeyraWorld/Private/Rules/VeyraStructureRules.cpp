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
}
