// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Engine/DeveloperSettings.h"
#include "Tuning/VeyraVanguardsTuning.h"

#include "VeyraGreyboxSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UVeyraUnitArtSet;

/** One colour-vision palette's side colours (Settings Bible §4.1; ADR-055 §1). */
USTRUCT()
struct FVeyraSideColorSet
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Sides")
	FLinearColor Own = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Sides")
	FLinearColor Ally = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Sides")
	FLinearColor Enemy = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Sides")
	FLinearColor Neutral = FLinearColor::Transparent;
};

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

	/** The colour-vision presets' side colours, by the Color Vision option that names them (SET-8; ADR-055 §1). */
	UPROPERTY(Config, EditAnywhere, Category = "Sides")
	TMap<FString, FVeyraSideColorSet> ColorVisionPresets;

	/** The named colours Custom colour vision offers each side, by option. */
	UPROPERTY(Config, EditAnywhere, Category = "Sides")
	TMap<FString, FLinearColor> SideColorPalette;

	/** Under Custom, how far the player's own colour lightens their ally colour toward white, from 0 to 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Sides", meta = (ClampMin = "0", ClampMax = "1"))
	float CustomOwnLightening = 0.0f;

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

	/** The resource bar of a Vanguard that spends Focus (ADR-031 §1). */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor FocusColor = FLinearColor::Transparent;

	/** The resource bar of a Vanguard that runs on Charge (ADR-033 §1). */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor ChargeColor = FLinearColor::Transparent;

	/** The colour of a resource bar of Family. */
	FLinearColor ResourceColorOf(EVeyraResourceFamily Family) const
	{
		return Family == EVeyraResourceFamily::Focus ? FocusColor : Family == EVeyraResourceFamily::Charge ? ChargeColor : ResourceColor;
	}

	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor BarBackgroundColor = FLinearColor::Transparent;

	/** HUD text. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor TextColor = FLinearColor::Transparent;

	/**
	 * The mastery emote's colour for each of its tiers, the first for tier 1 (ADR-045 §9); a tier past the last
	 * keeps the last. Presentation only.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	TArray<FLinearColor> MasteryEmoteTierColors;

	/** The line under each ability on the player's panel that says what it does. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor DescriptionColor = FLinearColor::Transparent;

	/** A Party Chat line in the match's chat log, marked apart from Team and All (ADR-046 §6). */
	UPROPERTY(Config, EditAnywhere, Category = "Chat")
	FLinearColor PartyChatColor = FLinearColor::Transparent;

	/** A direct message from or to a friend in the match's chat log. */
	UPROPERTY(Config, EditAnywhere, Category = "Chat")
	FLinearColor DirectChatColor = FLinearColor::Transparent;

	/** An ability whose empowerment waits for the next basic attack. */
	UPROPERTY(Config, EditAnywhere, Category = "Bars")
	FLinearColor EmpoweredColor = FLinearColor::Transparent;

	/** The minimap (Settings Bible §3.2; ADR-020 §2): its side, in pixels, bottom-right inside the HUD margin. */
	UPROPERTY(Config, EditAnywhere, Category = "Minimap", meta = (ClampMin = "1"))
	float MinimapSize = 0.0f;

	/** Its icons' sides, in pixels: Vanguards, structures and everything else. */
	UPROPERTY(Config, EditAnywhere, Category = "Minimap", meta = (ClampMin = "1"))
	float MinimapVanguardIcon = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Minimap", meta = (ClampMin = "1"))
	float MinimapStructureIcon = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Minimap", meta = (ClampMin = "1"))
	float MinimapUnitIcon = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Minimap")
	FLinearColor MinimapBackgroundColor = FLinearColor::Transparent;

	/** The connection warning's trouble: more than this fraction of packets lost, or a round trip above RoundTripMs (SET-21; ADR-055 §5). */
	UPROPERTY(Config, EditAnywhere, Category = "Warnings", meta = (ClampMin = "0"))
	float ConnectionWarningLossFraction = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Warnings", meta = (ClampMin = "0"))
	float ConnectionWarningRoundTripMs = 0.0f;

	/** The low-performance warning's trouble: foreground frames below this fraction of the cap (Proposal 110). */
	UPROPERTY(Config, EditAnywhere, Category = "Warnings", meta = (ClampMin = "0", ClampMax = "1"))
	float PerformanceWarningFraction = 0.0f;

	/** The frame rate an uncapped player is measured against. */
	UPROPERTY(Config, EditAnywhere, Category = "Warnings", meta = (ClampMin = "1"))
	float UncappedReferenceFps = 0.0f;

	/** How long trouble lasts before a warning starts, and calm before it clears, in seconds. */
	UPROPERTY(Config, EditAnywhere, Category = "Warnings", meta = (ClampMin = "0"))
	float WarningStartSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Warnings", meta = (ClampMin = "0"))
	float WarningClearSeconds = 0.0f;

	/** The fog of war on the minimap (ADR-054 §3), translucent over its ground. */
	UPROPERTY(Config, EditAnywhere, Category = "Minimap")
	FLinearColor MinimapFogColor = FLinearColor::Transparent;

	/** The battleground's walls on the minimap (ADR-043 §4); its river is drawn in RiverColor. */
	UPROPERTY(Config, EditAnywhere, Category = "Minimap")
	FLinearColor MinimapWallColor = FLinearColor::Transparent;

	/** Whether a left click or drag on the minimap moves the camera, and a right click there moves the Vanguard. */
	UPROPERTY(Config, EditAnywhere, Category = "Minimap")
	bool bMinimapClickMovesCamera = true;

	UPROPERTY(Config, EditAnywhere, Category = "Minimap")
	bool bMinimapRightClickMoves = true;

	/**
	 * How long a teammate's ping shows on the minimap and the ground (Settings Bible §3.2, "within tested
	 * bounds"; Match.json pings.keepSeconds is the most a client keeps one).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Pings", meta = (ClampMin = "1", ClampMax = "8"))
	float PingSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Pings")
	FLinearColor LookPingColor = FLinearColor::White;

	UPROPERTY(Config, EditAnywhere, Category = "Pings")
	FLinearColor DangerPingColor = FLinearColor::Red;

	/** A ping's marker on the ground, in pixels. */
	UPROPERTY(Config, EditAnywhere, Category = "Pings", meta = (ClampMin = "1"))
	float WorldPingSize = 0.0f;

	/** Short type labels on pings (Settings Bible SET-68), off by default. */
	UPROPERTY(Config, EditAnywhere, Category = "Pings")
	bool bPingTextLabels = false;

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

	/** A lingering area in its end warning, whichever side's: its end is about to land (ADR-026 §4). */
	UPROPERTY(Config, EditAnywhere, Category = "Telegraphs")
	FLinearColor EndingColor = FLinearColor::Transparent;

	/**
	 * The local player's indicator, before a cast (ADR-041 §2): its colour, and its outline in units at
	 * the Standard and Thick boundaries (Settings Bible §3.3).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Telegraphs")
	FLinearColor IndicatorColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Telegraphs", meta = (ClampMin = "0"))
	float IndicatorThickness = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Telegraphs", meta = (ClampMin = "0"))
	float IndicatorThickThickness = 0.0f;

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

	/**
	 * The structure kit's provisional art (Art Direction, Crucible structure greybox meshes), keyed by each
	 * kind's stable ID (StructureArtId), drawn in place of each structure's body: standing, then its wreck.
	 * Visual only: its capsule stays its only collision and navigation.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Battleground")
	TSoftObjectPtr<UVeyraUnitArtSet> StructureArt;

	/** A kind of structure's stable ID in its art kit, as the kit's manifest names it: "laneSpire" and the like. */
	static FName StructureArtId(EVeyraStructureKind Kind);

	/**
	 * The Fluxborn kit's provisional art (Art Direction, Fluxborn greybox meshes), keyed by each kind's content ID,
	 * drawn in place of each Fluxborn's body: active, then collapsed where it fell for its corpse's moment. A kind
	 * without art keeps its body. Visual only: its capsule stays its only collision and movement.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Battleground")
	TSoftObjectPtr<UVeyraUnitArtSet> FluxbornArt;

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

	/** The fog of war over the ground the viewer's side does not see (ADR-054 §3): dark and translucent. */
	UPROPERTY(Config, EditAnywhere, Category = "Vision")
	FLinearColor FogOfWarColor = FLinearColor::Transparent;

	/** How far above the floor the fog of war lies, in units: over the ground's markings, under the telegraphs. */
	UPROPERTY(Config, EditAnywhere, Category = "Vision", meta = (ClampMin = "0"))
	float FogOfWarLift = 0.0f;

	/** A presence ping: a ring over the fog an enemy is present in, fading until the next (ADR-016 §8). */
	UPROPERTY(Config, EditAnywhere, Category = "Vision")
	FLinearColor PresencePingColor = FLinearColor::Transparent;

	/** Sweeper's outline of an enemy in fog: a small ring where it stands. */
	UPROPERTY(Config, EditAnywhere, Category = "Vision")
	FLinearColor OutlineColor = FLinearColor::Transparent;

	/** The outline ring's radius, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "Vision", meta = (ClampMin = "1"))
	float OutlineMarkerRadius = 0.0f;

	/**
	 * A projected Echo's tether (ADR-050 §7): below this share of its Integrity its circle and stream read strained, in
	 * EchoStrainColor, warning that the circle is closing. Above 0 and below 1.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Echoes", meta = (ClampMin = "0", ClampMax = "1"))
	float EchoStrainShare = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Echoes")
	FLinearColor EchoStrainColor = FLinearColor::Transparent;

	/** How thick each marking is, in units. */
	UPROPERTY(Config, EditAnywhere, Category = "Battleground", meta = (ClampMin = "0"))
	float GroundMarkingThickness = 0.0f;

	/** How far each layer of markings sits above the one below (river, lanes, pads, then fog), so none flickers through another. */
	UPROPERTY(Config, EditAnywhere, Category = "Battleground", meta = (ClampMin = "0"))
	float GroundMarkingLift = 0.0f;

	// The HUD's deck and strips (VeyraHudDeck). The Art Bible leaves the in-game HUD open (v0.1 §9), so
	// its layout is provisional presentation, in the client's design language.

	/** The screen height the HUD's sizes are given for; it scales with the real height. */
	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	float HudReferenceHeight = 0.0f;

	/** The deck's and strips' smoked surface, their outline, and the accent for ready, lit and chosen things. */
	UPROPERTY(Config, EditAnywhere, Category = "Deck")
	FLinearColor HudSurfaceColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Deck")
	FLinearColor HudHairlineColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Deck")
	FLinearColor HudAccentColor = FLinearColor::Transparent;

	/** The player's own Health on the deck, and Gold. */
	UPROPERTY(Config, EditAnywhere, Category = "Deck")
	FLinearColor HealthColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Deck")
	FLinearColor GoldColor = FLinearColor::Transparent;

	/** Over a slot cooling down or not yet learned, and over the screen while the Vanguard is dead. */
	UPROPERTY(Config, EditAnywhere, Category = "Deck")
	FLinearColor ShadeColor = FLinearColor::Transparent;

	/** An ability's tile, a small tile (passive, Flux Spells, the vision tool), an item's, and the portrait, at the reference height. */
	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	float AbilityTileSize = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	float SmallTileSize = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	float ItemTileSize = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	float PortraitSize = 0.0f;

	/** Between tiles, and inside the deck's edge, at the reference height. */
	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "0"))
	float DeckGap = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "0"))
	float DeckPadding = 0.0f;

	/** The deck's Health and resource bars, at the reference height. */
	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	float DeckHealthHeight = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	float DeckResourceHeight = 0.0f;

	/** The rank pips under each ability, and the space between the Health and resource bars, at the reference height. */
	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	float DeckPipHeight = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "0"))
	float DeckBarSpacing = 0.0f;

	/**
	 * The least the deck shrinks to, as a share of its size at the player's scales, when it cannot fit beside the minimap
	 * inside the safe area (ADR-059 §2): the tested limit below which the HUD would no longer read.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "0.1", ClampMax = "1"))
	float DeckMinimumFit = 0.0f;

	/** The HUD's type, at the reference height: headings, body, small labels and key caps, the clock, and a headline. */
	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	int32 HudHeadingFontSize = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	int32 HudBodyFontSize = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	int32 HudSmallFontSize = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	int32 HudClockFontSize = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	int32 HudHeadlineFontSize = 0;

	/** A hovered slot's tooltip, at the reference height. */
	UPROPERTY(Config, EditAnywhere, Category = "Deck", meta = (ClampMin = "1"))
	float TooltipWidth = 0.0f;

	// The chat log and its composer at the bottom left (VeyraChatLogModel; ADR-029 §5).

	/** The log's and the composer's width, the composer's height, and how far the composer sits above the screen's bottom, at the reference height. */
	UPROPERTY(Config, EditAnywhere, Category = "Chat", meta = (ClampMin = "1"))
	float ChatWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Chat", meta = (ClampMin = "1"))
	float ChatInputHeight = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Chat", meta = (ClampMin = "0"))
	float ChatBottomOffset = 0.0f;

	/** How many of the newest lines show at once. */
	UPROPERTY(Config, EditAnywhere, Category = "Chat", meta = (ClampMin = "1"))
	int32 ChatLines = 0;

	/** The chat's type at each of the player's text sizes (SET-66), at the reference height. */
	UPROPERTY(Config, EditAnywhere, Category = "Chat", meta = (ClampMin = "1"))
	int32 ChatFontSize = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Chat", meta = (ClampMin = "1"))
	int32 ChatLargeFontSize = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Chat", meta = (ClampMin = "1"))
	int32 ChatExtraLargeFontSize = 0;

	/** How long a line stays whole without the player's setting, then how long it takes to fade, in seconds. */
	UPROPERTY(Config, EditAnywhere, Category = "Chat", meta = (ClampMin = "0"))
	float ChatFadeSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Chat", meta = (ClampMin = "0"))
	float ChatFadeOutSeconds = 0.0f;

	/** Behind the log's lines: the Standard backdrop and the High Contrast one (SET-67); Transparent draws none. */
	UPROPERTY(Config, EditAnywhere, Category = "Chat")
	FLinearColor ChatBackdropColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Chat")
	FLinearColor ChatHighContrastBackdropColor = FLinearColor::Transparent;

	/** How far a combat text number rises above its unit's bars while it shows, in units (ADR-052 §1). */
	UPROPERTY(Config, EditAnywhere, Category = "Combat text", meta = (ClampMin = "0"))
	float CombatTextRise = 0.0f;

	/** How long a number shows, rising and fading, in seconds. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat text", meta = (ClampMin = "0"))
	float CombatTextShowSeconds = 0.0f;

	/** Reduced density merges a number arriving within this many seconds of the last it would join. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat text", meta = (ClampMin = "0"))
	float CombatTextMergeSeconds = 0.0f;

	/** The numbers' size against the HUD's small type, and a crit's against that. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat text", meta = (ClampMin = "0"))
	float CombatTextScale = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Combat text", meta = (ClampMin = "1"))
	float CombatTextCritScale = 0.0f;

	/** Color-Coded damage (Proposal 52): each type its own colour, dealt or received. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat text")
	FLinearColor CombatTextPhysicalColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Combat text")
	FLinearColor CombatTextMagicColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Combat text")
	FLinearColor CombatTextTrueColor = FLinearColor::Transparent;

	/** Uniform damage: one colour for damage dealt, another for damage received. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat text")
	FLinearColor CombatTextUniformColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Combat text")
	FLinearColor CombatTextReceivedColor = FLinearColor::Transparent;

	/** Healing and shielding keep their own colours whichever damage appearance the player chose. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat text")
	FLinearColor CombatTextHealingColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Combat text")
	FLinearColor CombatTextShieldingColor = FLinearColor::Transparent;
};
