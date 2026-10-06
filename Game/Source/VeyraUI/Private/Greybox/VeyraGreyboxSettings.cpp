// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraGreyboxSettings.h"

#include "Algo/AnyOf.h"
#include "GameFramework/Actor.h"

#include "Greybox/VeyraUnitArtSet.h"
#include "Units/VeyraUnit.h"

FName UVeyraGreyboxSettings::StructureArtId(EVeyraStructureKind Kind)
{
	switch (Kind)
	{
	case EVeyraStructureKind::BaseTower:
		return TEXT("baseTower");
	case EVeyraStructureKind::Inhibitor:
		return TEXT("inhibitor");
	case EVeyraStructureKind::PrimeWell:
		return TEXT("primeWell");
	case EVeyraStructureKind::LaneSpire:
		break;
	}
	return TEXT("laneSpire");
}

float UVeyraGreyboxSettings::VisualScaleOf(const AActor& Unit) const
{
	switch (VeyraUnits::KindOf(&Unit).Get(EVeyraUnitKind::Marker))
	{
	case EVeyraUnitKind::Vanguard:
	case EVeyraUnitKind::Companion:
	case EVeyraUnitKind::Echo:
		return VanguardBodyScale;
	case EVeyraUnitKind::Structure:
		return StructureArtScale;
	default:
		return 1.0f;
	}
}

float UVeyraGreyboxSettings::VisualTopOf(const AActor& Unit) const
{
	// A body drawn from the capsule's foot reaches the capsule's height times its scale.
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
	return HalfHeight * (2.0f * VisualScaleOf(Unit) - 1.0f);
}

TOptional<FVector2f> UVeyraGreyboxSettings::EaseOf(const AActor& Unit) const
{
	switch (VeyraUnits::KindOf(&Unit).Get(EVeyraUnitKind::Marker))
	{
	case EVeyraUnitKind::Vanguard:
	case EVeyraUnitKind::Companion:
	case EVeyraUnitKind::Echo:
		return FVector2f(VanguardEaseLocationSeconds, VanguardEaseRotationSeconds);
	case EVeyraUnitKind::Fluxborn:
	case EVeyraUnitKind::Wildlife:
		return FVector2f(CreatureEaseLocationSeconds, CreatureEaseRotationSeconds);
	default:
		return {};
	}
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
		{ TEXT("PartyChatColor"), PartyChatColor },
		{ TEXT("DirectChatColor"), DirectChatColor },
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
		{ TEXT("MinimapFogColor"), MinimapFogColor },
		{ TEXT("AllyBaseColor"), AllyBaseColor },
		{ TEXT("EnemyBaseColor"), EnemyBaseColor },
		{ TEXT("DenseFogColor"), DenseFogColor },
		{ TEXT("FogOfWarColor"), FogOfWarColor },
		{ TEXT("WarningColor"), WarningColor },
		{ TEXT("PresencePingColor"), PresencePingColor },
		{ TEXT("OutlineColor"), OutlineColor },
		{ TEXT("EchoStrainColor"), EchoStrainColor },
		{ TEXT("EndingColor"), EndingColor },
		{ TEXT("OrderMoveColor"), OrderMoveColor },
		{ TEXT("HitFlashColor"), HitFlashColor },
		{ TEXT("ShopColor"), ShopColor },
		{ TEXT("OrderAttackColor"), OrderAttackColor },
		{ TEXT("ChatBackdropColor"), ChatBackdropColor },
		{ TEXT("ChatHighContrastBackdropColor"), ChatHighContrastBackdropColor },
		{ TEXT("CombatTextPhysicalColor"), CombatTextPhysicalColor },
		{ TEXT("CombatTextMagicColor"), CombatTextMagicColor },
		{ TEXT("CombatTextTrueColor"), CombatTextTrueColor },
		{ TEXT("CombatTextUniformColor"), CombatTextUniformColor },
		{ TEXT("CombatTextReceivedColor"), CombatTextReceivedColor },
		{ TEXT("CombatTextHealingColor"), CombatTextHealingColor },
		{ TEXT("CombatTextShieldingColor"), CombatTextShieldingColor },
	};
	for (const FNamedColor& Named : Colors)
	{
		Require(Named.Color.A > 0.0f, Named.Field, TEXT("the colour must not be fully transparent."));
	}
	// Colour vision: each preset's four colours and each named colour drawn (ADR-055 §1).
	for (const TCHAR* Preset : { TEXT("Protanopia"), TEXT("Deuteranopia"), TEXT("Tritanopia") })
	{
		const FVeyraSideColorSet* Set = ColorVisionPresets.Find(Preset);
		Require(Set && Set->Own.A > 0.0f && Set->Ally.A > 0.0f && Set->Enemy.A > 0.0f && Set->Neutral.A > 0.0f, TEXT("ColorVisionPresets"),
			*FString::Printf(TEXT("%s needs all four side colours, none fully transparent."), Preset));
	}
	Require(!SideColorPalette.IsEmpty() && !Algo::AnyOf(SideColorPalette, [](const TPair<FString, FLinearColor>& Named) { return !(Named.Value.A > 0.0f); }),
		TEXT("SideColorPalette"), TEXT("at least one named colour is required, none fully transparent."));
	Require(CustomOwnLightening >= 0.0f && CustomOwnLightening <= 1.0f, TEXT("CustomOwnLightening"), TEXT("must be from 0 to 1."));
	// The warnings (ADR-055 §5).
	Require(ConnectionWarningLossFraction > 0.0f && ConnectionWarningLossFraction < 1.0f, TEXT("ConnectionWarningLossFraction"), TEXT("must be above 0 and below 1."));
	Require(ConnectionWarningRoundTripMs > 0.0f, TEXT("ConnectionWarningRoundTripMs"), TEXT("must be above 0."));
	Require(PerformanceWarningFraction > 0.0f && PerformanceWarningFraction < 1.0f, TEXT("PerformanceWarningFraction"), TEXT("must be above 0 and below 1."));
	Require(UncappedReferenceFps >= 1.0f, TEXT("UncappedReferenceFps"), TEXT("must be at least 1."));
	Require(WarningStartSeconds > 0.0f && WarningClearSeconds > 0.0f, TEXT("WarningStartSeconds"), TEXT("both a warning's start and its clearing take some time."));
	Require(!MasteryEmoteTierColors.IsEmpty() && !MasteryEmoteTierColors.ContainsByPredicate([](const FLinearColor& Color) { return !(Color.A > 0.0f); }),
		TEXT("MasteryEmoteTierColors"), TEXT("at least one colour is required, and none may be fully transparent."));
	Require(!StructureArt.IsNull(), TEXT("StructureArt"), TEXT("the structure kit's art set is required."));
	Require(!FluxbornArt.IsNull(), TEXT("FluxbornArt"), TEXT("the Fluxborn kit's art set is required."));
	Require(!VanguardArt.IsNull(), TEXT("VanguardArt"), TEXT("the Vanguard body kit's art set is required."));
	Require(VanguardFootDiscHeight >= 0.1f, TEXT("VanguardFootDiscHeight"), TEXT("must be at least 0.1 units."));
	Require(VanguardBlendSeconds > 0.0f && VanguardRunBlendSpeed >= 1.0f, TEXT("VanguardBlendSeconds"),
		TEXT("a body's animations need some time to blend, and Run a speed of at least 1 at which it takes over."));
	Require(VanguardMinPlayRate > 0.0f && VanguardMinPlayRate <= 1.0f && VanguardMaxPlayRate >= 1.0f, TEXT("VanguardMinPlayRate"),
		TEXT("an animation must be able to play at its own speed: the slowest rate above 0 and at most 1, the fastest at least 1."));
	Require(StatusTintStrength > 0.0f && StatusTintStrength <= 1.0f, TEXT("StatusTintStrength"), TEXT("must be above 0 and at most 1."));
	Require(BarWidth >= 1.0f, TEXT("BarWidth"), TEXT("must be at least 1 pixel."));
	Require(BarHeight >= 1.0f, TEXT("BarHeight"), TEXT("must be at least 1 pixel."));
	Require(ResourceBarHeight >= 1.0f, TEXT("ResourceBarHeight"), TEXT("must be at least 1 pixel."));
	Require(BarLift >= 0.0f, TEXT("BarLift"), TEXT("must not be negative."));
	// Over the ground's markings and under the telegraphs.
	Require(FogOfWarSurfaceStep > 0.0f, TEXT("FogOfWarSurfaceStep"), TEXT("must be above zero."));
	Require(FogOfWarLift >= 0.0f && FogOfWarLift <= TelegraphLift, TEXT("FogOfWarLift"), TEXT("must not be negative, nor above TelegraphLift."));
	Require(FogOfWarColor.A < 1.0f && MinimapFogColor.A < 1.0f, TEXT("FogOfWarColor"), TEXT("the fog of war is translucent: the ground shows through it."));
	Require(ChannelBarWidth >= 1.0f, TEXT("ChannelBarWidth"), TEXT("must be at least 1 pixel."));
	Require(ChannelBarHeight >= 1.0f, TEXT("ChannelBarHeight"), TEXT("must be at least 1 pixel."));
	Require(ChannelBarLift >= 0.0f, TEXT("ChannelBarLift"), TEXT("must not be negative."));
	Require(HudMargin >= 0.0f, TEXT("HudMargin"), TEXT("must not be negative."));
	Require(DeckPipHeight >= 1.0f, TEXT("DeckPipHeight"), TEXT("must be at least 1 unit."));
	Require(DeckBarSpacing >= 0.0f, TEXT("DeckBarSpacing"), TEXT("must not be negative."));
	Require(DeckMinimumFit > 0.0f && DeckMinimumFit <= 1.0f, TEXT("DeckMinimumFit"), TEXT("must be above 0 and at most 1."));
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
	Require(OrderMarkSeconds > 0.0f, TEXT("OrderMarkSeconds"), TEXT("must be above 0."));
	Require(!HitFlashMaterial.IsNull(), TEXT("HitFlashMaterial"), TEXT("the generated hit flash material is required."));
	Require(!HoverOutlineMaterial.IsNull(), TEXT("HoverOutlineMaterial"), TEXT("the generated hover outline material is required."));
	Require(!ImpactEffect.IsNull() && !CastEffect.IsNull() && !DeathEffect.IsNull() && !LevelUpEffect.IsNull() && !TrailEffect.IsNull(), TEXT("ImpactEffect"),
		TEXT("the generated impact, cast, death, level-up and trail effects are required."));
	Require(SwingArcReach > 0.0f && SwingArcDegrees > 0.0f && SwingArcDegrees <= 360.0f && SwingArcSeconds > 0.0f, TEXT("SwingArcReach"),
		TEXT("a swing needs a reach, an arc of up to 360 degrees and some time to fade."));
	Require(!EffectColorParameter.IsNone(), TEXT("EffectColorParameter"), TEXT("the effects' colour parameter is required."));
	Require(!EffectScaleParameter.IsNone(), TEXT("EffectScaleParameter"), TEXT("the effects' scale parameter is required."));
	Require(!ImpactSound.IsNull() && !SwingSound.IsNull() && !CastSound.IsNull() && !DeathSound.IsNull() && !ClickSound.IsNull() && !LevelUpSound.IsNull(),
		TEXT("ImpactSound"), TEXT("the generated impact, swing, cast, death, click and level-up sounds are required."));
	Require(ShopOffset >= 0.0f && ShopRadius >= 1.0f && ShopHeight > ShopRadius * 2.0f, TEXT("ShopHeight"),
		TEXT("the shop needs a radius of at least 1 and must stand taller than it is wide."));
	Require(SoundAudibleRadius >= 1.0f && SoundFalloffDistance >= 1.0f && MaxCueSounds >= 1, TEXT("SoundAudibleRadius"),
		TEXT("the sounds' audible radius, falloff and the most at once must each be at least 1."));
	Require(!HoverEnemyColorParameter.IsNone() && !HoverAllyColorParameter.IsNone() && !HoverNeutralColorParameter.IsNone()
			&& !HoverEnemyStencilParameter.IsNone() && !HoverAllyStencilParameter.IsNone() && !HoverNeutralStencilParameter.IsNone(),
		TEXT("HoverEnemyColorParameter"), TEXT("the hover outline's three colour and three stencil parameters are required."));
	Require(!HitFlashColorParameter.IsNone() && !HitFlashStrengthParameter.IsNone(), TEXT("HitFlashColorParameter"),
		TEXT("the hit flash material's colour and strength parameters are required."));
	Require(HitFlashSeconds > 0.0f && RecoilSeconds > 0.0f && SnapSeconds > 0.0f && CollapseSeconds > 0.0f, TEXT("HitFlashSeconds"),
		TEXT("the flash, recoil, snap and collapse each take some time."));
	Require(ReducedFlashStrength > 0.0f && ReducedFlashStrength < 1.0f, TEXT("ReducedFlashStrength"), TEXT("must be above 0 and below 1: a weaker flash, not none."));
	Require(RecoilSquash >= 0.0f && RecoilSquash < 1.0f, TEXT("RecoilSquash"), TEXT("must be from 0 to below 1."));
	Require(CollapsedHeightShare > 0.0f && CollapsedHeightShare < 1.0f, TEXT("CollapsedHeightShare"), TEXT("must be above 0 and below 1."));
	Require(LeanDistance >= 0.0f && SnapDistance >= 0.0f, TEXT("LeanDistance"), TEXT("must not be negative."));
	Require(OrderMarkEndRadius >= 1.0f && OrderMarkStartRadius > OrderMarkEndRadius, TEXT("OrderMarkStartRadius"),
		TEXT("an order's mark closes: its end radius must be at least 1 unit and its start radius above that."));
	Require(OutlineMarkerRadius >= 1.0f, TEXT("OutlineMarkerRadius"), TEXT("must be at least 1 unit."));
	Require(EchoStrainShare > 0.0f && EchoStrainShare < 1.0f, TEXT("EchoStrainShare"), TEXT("must be above 0 and below 1."));
	Require(CombatTextShowSeconds > 0.0f, TEXT("CombatTextShowSeconds"), TEXT("must be above 0."));
	Require(CombatTextMergeSeconds >= 0.0f && CombatTextMergeSeconds < CombatTextShowSeconds, TEXT("CombatTextMergeSeconds"),
		TEXT("must not be negative, and must be shorter than CombatTextShowSeconds, or a total would vanish while it grows."));
	Require(CombatTextGoldMergeSeconds >= 0.0f && CombatTextGoldMergeSeconds < CombatTextShowSeconds, TEXT("CombatTextGoldMergeSeconds"),
		TEXT("must not be negative, and must be shorter than CombatTextShowSeconds."));
	Require(LevelUpFontSize >= 1 && LevelUpHeightShare > 0.0f && LevelUpHeightShare < 1.0f && LevelUpBannerSeconds > 0.0f && LevelUpFadeShare > 0.0f
		&& LevelUpFadeShare <= 1.0f, TEXT("LevelUpBannerSeconds"), TEXT("the level-up banner needs a size, a place on screen, some time to show and a share of it to fade."));
	Require(VanguardBodyScale > 0.0f && StructureArtScale > 0.0f, TEXT("VanguardBodyScale"), TEXT("bodies need a scale above 0."));
	Require(VanguardEaseLocationSeconds > 0.0f && VanguardEaseRotationSeconds > 0.0f && CreatureEaseLocationSeconds > 0.0f && CreatureEaseRotationSeconds > 0.0f,
		TEXT("VanguardEaseLocationSeconds"), TEXT("a drawn body needs some time to ease to each update, or it steps with its capsule."));
	Require(KillFeedRows >= 1 && KillFeedSeconds > 0.0f && KillFeedFaceSize >= 1.0f && AnnouncementSeconds > 0.0f && AnnouncementFontSize >= 1, TEXT("KillFeedSeconds"),
		TEXT("the kill feed needs rows, a time to show and a face size, and the announcement a time and a size."));
	Require(RankUpPulseSeconds > 0.0f && RankUpPulseFloor >= 0.0f && RankUpPulseFloor < 1.0f, TEXT("RankUpPulseSeconds"),
		TEXT("a pulse needs a period above 0 and a floor below whole."));
	Require(CombatTextScale > 0.0f, TEXT("CombatTextScale"), TEXT("must be above 0."));
	Require(CombatTextCritScale >= 1.0f, TEXT("CombatTextCritScale"), TEXT("must be at least 1."));
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
