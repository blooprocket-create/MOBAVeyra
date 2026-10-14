// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Settings/VeyraSettingsModels.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellLook.h"
#include "Shell/VeyraShellMotion.h"

#include "VeyraSettingsScreen.generated.h"

class FVeyraSettingsStore;
class UBorder;
class UEditableTextBox;
class UHorizontalBox;
class UOverlay;
class UScrollBox;
class UVerticalBox;
class UTextBlock;
class UVeyraDisplayApplier;
class UVeyraSettingsSubsystem;

/**
 * The Settings screen (ADR-024 §4–§5; Settings Bible §6, §13): the categories with real effects down
 * the left, the shown category's settings on the right, a search over every setting, one-step Undo,
 * and resets for one setting, a category or everything, the last two after a confirmation. Every
 * change is kept at once, with no Apply. It opens over the client's shell or over a live match, which
 * it never pauses; in a match, settings that change only outside matches show but stay locked.
 *
 * A binding changes by capture (Settings Bible §1.1, §6.2): the next key or mouse button pressed on the
 * screen becomes its key, and never reaches the game; Escape cancels. A key another binding has asks
 * Replace or Cancel first (SET-81), and an essential action left without a key is named (SET-133).
 *
 * In the shell it is a page between the top bar and the friends and party panels; elsewhere a window (ADR-083 §1). Its
 * settings show in their groups, each kind of value with its own control, the one under the pointer or focus described
 * beside them, laid out for the width it is given (ADR-083 §2-§5).
 */
UCLASS()
class VEYRAUI_API UVeyraSettingsScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Whether its window or a category's rows are still arriving (ADR-082 §9): captures wait for them. */
	bool IsRevealing() const { return Reveal.IsPlaying(); }

	virtual bool Initialize() override;

	/**
	 * Shows Settings' store and follows its changes; Close runs when the player closes the screen.
	 * bInLiveMatch locks the settings that change only outside matches (Settings Bible §6.2).
	 */
	void Show(UVeyraSettingsSubsystem& InSettings, bool bInLiveMatch, TFunction<void()> InClose, bool bInAsPage = false);

	/** How it lays out for the width it is given (ADR-083 §5). */
	enum class ELayout : uint8
	{
		Wide,
		Medium,
		Narrow,
	};

	ELayout GetLayout() const { return Layout; }

	/** Lays out as though it were Width wide, as its tick does from its geometry. For tests and scripts. */
	void ApplyWidth(float Width);

	/** The setting the About panel describes (ADR-083 §4), if any. */
	TOptional<FVeyraContentId> GetAboutSetting() const;

	/** Whether it is a page of the shell rather than a window. */
	bool IsPage() const { return bAsPage; }

	/** Shows Category's settings, and ends a search. */
	void ShowCategory(EVeyraSettingCategory Category);

	/** Searches as though the player typed Search into the search field (§6.3). For tests and scripts. */
	void SetSearch(const FString& Search);

	/** The binding waiting for a key, if any. */
	const TOptional<FVeyraContentId>& GetCapturing() const { return Capturing; }

	/** A key pressed while a binding waits for one, as the screen's key and mouse events give it. For tests and scripts. */
	void CaptureKey(const FKey& Key);

	const FVeyraSettingsModel& GetModel() const { return Model; }

	/** Every button on screen, in the order built. For tests and scripts. */
	TArray<UVeyraShellButton*> GetButtons() const;

	/** The button named Label, or null. */
	UVeyraShellButton* FindButton(const FText& Label) const;

	/** Every text on screen, joined by new lines, for tests. */
	FString DescribeText() const;

	/** The frame as last styled (ADR-055 §2–§3): the title's type size and the window's fill. For tests. */
	int32 GetTitleTextSize() const;
	FLinearColor GetWindowFill() const;

	/** How the screen names a setting's buttons, for tests and scripts: one of its values, a step down or up, and its reset. */
	static FText OptionLabel(const FVeyraSettingRowModel& Row, const FVeyraSettingOptionModel& Option);
	static FText StepLabel(const FVeyraSettingRowModel& Row, bool bHigher);
	static FText ResetLabel(const FVeyraSettingRowModel& Row);
	/** A binding's button, which waits for its new key. */
	static FText ChangeLabel(const FVeyraSettingRowModel& Row);
	/** A choice's dropdown, which opens its options (ADR-083 §3). */
	static FText DropdownLabel(const FVeyraSettingRowModel& Row);
	/** A group's entry under its category, which scrolls the list to it (ADR-083 §2). */
	static FText GroupLinkLabel(const FText& GroupName);
	/** Takes a key another binding has, leaving that one without (SET-81). */
	static FText ReplaceLabel();

	/** The screen's own buttons. */
	static FText CloseLabel();
	static FText UndoLabel();
	static FText ResetCategoryLabel();
	static FText ResetAllLabel();
	static FText ConfirmResetLabel();
	static FText CancelLabel();

	/** A disruptive display change's choice (SET-92). */
	static FText KeepChangesLabel();
	static FText RevertLabel();

protected:
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	enum class EConfirming : uint8
	{
		None,
		Category,
		All,
	};

	FVeyraSettingsStore* GetStore() const;
	void StopListening();
	void Rebuild();
	/** Styles the frame built once, the window, title and search, in the current look, which it remembers. */
	void StyleFrame();
	/** Puts the frame on the page, or in the window over the screen (ADR-083 §1). */
	void Present();
	/** The categories, the rows and the About panel, for the current layout (ADR-083 §5). */
	void BuildBody();
	void BuildNav(UPanelWidget& Parent);
	/** Rows[First, Last) as one group under Heading: a grid when they are choices sharing their options (ADR-083 §3). */
	void BuildGroup(const FText& Heading, const FText& Hint, int32 First, int32 Last);
	void BuildGrid(UPanelWidget& Box, int32 First, int32 Last);
	void BuildRow(UPanelWidget& Box, int32 Index);
	void BuildControl(UPanelWidget& Line, UPanelWidget& Under, const FVeyraSettingRowModel& Row);
	/** The row's frame, which the About panel follows when the pointer or focus is in it. */
	UBorder* MakeRowFrame(int32 Index, UWidget& Content);
	/** Shows AboutRow's description, and marks its row. */
	void UpdateAbout();
	bool IsGrid(int32 First, int32 Last) const;
	ELayout LayoutFor(float Width) const;
	void BuildFooter();
	void Change(const FVeyraContentId& Id, const FString& Value);
	UVeyraShellButton* AddButton(UPanelWidget& Parent, EVeyraShellButtonKind Kind, const FText& Label, const FText& Shown, TFunction<void()> Action,
		bool bEnabled = true, bool bSelected = false);

	UFUNCTION()
	void HandleSearchChanged(const FText& Text);

	TWeakObjectPtr<UVeyraSettingsSubsystem> Settings;
	FDelegateHandle ChangedHandle;
	/** Applies display settings, and holds a change that waits for Keep; null where there is none (a test). */
	TWeakObjectPtr<UVeyraDisplayApplier> Display;

	/**
	 * Makes the player's change through Change, a reset as much as a new value: a disruptive display
	 * setting it moved waits for Keep and reverts without it (SET-92).
	 */
	void ChangeStore(TFunctionRef<void(FVeyraSettingsStore&)> Change);
	FDelegateHandle ConfirmationHandle;
	bool bInMatch = false;
	TFunction<void()> Close;
	EVeyraSettingCategory Category = EVeyraSettingCategory::Controls;
	FString Search;
	EConfirming Confirming = EConfirming::None;
	FVeyraSettingsModel Model;

	/** A key another binding has, waiting for Replace or Cancel. */
	struct FPendingRebind
	{
		FVeyraContentId Id;
		FKey Key;
		FVeyraContentId Other;
	};

	/** Puts Key on Id: the developer's own key becomes the default again. */
	void Rebind(const FVeyraContentId& Id, const FKey& Key);

	TOptional<FVeyraContentId> Capturing;
	TOptional<FPendingRebind> PendingRebind;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> Actions;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Nav;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Rows;

	/** Holds the frame: on the page, or inside the window over the screen. */
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> Root;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Scrim;

	/** The categories, the list and the About panel, rebuilt for the layout. */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BodyHost;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ListScroll;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> NavScroll;

	/** Builds the categories, the list and About afresh, the list scrolled as far as the one before it (see BuildBody). */
	void MakeBodyWidgets();

	/**
	 * Ends Line with the row's place for Reset (ADR-083 §3): the same width on every row, holding Reset only where it would
	 * change something, so the row's controls stay where they were when it shows.
	 */
	void AddResetPlace(UHorizontalBox& Line, const FVeyraSettingRowModel& Row);

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> AboutHost;

	/** Each row's frame and the row it shows, which the About panel follows. */
	struct FRowFrame
	{
		TWeakObjectPtr<UBorder> Frame;
		int32 Row = INDEX_NONE;
	};
	TArray<FRowFrame> RowFrames;
	/** Each group's heading, which its entry under the category scrolls to. */
	TArray<TPair<FText, TWeakObjectPtr<UWidget>>> GroupHeadings;
	int32 AboutRow = INDEX_NONE;
	TOptional<FVeyraContentId> OpenDropdown;
	ELayout Layout = ELayout::Wide;
	bool bAsPage = false;

	/** The window's parts, which rise in as it opens; then a category's rows arrive as it is shown (ADR-082 §9). */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> WindowFrame;

	FVeyraShellReveal Reveal;
	bool bRevealWindow = false;
	bool bRevealingRows = false;
	TOptional<EVeyraSettingCategory> RevealedCategory;
	FString RevealedSearch;

	/** Reveals what a show or a new category brought; carries a reveal under way over to a rebuild's parts. */
	void RevealArrivals();

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> Footer;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> SearchBox;

	/**
	 * The search at its width, and where it goes (ADR-083 §5): beside the title, or on a line of its own under the title
	 * when the layout is narrow, so the header never runs past its page.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> SearchHolder;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> WideSearchHost;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> NarrowSearchHost;

	/** Puts the search where the present layout wants it. */
	void PlaceSearch();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> WindowPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Title;

	/** The look the frame was last styled in: a change restyles it (ADR-055 §2–§3). */
	FVeyraShellLook FrameLook;

	/** The countdown to a disruptive display change's revert, while one waits. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RevertCountdown;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVeyraShellButton>> Buttons;
};
