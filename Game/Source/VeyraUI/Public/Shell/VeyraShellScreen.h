// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Shell/VeyraShellModels.h"

#include "VeyraShellScreen.generated.h"

class IVeyraClientIntents;
class UPanelWidget;
class UTextBlock;
class UVerticalBox;
class UVeyraShellButton;

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
	void BuildChampionSelect(const FVeyraClientSnapshot& Snapshot);
	void BuildReconnectOnly(const FVeyraClientSnapshot& Snapshot);
	void BuildResults(const FVeyraClientSnapshot& Snapshot);
	/** The problem on screen and its Retry, if any. */
	void BuildProblem(const FVeyraClientSnapshot& Snapshot);

	UTextBlock* AddText(UPanelWidget& Parent, const FText& Text, uint8 Role);
	UVeyraShellButton* AddButton(UPanelWidget& Parent, const FText& Label, TFunction<void()> Action, bool bEnabled = true, bool bSelected = false);
	void ShowPage(EVeyraShellPage NewPage);

	IVeyraClientIntents* Client = nullptr;
	FDelegateHandle ChangedHandle;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Content;

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
};
