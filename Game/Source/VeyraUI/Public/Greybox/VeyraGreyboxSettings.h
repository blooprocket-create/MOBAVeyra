// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "VeyraGreyboxSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * How the grey-box presentation looks (ADR-008 §1): engine shapes, colours and sizes. Presentation,
 * not tuning, stored in Config/DefaultGame.ini. Every value is required: the grey-box draws nothing
 * and logs the problems when Validate finds any.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Veyra Grey-box Presentation"))
class VEYRAUI_API UVeyraGreyboxSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Every problem with these settings, as "Field: message"; empty when the grey-box can use them. */
	TArray<FString> Validate() const;

	/** A unit's body: stretched to its collision capsule. */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UStaticMesh> BodyMesh;

	/** A projectile: scaled to its radius. */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UStaticMesh> ProjectileMesh;

	/** The material bodies and projectiles wear; ColorParameter sets its colour. */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TSoftObjectPtr<UMaterialInterface> ShapeMaterial;

	/** The vector parameter of ShapeMaterial that holds the colour. */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	FName ColorParameter;

	/** The viewer's own Vanguard. */
	UPROPERTY(Config, EditAnywhere, Category = "Sides")
	FLinearColor OwnColor = FLinearColor::Transparent;

	/** The viewer's side. A viewer on no side sees side A as allies. */
	UPROPERTY(Config, EditAnywhere, Category = "Sides")
	FLinearColor AllyColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Sides")
	FLinearColor EnemyColor = FLinearColor::Transparent;

	/** Units on no side. */
	UPROPERTY(Config, EditAnywhere, Category = "Sides")
	FLinearColor NeutralColor = FLinearColor::Transparent;

	/** The tint of a stunned unit. */
	UPROPERTY(Config, EditAnywhere, Category = "Statuses")
	FLinearColor StunColor = FLinearColor::Transparent;

	/** The tint of a slowed unit. */
	UPROPERTY(Config, EditAnywhere, Category = "Statuses")
	FLinearColor SlowColor = FLinearColor::Transparent;

	/** The tint of a Camouflaged unit, which only its own side and those who detect it see. */
	UPROPERTY(Config, EditAnywhere, Category = "Statuses")
	FLinearColor CamouflageColor = FLinearColor::Transparent;

	/** How far a status tint moves a body from its side's colour: above 0, at most 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Statuses", meta = (ClampMin = "0", ClampMax = "1"))
	float StatusTintStrength = 0.0f;

	/** Shields on the Health bar. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor ShieldColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor ResourceColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor BarBackgroundColor = FLinearColor::Transparent;

	/** HUD text. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor TextColor = FLinearColor::Transparent;

	/** The line under each ability on the player's panel that says what it does. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor DescriptionColor = FLinearColor::Transparent;

	/** An ability whose empowerment waits for the next basic attack. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor EmpoweredColor = FLinearColor::Transparent;

	/** A warning to the player, such as that the server counts it AFK. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor WarningColor = FLinearColor::Transparent;

	/** The overhead Health bar's size, in pixels. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars", meta = (ClampMin = "1"))
	float BarWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Bars", meta = (ClampMin = "1"))
	float BarHeight = 0.0f;

	/** The resource bar under it, in pixels. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars", meta = (ClampMin = "1"))
	float ResourceBarHeight = 0.0f;

	/** How far above a unit's head its bars sit, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars", meta = (ClampMin = "0"))
	float BarLift = 0.0f;

	/** A channel's bar, such as Recall's, centred near the bottom of the screen: its colour, size and height above the bottom edge, in pixels. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor ChannelColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Bars", meta = (ClampMin = "1"))
	float ChannelBarWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Bars", meta = (ClampMin = "1"))
	float ChannelBarHeight = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Bars", meta = (ClampMin = "0"))
	float ChannelBarLift = 0.0f;

	/** The HUD panel's distance from the screen's edge, in pixels. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars", meta = (ClampMin = "0"))
	float HudMargin = 0.0f;

	/** Telegraph outlines, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "Telegraphs", meta = (ClampMin = "0"))
	float TelegraphThickness = 0.0f;

	/** Straight segments in a whole circle's outline; arcs use their share. At least 3. */
	UPROPERTY(Config, EditAnywhere, Category = "Telegraphs", meta = (ClampMin = "3"))
	int32 CircleSegments = 0;

	/** How far above the ground telegraphs are drawn, in units, so the floor does not hide them. */
	UPROPERTY(Config, EditAnywhere, Category = "Telegraphs", meta = (ClampMin = "0"))
	float TelegraphLift = 0.0f;

	/** How far above and below a telegraph's origin the ground is looked for, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "Telegraphs", meta = (ClampMin = "1"))
	float GroundProbeDistance = 0.0f;

	/** The battleground's lanes and river, flat on its floor: stretched to each stretch of road or water. */
	UPROPERTY(Config, EditAnywhere, Category = "Battleground")
	TSoftObjectPtr<UStaticMesh> GroundMesh;

	/** Each base's pad around its Prime Well: a disc. */
	UPROPERTY(Config, EditAnywhere, Category = "Battleground")
	TSoftObjectPtr<UStaticMesh> PadMesh;

	/** The lanes' road, the river, and each side's base as the viewer sees it. */
	UPROPERTY(Config, EditAnywhere, Category = "Battleground")
	FLinearColor LaneColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Battleground")
	FLinearColor RiverColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Battleground")
	FLinearColor AllyBaseColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Battleground")
	FLinearColor EnemyBaseColor = FLinearColor::Transparent;

	/** The battleground's Dense Fog, its bush (Battleground Bible §11): a dark patch on the floor. */
	UPROPERTY(Config, EditAnywhere, Category = "Battleground")
	FLinearColor DenseFogColor = FLinearColor::Transparent;

	/** A presence ping: a ring over the fog an enemy is present in, fading until the next (ADR-016 §8). */
	UPROPERTY(Config, EditAnywhere, Category = "Vision")
	FLinearColor PresencePingColor = FLinearColor::Transparent;

	/** Sweeper's outline of an enemy in fog: a small ring where it stands. */
	UPROPERTY(Config, EditAnywhere, Category = "Vision")
	FLinearColor OutlineColor = FLinearColor::Transparent;

	/** The outline ring's radius, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "Vision", meta = (ClampMin = "1"))
	float OutlineMarkerRadius = 0.0f;

	/** How thick each marking is, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "Battleground", meta = (ClampMin = "0"))
	float GroundMarkingThickness = 0.0f;

	/** How far each layer of markings sits above the one below (river, lanes, pads, then fog), so none flickers through another. */
	UPROPERTY(Config, EditAnywhere, Category = "Battleground", meta = (ClampMin = "0"))
	float GroundMarkingLift = 0.0f;
};
