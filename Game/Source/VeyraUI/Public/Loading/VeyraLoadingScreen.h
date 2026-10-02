// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Loading/VeyraLoadingModel.h"

#include "VeyraLoadingScreen.generated.h"

class FVeyraSettingsStore;
class UPanelWidget;
class UTextBlock;
class UVeyraShellButton;

/**
 * The match loading screen (Match Flow Bible §13; SET-111–120; ADR-053 §3), built in C++. It covers the battleground with
 * the stage in plain words over an activity indicator, never a percentage, and the player's tips and lore with Previous
 * and Next. Its owner closes it the moment loading completes: nothing on it holds the match up.
 */
UCLASS()
class VEYRAUI_API UVeyraLoadingScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/** Rotates through Entries in an order Seed shuffles, each timed by Timing, from Now; no entries shows none. */
	void Show(TArray<FVeyraLoadingEntry> InEntries, int32 Seed, const FVeyraLoadingTiming& InTiming, double Now);

	/**
	 * Shows the tips and lore Store's player chose, as Show does, and follows Store while shown: a change of
	 * categories applies at once (SET-118). Null shows the defaults. Store must outlive the binding (UnbindSettings).
	 */
	void ShowFor(FVeyraSettingsStore* Store, int32 Seed, const FVeyraLoadingTiming& InTiming, double Now);

	/** Stops following the settings store. */
	void UnbindSettings();

	/** Left browses to the previous entry and Right to the next, as the buttons do (SET-117). True if Key browsed. */
	bool BrowseWithKey(const FKey& Key, double Now);

	/** What takes the keyboard while the screen is up: Next, so Enter shows the next entry and Tab reaches Previous. */
	TSharedPtr<SWidget> GetFocusTarget();

	/** Says Stage. */
	void SetStage(EVeyraLoadingStage InStage);

	/** Moves the rotation on as Now passes. */
	void Update(double Now);

	/** Previous (-1) or Next (+1): the entry changes at once, and automatic rotation stops (SET-117). */
	void Browse(int32 Step, double Now);

	/** How the screen says Stage. */
	static FText StageText(EVeyraLoadingStage Stage);

protected:
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeDestruct() override;

public:

	/** Whether its activity indicator spins: not under Reduce Interface Animation (SET-65). */
	bool IsSpinning() const { return bSpinning; }

	/** What the screen shows now. For tests. */
	EVeyraLoadingStage GetStage() const { return Stage; }
	const FVeyraLoadingRotation& GetRotation() const { return Rotation; }
	FText GetShownText() const;
	TArray<UVeyraShellButton*> GetButtons() const;

private:
	void ShowEntry();

	/** Shows the categories the followed store chooses now, if they changed; from the start when bAlways. */
	void ApplyCategories(double Now, bool bAlways);

	bool bSpinning = false;
	FVeyraSettingsStore* SettingsStore = nullptr;
	FDelegateHandle SettingsHandle;
	TOptional<EVeyraLoadingContent> ShownContent;
	int32 ShownSeed = 0;

	EVeyraLoadingStage Stage = EVeyraLoadingStage::LoadingMatch;
	TArray<FVeyraLoadingEntry> Entries;
	FVeyraLoadingRotation Rotation;
	FVeyraLoadingTiming Timing;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StageLabel;

	/** The tips and lore panel, collapsed without entries. */
	UPROPERTY(Transient)
	TObjectPtr<UPanelWidget> EntryPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EntryKind;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EntryText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVeyraShellButton>> Buttons;
};
