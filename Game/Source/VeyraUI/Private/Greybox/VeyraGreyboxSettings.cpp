// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraGreyboxSettings.h"

TArray<FString> UVeyraGreyboxSettings::Validate() const
{
	TArray<FString> Problems;
	const auto Require = [&Problems](bool bValid, const TCHAR* Field, const TCHAR* Message) {
		if (!bValid)
		{
			Problems.Add(FString::Printf(TEXT("%s: %s"), Field, Message));
		}
	};
	Require(!BodyMesh.IsNull(), TEXT("BodyMesh"), TEXT("a static mesh is required."));
	Require(!ProjectileMesh.IsNull(), TEXT("ProjectileMesh"), TEXT("a static mesh is required."));
	Require(!ShapeMaterial.IsNull(), TEXT("ShapeMaterial"), TEXT("a material is required."));
	Require(!ColorParameter.IsNone(), TEXT("ColorParameter"), TEXT("the material's colour parameter is required."));
	// A colour of zero alpha draws nothing, so every colour must be visible.
	struct FNamedColor
	{
		const TCHAR* Field;
		const FLinearColor& Color;
	};
	const FNamedColor Colors[] = {
		{ TEXT("OwnColor"), OwnColor },
		{ TEXT("AllyColor"), AllyColor },
		{ TEXT("EnemyColor"), EnemyColor },
		{ TEXT("NeutralColor"), NeutralColor },
		{ TEXT("StunColor"), StunColor },
		{ TEXT("SlowColor"), SlowColor },
		{ TEXT("ShieldColor"), ShieldColor },
		{ TEXT("ResourceColor"), ResourceColor },
		{ TEXT("BarBackgroundColor"), BarBackgroundColor },
		{ TEXT("TextColor"), TextColor },
		{ TEXT("DescriptionColor"), DescriptionColor },
		{ TEXT("EmpoweredColor"), EmpoweredColor },
	};
	for (const FNamedColor& Named : Colors)
	{
		Require(Named.Color.A > 0.0f, Named.Field, TEXT("the colour must not be fully transparent."));
	}
	Require(StatusTintStrength > 0.0f && StatusTintStrength <= 1.0f, TEXT("StatusTintStrength"), TEXT("must be above 0 and at most 1."));
	Require(BarWidth >= 1.0f, TEXT("BarWidth"), TEXT("must be at least 1 pixel."));
	Require(BarHeight >= 1.0f, TEXT("BarHeight"), TEXT("must be at least 1 pixel."));
	Require(ResourceBarHeight >= 1.0f, TEXT("ResourceBarHeight"), TEXT("must be at least 1 pixel."));
	Require(BarLift >= 0.0f, TEXT("BarLift"), TEXT("must not be negative."));
	Require(HudMargin >= 0.0f, TEXT("HudMargin"), TEXT("must not be negative."));
	Require(TelegraphThickness > 0.0f, TEXT("TelegraphThickness"), TEXT("must be above 0."));
	Require(CircleSegments >= 3, TEXT("CircleSegments"), TEXT("must be at least 3."));
	Require(TelegraphLift >= 0.0f, TEXT("TelegraphLift"), TEXT("must not be negative."));
	Require(GroundProbeDistance >= 1.0f, TEXT("GroundProbeDistance"), TEXT("must be at least 1 unit."));
	return Problems;
}
