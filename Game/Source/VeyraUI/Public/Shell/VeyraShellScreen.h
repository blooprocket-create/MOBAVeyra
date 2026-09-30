// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Shell/VeyraShellModels.h"
#include "Types/SlateEnums.h"

#include "VeyraShellScreen.generated.h"

class IVeyraClientIntents;
struct FVeyraHistoryOption;
class UEditableTextBox;
class UImage;
class UOverlay;
class UPanelWidget;
class UProgressBar;
class UScaleBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UVeyraShellButton;
enum class EVeyraShellButtonKind : uint8;
class UWidget;

/** A match report's two views (UX-50): the two-team Scoreboard, and Detailed Statistics by category. */
enum class EVeyraReportView : uint8
{
	Scoreboard,
	Details,
};

/** The ordinary pre-game pages the shell's navigation moves between (UX §1). */
enum class EVeyraShellPage : uint8
{
	Home,
	Play,
	/** The player's completed matches (UX-51). */
	History,
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

	/** Which view of a match report the screen shows. */
	EVeyraReportView GetReportView() const { return ReportView; }

	/** Whether the lobby's bot picker shows, for one of its seats. */
	bool IsBotPickerOpen() const { return BotPickerIndex != INDEX_NONE; }

	/** Types Name into the friends panel's name field, as the player would. For tests and scripts. */
	void SetFriendNameDraft(const FString& Name);

	/** The art behind the screen: the Vanguard champion select shows, or null. */
	UTexture2D* GetBackdrop() const;

	/** Champion select's toggle that lays the shown Vanguard's abilities over its art. */
	static FText AbilitiesLabel(bool bShowing);

	/** The settings choice's buttons: keep this device's settings, or the account's. */
	static FText SettingsChoiceLabel(bool bThisDevice);

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
	/** Match History: its filters and list, or an opened match's report (UX-51, UX-64, UX-67). */
	void BuildHistory(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);
	/** A filter's choices as a row of buttons; choosing one reads the first page again with it. */
	void AddHistoryFilter(UPanelWidget& Parent, const TArray<FVeyraHistoryOption>& Options,
		TFunction<void(VeyraBackendProtocol::FHistoryFilter&, const FString&)> Apply);
	/**
	 * The party bar along the bottom of every page of the shell while the player has a party (UX §3;
	 * Art Bible §7.1): its mode and members, and Ready, Find Match and the queue's time.
	 */
	void BuildParty(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);

	/** The shell's bar across the top: the name, the pages (unless bPages is false, as in a lobby), the player and Quit. */
	void BuildTopBar(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent, bool bPages = true);

	/**
	 * A custom lobby (ADR-021; VeyraShellLobby.cpp), laid out as League's: both sides' seats with their
	 * humans and bots, the session's rules, Start and Leave, and the friends panel.
	 */
	void BuildLobby(const FVeyraClientSnapshot& Snapshot);
	/** One side's column of seats. */
	UWidget& MakeLobbySide(const FText& Title, const TArray<FVeyraLobbySeatModel>& Seats);
	UWidget& MakeLobbySeat(const FVeyraLobbySeatModel& Seat);
	/** Victory and starting Gold: the host's choices, or what the host chose. */
	void BuildLobbyRules(const FVeyraLobbyModel& Model, UPanelWidget& Parent);
	/** The bot picker over the lobby, for the seat it was opened on. */
	void BuildBotPicker(const FVeyraClientSnapshot& Snapshot);
	/** Opens the bot picker on a seat at Difficulty, or closes it when it is open there. */
	void OpenBotPicker(const FString& Side, int32 Index, const FString& Difficulty);
	/** The friends panel down the right (Parties & Social Bible §1; Art Bible §7): add by name, requests, invitations and friends. */
	void BuildFriends(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);
	/** Sends the friend request the name field holds. */
	void SubmitFriendName();

	UFUNCTION()
	void HandleFriendNameChanged(const FText& Text);

	UFUNCTION()
	void HandleFriendNameCommitted(const FText& Text, ETextCommit::Type Method);

	/**
	 * A card showing VanguardId's illustration with a plate of text along its bottom, as a button named
	 * Label; dimmed when it cannot be chosen.
	 */
	UVeyraShellButton* AddArtCard(UPanelWidget& Parent, const FText& Label, const FString& VanguardId, const TArray<TPair<FText, uint8>>& Plate,
		TFunction<void()> Action, bool bEnabled, bool bSelected);

	/** Centres Child across the screen, DialogWidth wide, with space above and below. */
	void AddCentred(UWidget& Child);

	/** Shows VanguardId's illustration behind the screen as a showcase, with the scrims that let text read over it. */
	void ShowShowcase(const FString& VanguardId);

	/** A Vanguard with art, chosen at random once each time the game runs; HomeVanguard when none has art. */
	static FString PickFeaturedVanguard();
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
	/** A match's Scoreboard or Detailed Statistics, as the report view says, with the switch between them (UX-50). */
	void BuildReport(const FVeyraMatchReport& Report, UPanelWidget& Parent);
	void BuildScoreboard(const FVeyraMatchReport& Report, UPanelWidget& Parent);
	void BuildDetails(const FVeyraMatchReport& Report, UPanelWidget& Parent);
	/** A text in a column Width wide. */
	UTextBlock* AddCell(UPanelWidget& Row, const FText& Text, float Width, uint8 Role);
	void ShowReportView(EVeyraReportView NewView);
	/** The problem on screen and its Retry, if any. */
	void BuildProblem(const FVeyraClientSnapshot& Snapshot);
	/** The choice between this device's settings and the account's, over everything, while it waits (ADR-024 §1). */
	void BuildSettingsConflict(const FVeyraClientSnapshot& Snapshot);

	UTextBlock* AddText(UPanelWidget& Parent, const FText& Text, uint8 Role);
	UVeyraShellButton* AddButton(UPanelWidget& Parent, const FText& Label, TFunction<void()> Action, bool bEnabled = true, bool bSelected = false);

	/** A button of Kind; as AddButton otherwise. */
	UVeyraShellButton* AddKindButton(UPanelWidget& Parent, EVeyraShellButtonKind Kind, const FText& Label, TFunction<void()> Action, bool bEnabled = true,
		bool bSelected = false);
	/** A button showing ButtonContent, named Label. */
	UVeyraShellButton* AddContentButton(UPanelWidget& Parent, const FText& Label, UWidget& ButtonContent, TFunction<void()> Action, bool bEnabled,
		bool bSelected);
	/** A button of Kind showing ShownText on one line, named Label (UVeyraShellButton::MakeKindNamed). */
	UVeyraShellButton* AddNamedButton(UPanelWidget& Parent, EVeyraShellButtonKind Kind, const FText& Label, const FText& ShownText, TFunction<void()> Action,
		bool bEnabled = true, bool bSelected = false);
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

	/** Over a showcase's art, so text reads on it: from the left edge, and from the bottom. */
	UPROPERTY(Transient)
	TObjectPtr<UImage> ScrimLeft;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ScrimBottom;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ScrimTop;

	/** The Vanguard whose art the showcases show this time the game runs, chosen at random. */
	FString FeaturedVanguard;

	/** The scrims' gradients, kept for as long as the screen shows them. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> Gradients;

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
	EVeyraReportView ReportView = EVeyraReportView::Scoreboard;

	/** The seat the lobby's bot picker shows for, and the difficulty chosen in it; INDEX_NONE while it is closed. */
	FString BotPickerSide;
	int32 BotPickerIndex = INDEX_NONE;
	FString BotDifficulty;

	/** The friends panel's name field, rebuilt with the screen; what it holds outlives it. */
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> FriendNameBox;

	FString FriendNameDraft;
};
