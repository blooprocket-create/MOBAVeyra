// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraGreyboxSettings.h"

const FVeyraStructureArt& UVeyraGreyboxSettings::ArtOf(EVeyraStructureKind Kind) const
{
	switch (Kind)
	{
	case EVeyraStructureKind::BaseTower:
		return BaseTowerArt;
	case EVeyraStructureKind::Inhibitor:
		return InhibitorArt;
	case EVeyraStructureKind::PrimeWell:
		return PrimeWellArt;
	case EVeyraStructureKind::LaneSpire:
		break;
	}
	return LaneSpireArt;
}

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
	Require(!GroundMesh.IsNull(), TEXT("GroundMesh"), TEXT("a static mesh is required."));
	Require(!PadMesh.IsNull(), TEXT("PadMesh"), TEXT("a static mesh is required."));
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
		{ TEXT("FocusColor"), FocusColor },
		{ TEXT("ChargeColor"), ChargeColor },
		{ TEXT("BarBackgroundColor"), BarBackgroundColor },
		{ TEXT("TextColor"), TextColor },
		{ TEXT("DescriptionColor"), DescriptionColor },
		{ TEXT("EmpoweredColor"), EmpoweredColor },
		{ TEXT("ChannelColor"), ChannelColor },
		{ TEXT("LaneColor"), LaneColor },
		{ TEXT("HudSurfaceColor"), HudSurfaceColor },
		{ TEXT("HudHairlineColor"), HudHairlineColor },
		{ TEXT("HudAccentColor"), HudAccentColor },
		{ TEXT("HealthColor"), HealthColor },
		{ TEXT("GoldColor"), GoldColor },
		{ TEXT("ShadeColor"), ShadeColor },
		{ TEXT("RiverColor"), RiverColor },
		{ TEXT("MinimapWallColor"), MinimapWallColor },
		{ TEXT("AllyBaseColor"), AllyBaseColor },
		{ TEXT("EnemyBaseColor"), EnemyBaseColor },
		{ TEXT("DenseFogColor"), DenseFogColor },
		{ TEXT("PresencePingColor"), PresencePingColor },
		{ TEXT("OutlineColor"), OutlineColor },
		{ TEXT("EndingColor"), EndingColor },
		{ TEXT("ChatBackdropColor"), ChatBackdropColor },
		{ TEXT("ChatHighContrastBackdropColor"), ChatHighContrastBackdropColor },
	};
	for (const FNamedColor& Named : Colors)
	{
		Require(Named.Color.A > 0.0f, Named.Field, TEXT("the colour must not be fully transparent."));
	}
	for (const EVeyraStructureKind Kind : { EVeyraStructureKind::LaneSpire, EVeyraStructureKind::BaseTower, EVeyraStructureKind::Inhibitor, EVeyraStructureKind::PrimeWell })
	{
		const FVeyraStructureArt& Art = ArtOf(Kind);
		Require(!Art.Standing.IsNull() && !Art.Destroyed.IsNull(), *(UEnum::GetValueAsName(Kind).ToString() + TEXT("Art")), TEXT("needs a standing and a destroyed mesh."));
	}
	Require(!StructureFluxSlot.IsNone() && !StructureFluxParameter.IsNone(), TEXT("StructureFluxSlot"), TEXT("the Flux slot and its parameter must be named."));
	Require(StatusTintStrength > 0.0f && StatusTintStrength <= 1.0f, TEXT("StatusTintStrength"), TEXT("must be above 0 and at most 1."));
	Require(BarWidth >= 1.0f, TEXT("BarWidth"), TEXT("must be at least 1 pixel."));
	Require(BarHeight >= 1.0f, TEXT("BarHeight"), TEXT("must be at least 1 pixel."));
	Require(ResourceBarHeight >= 1.0f, TEXT("ResourceBarHeight"), TEXT("must be at least 1 pixel."));
	Require(BarLift >= 0.0f, TEXT("BarLift"), TEXT("must not be negative."));
	Require(ChannelBarWidth >= 1.0f, TEXT("ChannelBarWidth"), TEXT("must be at least 1 pixel."));
	Require(ChannelBarHeight >= 1.0f, TEXT("ChannelBarHeight"), TEXT("must be at least 1 pixel."));
	Require(ChannelBarLift >= 0.0f, TEXT("ChannelBarLift"), TEXT("must not be negative."));
	Require(HudMargin >= 0.0f, TEXT("HudMargin"), TEXT("must not be negative."));
	Require(ChatWidth >= 1.0f, TEXT("ChatWidth"), TEXT("must be at least 1 pixel."));
	Require(ChatInputHeight >= 1.0f, TEXT("ChatInputHeight"), TEXT("must be at least 1 pixel."));
	Require(ChatBottomOffset >= 0.0f, TEXT("ChatBottomOffset"), TEXT("must not be negative."));
	Require(ChatLines >= 1, TEXT("ChatLines"), TEXT("must be at least 1."));
	Require(ChatFontSize >= 1 && ChatLargeFontSize > ChatFontSize && ChatExtraLargeFontSize > ChatLargeFontSize, TEXT("ChatFontSize"),
		TEXT("the three chat sizes must be at least 1 and grow from Standard to Extra Large."));
	Require(ChatFadeSeconds > 0.0f, TEXT("ChatFadeSeconds"), TEXT("must be above 0."));
	Require(ChatFadeOutSeconds >= 0.0f, TEXT("ChatFadeOutSeconds"), TEXT("must not be negative."));
	Require(TelegraphThickness > 0.0f, TEXT("TelegraphThickness"), TEXT("must be above 0."));
	Require(CircleSegments >= 3, TEXT("CircleSegments"), TEXT("must be at least 3."));
	Require(OutlineMarkerRadius >= 1.0f, TEXT("OutlineMarkerRadius"), TEXT("must be at least 1 unit."));
	Require(TelegraphLift >= 0.0f, TEXT("TelegraphLift"), TEXT("must not be negative."));
	Require(GroundProbeDistance >= 1.0f, TEXT("GroundProbeDistance"), TEXT("must be at least 1 unit."));
	Require(GroundMarkingThickness > 0.0f, TEXT("GroundMarkingThickness"), TEXT("must be above 0."));
	Require(GroundMarkingLift > 0.0f, TEXT("GroundMarkingLift"), TEXT("must be above 0."));
	// The river, the lanes, the pads and the fog, each a lift above the last, all under the telegraphs.
	constexpr int32 GroundMarkingLayers = 4;
	Require(GroundMarkingThickness + GroundMarkingLift * GroundMarkingLayers < TelegraphLift, TEXT("GroundMarkingLift"),
		TEXT("the thickness and four lifts must stay under TelegraphLift, or the ground hides telegraphs."));
	return Problems;
}
