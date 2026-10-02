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

	/** The Vanguard whose illustration a mode's card shows. */
	FString ModeArtOf(const FString& Mode) const;
};
