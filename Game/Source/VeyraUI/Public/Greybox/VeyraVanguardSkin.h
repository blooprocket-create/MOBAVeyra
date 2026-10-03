// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Greybox/VeyraVanguardAnimation.h"
#include "Teams/VeyraTeam.h"

class APawn;
class USkeletalMeshComponent;
class UVeyraGreyboxSettings;
struct FVeyraVanguardArt;

/** A Vanguard's generated body in the grey-box presentation (ADR-064 §3): visual only, as every drawn body is. */
namespace VeyraVanguardSkin
{
	/** A skeletal mesh component on Unit for its body, standing at the capsule's foot; it blocks nothing and shapes no navigation. */
	VEYRAUI_API USkeletalMeshComponent* Attach(APawn& Unit);

	/** Dresses Skin in Art and fits its animation to it and Shape; does nothing once it wears Art's mesh. */
	VEYRAUI_API void Dress(USkeletalMeshComponent& Skin, const FVeyraVanguardArt& Art, const FVeyraVanguardAnimShape& Shape);

	/** The animation's blend times and play rates from the grey-box settings; the art fills in the rest. */
	VEYRAUI_API FVeyraVanguardAnimShape ShapeOf(const UVeyraGreyboxSettings& Settings);

	/** What Unit's body is doing as Viewer's side sees it: its ground speed, life, recall and windups. */
	VEYRAUI_API FVeyraVanguardAnimInputs InputsOf(const APawn& Unit, EVeyraTeam Viewer);
}
