// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Scoreboard/VeyraScoreboardModel.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellLook.h"

#include "VeyraScoreboard.generated.h"

class APlayerController;
class FVeyraSettingsStore;
class UHorizontalBox;
class UVerticalBox;

/**
 * The in-match scoreboard (Settings Bible #56; ADR-017 §4), built in C++:
 * both teams side by side, the viewer's first, each player's Vanguard, level, K/D/A, creep
 * score and items, and each team's kills. It shows only what every client receives and takes no input:
 * it is open while its key is held.
 */
UCLASS()
class VEYRAUI_API UVeyraScoreboard : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Builds its frame, before anything shows it. */
	virtual bool Initialize() override;

	/** Shows the match as Controller's player sees it. */
	void Show(const APlayerController& InController);

	/**
	 * Reads the game state, and rebuilds when what it shows has changed. The scoreboard does so each frame
	 * it is painted; a script that reads it where nothing paints (a -nullrhi client) calls this first.
	 */
	void Refresh();

	/** What it shows now. */
	const FVeyraScoreboardView& GetView() const { return View; }

	/** The side colours its headings and faces wear: the player's colour vision (SET-8; ADR-055 §1). */
	const FVeyraSideColors& GetSides() const { return Sides; }

	/** The look its last build used: always the standard one, the HUD's (ADR-055 §2). For tests. */
	const FVeyraShellLook& GetBuiltLook() const { return BuiltLook; }

	/** Reads the player's settings from Store rather than the game instance's. For tests. */
	void BindSettings(const FVeyraSettingsStore& Store) { SettingsStore = &Store; }

	/** Every line of text it shows, in the order built. For tests and scripts. */
	TArray<FString> GetLines() const;

	/** A team's heading, as the scoreboard shows it. */
	static FText SideHeading(const FVeyraScoreboardSide& Side);

	/** A player's Vanguard, as its row names it over its player. */
	static FText VanguardLine(const FVeyraScoreboardRow& Row);

	/** A player's name under its Vanguard, marked while the player is away. */
	static FText RowLine(const FVeyraScoreboardRow& Row);

	/** A player's items in words, in slot order, empty slots marked: the build's tooltip. */
	static FText ItemsLine(const FVeyraScoreboardRow& Row);

	/** How many item tiles its last build drew. For tests. */
	int32 GetItemTiles() const { return ItemTiles; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Rebuild();

	TWeakObjectPtr<const APlayerController> Controller;
	FVeyraScoreboardView View;
	FVeyraSideColors Sides;
	FVeyraShellLook BuiltLook;
	const FVeyraSettingsStore* SettingsStore = nullptr;
	bool bBuilt = false;
	int32 ItemTiles = 0;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Board;
};
