// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Shell/VeyraShellModels.h"

#include "VeyraShellScreen.generated.h"

class IVeyraClientIntents;
class UImage;
class UOverlay;
class UPanelWidget;
class UProgressBar;
class UScaleBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UVeyraShellButton;
class UWidget;

/** The ordinary pre-game pages the shell's navigation moves between (UX §1). */
enum class EVeyraShellPage : uint8
{
	Home,
	Play,
};

/**
 * The shell's screens (ADR-010 §4), built in C++ with no widget Blueprint: signing in, the starter
 * choice, Home and Play with their mode cards and the party panel, Match Found, champion select,
 * Match Starting and Connecting, Reconnect-only, results, and the problem banner with Retry. It shows
 * the coordinator's snapshot and asks through its intents, with buttons enabled only when CanIssue
 * allows; it decides nothing.
 *
 * Match Found, champion select and Reconnect-only own the whole screen: no navigation, and on
 * Reconnect-only no action but Reconnect (UX §5, UX-4, UX-17). The countdowns and the queue's time
 * follow the backend's timers every frame; the rest is rebuilt only when what it shows changes, so a
 * poll that changes nothing never interrupts a click.
 */
UCLASS()
class VEYRAUI_API UVeyraShellScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows Client and asks it for what the player clicks. Client must stay alive until Unbind. */
	void Bind(IVeyraClientIntents& InClient);

	/** Stops showing and asking the client; the screen then shows nothing new. */
	void Unbind();

	/** Brings the screen up to date. Changes to the snapshot call it; tests call it directly. */
	void Refresh();

	EVeyraShellScreen GetShownScreen() const { return Shown; }
	EVeyraShellPage GetPage() const { return Page; }

	/** The Flux Spell slot whose picker champion select shows, or INDEX_NONE. */
	int32 GetOpenSpellSlot() const { return OpenSpellSlot; }

	/** The art behind the screen: the Vanguard champion select shows, or null. */
	UTexture2D* GetBackdrop() const;

	/** Champion select's toggle that lays the shown Vanguard's abilities over its art. */
	static FText AbilitiesLabel(bool bShowing);

	/** Every button on screen, in the order built. For tests and scripts. */
	TArray<UVeyraShellButton*> GetButtons() const;

	/**
	 * The button labelled Label, or null; Occurrence picks a later one where several share the label,
	 * as each Flux Spell slot's choices do.
	 */
	UVeyraShellButton* FindButton(const FText& Label, int32 Occurrence = 0) const;

	/** Every text on screen, joined by new lines, for tests. */
	FString DescribeText() const;

	/** Builds the screen's frame, before anything shows it. */
	virtual bool Initialize() override;

protected:
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Rebuild(const FVeyraClientSnapshot& Snapshot);
	void BuildStatus(const FVeyraClientSnapshot& Snapshot);
	void BuildStopped(const FVeyraClientSnapshot& Snapshot);
	void BuildStarterChoice(const FVeyraClientSnapshot& Snapshot);
	void BuildShell(const FVeyraClientSnapshot& Snapshot);
	void BuildHome(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);
	void BuildPlay(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);
	/** The party panel, on every page of the shell while the player has a party (UX §3). */
	void BuildParty(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);
	void BuildMatchFound(const FVeyraClientSnapshot& Snapshot);
	/** Champion select in League's layout (VeyraShellChampionSelect.cpp). */
	void BuildChampionSelect(const FVeyraClientSnapshot& Snapshot);
	UWidget& MakeSelectHeader(const FVeyraSelectModel& Model);
	UWidget& MakeSeatColumn(const FVeyraSelectModel& Model, bool bAllies);
	UWidget& MakeSeatRow(const FVeyraSelectSeatModel& Seat);
	UWidget& MakeCentre(const FVeyraSelectModel& Model);
	UWidget& MakeSelectFooter(const FVeyraSelectModel& Model);
	/** The open Flux Spell slot's picker, over everything. */
	void BuildSpellPicker(const FVeyraSelectModel& Model);
	/** Opens SpellSlot's picker, or closes it when it is open. */
	void OpenSpellPicker(int32 SpellSlot);
	/** Shows Hero behind everything, dimmed; null hides the art. */
	void ShowBackdrop(UTexture2D* Hero);
	/** The countdown's bars, as full as the pick timer is. */
	void UpdatePickBars();
	void BuildReconnectOnly(const FVeyraClientSnapshot& Snapshot);
	void BuildResults(const FVeyraClientSnapshot& Snapshot);
	/** The problem on screen and its Retry, if any. */
	void BuildProblem(const FVeyraClientSnapshot& Snapshot);

	UTextBlock* AddText(UPanelWidget& Parent, const FText& Text, uint8 Role);
	UVeyraShellButton* AddButton(UPanelWidget& Parent, const FText& Label, TFunction<void()> Action, bool bEnabled = true, bool bSelected = false);
	/** A button showing ButtonContent, named Label. */
	UVeyraShellButton* AddContentButton(UPanelWidget& Parent, const FText& Label, UWidget& ButtonContent, TFunction<void()> Action, bool bEnabled,
		bool bSelected);
	void ShowPage(EVeyraShellPage NewPage);

	IVeyraClientIntents* Client = nullptr;
	FDelegateHandle ChangedHandle;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Content;

	/** The shown Vanguard's art behind champion select, scaled to fill the screen. */
	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> BackdropBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Backdrop;

	/** Over everything: champion select's Flux Spell picker. */
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> Popup;

	/** The champion-select countdown's two draining bars, updated every frame. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> PickBars;

	/** Champion select's or Match Found's countdown, updated every frame. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Countdown;

	/** The party panel's time in the queue, updated every frame. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QueueStatus;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVeyraShellButton>> Buttons;

	EVeyraShellScreen Shown = EVeyraShellScreen::None;
	EVeyraShellPage Page = EVeyraShellPage::Home;
	FString ShownSignature;
	/** The pick timer's full length, for its bars; 0 when the backend does not say. */
	double PickSeconds = 0.0;
	int32 OpenSpellSlot = INDEX_NONE;
	bool bShowAbilities = false;
};
