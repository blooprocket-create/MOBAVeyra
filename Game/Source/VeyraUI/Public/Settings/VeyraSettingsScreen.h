// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Settings/VeyraSettingsModels.h"
#include "Shell/VeyraShellButton.h"

#include "VeyraSettingsScreen.generated.h"

class FVeyraSettingsStore;
class UEditableTextBox;
class UHorizontalBox;
class UVerticalBox;
class UVeyraSettingsSubsystem;

/**
 * The Settings screen (ADR-024 §4–§5; Settings Bible §6, §13): the categories with real effects down
 * the left, the shown category's settings on the right, a search over every setting, one-step Undo,
 * and resets for one setting, a category or everything, the last two after a confirmation. Every
 * change is kept at once, with no Apply. It opens over the client's shell or over a live match, which
 * it never pauses; in a match, settings that change only outside matches show but stay locked.
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

	const FVeyraSettingsModel& GetModel() const { return Model; }

	/** Every button on screen, in the order built. For tests and scripts. */
	TArray<UVeyraShellButton*> GetButtons() const;

	/** The button named Label, or null. */
	UVeyraShellButton* FindButton(const FText& Label) const;

	/** Every text on screen, joined by new lines, for tests. */
	FString DescribeText() const;

	/** How the screen names a setting's buttons, for tests and scripts: one of its values, a step down or up, and its reset. */
	static FText OptionLabel(const FVeyraSettingRowModel& Row, const FVeyraSettingOptionModel& Option);
	static FText StepLabel(const FVeyraSettingRowModel& Row, bool bHigher);
	static FText ResetLabel(const FVeyraSettingRowModel& Row);

	/** The screen's own buttons. */
	static FText CloseLabel();
	static FText UndoLabel();
	static FText ResetCategoryLabel();
	static FText ResetAllLabel();
	static FText ConfirmResetLabel();
	static FText CancelLabel();

protected:
	virtual void NativeDestruct() override;

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
	void BuildRow(const FVeyraSettingRowModel& Row);
	void BuildFooter();
	void Change(const FVeyraContentId& Id, const FString& Value);
	UVeyraShellButton* AddButton(UPanelWidget& Parent, EVeyraShellButtonKind Kind, const FText& Label, const FText& Shown, TFunction<void()> Action,
		bool bEnabled = true, bool bSelected = false);

	UFUNCTION()
	void HandleSearchChanged(const FText& Text);

	TWeakObjectPtr<UVeyraSettingsSubsystem> Settings;
	FDelegateHandle ChangedHandle;
	bool bInMatch = false;
	TFunction<void()> Close;
	EVeyraSettingCategory Category = EVeyraSettingCategory::Controls;
	FString Search;
	EConfirming Confirming = EConfirming::None;
	FVeyraSettingsModel Model;

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
	TArray<TObjectPtr<UVeyraShellButton>> Buttons;
};
