// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Shell/VeyraChatModels.h"
#include "Shell/VeyraProgressionModels.h"
#include "Shell/VeyraShellModels.h"
#include "Types/SlateEnums.h"

#include "VeyraShellScreen.generated.h"

class IVeyraClientIntents;
struct FVeyraHistoryOption;
struct FVeyraProfileCardModel;
class UEditableTextBox;
class UImage;
class UOverlay;
class UPanelWidget;
class UProgressBar;
class UScaleBox;
class UScrollBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UVeyraSettingsScreen;
class UVeyraSettingsSubsystem;
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
	/** Every released Vanguard, with the player's ownership and Mastery, and Buy (ADR-045 §8). */
	Collection,
	/** The player's own profile as others see it, and its choices (ADR-048 §5). */
	Profile,
};

/** What a card's action asks the player to confirm before it is sent (UX-11; ADR-044 §4). */
enum class EVeyraShellConfirm : uint8
{
	None,
	/** Make Party Leader, on a member's card. */
	PartyLeader,
	/** Block, on a friend's card or a friend request. */
	Block,
	/** Buy, on a Vanguard's Collection card: the price and currency named (Account, Collection & Mastery Bible §7). */
	Purchase,
	/** A display-name change, on the Profile page: the price named, and the old name free to anyone at once (ADR-049 §6). */
	NameChange,
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
/** What a match found did to ask for the player's attention (SET-50, SET-71; ADR-053 §2). */
struct FVeyraMatchFoundAlert
{
	/** The player allows the taskbar to draw attention to a client in the background. */
	bool bAttentionAllowed = false;
	/** The match-ready sound played. */
	bool bSound = false;
};

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

	/**
	 * Opens the Settings screen over the shell, where the top bar or the results offer it: in the shell,
	 * the lobby and the results, never in champion select, Match Found or Reconnect-only (ADR-024 §4).
	 */
	void OpenSettings();
	void CloseSettings();
	bool IsSettingsOpen() const { return SettingsScreen != nullptr; }
	UVeyraSettingsScreen* GetSettingsScreen() const { return SettingsScreen; }

	/** The button that opens Settings. */
	static FText SettingsLabel();

	/** Tests: the settings the screen opens, in place of its game instance's. */
	void SetSettingsForTests(UVeyraSettingsSubsystem* Settings);

	/** Whether the lobby's bot picker shows, for one of its seats. */
	bool IsBotPickerOpen() const { return BotPickerIndex != INDEX_NONE; }

	/** Types Name into the friends panel's name field, as the player would. For tests and scripts. */
	void SetFriendNameDraft(const FString& Name);

	/** Types Text into the chat composer that shows, as the player would. For tests and scripts. */
	void SetChatDraft(const FString& Text);

	/** Types Text into the open report form's details, as the player would. For tests and scripts. */
	void SetReportDetailsDraft(const FString& Text);

	/** The player whose menu shows on the report, empty while none does (UX-57). */
	const FString& GetOpenPlayerMenu() const { return OpenPlayerMenu; }

	/** The Profile page's choices as the player is making them, before Save (ADR-048 §5). */
	const VeyraBackendProtocol::FProfileSettings& GetProfileDraft() const { return ProfileDraft; }

	/** Types Name into the new-name field that shows, as the player would. For tests and scripts. */
	void SetNameDraft(const FString& Name);

	/** The chat composer that shows, or null. */
	UEditableTextBox* GetChatBox() const { return ChatBox; }

	/** Champion select's composer's recipient, "Team" or "Party"; empty where the composer names none. */
	FText GetChatRecipient() const;

	/**
	 * The card whose actions show, empty while none is open (ADR-044 §2): a party member's card is its
	 * MemberCardKey, a friend's its FriendCardKey, so one player's two cards open apart.
	 */
	const FString& GetOpenCard() const { return OpenCardId; }
	static FString MemberCardKey(const FString& AccountId) { return TEXT("member:") + AccountId; }
	static FString FriendCardKey(const FString& AccountId) { return TEXT("friend:") + AccountId; }
	/** A Vanguard's card on the Collection page. */
	static FString CollectionCardKey(const FString& VanguardId) { return TEXT("collection:") + VanguardId; }

	/** How many times a draft turn of the player's own asked for their attention (UX-31, UX-32). For tests. */
	int32 GetTurnAttentionCount() const { return TurnAttentions; }

	/** How many matches found asked for the player's attention, and what the last one did (SET-50, SET-71; ADR-053 §2). For tests. */
	int32 GetMatchFoundAlertCount() const { return MatchFoundAlerts; }
	const FVeyraMatchFoundAlert& GetLastMatchFoundAlert() const { return LastMatchFoundAlert; }

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
	 * The Collection (VeyraShellCollection.cpp; ADR-045 §8): every released Vanguard as a card, owned or not, with the
	 * player's Mastery; an opened card's detail and Buy in either currency, each behind a confirmation naming its price.
	 */
	void BuildCollection(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);
	/** The opened card's detail and its Buy actions, or the purchase's question in their place. */
	void BuildCollectionDetail(const FVeyraClientSnapshot& Snapshot, const FVeyraCollectionCard& Card, UPanelWidget& Parent);
	/** The top bar's account readout: the Account Level, its XP and the account currencies. */
	void AddProgressionReadout(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Bar);
	/** What the match gave the player, on the results screen, apart from its own Gold and XP. */
	void BuildRewards(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);
	/**
	 * The party bar along the bottom of every page of the shell while the player has a party (UX §3;
	 * Art Bible §7.1): its mode and members, and Ready, Find Match and the queue's time.
	 */
	void BuildParty(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);

	/** The shell's bar across the top: the name, the pages (unless bPages is false, as in a lobby), the player and Quit. */
	void BuildTopBar(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent, bool bPages = true);

	/**
	 * A custom lobby (ADR-021; VeyraShellLobby.cpp): both sides' seats with their
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
	/** A friend's line: their card, which opens its actions, and the one action it always offers. */
	void BuildFriend(const FVeyraFriendModel& Friend, UPanelWidget& Parent);
	/** A block's question and its answers, in place of the actions that asked it. */
	void AddBlockConfirmation(UPanelWidget& Parent, const FString& AccountId, const FString& Name);
	/** Opens the card Key names (MemberCardKey, FriendCardKey), or closes it when it is open; either forgets a confirmation. */
	void OpenCard(const FString& Key);
	/** Asks the player to confirm Kind for AccountId, or withdraws the question with None. */
	void AskToConfirm(EVeyraShellConfirm Kind, const FString& AccountId);
	/** Forgets the open card and its confirmation, as once its action is sent. */
	void CloseCard();

	UFUNCTION()
	void HandleFriendNameChanged(const FText& Text);

	UFUNCTION()
	void HandleFriendNameCommitted(const FText& Text, ETextCommit::Type Method);

	/** A text field in the shell's style, holding Draft; dimmed when it cannot be used. */
	UEditableTextBox* MakeTextField(const FText& Hint, const FString& Draft, bool bEnabled);
	/** The sidebar's chat under the friends (VeyraShellChat.cpp; ADR-046 §6): the direct conversation the player opened, else Party Chat. */
	void BuildSidebarChat(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);
	/** One chat panel: its title, its lines and its composer, which sends to the model's conversation. */
	void BuildChatPanel(const FVeyraChatPanelModel& Model, UPanelWidget& Parent);
	/** Sends what the chat composer holds to its conversation. */
	void SubmitChat();
	/** Champion select's compact chat beside the ally column (UX-33), or its Show Chat once collapsed. */
	void BuildSelectChat(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);
	/** The results screen's optional post-match chat beside the report (UX-59–60). */
	void BuildPostMatchChat(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);

	UFUNCTION()
	void HandleChatChanged(const FText& Text);

	UFUNCTION()
	void HandleChatCommitted(const FText& Text, ETextCommit::Type Method);

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
	/** Champion select (VeyraShellChampionSelect.cpp). */
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
	void BuildReport(const FVeyraClientSnapshot& Snapshot, const FVeyraMatchReport& Report, UPanelWidget& Parent);
	/** The scoreboard; another human's name opens their player menu beneath their line (UX-57). */
	void BuildScoreboard(const FVeyraClientSnapshot& Snapshot, const FVeyraMatchReport& Report, UPanelWidget& Parent);
	/**
	 * A player menu (VeyraShellConduct.cpp; ADR-047 §5): Add Friend, Invite to Party, Commend and Report, as the
	 * screen and the player's record allow, or the report form once Report is pressed.
	 */
	void BuildPlayerMenu(const FVeyraClientSnapshot& Snapshot, const FString& Name, UPanelWidget& Parent);
	/** The report form: the backend's reasons, optional details within their limit, Submit and Cancel. */
	void BuildReportForm(const FVeyraClientSnapshot& Snapshot, const FString& Name, UPanelWidget& Parent);
	/** Opens Name's player menu, or closes it when it is open; either closes a report form. */
	void TogglePlayerMenu(const FString& Name);

	UFUNCTION()
	void HandleReportDetailsChanged(const FText& Text);

	/**
	 * The Profile page (VeyraShellProfile.cpp; ADR-048 §5): the player's profile as others see it, and pickers for
	 * the icon, the background and the featured Vanguard, the Match History toggle and Save.
	 */
	void BuildProfilePage(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);
	/** Another player's profile over the screen, with its shared Match History, until Close. */
	void BuildProfileOverlay(const FVeyraClientSnapshot& Snapshot);
	/** A profile's card: its icon, name, level and featured Vanguard over its background. */
	void AddProfileCard(const FVeyraProfileCardModel& Card, UPanelWidget& Parent);
	/** The Profile page's Display Name section: the name, the next change's price and cooldown, and the change behind a confirmation. */
	void BuildDisplayName(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);
	/** In place of the shell, for an account whose name another player claimed: choose a new one, for free (ADR-049 §4). */
	void BuildChooseName(const FVeyraClientSnapshot& Snapshot);

	UFUNCTION()
	void HandleNameChanged(const FText& Text);
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

	/** Over the popups: the Settings screen, which a rebuild of the rest leaves open. */
	UPROPERTY(Transient)
	TObjectPtr<UOverlay> SettingsLayer;

	UPROPERTY(Transient)
	TObjectPtr<UVeyraSettingsScreen> SettingsScreen;

	TWeakObjectPtr<UVeyraSettingsSubsystem> TestSettings;

	/** The settings Settings opens: the game instance's, or a test's; null where there are none. */
	UVeyraSettingsSubsystem* FindSettings() const;

	/** The Settings button, where the screen offers it. */
	void AddSettingsButton(UPanelWidget& Parent);

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

	/** The chat composer, rebuilt with the screen, and the conversation it sends to (ADR-046 §6). */
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> ChatBox;

	/** A chat panel's lines built since the last frame, which the next frame scrolls to the newest. */
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ChatScroll;

	VeyraBackendProtocol::EChatKind ChatBoxKind = VeyraBackendProtocol::EChatKind::Party;
	FString ChatBoxTarget;
	/** Each conversation's unsent text, so changing conversation, or a rebuild, keeps it. */
	FString ChatBoxKey;
	TMap<FString, FString> ChatDrafts;

	/** Champion select's composer's recipient, Team or Party, which follows the draft as it is typed (UX-34). */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ChatRecipient;

	/** The player collapsed champion select's chat panel (UX-33). */
	bool bSelectChatHidden = false;

	/** What came of the player's last command in the post-match chat, such as a mute; empty for none. */
	FText ChatNotice;

	/**
	 * The match the player menus are of, the player whose menu shows, and the report form's player, reason
	 * and details: one report screen's, forgotten when it shows another match (ADR-047 §5).
	 */
	FString PlayerMenuMatch;
	FString OpenPlayerMenu;

	/** The Profile page's choices before Save, and the saved choices they began from (ADR-048 §5). */
	VeyraBackendProtocol::FProfileSettings ProfileDraft;
	/** The new-name field, rebuilt with the screen; what it holds outlives it. */
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> NameBox;
	FString NameDraft;
	VeyraBackendProtocol::FProfileSettings ProfileDraftBase;
	bool bProfileDraftReady = false;
	FString ReportFormName;
	FString ReportReason;
	FString ReportDetailsDraft;

	/** The report form's details field and its count, rebuilt with the screen. */
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> ReportDetailsBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReportDetailsCount;

	/** The card whose actions show, and the confirmation one of them asked, by account (ADR-044 §2, §4). */
	FString OpenCardId;
	EVeyraShellConfirm Confirm = EVeyraShellConfirm::None;
	FString ConfirmId;

	/** The player's draft turn that last asked for attention, and how many have (VeyraShellModels::PlayersTurn). */
	FString AttendedTurn;
	int32 TurnAttentions = 0;

	/** Once as the player's draft turn begins: the window asks to come forward, or draws attention, and the cue plays. */
	void DrawTurnAttention();

	/** The match found that last asked for attention, by its ID, how many have, and what the last did. */
	FString AttendedMatchFound;
	int32 MatchFoundAlerts = 0;
	FVeyraMatchFoundAlert LastMatchFoundAlert;

	/** The break reminder, dismissible, where it applies (ADR-053 §4). */
	void AddPlayReminder(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent);

	/** Whether the break reminder shows now, as the player set it. */
	bool ShowsPlayReminder(const FVeyraClientSnapshot& Snapshot) const;

	/** Once as a match is found: the taskbar draws attention to a background client, and the match-ready sound plays, as the player allows. */
	void AnnounceMatchFound();

	/** Asks the window, if it is not the active one, to draw attention until activated; first to come forward when bBringToFront. */
	void DrawWindowAttention(bool bBringToFront);

	/** Plays one brief cue made of TonesHz in turn, each ToneSeconds long and fading in and out, at Volume. */
	void PlayCue(TConstArrayView<float> TonesHz, float ToneSeconds, float Volume);
};
