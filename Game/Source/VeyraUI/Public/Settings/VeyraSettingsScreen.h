// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Settings/VeyraSettingsModels.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellLook.h"

#include "VeyraSettingsScreen.generated.h"

class FVeyraSettingsStore;
class UBorder;
class UEditableTextBox;
class UHorizontalBox;
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
 */
UCLASS()
class VEYRAUI_API UVeyraSettingsScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/**
	 * Shows Settings' store and follows its changes; Close runs when the player closes the screen.
	 * bInLiveMatch locks the settings that change only outside matches (Settings Bible §6.2).
	 */
	void Show(UVeyraSettingsSubsystem& InSettings, bool bInLiveMatch, TFunction<void()> InClose);

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
	void BuildRow(const FVeyraSettingRowModel& Row);
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

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> Footer;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> SearchBox;

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
