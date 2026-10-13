// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "VeyraShellStyleSettings.generated.h"

/** The art on a mode's card: a Vanguard's illustration, until modes have key art of their own. */
USTRUCT()
struct FVeyraModeArt
{
	GENERATED_BODY()

	/** The mode's ID, such as "casual_select". */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FString Mode;

	/** The Vanguard whose illustration its card shows, such as "raska". */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FString Vanguard;
};

/** Where a Vanguard's face is in its hero illustration, for the portraits cropped from it. */
USTRUCT()
struct FVeyraVanguardPortrait
{
	GENERATED_BODY()

	/** The Vanguard's content ID, such as "cairn"; empty for the default. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FString Vanguard;

	/** The face's centre, from 0 to 1 across and down the illustration. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FVector2D Focus = FVector2D::ZeroVector;

	/** How much of the illustration's height a portrait shows, above 0 and at most 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	float CropHeight = 0.0f;
};

/** One typeface of a font family (ADR-078 §2): its name, such as "Regular" or "SemiBold", and its file. */
USTRUCT()
struct FVeyraFontFace
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FName Typeface;

	/** The font file under the project's Content folder, such as "Veyra/Fonts/Inter/Inter-Regular.ttf"; staged with the game. */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FString File;
};

/** The client's three families of type (Art Bible §4.3): the display tier, the functional tier, and its numerals. */
UENUM()
enum class EVeyraFontFamily : uint8
{
	/** Identity: headlines, Vanguard names, the wordmark. */
	Display,
	/** Function: labels, body text, buttons, chat. */
	Interface,
	/** Timers, counts and currencies, in a narrower cut of the functional face. */
	Numeric,
};

/** Which family and typeface a role of text is set in (ADR-078 §2). */
USTRUCT()
struct FVeyraTextFace
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Design")
	EVeyraFontFamily Family = EVeyraFontFamily::Interface;

	/** One of the family's typefaces. */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FName Typeface;
};

/**
 * How the shell's screens and the in-match menu look (ADR-010 §4): grey-box colours, text sizes and
 * spacing. Presentation, not tuning, stored in Config/DefaultGame.ini. Every value is required: the
 * shell logs the problems Validate finds and shows no screen.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Veyra Shell Style"))
class VEYRAUI_API UVeyraShellStyleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Every problem with these settings, as "Field: message"; empty when the shell can use them. */
	TArray<FString> Validate() const;

	/** The turn cue's sample rate: the audio format it is made in, not presentation to tune. */
	static constexpr int32 TurnCueSampleRate = 48000;

	/** Behind every screen. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor BackgroundColor = FLinearColor::Transparent;

	/** Behind cards, the team overview and the in-match menu. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor PanelColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor TextColor = FLinearColor::Transparent;

	/** Secondary text: explanations, labels. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor MutedTextColor = FLinearColor::Transparent;

	/** Titles and the countdown. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor AccentColor = FLinearColor::Transparent;

	/** Behind a problem and its Retry. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor ProblemColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor ButtonColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor ButtonHoveredColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor ButtonPressedColor = FLinearColor::Transparent;

	/** A button that cannot be used now. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor ButtonDisabledColor = FLinearColor::Transparent;

	/** The hovered or chosen card. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor SelectedColor = FLinearColor::Transparent;

	/** Over the match while its menu is open; translucent, so the match shows through. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor MenuScrimColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Text", meta = (ClampMin = "1"))
	int32 TitleFontSize = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Text", meta = (ClampMin = "1"))
	int32 HeadingFontSize = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Text", meta = (ClampMin = "1"))
	int32 BodyFontSize = 0;

	/** The champion-select countdown. */
	UPROPERTY(Config, EditAnywhere, Category = "Text", meta = (ClampMin = "1"))
	int32 CountdownFontSize = 0;

	/** Between the screen's edge and its content, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float ScreenPadding = 0.0f;

	/** Between one element and the next, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float Spacing = 0.0f;

	/** Inside a button, around its label, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float ButtonPadding = 0.0f;

	/** A Vanguard or mode card, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float CardWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float CardHeight = 0.0f;

	/** The in-match menu's width, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float MenuWidth = 0.0f;

	/** The shop's size, in slate units; it scrolls when its lists are longer. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float ShopWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float ShopHeight = 0.0f;

	/** The in-match scoreboard, both teams side by side, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float ScoreboardWidth = 0.0f;

	/** Champion select's names under portraits and statuses. */
	UPROPERTY(Config, EditAnywhere, Category = "Text", meta = (ClampMin = "1"))
	int32 SmallFontSize = 0;

	/** Multiplies the shown Vanguard's art behind champion select, darkening it so the text reads. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor BackdropTint = FLinearColor::Transparent;

	/** The frame around the shown Vanguard's art, the player's own pick and the hovered portrait. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor FrameColor = FLinearColor::Transparent;

	/** Rings the player's team's portraits. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor AllyColor = FLinearColor::Transparent;

	/** Rings the enemy team's portraits. */
	UPROPERTY(Config, EditAnywhere, Category = "Colours")
	FLinearColor EnemyColor = FLinearColor::Transparent;

	/** Inside a portrait or tile button, around its content, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float TilePadding = 0.0f;

	/** A roster tile's rounded corners, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float TileCornerRadius = 0.0f;

	/** The outline around portraits and the shown Vanguard's art, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float FrameWidth = 0.0f;

	/** A roster portrait across champion select's top, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float RosterTileSize = 0.0f;

	/** The widest champion select's row of portraits grows before it scrolls, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float RosterBenchWidth = 0.0f;

	/** Each of the countdown's two draining bars, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float PickBarWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float PickBarHeight = 0.0f;

	/** Each team's column of seats, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float SeatColumnWidth = 0.0f;

	/** A seat's round portrait, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float PortraitSize = 0.0f;

	/** Each of a seat's starting spells beside its portrait, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float SeatSpellSize = 0.0f;

	/** The shown Vanguard's framed art in the middle, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float SplashWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float SplashHeight = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float SplashCornerRadius = 0.0f;

	/** A Flux Spell slot's tile along the bottom, and Lock In's height, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float SpellTileSize = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float LockInWidth = 0.0f;

	/** The Flux Spell picker, and each spell's tile in it, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float PickerWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float PickerTileWidth = 0.0f;

	/** A match report's name and label columns, and each of its other columns, in slate units (UX-50). */
	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float ReportLabelWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1"))
	float ReportColumnWidth = 0.0f;

	/** Where the Vanguards' hero textures are imported, such as "/Game/Veyra/UI/Vanguards" (VeyraShellArt). */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FString VanguardArtFolder;

	/** Where the items' icons are imported, such as "/Game/Veyra/UI/Items" (VeyraShellArt). */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FString ItemArtFolder;

	/** Where the abilities' and Flux Spells' icons are imported, such as "/Game/Veyra/UI/Abilities" (VeyraShellArt). */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FString AbilityArtFolder;

	/** An ability's icon beside its name and description, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Art", meta = (ClampMin = "1"))
	float AbilityIconSize = 0.0f;

	/** How an item's icon is tinted while it cannot be bought. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FLinearColor ItemDimTint = FLinearColor::Transparent;

	/** Where a Vanguard's face is when VanguardPortraits does not say. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	FVeyraVanguardPortrait DefaultPortrait;

	/** Where each Vanguard's face is in its hero illustration; one entry per Vanguard at most. */
	UPROPERTY(Config, EditAnywhere, Category = "Art")
	TArray<FVeyraVanguardPortrait> VanguardPortraits;

	// The design system (Art Bible v0.1 §4): surfaces, the primary action, and the display and label
	// tiers of type. Where the bible leaves UI geometry and fonts open, these are provisional.

	/** A smoked, translucent panel or card over the world behind it (Art Bible §4.2). */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FLinearColor SurfaceColor = FLinearColor::Transparent;

	/** A surface that stands out: a bar, a table's header, a highlighted row. */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FLinearColor SurfaceRaisedColor = FLinearColor::Transparent;

	/** The thin outline around surfaces and the rules between groups. */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FLinearColor HairlineColor = FLinearColor::Transparent;

	/** The one action a screen leads to: Play, Accept, Find Match, Lock In, Continue. */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FLinearColor PrimaryColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FLinearColor PrimaryHoveredColor = FLinearColor::Transparent;

	/** A primary action's label, dark on the primary colour. */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FLinearColor PrimaryTextColor = FLinearColor::Transparent;

	/** The dark that scrims fade art to, so text reads over it. */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FLinearColor ScrimColor = FLinearColor::Transparent;

	/** Multiplies a showcase's art (Home, Play's cards, the results), which scrims darken where text sits. */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FLinearColor ShowcaseTint = FLinearColor::Transparent;

	/** Victory's and defeat's headlines on the results (Art Bible §6.5: different, and never humiliating). */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FLinearColor VictoryColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FLinearColor DefeatColor = FLinearColor::Transparent;

	/** A screen's headline, the display tier. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	int32 DisplayFontSize = 0;

	/** The small tracked labels above titles and over table columns. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	int32 EyebrowFontSize = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	int32 ButtonFontSize = 0;

	/** Tracking, in thousandths of an em, for the display tier, eyebrows and button labels. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "0"))
	int32 DisplayLetterSpacing = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "0"))
	int32 EyebrowLetterSpacing = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "0"))
	int32 ButtonLetterSpacing = 0;

	/** Surfaces' and buttons' rounded corners, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "0"))
	float PanelCornerRadius = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "0"))
	float ButtonCornerRadius = 0.0f;

	/** The primary action's least width, so it reads as the way forward, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float PrimaryButtonWidth = 0.0f;

	/** Home's column of text and actions, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float HomeColumnWidth = 0.0f;

	/** A mode's card on Play, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ModeCardWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ModeCardHeight = 0.0f;

	/**
	 * The shop (ADR-012 §11): a catalog tile, the smaller tiles of the quick-buy
	 * panels, recipes and upgrades, and the widths of the quick-buy column and the selected item's pane,
	 * in slate units.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ShopTileSize = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ShopMarkSize = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ShopQuickWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ShopDetailsWidth = 0.0f;

	/** The shop's panel's opacity over the match, so nothing drawn behind it reads through (ADR-082 §8). */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "0", ClampMax = "1"))
	float ShopPanelOpacity = 0.0f;

	/** Match Found's and the status screens' centred panel, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float DialogWidth = 0.0f;

	/** The Settings screen's window over the client or the match (ADR-024 §4), in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float SettingsWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float SettingsHeight = 0.0f;

	/** Its categories' column, its search field, and the column of each setting's controls, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float SettingsNavWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float SettingsSearchWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float SettingsControlWidth = 0.0f;

	/** The side of each colour swatch a colour setting previews beside its name (SET-8; ADR-055 §1). */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float SettingsSwatchSize = 0.0f;

	/** The friends panel down the right of the shell and the lobby (Art Bible §7), in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float FriendsPanelWidth = 0.0f;

	/** The height of a chat panel's lines (ADR-046 §6): the sidebar's, champion select's and the results screen's, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ChatLinesHeight = 0.0f;

	/** A profile's icon on its card, and the featured Vanguard's art beside it (ADR-048 §5), in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ProfileIconSize = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ProfileFeaturedHeight = 0.0f;

	/** The tallest an opened profile's shared Match History grows before it scrolls, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ProfileHistoryMaxHeight = 0.0f;

	/**
	 * The one brief, distinct cue as the player's draft turn begins (Pre-Game Client UX Bible 32): its tones
	 * in turn, in hertz, each tone's length in seconds, and its volume. Presentation, made as it plays.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Sound")
	TArray<float> TurnCueTonesHz;

	UPROPERTY(Config, EditAnywhere, Category = "Sound", meta = (ClampMin = "0.01"))
	float TurnCueToneSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Sound", meta = (ClampMin = "0", ClampMax = "1"))
	float TurnCueVolume = 0.0f;

	/** The enhanced keyboard focus outline (SET-74; ADR-055 §3): high-contrast against every surface, and its width in pixels. */
	UPROPERTY(Config, EditAnywhere, Category = "Focus")
	FLinearColor FocusOutlineColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Focus", meta = (ClampMin = "1"))
	float FocusOutlineWidth = 0.0f;

	/** Interface Text Size's factor on the menus' text, by option (SET-62; ADR-055 §2). */
	UPROPERTY(Config, EditAnywhere, Category = "Text")
	TMap<FString, float> TextSizeScales;

	/** The break reminder's options in seconds of continuous play, by option (ADR-053 §4); Off has none. */
	UPROPERTY(Config, EditAnywhere, Category = "Reminders")
	TMap<FString, float> PlayReminderSeconds;

	/** The match loading screen's column, in slate units (ADR-053 §3). */
	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (ClampMin = "1"))
	float LoadingPanelWidth = 0.0f;

	/**
	 * How long an automatically shown tip or fact stays (SET-120): at least LoadingEntryMinimumSeconds, and a second more for
	 * each LoadingEntryCharactersPerSecond characters past LoadingEntryBaseCharacters.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (ClampMin = "8"))
	float LoadingEntryMinimumSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (ClampMin = "0"))
	int32 LoadingEntryBaseCharacters = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (ClampMin = "1"))
	int32 LoadingEntryCharactersPerSecond = 0;

	/** The match-ready sound as a match is found (SET-71; ADR-053 §2): its tones, each tone's length and its volume, made as it plays. */
	UPROPERTY(Config, EditAnywhere, Category = "Sound")
	TArray<float> MatchReadyCueTonesHz;

	UPROPERTY(Config, EditAnywhere, Category = "Sound", meta = (ClampMin = "0.01"))
	float MatchReadyCueToneSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Sound", meta = (ClampMin = "0", ClampMax = "1"))
	float MatchReadyCueVolume = 0.0f;

	/** A seat of a custom lobby's two columns, in slate units (ADR-021). */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float LobbySeatWidth = 0.0f;

	/**
	 * The starting Gold a lobby's host may choose from, besides the game's own (Custom Matches Bible §4).
	 * Presentation only: the lobby's range decides what is allowed, and a choice outside it is not offered.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	TArray<float> LobbyStartingGoldChoices;

	/** The Vanguard whose illustration fills Home until its scene is designed (Art Bible §6.1). */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FString HomeVanguard;

	/** Each mode card's art; a mode not listed shows HomeVanguard's. */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	TArray<FVeyraModeArt> ModeArt;

	// The Smoked Crucible (ADR-078): the client's type and the surfaces' shape, approved by the author on 2026-10-13.

	/** The families' typefaces (§2): files the game ships under Content, each family naming its own typefaces. */
	UPROPERTY(Config, EditAnywhere, Category = "Type")
	TArray<FVeyraFontFace> DisplayFontFaces;

	UPROPERTY(Config, EditAnywhere, Category = "Type")
	TArray<FVeyraFontFace> InterfaceFontFaces;

	UPROPERTY(Config, EditAnywhere, Category = "Type")
	TArray<FVeyraFontFace> NumericFontFaces;

	/** The face of each role of text (VeyraShellStyle::EVeyraShellText); Muted reads as Body, Column as Eyebrow. */
	UPROPERTY(Config, EditAnywhere, Category = "Type")
	FVeyraTextFace TitleFace;

	UPROPERTY(Config, EditAnywhere, Category = "Type")
	FVeyraTextFace HeadingFace;

	UPROPERTY(Config, EditAnywhere, Category = "Type")
	FVeyraTextFace BodyFace;

	UPROPERTY(Config, EditAnywhere, Category = "Type")
	FVeyraTextFace SmallFace;

	UPROPERTY(Config, EditAnywhere, Category = "Type")
	FVeyraTextFace CountdownFace;

	UPROPERTY(Config, EditAnywhere, Category = "Type")
	FVeyraTextFace DisplayFace;

	UPROPERTY(Config, EditAnywhere, Category = "Type")
	FVeyraTextFace EyebrowFace;

	UPROPERTY(Config, EditAnywhere, Category = "Type")
	FVeyraTextFace ButtonFace;

	UPROPERTY(Config, EditAnywhere, Category = "Type")
	FVeyraTextFace PrimaryButtonFace;

	/** Whether the display tier is set in capitals; headlines otherwise read as written. */
	UPROPERTY(Config, EditAnywhere, Category = "Type")
	bool bDisplayInCapitals = false;

	/**
	 * The one ornament (§3): a cut across two opposite corners of every surface and button, so many slate units along
	 * each edge.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Surface", meta = (ClampMin = "1"))
	float PanelChamfer = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Surface", meta = (ClampMin = "1"))
	float ButtonChamfer = 0.0f;

	/** A surface's hairline edge, and the tick that finishes each cut corner. */
	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	FLinearColor EdgeColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	FLinearColor EdgeTickColor = FLinearColor::Transparent;

	/** An ordinary button's edge; hovered, it takes the accent. */
	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	FLinearColor ButtonEdgeColor = FLinearColor::Transparent;

	/** How much darker a surface is at its foot than at its head, from 0 (flat) to below 1: its depth. */
	UPROPERTY(Config, EditAnywhere, Category = "Surface", meta = (ClampMin = "0", ClampMax = "0.95"))
	float SurfaceShade = 0.0f;

	/** The primary action's brass, from its lit head (PrimaryHighlightColor) through PrimaryColor to its foot. */
	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	FLinearColor PrimaryHighlightColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	FLinearColor PrimaryShadeColor = FLinearColor::Transparent;

	/** The thin light along the top of the primary action, inside its edge. */
	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	FLinearColor PrimaryShineColor = FLinearColor::Transparent;

	/** The line under the shown tab, in the accent, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Surface", meta = (ClampMin = "1"))
	float TabUnderlineHeight = 0.0f;

	/** The dot beside a friend's status (ADR-061 §2): available, busy in a game, and away or offline. The words say it too. */
	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	FLinearColor PresenceAvailableColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	FLinearColor PresenceBusyColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Surface")
	FLinearColor PresenceAwayColor = FLinearColor::Transparent;

	UPROPERTY(Config, EditAnywhere, Category = "Surface", meta = (ClampMin = "1"))
	float PresenceDotSize = 0.0f;

	/** Home's Play, the clear way in (Art Bible §6.1): its least width and height, and its label's size, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float HomePlayWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float HomePlayHeight = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	int32 HomePlayFontSize = 0;

	/** The line under the top bar's level that fills toward the next (ADR-045 §8), in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ProgressionBarWidth = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ProgressionBarHeight = 0.0f;

	/** The top bar (ADR-078 §5): the Flux mark beside the wordmark, the level's badge, and each balance's mark, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float WordmarkGlyphSize = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float LevelBadgeSize = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float CurrencyGlyphSize = 0.0f;

	/** A friend's or party member's initial on its tile, beside the name, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float AvatarSize = 0.0f;

	/** Multiplies champion select's art, the screen's centrepiece, which scrims darken only at its edges (ADR-078 §5). */
	UPROPERTY(Config, EditAnywhere, Category = "Design")
	FLinearColor SelectArtTint = FLinearColor::Transparent;

	/** The kit's icons under the shown Vanguard's name in champion select, in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float SelectKitIconSize = 0.0f;

	/** A scoreboard row's portrait, and its build's item and Flux Spell icons (ADR-078 §5), in slate units. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ReportPortraitSize = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "1"))
	float ReportIconSize = 0.0f;

	/** How far Home's welcome sits below the top bar, as a share of the room, from 0 to 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Design", meta = (ClampMin = "0", ClampMax = "1"))
	float HomeHeroHeight = 0.0f;

	// The menus' motion (ADR-082 §7), in the HUD's curves; Reduce UI Animation holds it all still.

	/**
	 * A page arriving: each of its parts rises RevealRise or slides RevealSlide with RevealShear degrees of lean into place over
	 * RevealSeconds, each RevealStagger seconds after the one before.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	float RevealSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	float RevealStagger = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	float RevealRise = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	float RevealSlide = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	float RevealShear = 0.0f;

	/** A button's sheen crossing it as the cursor arrives: its seconds and its light; and the burst where a button is pressed, this far across. */
	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	float SheenSeconds = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0", ClampMax = "1"))
	float SheenOpacity = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "1"))
	float PressBurstRadius = 0.0f;

	/**
	 * Flux motes drifting up behind Home, as on the website: one for every MoteArea square units, rising MoteRiseMin to
	 * MoteRiseMax units a second, swaying MoteSway either side, MoteRadiusMin to MoteRadiusMax across; MoteBrassShare of them
	 * brass rather than the Flux teal, MoteHotShare with a white-hot heart.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "1"))
	float MoteArea = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	int32 MoteMost = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	float MoteRiseMin = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	float MoteRiseMax = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	float MoteSway = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	float MoteRadiusMin = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0"))
	float MoteRadiusMax = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0", ClampMax = "1"))
	float MoteBrassShare = 0.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion", meta = (ClampMin = "0", ClampMax = "1"))
	float MoteHotShare = 0.0f;

	/** The faces' files, by family. */
	const TArray<FVeyraFontFace>& FacesOf(EVeyraFontFamily Family) const;

	/** The Vanguard whose illustration a mode's card shows. */
	FString ModeArtOf(const FString& Mode) const;
};
