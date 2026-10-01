// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Input/VeyraCursorPicks.h"

namespace VeyraCursorPicks
{
AActor* Enemy(TConstArrayView<FVeyraCursorUnit> Under, bool bVanguardsOnly)
{
	const FVeyraCursorUnit* Picked = Under.FindByPredicate([bVanguardsOnly](const FVeyraCursorUnit& Unit) {
		return Unit.bHostile && (!bVanguardsOnly || Unit.Kind == EVeyraUnitKind::Vanguard);
	});
	return Picked ? Picked->Actor : nullptr;
}

AActor* ForCast(TConstArrayView<FVeyraCursorUnit> Under, bool bVanguardsOnly)
{
	const FVeyraCursorUnit* Picked = Under.FindByPredicate([bVanguardsOnly](const FVeyraCursorUnit& Unit) {
		return !bVanguardsOnly || Unit.Kind == EVeyraUnitKind::Vanguard;
	});
	return Picked ? Picked->Actor : nullptr;
}

bool HasAlliedVanguard(TConstArrayView<FVeyraCursorUnit> Under)
{
	return Under.ContainsByPredicate([](const FVeyraCursorUnit& Unit) { return !Unit.bHostile && Unit.Kind == EVeyraUnitKind::Vanguard; });
}
}
