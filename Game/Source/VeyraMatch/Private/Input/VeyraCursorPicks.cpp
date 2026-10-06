// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Input/VeyraCursorPicks.h"

#include "Targeting/VeyraTargeting.h"

namespace VeyraCursorPicks
{
AActor* Enemy(TConstArrayView<FVeyraCursorUnit> Under, bool bVanguardsOnly)
{
	const FVeyraCursorUnit* Picked = Under.FindByPredicate([bVanguardsOnly](const FVeyraCursorUnit& Unit) {
		return Unit.bHostile && (!bVanguardsOnly || Unit.Kind == EVeyraUnitKind::Vanguard);
	});
	return Picked ? Picked->Actor : nullptr;
}

AActor* Ally(TConstArrayView<FVeyraCursorUnit> Under)
{
	const FVeyraCursorUnit* Picked = Under.FindByPredicate([](const FVeyraCursorUnit& Unit) { return !Unit.bHostile && Unit.Kind == EVeyraUnitKind::Vanguard; });
	return Picked ? Picked->Actor : nullptr;
}

AActor* ForCast(TConstArrayView<FVeyraCursorUnit> Under, bool bVanguardsOnly, bool bNamesAlly)
{
	// Target Vanguards Only narrows a cast at enemies to enemy Vanguards (Settings Bible §1.4); an ally's cast
	// names an allied Vanguard already.
	return bNamesAlly ? Ally(Under) : Enemy(Under, bVanguardsOnly);
}

AActor* ForSelect(TConstArrayView<FVeyraCursorUnit> Under, bool bVanguardsOnly)
{
	const FVeyraCursorUnit* Picked = Under.FindByPredicate([bVanguardsOnly](const FVeyraCursorUnit& Unit) { return !bVanguardsOnly || Unit.Kind == EVeyraUnitKind::Vanguard; });
	return Picked ? Picked->Actor : nullptr;
}

bool SmartSelfCasts(const AActor& Caster, TConstArrayView<FVeyraCursorUnit> Under, double CastRange)
{
	return VeyraTargeting::CheckAllyTarget(Caster, Ally(Under), CastRange) != EVeyraTargetValidity::Valid;
}
}
