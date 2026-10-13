// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellLook.h"
#include "Shell/VeyraShellBrushes.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Client/VeyraClientIntents.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "Sound/SoundWaveProcedural.h"
#include "Widgets/SWindow.h"
#include "Settings/VeyraSettingsScreen.h"
#include "Shell/VeyraConductModels.h"
#include "Shell/VeyraMatchHistoryModel.h"
#include "Shell/VeyraProfileModels.h"
#include "Shell/VeyraShellArt.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellMotes.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Text/VeyraContentText.h"
#include "VeyraSettingsSubsystem.h"

#define LOCTEXT_NAMESPACE "VeyraShell"

namespace
{
	using VeyraShellStyle::EVeyraShellText;
	using VeyraShellStyle::EVeyraShellSurface;

	const UVeyraShellStyleSettings& ShellStyle()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	uint8 RoleOf(EVeyraShellText Role)
	{
		return static_cast<uint8>(Role);
	}

	/** Adds Child to Column so that it takes the room the column has left. */
	void AddFilling(UPanelWidget& Parent, UWidget& Child)
	{
		if (UVerticalBox* Column = Cast<UVerticalBox>(&Parent))
		{
			Column->AddChildToVerticalBox(&Child)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		else if (UHorizontalBox* Row = Cast<UHorizontalBox>(&Parent))
		{
			UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(&Child);
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		else
		{
			Parent.AddChild(&Child);
		}
	}

	/** Room that stretches, so what follows it sits at the far end of its box. */
	void AddStretch(UWidgetTree& Tree, UPanelWidget& Parent)
	{
		AddFilling(Parent, *Tree.ConstructWidget<USpacer>(USpacer::StaticClass()));
	}

	/** A fixed gap of Size slate units in both directions. */
	void AddGap(UWidgetTree& Tree, UPanelWidget& Parent, float Size)
	{
		USpacer* Gap = Tree.ConstructWidget<USpacer>(USpacer::StaticClass());
		Gap->SetSize(FVector2D(Size, Size));
		Parent.AddChild(Gap);
	}

	/** Whether the player's side won, lost or neither, for the results' headline colour. */
	FLinearColor HeadlineColorOf(const FVeyraClientSnapshot& Snapshot)
	{
		const UVeyraShellStyleSettings& Style = ShellStyle();
		if (!Snapshot.Result.IsSet() || !Snapshot.Result->bHasResult || Snapshot.Result->Winner.IsEmpty())
		{
			return Style.TextColor;
		}
		const bool bWon = Snapshot.Result->Winner == Snapshot.Result->Side && !Snapshot.Result->bPersonalLoss;
		return bWon ? Style.VictoryColor : Style.DefeatColor;
	}
}

bool UVeyraShellScreen::Initialize()
{
	const bool bFirst = Super::Initialize();
	if (bFirst && WidgetTree && !WidgetTree->RootWidget)
	{
		const UVeyraShellStyleSettings& Style = ShellStyle();
		// In layers: the background, a Vanguard's art over it, the scrims that let text read over the art,
		// the content, and champion select's picker over everything.
		UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		const auto Fill = [Root](UWidget* Layer) {
			UOverlaySlot* Layered = Root->AddChildToOverlay(Layer);
			Layered->SetHorizontalAlignment(HAlign_Fill);
			Layered->SetVerticalAlignment(VAlign_Fill);
		};
		Fill(VeyraShellStyle::MakeBorder(*WidgetTree, Style.BackgroundColor, 0.0f));
		BackdropBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		BackdropBox->SetStretch(EStretch::ScaleToFill);
		Backdrop = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		BackdropBox->AddChild(Backdrop);
		BackdropBox->SetVisibility(ESlateVisibility::Collapsed);
		Fill(BackdropBox);
		UTexture2D* FromLeft = VeyraShellStyle::CreateGradient(VeyraShellStyle::EVeyraShellGradient::FromLeft);
		UTexture2D* FromBottom = VeyraShellStyle::CreateGradient(VeyraShellStyle::EVeyraShellGradient::FromBottom);
		UTexture2D* FromTop = VeyraShellStyle::CreateGradient(VeyraShellStyle::EVeyraShellGradient::FromTop);
		UTexture2D* FromRight = VeyraShellStyle::CreateGradient(VeyraShellStyle::EVeyraShellGradient::FromRight);
		Gradients = { FromLeft, FromBottom, FromTop, FromRight };
		ScrimLeft = VeyraShellStyle::MakeGradient(*WidgetTree, FromLeft, Style.ScrimColor);
		ScrimBottom = VeyraShellStyle::MakeGradient(*WidgetTree, FromBottom, Style.ScrimColor);
		ScrimTop = VeyraShellStyle::MakeGradient(*WidgetTree, FromTop, Style.ScrimColor);
		ScrimRight = VeyraShellStyle::MakeGradient(*WidgetTree, FromRight, Style.ScrimColor);
		for (UImage* Scrim : { ScrimLeft.Get(), ScrimBottom.Get(), ScrimTop.Get(), ScrimRight.Get() })
		{
			Scrim->SetVisibility(ESlateVisibility::Collapsed);
			Fill(Scrim);
		}
		// Flux motes drifting behind the menus, as on the website (ADR-082 §7).
		Motes = WidgetTree->ConstructWidget<UVeyraShellMotes>(UVeyraShellMotes::StaticClass());
		Motes->SetVisibility(ESlateVisibility::Collapsed);
		Fill(Motes);
		FeaturedVanguard = PickFeaturedVanguard();
		UBorder* Frame = VeyraShellStyle::MakeBorder(*WidgetTree, FLinearColor::Transparent, Style.ScreenPadding);
		Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Frame->SetContent(Content);
		Fill(Frame);
		Popup = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		// Empty, it lets clicks through to the content under it.
		Popup->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Fill(Popup);
		SettingsLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		SettingsLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Fill(SettingsLayer);
		WidgetTree->RootWidget = Root;
		// The shell's input mode gives the screen keyboard focus.
		SetIsFocusable(true);
	}
	return bFirst;
}

void UVeyraShellScreen::Bind(IVeyraClientIntents& InClient)
{
	if (Client)
	{
		Client->OnChanged().Remove(ChangedHandle);
	}
	Client = &InClient;
	ChangedHandle = Client->OnChanged().AddUObject(this, &UVeyraShellScreen::Refresh);
	ShownSignature.Reset();
	// The game instance's settings, unless the screen already follows a store of its own.
	UVeyraSettingsSubsystem* Settings = UVeyraSettingsSubsystem::Get(this);
	if (Settings && !SettingsStore)
	{
		BindSettings(Settings->GetStore());
	}
	Refresh();
}

void UVeyraShellScreen::BindSettings(FVeyraSettingsStore& Store)
{
	if (SettingsStore)
	{
		SettingsStore->OnChanged.Remove(SettingsHandle);
	}
	SettingsStore = &Store;
	// The signature reads the settings shown, so a change to any other redraws nothing.
	SettingsHandle = Store.OnChanged.AddWeakLambda(this, [this](const FVeyraContentId& /*Id*/) { Refresh(); });
	Refresh();
}

FVeyraInterfacePreferences UVeyraShellScreen::InterfacePreferences() const
{
	return VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), SettingsStore ? SettingsStore : VeyraInterfacePreferences::StoreOf(this));
}

void UVeyraShellScreen::Unbind()
{
	if (Client)
	{
		Client->OnChanged().Remove(ChangedHandle);
	}
	ChangedHandle.Reset();
	Client = nullptr;
	if (SettingsStore)
	{
		SettingsStore->OnChanged.Remove(SettingsHandle);
	}
	SettingsHandle.Reset();
	SettingsStore = nullptr;
}

void UVeyraShellScreen::NativeDestruct()
{
	// A screen taken off the viewport stops listening.
	Unbind();
	Super::NativeDestruct();
}

void UVeyraShellScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// A chat panel built since the last frame shows its newest lines, once its scroll box exists to scroll.
	if (ChatScroll)
	{
		ChatScroll->ScrollToEnd();
		ChatScroll = nullptr;
	}
	if (!Client)
	{
		return;
	}

	if (Countdown && Shown == EVeyraShellScreen::ChampionSelect)
	{
		Countdown->SetText(VeyraShellModels::FormatCountdown(Client->GetRemainingPickSeconds()));
		UpdatePickBars();
	}
	else if (Countdown && Shown == EVeyraShellScreen::MatchFound)
	{
		Countdown->SetText(VeyraShellModels::FormatCountdown(Client->GetRemainingAcceptSeconds()));
	}
	if (QueueStatus && Shown == EVeyraShellScreen::Shell)
	{
		QueueStatus->SetText(VeyraShellModels::FormatQueueStatus(Client->GetQueuedSeconds()));
	}
}

void UVeyraShellScreen::Refresh()
{
	if (!Client || !Content)
	{
		return;
	}
	const FVeyraClientSnapshot& Snapshot = Client->GetSnapshot();
	// Built in the player's look, and rebuilt when it changes (ADR-055 §2–§3).
	const FVeyraInterfacePreferences Preferences = InterfacePreferences();
	const bool bNewLook = VeyraShellLook::Use(VeyraShellLook::For(Preferences));
	if (bNewLook)
	{
		ShownSignature.Reset();
	}
	// A draft turn of the player's own asks for their attention once, as it begins (UX-31, UX-32).
	if (const FString Turn = VeyraShellModels::PlayersTurn(Snapshot); Turn != AttendedTurn)
	{
		AttendedTurn = Turn;
		if (!Turn.IsEmpty())
		{
			DrawTurnAttention();
		}
	}
	// A match found asks for attention once, as it arrives (SET-50, SET-71; ADR-053 §2).
	if (const FString Found = Snapshot.State == EVeyraClientState::MatchFound ? Snapshot.MatchFound.Id : FString(); Found != AttendedMatchFound)
	{
		AttendedMatchFound = Found;
		if (!Found.IsEmpty())
		{
			AnnounceMatchFound();
		}
	}
	const FString Signature = FString::Printf(TEXT("page %d|spells %d|abilities %d|report %d|bots %s%d:%s|card %s|confirm %d:%s|select chat %d|menu %s|form %s:%s|block %s|"),
								  static_cast<int32>(Page), OpenSpellSlot, bShowAbilities ? 1 : 0, static_cast<int32>(ReportView), *BotPickerSide, BotPickerIndex,
								  *BotDifficulty, *OpenCardId, static_cast<int32>(Confirm), *ConfirmId, bSelectChatHidden ? 1 : 0, *OpenPlayerMenu, *ReportFormName,
								  *ReportReason, *BlockConfirmName) +
		// The Profile page's choices before Save (ADR-048 §5).
		FString::Printf(TEXT("profile draft %d:%s:%s:%s:%d|"), bProfileDraftReady ? 1 : 0, *ProfileDraft.Icon, *ProfileDraft.Background,
			*ProfileDraft.FeaturedVanguardId, ProfileDraft.bShowMatchHistory ? 1 : 0) +
		// The break reminder comes and goes with the player's setting as well as the snapshot (ADR-053 §4).
		FString::Printf(TEXT("reminder %d|"), ShowsPlayReminder(Snapshot) ? 1 : 0) +
		// The seats' side colours follow the player's colour vision (ADR-055 §1).
		FString::Printf(TEXT("sides %s:%s|"), *Preferences.SideColors.Ally.ToString(), *Preferences.SideColors.Enemy.ToString()) +
		VeyraShellModels::Signature(Snapshot);
	if (Signature == ShownSignature)
	{
		return;
	}
	ShownSignature = Signature;
	Rebuild(Snapshot);
}

void UVeyraShellScreen::DrawTurnAttention()
{
	++TurnAttentions;
	// Asked for once, never held: the window comes forward, and where the OS refuses it the taskbar
	// draws attention until the player comes back (UX-31). The timer never waits for them.
	DrawWindowAttention(/*bBringToFront*/ true);
	// One brief, distinct cue (UX-32).
	const UVeyraShellStyleSettings& Style = ShellStyle();
	PlayCue(Style.TurnCueTonesHz, Style.TurnCueToneSeconds, Style.TurnCueVolume);
}

bool UVeyraShellScreen::ShowsPlayReminder(const FVeyraClientSnapshot& Snapshot) const
{
	const FVeyraInterfacePreferences Preferences = InterfacePreferences();
	return VeyraShellModels::ShowsPlayReminder(Snapshot, ShellStyle().PlayReminderSeconds.FindRef(Preferences.PlayReminder));
}

void UVeyraShellScreen::AddPlayReminder(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	// Optional and dismissible; it never locks anything or costs anything (Account Bible §2).
	if (!ShowsPlayReminder(Snapshot))
	{
		return;
	}
	const UVeyraShellStyleSettings& Style = ShellStyle();
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* Words = VeyraShellStyle::MakeText(*WidgetTree, VeyraShellModels::PlayReminderText(Snapshot.PlayedSeconds), EVeyraShellText::Body);
	Words->SetAutoWrapText(true);
	UHorizontalBoxSlot* WordsSlot = Row->AddChildToHorizontalBox(Words);
	WordsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	WordsSlot->SetVerticalAlignment(VAlign_Center);
	UVeyraShellButton* Dismiss = UVeyraShellButton::MakeKind(*WidgetTree, EVeyraShellButtonKind::Secondary, LOCTEXT("DismissReminder", "Dismiss"),
		[this] { Client->DismissPlayReminder(); }, Client->CanIssue(EVeyraClientIntent::DismissPlayReminder));
	Buttons.Add(Dismiss);
	Row->AddChildToHorizontalBox(Dismiss)->SetVerticalAlignment(VAlign_Center);
	Panel->SetContent(Row);
	VeyraShellStyle::AddSpaced(Parent, *Panel);
}

void UVeyraShellScreen::AnnounceMatchFound()
{
	++MatchFoundAlerts;
	const FVeyraInterfacePreferences Preferences = InterfacePreferences();
	LastMatchFoundAlert = FVeyraMatchFoundAlert{ Preferences.bBackgroundMatchNotification, Preferences.bMatchReadySound, Preferences.bReduceFlashing };
	// Taskbar attention only: the client never takes focus for itself, accepts, or tells anything of the party (SET-50).
	if (Preferences.bBackgroundMatchNotification)
	{
		DrawWindowAttention(/*bBringToFront*/ false);
	}
	// The sound in the foreground or the background; turned off, the prompt and the attention stay (SET-71).
	if (Preferences.bMatchReadySound)
	{
		const UVeyraShellStyleSettings& Style = ShellStyle();
		PlayCue(Style.MatchReadyCueTonesHz, Style.MatchReadyCueToneSeconds, Style.MatchReadyCueVolume);
	}
}

void UVeyraShellScreen::DrawWindowAttention(bool bBringToFront)
{
	const TSharedPtr<SWidget> Widget = GetCachedWidget();
	if (!FSlateApplication::IsInitialized() || !Widget.IsValid())
	{
		return;
	}
	if (const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()))
	{
		if (bBringToFront)
		{
			Window->BringToFront(/*bForce*/ true);
		}
		if (Window->IsActive())
		{
			return;
		}
		// Under Reduce Flashing the taskbar button lights once and stays lit, rather than flashing until activated (SET-18).
		if (InterfacePreferences().bReduceFlashing)
		{
#if PLATFORM_WINDOWS
			if (const TSharedPtr<FGenericWindow> Native = Window->GetNativeWindow(); Native.IsValid() && Native->GetOSWindowHandle())
			{
				FLASHWINFO Light = { sizeof(FLASHWINFO), static_cast<HWND>(Native->GetOSWindowHandle()), FLASHW_TRAY, 1, 0 };
				::FlashWindowEx(&Light);
			}
#endif
			return;
		}
		Window->DrawAttention(FWindowDrawAttentionParameters(EWindowDrawAttentionRequestType::UntilActivated));
	}
}

void UVeyraShellScreen::PlayCue(TConstArrayView<float> TonesHz, float ToneSeconds, float Volume)
{
	// Made from the tones as it plays: each fades in and out over its length, so it neither clicks nor rings on.
	UWorld* World = GetWorld();
	if (!World || !FApp::CanEverRenderAudio() || TonesHz.IsEmpty())
	{
		return;
	}
	constexpr int32 SampleRate = UVeyraShellStyleSettings::TurnCueSampleRate;
	const int32 ToneSamples = FMath::Max(1, FMath::RoundToInt32(ToneSeconds * SampleRate));
	TArray<int16> Samples;
	Samples.Reserve(ToneSamples * TonesHz.Num());
	for (const float Hz : TonesHz)
	{
		for (int32 Index = 0; Index < ToneSamples; ++Index)
		{
			const float Envelope = FMath::Sin(UE_PI * Index / ToneSamples);
			Samples.Add(static_cast<int16>(MAX_int16 * Envelope * FMath::Sin(UE_TWO_PI * Hz * Index / SampleRate)));
		}
	}
	USoundWaveProcedural* Cue = NewObject<USoundWaveProcedural>(this, NAME_None, RF_Transient);
	Cue->SetSampleRate(SampleRate);
	Cue->NumChannels = 1;
	Cue->Duration = static_cast<float>(Samples.Num()) / SampleRate;
	Cue->bLooping = false;
	Cue->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
	UGameplayStatics::PlaySound2D(World, Cue, Volume);
}

void UVeyraShellScreen::Rebuild(const FVeyraClientSnapshot& Snapshot)
{
	// A player typing a friend's name keeps typing across a rebuild, into the field that replaces it.
	const bool bRefocusFriendName = FriendNameBox && FriendNameBox->HasKeyboardFocus();
	FriendNameBox = nullptr;
	// The same for a player typing a chat message while lines arrive (ADR-046 §6).
	const bool bRefocusChat = ChatBox && ChatBox->HasKeyboardFocus();
	ChatBox = nullptr;
	// And for a player typing a new name.
	const bool bRefocusName = NameBox && NameBox->HasKeyboardFocus();
	NameBox = nullptr;
	// And for a player writing a report's details.
	const bool bRefocusReport = ReportDetailsBox && ReportDetailsBox->HasKeyboardFocus();
	ReportDetailsBox = nullptr;
	// And for a player searching a roster (ADR-058 §2).
	const bool bRefocusRosterSearch = RosterSearchBox && RosterSearchBox->HasKeyboardFocus();
	RosterSearchBox = nullptr;
	RosterCards.Reset();
	RosterNoMatch = nullptr;
	ReportDetailsCount = nullptr;
	ChatScroll = nullptr;
	ChatRecipient = nullptr;
	Content->ClearChildren();
	Popup->ClearChildren();
	Buttons.Reset();
	PickBars.Reset();
	Countdown = nullptr;
	QueueStatus = nullptr;
	PageBody = nullptr;
	Shown = VeyraShellModels::ScreenFor(Snapshot.State);
	// Settings belong to the shell, the lobby and the results, never to champion select, Match Found or
	// Reconnect-only (ADR-024 §4); the choice between this device's settings and the account's comes first.
	const bool bSettingsHere = Shown == EVeyraShellScreen::Shell || Shown == EVeyraShellScreen::Lobby || Shown == EVeyraShellScreen::Results;
	if (!bSettingsHere || Snapshot.bSettingsConflict)
	{
		CloseSettings();
	}
	if (Shown != EVeyraShellScreen::ChampionSelect)
	{
		// The picker and the abilities belong to one select.
		OpenSpellSlot = INDEX_NONE;
		bShowAbilities = false;
	}
	if (Shown != EVeyraShellScreen::Lobby)
	{
		// The bot picker belongs to one lobby.
		BotPickerIndex = INDEX_NONE;
	}
	if (Shown != EVeyraShellScreen::Shell && Shown != EVeyraShellScreen::Lobby)
	{
		// Cards and their confirmations belong to the party and friends panels, which only those show.
		OpenCardId.Reset();
		Confirm = EVeyraShellConfirm::None;
		ConfirmId.Reset();
	}
	if (Shown != EVeyraShellScreen::Results)
	{
		// The post-match chat, its notice and its draft belong to one results screen (UX-60).
		ChatNotice = FText::GetEmpty();
		ChatDrafts.Remove(FString(VeyraBackendProtocol::ChatKindName(VeyraBackendProtocol::EChatKind::PostMatch)) + TEXT(":"));
	}
	ON_SCOPE_EXIT
	{
		if (bRefocusFriendName && FriendNameBox)
		{
			FriendNameBox->SetKeyboardFocus();
		}
		if (bRefocusChat && ChatBox)
		{
			ChatBox->SetKeyboardFocus();
		}
		if (bRefocusReport && ReportDetailsBox)
		{
			ReportDetailsBox->SetKeyboardFocus();
		}
		if (bRefocusRosterSearch && RosterSearchBox)
		{
			RosterSearchBox->SetKeyboardFocus();
		}
		if (bRefocusName && NameBox)
		{
			NameBox->SetKeyboardFocus();
		}
	};
	// Player menus and a report form belong to one shown match (ADR-047 §5).
	if (const FString Match = VeyraConductModels::ShownMatch(Snapshot); Match != PlayerMenuMatch)
	{
		PlayerMenuMatch = Match;
		OpenPlayerMenu.Reset();
		ReportFormName.Reset();
		ReportReason.Reset();
		ReportDetailsDraft.Reset();
	}
	// Each screen chooses its own art; champion select shows the Vanguard it is looking at.
	ShowBackdrop(nullptr);
	for (UImage* Scrim : { ScrimLeft.Get(), ScrimBottom.Get(), ScrimTop.Get(), ScrimRight.Get() })
	{
		Scrim->SetVisibility(ESlateVisibility::Collapsed);
	}
	switch (Shown)
	{
	case EVeyraShellScreen::None:
		return;
	case EVeyraShellScreen::Status:
		BuildStatus(Snapshot);
		break;
	case EVeyraShellScreen::Stopped:
		BuildStopped(Snapshot);
		return;
	case EVeyraShellScreen::StarterChoice:
		BuildStarterChoice(Snapshot);
		break;
	case EVeyraShellScreen::Shell:
		BuildShell(Snapshot);
		break;
	case EVeyraShellScreen::Lobby:
		BuildLobby(Snapshot);
		break;
	case EVeyraShellScreen::MatchFound:
		BuildMatchFound(Snapshot);
		break;
	case EVeyraShellScreen::ChampionSelect:
		BuildChampionSelect(Snapshot);
		break;
	case EVeyraShellScreen::ReconnectOnly:
		BuildReconnectOnly(Snapshot);
		break;
	case EVeyraShellScreen::Results:
		BuildResults(Snapshot);
		break;
	}
	// Another player's profile opens over the shell, a lobby or the results (ADR-048 §5).
	if (Shown == EVeyraShellScreen::Shell || Shown == EVeyraShellScreen::Lobby || Shown == EVeyraShellScreen::Results)
	{
		BuildProfileOverlay(Snapshot);
	}
	BuildProblem(Snapshot);
	BuildSettingsConflict(Snapshot);
	RevealArrivals();
	// The motes drift behind the shell, a lobby and the results; champion select and the moments before a match keep still.
	const bool bDrifting = Shown == EVeyraShellScreen::Shell || Shown == EVeyraShellScreen::Lobby || Shown == EVeyraShellScreen::Results;
	Motes->SetVisibility(bDrifting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UVeyraShellScreen::RevealArrivals()
{
	// A new screen's parts arrive in turn, its top bar sliding in; a new page in the shell brings in its own parts alone. A
	// rebuild of what shows carries a reveal under way over to the parts it made anew, from where it had reached.
	const bool bNewScreen = Shown != RevealedScreen;
	const bool bNewPage = !bNewScreen && Shown == EVeyraShellScreen::Shell && Page != RevealedPage;
	if (bNewScreen || bNewPage)
	{
		bRevealingPage = bNewPage;
	}
	TArray<UWidget*> Parts = bRevealingPage && PageBody ? PageBody->GetAllChildren() : Content->GetAllChildren();
	TArray<EVeyraReveal> Kinds;
	if (!bRevealingPage && Shown == EVeyraShellScreen::Shell && !Parts.IsEmpty())
	{
		Kinds.Add(EVeyraReveal::Slide);
	}
	if (bNewScreen || bNewPage)
	{
		Reveal.Play(Parts, Kinds);
	}
	else
	{
		Reveal.Continue(Parts, Kinds);
	}
	RevealedScreen = Shown;
	RevealedPage = Page;
}

void UVeyraShellScreen::ShowShowcase(const FString& VanguardId)
{
	ShowBackdrop(VeyraShellArt::HeroOf(VanguardId));
	if (!GetBackdrop())
	{
		return;
	}
	// The art at near its full strength; the scrims darken it only where text sits (Art Bible §6.1).
	Backdrop->SetColorAndOpacity(ShellStyle().ShowcaseTint);
	for (UImage* Scrim : { ScrimLeft.Get(), ScrimBottom.Get(), ScrimTop.Get() })
	{
		Scrim->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

FString UVeyraShellScreen::PickFeaturedVanguard()
{
	// Once each time the game runs, so each run shows a different Vanguard, and one run always the same.
	static const FString Chosen = [] {
		const UVeyraShellStyleSettings& Style = ShellStyle();
		TArray<FString> WithArt;
		for (const FVeyraVanguardPortrait& Portrait : Style.VanguardPortraits)
		{
			if (VeyraShellArt::HeroOf(Portrait.Vanguard))
			{
				WithArt.Add(Portrait.Vanguard);
			}
		}
		return WithArt.IsEmpty() ? Style.HomeVanguard : WithArt[FMath::RandRange(0, WithArt.Num() - 1)];
	}();
	return Chosen;
}

void UVeyraShellScreen::AddCentred(UWidget& Child)
{
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Width->SetWidthOverride(ShellStyle().DialogWidth);
	Width->AddChild(&Child);
	AddStretch(*WidgetTree, *Content);
	Content->AddChildToVerticalBox(Width)->SetHorizontalAlignment(HAlign_Center);
	AddStretch(*WidgetTree, *Content);
}

void UVeyraShellScreen::BuildStatus(const FVeyraClientSnapshot& Snapshot)
{
	const FVeyraStatusModel Model = VeyraShellModels::DescribeStatus(Snapshot);
	// Match Starting names the confirmed Vanguard (UX-40), and stands it behind the screen as the player
	// is deployed into the Crucible (Art Bible §6.4).
	const VeyraBackendProtocol::FSelectSeat* You = Snapshot.Select.FindYou();
	const bool bDeploying = (Snapshot.State == EVeyraClientState::MatchStarting || Snapshot.State == EVeyraClientState::Connecting) && You && !You->Locked.IsEmpty();
	ShowShowcase(bDeploying ? You->Locked : FeaturedVanguard);
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Panel, FMargin(ShellStyle().ScreenPadding));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	AddText(*Rows, LOCTEXT("StatusEyebrow", "Veyra"), RoleOf(EVeyraShellText::Eyebrow));
	AddText(*Rows, Model.Title, RoleOf(EVeyraShellText::Title));
	AddText(*Rows, Model.Detail, RoleOf(EVeyraShellText::Body));
	if (bDeploying)
	{
		AddText(*Rows, FText::Format(LOCTEXT("StatusVanguard", "Your Vanguard: {0}"), VeyraShellModels::VanguardNameOf(You->Locked)), RoleOf(EVeyraShellText::Muted));
	}
	AddCentred(*Panel);
}

void UVeyraShellScreen::BuildStopped(const FVeyraClientSnapshot& Snapshot)
{
	const FVeyraStatusModel Model = VeyraShellModels::DescribeStatus(Snapshot);
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Panel, FMargin(ShellStyle().ScreenPadding));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	AddText(*Rows, Model.Title, RoleOf(EVeyraShellText::Title));
	AddText(*Rows, Model.Detail, RoleOf(EVeyraShellText::Body));
	AddButton(*Rows, LOCTEXT("Quit", "Quit"), [this] { Client->Quit(); });
	AddCentred(*Panel);
}

void UVeyraShellScreen::BuildStarterChoice(const FVeyraClientSnapshot& Snapshot)
{
	ShowShowcase(FeaturedVanguard);
	// The choice centred and its cards large (ADR-082 §11): the player's first decision, and the Vanguards' art its subject.
	const UVeyraShellStyleSettings& Style = ShellStyle();
	const auto Centred = [this](const FText& Text, EVeyraShellText Role) {
		UTextBlock* Block = AddText(*Content, Text, RoleOf(Role));
		// One line each: wrapping a centred line breaks it at its own width.
		Block->SetAutoWrapText(false);
		Block->SetJustification(ETextJustify::Center);
		Cast<UVerticalBoxSlot>(Block->Slot)->SetHorizontalAlignment(HAlign_Center);
	};
	Centred(LOCTEXT("StarterEyebrow", "Your first Vanguard"), EVeyraShellText::Eyebrow);
	Centred(LOCTEXT("StarterTitle", "Choose Your Starter"), EVeyraShellText::Display);
	Centred(LOCTEXT("StarterDetail", "The tutorial is coming later. Choose a starter Vanguard: it is yours to keep."), EVeyraShellText::Muted);
	UHorizontalBox* Cards = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const bool bCanChoose = Client->CanIssue(EVeyraClientIntent::ChooseStarter);
	for (const FString& Starter : Snapshot.Starters)
	{
		const FVeyraContentId Id = FVeyraContentId::FromText(Starter).Get(FVeyraContentId());
		const TArray<TPair<FText, uint8>> Plate = {
			{ VeyraShellModels::VanguardNameOf(Starter), RoleOf(EVeyraShellText::Heading) },
			{ VeyraContentText::VanguardTitle(Id), RoleOf(EVeyraShellText::Muted) },
		};
		AddArtCard(*Cards, VeyraShellModels::VanguardNameOf(Starter), Starter, Plate, [this, Starter] { Client->ChooseStarter(Starter); }, bCanChoose, false,
			FVector2D(Style.StarterCardWidth, Style.StarterCardHeight));
	}
	VeyraShellStyle::AddSpaced(*Content, *Cards);
	Cast<UVerticalBoxSlot>(Cards->Slot)->SetHorizontalAlignment(HAlign_Center);
}

void UVeyraShellScreen::BuildTopBar(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent, bool bPages)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	UHorizontalBox* Bar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	// The Flux mark and the name, in the display tier at a smaller size, wider tracking and always in capitals (ADR-078 §5).
	UHorizontalBox* Brand = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Brand->AddChildToHorizontalBox(VeyraShellStyle::MakeGlyph(*WidgetTree, VeyraShellBrushes::Diamond(Style.WordmarkGlyphSize, Style.AccentColor, FLinearColor::Transparent)))
		->SetVerticalAlignment(VAlign_Center);
	AddGap(*WidgetTree, *Brand, Style.Spacing);
	UTextBlock* Mark = VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("Wordmark", "Veyra"), EVeyraShellText::Display);
	FSlateFontInfo MarkFont = VeyraShellStyle::FontFor(EVeyraShellText::Display);
	MarkFont.Size = Style.TitleFontSize;
	MarkFont.LetterSpacing = Style.EyebrowLetterSpacing * 2;
	Mark->SetFont(MarkFont);
	Mark->SetAutoWrapText(false);
	Mark->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	Brand->AddChildToHorizontalBox(Mark)->SetVerticalAlignment(VAlign_Center);
	VeyraShellStyle::AddSpaced(*Bar, *Brand);
	AddGap(*WidgetTree, *Bar, Style.ScreenPadding);
	// Navigation between ordinary pages (UX §1); a lobby holds the player until they leave it.
	if (bPages)
	{
		AddKindButton(*Bar, EVeyraShellButtonKind::Tab, LOCTEXT("NavHome", "Home"), [this] { ShowPage(EVeyraShellPage::Home); }, true, Page == EVeyraShellPage::Home)
			->KeepLabelOnOneLine();
		AddKindButton(*Bar, EVeyraShellButtonKind::Tab, LOCTEXT("NavPlay", "Play"), [this] { ShowPage(EVeyraShellPage::Play); }, true, Page == EVeyraShellPage::Play)
			->KeepLabelOnOneLine();
		// Opening Match History reads its first page afresh, with the filters last used.
		AddKindButton(*Bar, EVeyraShellButtonKind::Tab, LOCTEXT("NavHistory", "Match History"), [this] {
			ShowPage(EVeyraShellPage::History);
			Client->LoadHistory(Client->GetSnapshot().History.Filter);
		}, true, Page == EVeyraShellPage::History)->KeepLabelOnOneLine();
		// Opening the Collection reads it afresh, with the level and balances (ADR-045 §8).
		AddKindButton(*Bar, EVeyraShellButtonKind::Tab, LOCTEXT("NavCollection", "Collection"), [this] {
			ShowPage(EVeyraShellPage::Collection);
			Client->LoadCollection();
		}, true, Page == EVeyraShellPage::Collection)->KeepLabelOnOneLine();
		// Opening the Profile page reads the choices afresh, and starts the draft from them (ADR-048 §5).
		AddKindButton(*Bar, EVeyraShellButtonKind::Tab, VeyraProfileModels::PageLabel(), [this] {
			bProfileDraftReady = false;
			ShowPage(EVeyraShellPage::Profile);
			Client->LoadProfileSettings();
			Client->LoadDisplayName();
		}, true, Page == EVeyraShellPage::Profile)->KeepLabelOnOneLine();
	}
	AddStretch(*WidgetTree, *Bar);
	// The account: its level, name and balances; before they are read, who is signed in.
	if (!AddProgressionReadout(Snapshot, *Bar))
	{
		UTextBlock* Player = AddText(*Bar, FText::Format(LOCTEXT("SignedInAs", "Signed in as {0}"), FText::FromString(Snapshot.DisplayName)), RoleOf(EVeyraShellText::Muted));
		Player->SetAutoWrapText(false);
	}
	AddSettingsButton(*Bar);
	AddKindButton(*Bar, EVeyraShellButtonKind::Quiet, LOCTEXT("Quit", "Quit"), [this] { Client->Quit(); })->KeepLabelOnOneLine();
	Parent.AddChild(Bar);
	AddGap(*WidgetTree, Parent, Style.Spacing);
	Parent.AddChild(VeyraShellStyle::MakeRule(*WidgetTree));
	AddGap(*WidgetTree, Parent, Style.Spacing * 2.0f);
}

void UVeyraShellScreen::BuildShell(const FVeyraClientSnapshot& Snapshot)
{
	// Another player claimed the name: a new one comes before anything else (ADR-049 §4).
	if (Snapshot.bRenameRequired)
	{
		BuildChooseName(Snapshot);
		return;
	}
	BuildTopBar(Snapshot, *Content);
	if (const FText Notice = VeyraShellModels::DescribeNotice(Snapshot.Notice); !Notice.IsEmpty())
	{
		UBorder* Banner = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(ShellStyle().Spacing));
		Banner->SetContent(VeyraShellStyle::MakeText(*WidgetTree, Notice, EVeyraShellText::Body));
		VeyraShellStyle::AddSpaced(*Content, *Banner);
	}
	// The page with the party bar under it, and the friends rail down the whole right side (Art Bible §7; ADR-078 §5):
	// two persistent surfaces that never cover each other.
	UHorizontalBox* Split = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddFilling(*Content, *Split);
	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Split->AddChildToHorizontalBox(Main)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddFilling(*Main, *Body);
	PageBody = Body;
	AddGap(*WidgetTree, *Split, ShellStyle().Spacing * 2.0f);
	BuildFriends(Snapshot, *Split);
	if (Page == EVeyraShellPage::Play)
	{
		BuildPlay(Snapshot, *Body);
	}
	else if (Page == EVeyraShellPage::History)
	{
		BuildHistory(Snapshot, *Body);
	}
	else if (Page == EVeyraShellPage::Collection)
	{
		BuildCollection(Snapshot, *Body);
	}
	else if (Page == EVeyraShellPage::Profile)
	{
		BuildProfilePage(Snapshot, *Body);
	}
	else
	{
		BuildHome(Snapshot, *Body);
	}
	BuildParty(Snapshot, *Main);
}

void UVeyraShellScreen::BuildHistory(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	const FVeyraHistoryModel Model = VeyraMatchHistoryModel::Describe(Snapshot, Client->CanIssue(EVeyraClientIntent::LoadMoreHistory));
	if (Model.Opened.IsSet())
	{
		// A saved record: the same Scoreboard and Detailed Statistics as the results screen (UX-51).
		AddKindButton(Parent, EVeyraShellButtonKind::Quiet, LOCTEXT("BackToHistory", "Back to Match History"), [this] { Client->CloseHistoryMatch(); },
			Client->CanIssue(EVeyraClientIntent::CloseHistoryMatch))
			->KeepLabelOnOneLine();
		// Headed as the results screen is (ADR-082 §11): the outcome in the display face, its facts in a line under it.
		UTextBlock* Headline = AddText(Parent, Model.Opened->Headline, RoleOf(EVeyraShellText::Display));
		const FString& Tone = Model.Opened->Tone;
		Headline->SetColorAndOpacity(FSlateColor(Tone == TEXT("win") ? Style.VictoryColor : Tone == TEXT("loss") ? Style.DefeatColor : Style.TextColor));
		UWrapBox* Facts = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		for (const FText& Line : Model.Opened->Lines)
		{
			AddText(*Facts, Line, RoleOf(EVeyraShellText::Muted))->SetAutoWrapText(false);
		}
		VeyraShellStyle::AddSpaced(Parent, *Facts);
		BuildReport(Snapshot, Model.Opened->Report, Parent);
		return;
	}
	AddText(Parent, LOCTEXT("HistoryEyebrow", "Your record"), RoleOf(EVeyraShellText::Eyebrow));
	AddText(Parent, LOCTEXT("HistoryTitle", "Match History"), RoleOf(EVeyraShellText::Display));
	const VeyraBackendProtocol::FHistoryFilter& Current = Snapshot.History.Filter;
	const auto Load = [this](const VeyraBackendProtocol::FHistoryFilter& Filter) { Client->LoadHistory(Filter); };
	const bool bCanLoad = Client->CanIssue(EVeyraClientIntent::LoadHistory);
	AddHistoryFilter(Parent, Model.Vanguards, Current, [](VeyraBackendProtocol::FHistoryFilter& Filter, const FString& Value) { Filter.VanguardId = Value; }, Load, bCanLoad);
	AddHistoryFilter(Parent, Model.Modes, Current, [](VeyraBackendProtocol::FHistoryFilter& Filter, const FString& Value) { Filter.Mode = Value; }, Load, bCanLoad);
	AddHistoryFilter(Parent, Model.Outcomes, Current, [](VeyraBackendProtocol::FHistoryFilter& Filter, const FString& Value) { Filter.Outcome = Value; }, Load, bCanLoad);
	if (!Model.Empty.IsEmpty())
	{
		AddText(Parent, Model.Empty, RoleOf(EVeyraShellText::Muted));
	}
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(List);
	AddFilling(Parent, *Scroll);
	const bool bCanOpen = Client->CanIssue(EVeyraClientIntent::OpenHistoryMatch);
	for (const FVeyraHistoryRow& Row : Model.Rows)
	{
		// Each match a card that opens it (ADR-082 §11): its outcome's colour down its edge, the player's Vanguard, the outcome
		// over the Vanguard's name, the mode over when it ended, and how long it ran.
		const FLinearColor OutcomeColor = Row.OutcomeKind == TEXT("win") ? Style.VictoryColor : Row.OutcomeKind == TEXT("loss") ? Style.DefeatColor : Style.MutedTextColor;
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* Edge = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Edge->SetWidthOverride(Style.HistoryOutcomeBarWidth);
		Edge->SetHeightOverride(Style.ReportPortraitSize);
		Edge->AddChild(VeyraShellStyle::MakeBorder(*WidgetTree, OutcomeColor, 0.0f));
		Line->AddChildToHorizontalBox(Edge)->SetVerticalAlignment(VAlign_Center);
		AddGap(*WidgetTree, *Line, Style.Spacing);
		UTexture2D* Hero = VeyraShellArt::HeroOf(Row.VanguardId);
		const FBox2f Face = Hero ? VeyraShellArt::Crop(Row.VanguardId, Hero->GetSizeX(), Hero->GetSizeY(), 1.0f, /*bPortrait*/ true) : FBox2f(FVector2f::ZeroVector, FVector2f::UnitVector);
		USizeBox* FaceBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		FaceBox->SetWidthOverride(Style.ReportPortraitSize);
		FaceBox->SetHeightOverride(Style.ReportPortraitSize);
		UImage* Portrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Portrait->SetBrush(VeyraShellArt::Brush(Hero, Face, FVector2D(Style.ReportPortraitSize), Style.ButtonCornerRadius, Style.SurfaceColor, OutcomeColor, 1.0f));
		FaceBox->AddChild(Portrait);
		Line->AddChildToHorizontalBox(FaceBox)->SetVerticalAlignment(VAlign_Center);
		AddGap(*WidgetTree, *Line, Style.Spacing);
		const auto Pair = [this, &Line, &Style](const FText& Top, EVeyraShellText TopRole, const FLinearColor* TopColor, const FText& Under, float Width) {
			USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			Cell->SetWidthOverride(Width);
			UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UTextBlock* First = AddText(*Stack, Top, RoleOf(TopRole));
			First->SetAutoWrapText(false);
			if (TopColor)
			{
				First->SetColorAndOpacity(FSlateColor(*TopColor));
			}
			AddText(*Stack, Under, RoleOf(EVeyraShellText::Muted))->SetAutoWrapText(false);
			Cell->AddChild(Stack);
			Line->AddChildToHorizontalBox(Cell)->SetVerticalAlignment(VAlign_Center);
		};
		Pair(Row.Outcome, EVeyraShellText::Heading, &OutcomeColor, Row.Vanguard, Style.ReportLabelWidth);
		Pair(Row.Mode, EVeyraShellText::Body, nullptr, Row.When, Style.ReportLabelWidth);
		UTextBlock* Duration = AddText(*Line, Row.Duration, RoleOf(EVeyraShellText::Body));
		Duration->SetAutoWrapText(false);
		const FString MatchId = Row.MatchId;
		UVeyraShellButton* Open = AddContentButton(*List, LOCTEXT("OpenMatch", "Open"), *Line, [this, MatchId] { Client->OpenHistoryMatch(MatchId); }, bCanOpen, false);
		Open->SetToolTipText(Row.Summary);
		// The card reads from its edge, not centred as a label is.
		if (UButtonSlot* LineSlot = Cast<UButtonSlot>(Line->Slot))
		{
			LineSlot->SetHorizontalAlignment(HAlign_Left);
		}
		if (UVerticalBoxSlot* OpenSlot = Cast<UVerticalBoxSlot>(Open->Slot))
		{
			OpenSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
	// Older records come in batches (UX-67).
	if (Model.bOffersLoadMore)
	{
		AddButton(*List, LOCTEXT("LoadMore", "Load More"), [this] { Client->LoadMoreHistory(); });
	}
}

void UVeyraShellScreen::AddHistoryFilter(UPanelWidget& Parent, const TArray<FVeyraHistoryOption>& Options, const VeyraBackendProtocol::FHistoryFilter& Current,
	TFunction<void(VeyraBackendProtocol::FHistoryFilter&, const FString&)> Apply, TFunction<void(const VeyraBackendProtocol::FHistoryFilter&)> Load, bool bCanLoad)
{
	// The options wrap within the page, however many there are (ADR-082 §11).
	UWrapBox* Row = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	for (const FVeyraHistoryOption& Option : Options)
	{
		const FString Value = Option.Value;
		AddKindButton(*Row, EVeyraShellButtonKind::Tab, Option.Label, [Current, Value, Apply, Load] {
			VeyraBackendProtocol::FHistoryFilter Filter = Current;
			Apply(Filter, Value);
			Load(Filter);
		}, bCanLoad, Option.bSelected)->KeepLabelOnOneLine();
	}
	VeyraShellStyle::AddSpaced(Parent, *Row);
}

void UVeyraShellScreen::BuildParty(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	FVeyraPartyPermissions Permissions;
	Permissions.bCanKick = Client->CanIssue(EVeyraClientIntent::KickFromParty);
	Permissions.bCanTransfer = Client->CanIssue(EVeyraClientIntent::TransferPartyLeader);
	Permissions.bCanSetPrivacy = Client->CanIssue(EVeyraClientIntent::SetPartyPrivacy);
	Permissions.bCanLeave = Client->CanIssue(EVeyraClientIntent::LeaveParty);
	const FVeyraPartyModel Model = VeyraShellModels::DescribeParty(Snapshot, Client->CanIssue(EVeyraClientIntent::SetReady),
		Client->CanIssue(EVeyraClientIntent::FindMatch), Client->CanIssue(EVeyraClientIntent::CancelQueue), Permissions);
	if (!Model.bShown)
	{
		return;
	}
	const UVeyraShellStyleSettings& Style = ShellStyle();
	UBorder* Bar = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.ScreenPadding / 2.0f, Style.Spacing));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Bar->SetContent(Row);
	UVerticalBox* Who = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddText(*Who, LOCTEXT("PartyTitle", "Party"), RoleOf(EVeyraShellText::Eyebrow));
	AddText(*Who, Model.Mode, RoleOf(EVeyraShellText::Heading))->SetAutoWrapText(false);
	UHorizontalBox* Settings = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddText(*Settings, Model.Privacy, RoleOf(EVeyraShellText::Muted))->SetAutoWrapText(false);
	// The leader's privacy (Parties & Social Bible §1), and anyone's Leave Party.
	if (Model.bOffersPrivacy)
	{
		const VeyraBackendProtocol::EPartyPrivacy Target = Model.PrivacyTarget;
		AddKindButton(*Settings, EVeyraShellButtonKind::Quiet, Model.PrivacyLabel, [this, Target] { Client->SetPartyPrivacy(Target); }, Model.bCanSetPrivacy)
			->KeepLabelOnOneLine();
	}
	AddKindButton(*Settings, EVeyraShellButtonKind::Quiet, VeyraShellModels::LeavePartyLabel(), [this] {
		Client->LeaveParty();
		CloseCard();
	}, Model.bCanLeave)->KeepLabelOnOneLine();
	Who->AddChildToVerticalBox(Settings);
	VeyraShellStyle::AddSpaced(*Row, *Who);
	AddGap(*WidgetTree, *Row, Style.ScreenPadding / 2.0f);
	// The member cards (UX-10). The leader selects another member's card for its actions, which open
	// beneath it; Make Party Leader asks first, naming the recipient (UX-11).
	// Side by side, each member a chip: their initial and their line (ADR-078 §5); a member's actions open under its chip.
	UHorizontalBox* Members = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (const FVeyraPartyMemberModel& Card : Model.Cards)
	{
		UVerticalBox* Chip = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* ChipLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		ChipLine->AddChildToHorizontalBox(VeyraShellStyle::MakeInitialTile(*WidgetTree, FText::FromString(Card.Name), Style.AvatarSize))
			->SetVerticalAlignment(VAlign_Center);
		AddGap(*WidgetTree, *ChipLine, Style.Spacing / 2.0f);
		Chip->AddChildToVerticalBox(ChipLine);
		VeyraShellStyle::AddSpaced(*Members, *Chip);
		if (!Card.bOffersActions)
		{
			AddText(*ChipLine, Card.Line, RoleOf(EVeyraShellText::Body))->SetAutoWrapText(false);
			continue;
		}
		const FString Id = Card.AccountId;
		const FString Key = MemberCardKey(Id);
		const bool bOpen = OpenCardId == Key;
		AddNamedButton(*ChipLine, EVeyraShellButtonKind::Quiet, VeyraShellModels::PartyMemberLabel(Card.Name), Card.Line, [this, Key] { OpenCard(Key); }, true, bOpen);
		if (!bOpen)
		{
			continue;
		}
		UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (Confirm == EVeyraShellConfirm::PartyLeader && ConfirmId == Id)
		{
			AddText(*Actions, VeyraShellModels::ConfirmLeaderPrompt(Card.Name), RoleOf(EVeyraShellText::Muted))->SetAutoWrapText(false);
			AddNamedButton(*Actions, EVeyraShellButtonKind::Primary, VeyraShellModels::ConfirmLeaderLabel(Card.Name), LOCTEXT("ConfirmLeader", "Confirm"),
				[this, Id] {
					Client->TransferPartyLeader(Id);
					CloseCard();
				},
				Card.bCanMakeLeader);
			AddNamedButton(*Actions, EVeyraShellButtonKind::Quiet, VeyraShellModels::CancelConfirmLabel(), LOCTEXT("CancelLeader", "Cancel"),
				[this] { AskToConfirm(EVeyraShellConfirm::None, FString()); });
		}
		else
		{
			AddNamedButton(*Actions, EVeyraShellButtonKind::Secondary, VeyraShellModels::MakeLeaderLabel(Card.Name), LOCTEXT("MakeLeader", "Make Party Leader"),
				[this, Id] { AskToConfirm(EVeyraShellConfirm::PartyLeader, Id); }, Card.bCanMakeLeader);
			AddNamedButton(*Actions, EVeyraShellButtonKind::Quiet, VeyraShellModels::RemoveFromPartyLabel(Card.Name), LOCTEXT("RemoveMember", "Remove"),
				[this, Id] {
					Client->KickFromParty(Id);
					CloseCard();
				},
				Card.bCanRemove);
		}
		VeyraShellStyle::AddSpaced(*Chip, *Actions);
	}
	VeyraShellStyle::AddSpaced(*Row, *Members);
	AddStretch(*WidgetTree, *Row);
	if (Model.bQueued)
	{
		QueueStatus = AddText(*Row, VeyraShellModels::FormatQueueStatus(Client->GetQueuedSeconds()), RoleOf(EVeyraShellText::Heading));
		QueueStatus->SetColorAndOpacity(FSlateColor(Style.AccentColor));
		QueueStatus->SetAutoWrapText(false);
	}
	else if (!Model.Status.IsEmpty())
	{
		AddText(*Row, Model.Status, RoleOf(EVeyraShellText::Muted))->SetAutoWrapText(false);
	}
	if (!Model.bQueued)
	{
		const bool bReady = Model.bReadyTarget;
		AddButton(*Row, Model.ReadyLabel, [this, bReady] { Client->SetReady(bReady); }, Model.bCanReady)->KeepLabelOnOneLine();
	}
	if (Model.bOffersFindMatch)
	{
		AddKindButton(*Row, EVeyraShellButtonKind::Primary, LOCTEXT("FindMatch", "Find Match"), [this] { Client->FindMatch(); }, Model.bCanFindMatch)
			->KeepLabelOnOneLine();
	}
	if (Model.bOffersCancel)
	{
		AddKindButton(*Row, EVeyraShellButtonKind::Quiet, LOCTEXT("CancelQueue", "Cancel"), [this] { Client->CancelQueue(); }, Model.bCanCancel)
			->KeepLabelOnOneLine();
	}
	AddGap(*WidgetTree, Parent, Style.Spacing);
	Parent.AddChild(Bar);
}

void UVeyraShellScreen::BuildMatchFound(const FVeyraClientSnapshot& Snapshot)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	// The threshold into competition (Art Bible §6.2): the world recedes and the choice is all there is.
	ShowShowcase(FeaturedVanguard);
	const bool bCanAnswer = Client->CanIssue(EVeyraClientIntent::AcceptMatch);
	const FVeyraMatchFoundModel Model = VeyraShellModels::DescribeMatchFound(Snapshot, Client->GetRemainingAcceptSeconds(), bCanAnswer);
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.ScreenPadding));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	// Centred lines wrap at the panel's inner width, known before layout, so a wrapped line never runs into what follows.
	const float LineWidth = Style.DialogWidth - Style.ScreenPadding * 2.0f;
	const auto Centre = [LineWidth](UTextBlock* Text) {
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(false);
		Text->SetWrapTextAt(LineWidth);
	};
	Centre(AddText(*Rows, Model.Mode, RoleOf(EVeyraShellText::Eyebrow)));
	Centre(AddText(*Rows, Model.Title, RoleOf(EVeyraShellText::Display)));
	Countdown = AddText(*Rows, Model.Countdown, RoleOf(EVeyraShellText::Countdown));
	Centre(Countdown);
	Centre(AddText(*Rows, Model.Progress, RoleOf(EVeyraShellText::Body)));
	Centre(AddText(*Rows, Model.Phase, RoleOf(EVeyraShellText::Muted)));
	UHorizontalBox* Answers = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddKindButton(*Answers, EVeyraShellButtonKind::Primary, LOCTEXT("Accept", "Accept"), [this] { Client->AcceptMatch(); }, Model.bCanAnswer)
		->KeepLabelOnOneLine();
	AddKindButton(*Answers, EVeyraShellButtonKind::Quiet, LOCTEXT("Decline", "Decline"), [this] { Client->DeclineMatch(); }, Model.bCanAnswer)
		->KeepLabelOnOneLine();
	Rows->AddChildToVerticalBox(Answers)->SetHorizontalAlignment(HAlign_Center);
	AddCentred(*Panel);
}

void UVeyraShellScreen::BuildHome(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	// Looking into the world, with Play the clear way in (Art Bible §6.1): the welcome high on the left, over the art's
	// darkened side, and the Vanguard the art shows named at the foot of the same column (ADR-078 §5).
	ShowShowcase(FeaturedVanguard);
	USizeBox* ColumnWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ColumnWidth->SetWidthOverride(Style.HomeColumnWidth);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ColumnWidth->AddChild(Column);
	if (UVerticalBox* PageColumn = Cast<UVerticalBox>(&Parent))
	{
		UVerticalBoxSlot* ColumnSlot = PageColumn->AddChildToVerticalBox(ColumnWidth);
		ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ColumnSlot->SetHorizontalAlignment(HAlign_Left);
	}
	else
	{
		Parent.AddChild(ColumnWidth);
	}
	// The room above the welcome and below it, in the style's proportion.
	const auto AddShare = [this, Column](float Share) {
		USpacer* Room = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = Share;
		Column->AddChildToVerticalBox(Room)->SetSize(Size);
	};
	AddShare(Style.HomeHeroHeight);
	AddText(*Column, LOCTEXT("HomeEyebrow", "The Meridian Crucible"), RoleOf(EVeyraShellText::Eyebrow));
	AddText(*Column, FText::Format(LOCTEXT("HomeWelcome", "Welcome, {0}."), FText::FromString(Snapshot.DisplayName)), RoleOf(EVeyraShellText::Display));
	AddText(*Column, LOCTEXT("HomeLead", "Every Vanguard answers the Crucible's call. Choose a mode, then the Vanguard you will be."),
		RoleOf(EVeyraShellText::Body));
	// Room before the way in, so the lead reads as its own line.
	AddGap(*WidgetTree, *Column, Style.Spacing * 2.0f);
	AddPlayReminder(Snapshot, *Column);
	// Play, larger than any other action on the screen: the clear way in (Art Bible §6.1; ADR-078 §5).
	USizeBox* PlaySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PlaySize->SetMinDesiredWidth(Style.HomePlayWidth);
	PlaySize->SetMinDesiredHeight(Style.HomePlayHeight);
	UVeyraShellButton* Play = AddKindButton(*PlaySize, EVeyraShellButtonKind::Primary, LOCTEXT("HomePlay", "Play"), [this] { ShowPage(EVeyraShellPage::Play); });
	if (UTextBlock* PlayLabel = Cast<UTextBlock>(Play->GetContent()))
	{
		FSlateFontInfo Font = PlayLabel->GetFont();
		Font.Size = VeyraShellLook::ScaledFontSize(Style.HomePlayFontSize);
		PlayLabel->SetFont(Font);
	}
	PlaySize->AddChild(Play);
	Column->AddChildToVerticalBox(PlaySize)->SetHorizontalAlignment(HAlign_Left);
	AddShare(1.0f - Style.HomeHeroHeight);
	// The Vanguard the art shows, named at the column's foot.
	const FVeyraContentId Featured = FVeyraContentId::FromText(FeaturedVanguard).Get(FVeyraContentId());
	UTextBlock* FeaturedLabel = AddText(*Column, LOCTEXT("HomeFeatured", "Featured Vanguard"), RoleOf(EVeyraShellText::Eyebrow));
	FeaturedLabel->SetAutoWrapText(false);
	FeaturedLabel->SetColorAndOpacity(FSlateColor(Style.MutedTextColor));
	AddText(*Column, VeyraShellModels::VanguardNameOf(FeaturedVanguard), RoleOf(EVeyraShellText::Title))->SetAutoWrapText(false);
	AddText(*Column, VeyraContentText::VanguardTitle(Featured), RoleOf(EVeyraShellText::Muted));
}

void UVeyraShellScreen::BuildPlay(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	ShowShowcase(FeaturedVanguard);
	AddText(Parent, LOCTEXT("PlayEyebrow", "Choose your mode"), RoleOf(EVeyraShellText::Eyebrow));
	AddText(Parent, LOCTEXT("PlayTitle", "Play"), RoleOf(EVeyraShellText::Display));

	// The modes in their categories, in the author's order: Ranked, Casual, AI, then Customs (ADR-039 §6).
	// Each is a heading over its cards, side by side.
	UHorizontalBox* Sections = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const auto AddSection = [this, Sections](const FText& Title) {
		UVerticalBox* Section = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddText(*Section, Title, RoleOf(EVeyraShellText::Eyebrow));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		VeyraShellStyle::AddSpaced(*Section, *Row);
		VeyraShellStyle::AddSpaced(*Sections, *Section);
		return Row;
	};
	const TArray<FVeyraModeCardModel> ModeCards = VeyraShellModels::DescribeModes(Snapshot);
	TMap<VeyraBackendProtocol::EModeCategory, UHorizontalBox*> Rows;
	for (const TPair<VeyraBackendProtocol::EModeCategory, FText>& Category : {
			 TPair<VeyraBackendProtocol::EModeCategory, FText>{ VeyraBackendProtocol::EModeCategory::Ranked, LOCTEXT("CategoryRanked", "Ranked") },
			 TPair<VeyraBackendProtocol::EModeCategory, FText>{ VeyraBackendProtocol::EModeCategory::Casual, LOCTEXT("CategoryCasual", "Casual") },
			 TPair<VeyraBackendProtocol::EModeCategory, FText>{ VeyraBackendProtocol::EModeCategory::AI, LOCTEXT("CategoryAI", "AI") } })
	{
		if (ModeCards.ContainsByPredicate([&Category](const FVeyraModeCardModel& Card) { return Card.Category == Category.Key; }))
		{
			Rows.Add(Category.Key, AddSection(Category.Value));
		}
	}

	// A mode card: its art, name, team format and whether it is available (UX-12). Choosing one makes
	// the player a party of one, or changes the mode of the party they lead (UX-6).
	const bool bCanSelectMode = Client->CanIssue(EVeyraClientIntent::SelectMode);
	for (const FVeyraModeCardModel& ModeCard : ModeCards)
	{
		UHorizontalBox* Cards = Rows.FindChecked(ModeCard.Category);
		TArray<TPair<FText, uint8>> Plate = {
			{ ModeCard.Format, RoleOf(EVeyraShellText::Eyebrow) },
			{ ModeCard.Name, RoleOf(EVeyraShellText::Heading) },
		};
		if (!ModeCard.Availability.IsEmpty())
		{
			Plate.Add({ ModeCard.Availability, RoleOf(EVeyraShellText::Muted) });
		}
		const FString ModeId = ModeCard.ModeId;
		AddArtCard(*Cards, ModeCard.Name, ShellStyle().ModeArtOf(ModeId), Plate, [this, ModeId] { Client->SelectMode(ModeId); },
			bCanSelectMode && ModeCard.bAvailable, ModeCard.bSelected);
	}
	// Customs: custom practice starts at once, with no party or queue (ADR-010 §7); a custom game is a lobby.
	UHorizontalBox* Cards = AddSection(LOCTEXT("CategoryCustoms", "Customs"));
	const TArray<TPair<FText, uint8>> Custom = {
		{ LOCTEXT("CustomPlayers", "1 player"), RoleOf(EVeyraShellText::Eyebrow) },
		{ LOCTEXT("CustomTitle", "Practice"), RoleOf(EVeyraShellText::Heading) },
		{ LOCTEXT("CustomDetail", "Alone on the battleground against a few bots, to try a Vanguard; it ends when you end it."),
			RoleOf(EVeyraShellText::Muted) },
	};
	AddArtCard(*Cards, LOCTEXT("Practice", "Practice"), ShellStyle().ModeArtOf(TEXT("practice")), Custom, [this] { Client->StartPractice(); },
		Client->CanIssue(EVeyraClientIntent::StartPractice), false);
	// A custom game is a lobby the player hosts: friends and bots on either side, and its own rules (ADR-021).
	const TArray<TPair<FText, uint8>> CustomGame = {
		{ LOCTEXT("CustomGamePlayers", "Friends and bots"), RoleOf(EVeyraShellText::Eyebrow) },
		{ LOCTEXT("CustomGameTitle", "Custom Game"), RoleOf(EVeyraShellText::Heading) },
		{ LOCTEXT("CustomGameDetail", "Your own lobby: invite friends, seat bots on either side and set the rules."), RoleOf(EVeyraShellText::Muted) },
	};
	AddArtCard(*Cards, LOCTEXT("CustomGame", "Custom Game"), ShellStyle().ModeArtOf(TEXT("custom_game")), CustomGame, [this] { Client->CreateLobby(); },
		Client->CanIssue(EVeyraClientIntent::CreateLobby), false);
	VeyraShellStyle::AddSpaced(Parent, *Sections);
}

UVeyraShellButton* UVeyraShellScreen::AddArtCard(UPanelWidget& Parent, const FText& Label, const FString& VanguardId, const TArray<TPair<FText, uint8>>& Plate,
	TFunction<void()> Action, bool bEnabled, bool bSelected, TOptional<FVector2D> CardSize)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	const FVector2D Size = CardSize.Get(FVector2D(Style.ModeCardWidth, Style.ModeCardHeight));
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Size.X);
	Box->SetHeightOverride(Size.Y);
	UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Box->AddChild(Card);

	UImage* Art = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	UTexture2D* Hero = VeyraShellArt::HeroOf(VanguardId);
	const FBox2f Region = Hero ? VeyraShellArt::Crop(VanguardId, Hero->GetSizeX(), Hero->GetSizeY(), static_cast<float>(Size.X / Size.Y), false) : FBox2f(FVector2f::ZeroVector, FVector2f::UnitVector);
	Art->SetBrush(VeyraShellArt::Brush(Hero, Region, Size, Style.PanelCornerRadius, Style.SurfaceColor, FLinearColor::Transparent, 0.0f));
	// What cannot be chosen now stands back.
	const FLinearColor Tint = bEnabled || bSelected ? Style.ShowcaseTint : FLinearColor(Style.ShowcaseTint * 0.35f).CopyWithNewOpacity(1.0f);
	Art->SetColorAndOpacity(Tint);
	UOverlaySlot* ArtSlot = Card->AddChildToOverlay(Art);
	ArtSlot->SetHorizontalAlignment(HAlign_Fill);
	ArtSlot->SetVerticalAlignment(VAlign_Fill);

	UBorder* PlateSurface = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing));
	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PlateSurface->SetContent(Lines);
	for (const TPair<FText, uint8>& Line : Plate)
	{
		UTextBlock* Text = VeyraShellStyle::MakeText(*WidgetTree, Line.Key, static_cast<EVeyraShellText>(Line.Value));
		Lines->AddChildToVerticalBox(Text);
	}
	UOverlaySlot* PlateSlot = Card->AddChildToOverlay(PlateSurface);
	PlateSlot->SetHorizontalAlignment(HAlign_Fill);
	PlateSlot->SetVerticalAlignment(VAlign_Bottom);
	PlateSlot->SetPadding(FMargin(Style.Spacing / 2.0f));
	return AddContentButton(Parent, Label, *Box, MoveTemp(Action), bEnabled, bSelected);
}

void UVeyraShellScreen::BuildReconnectOnly(const FVeyraClientSnapshot& Snapshot)
{
	ShowShowcase(FeaturedVanguard);
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Panel, FMargin(ShellStyle().ScreenPadding));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	AddText(*Rows, LOCTEXT("ReconnectEyebrow", "Your match goes on"), RoleOf(EVeyraShellText::Eyebrow));
	AddText(*Rows, LOCTEXT("ReconnectTitle", "Match in Progress"), RoleOf(EVeyraShellText::Title));
	AddText(*Rows, LOCTEXT("ReconnectDetail", "Your match is still running. Until it ends, Reconnect is the only thing you can do."),
		RoleOf(EVeyraShellText::Body));
	if (const FText Notice = VeyraShellModels::DescribeNotice(Snapshot.Notice); !Notice.IsEmpty())
	{
		AddText(*Rows, Notice, RoleOf(EVeyraShellText::Body));
	}
	AddKindButton(*Rows, EVeyraShellButtonKind::Primary, LOCTEXT("Reconnect", "Reconnect"), [this] { Client->Reconnect(); },
		Client->CanIssue(EVeyraClientIntent::Reconnect));
	AddCentred(*Panel);
}

void UVeyraShellScreen::BuildResults(const FVeyraClientSnapshot& Snapshot)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	const FVeyraResultsModel Model = VeyraShellModels::DescribeResults(Snapshot);
	// Emotional closure first, over the player's own Vanguard; then the numbers, clearly (Art Bible §6.5).
	ShowShowcase(Snapshot.Result.IsSet() && !Snapshot.Result->VanguardId.IsEmpty() ? Snapshot.Result->VanguardId : FeaturedVanguard);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox* Outcome = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddText(*Outcome, LOCTEXT("ResultsEyebrow", "The Crucible settles"), RoleOf(EVeyraShellText::Eyebrow));
	UTextBlock* Headline = AddText(*Outcome, Model.Headline, RoleOf(EVeyraShellText::Display));
	Headline->SetColorAndOpacity(FSlateColor(HeadlineColorOf(Snapshot)));
	UWrapBox* Facts = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	for (const FText& Line : Model.Lines)
	{
		AddText(*Facts, Line, RoleOf(EVeyraShellText::Muted))->SetAutoWrapText(false);
	}
	VeyraShellStyle::AddSpaced(*Outcome, *Facts);
	AddFilling(*Header, *Outcome);
	// Settings sits with the screen's other actions, at the foot of the header, on a surface of its own: the art behind
	// the header is the player's Vanguard at full strength, which quiet text would not read over.
	AddSettingsButton(*Header);
	UWidget* SettingsWidget = Header->GetChildAt(Header->GetChildrenCount() - 1);
	Cast<UHorizontalBoxSlot>(SettingsWidget->Slot)->SetVerticalAlignment(VAlign_Bottom);
	if (UVeyraShellButton* SettingsButton = Cast<UVeyraShellButton>(SettingsWidget))
	{
		SettingsButton->SetStyle(VeyraShellStyle::ButtonStyleFor(EVeyraShellButtonKind::Secondary, false));
		if (UTextBlock* SettingsLabel = Cast<UTextBlock>(SettingsButton->GetContent()))
		{
			SettingsLabel->SetColorAndOpacity(FSlateColor(Style.TextColor));
		}
	}
	UVeyraShellButton* Continue = UVeyraShellButton::MakeKind(*WidgetTree, EVeyraShellButtonKind::Primary, LOCTEXT("Continue", "Continue"),
		[this] { Client->ContinueFromResults(); }, Client->CanIssue(EVeyraClientIntent::ContinueFromResults));
	// Play Again leaves as Continue does and opens Play with the party panel; it readies, queues and changes nothing (UX-62).
	UVeyraShellButton* PlayAgain = UVeyraShellButton::MakeKind(*WidgetTree, EVeyraShellButtonKind::Secondary, VeyraConductModels::PlayAgainLabel(),
		[this] {
			Page = EVeyraShellPage::Play;
			Client->ContinueFromResults();
		},
		Client->CanIssue(EVeyraClientIntent::ContinueFromResults));
	Buttons.Add(PlayAgain);
	Header->AddChildToHorizontalBox(PlayAgain)->SetVerticalAlignment(VAlign_Bottom);
	Buttons.Add(Continue);
	Header->AddChildToHorizontalBox(Continue)->SetVerticalAlignment(VAlign_Bottom);
	VeyraShellStyle::AddSpaced(*Content, *Header);
	AddPlayReminder(Snapshot, *Content);
	BuildRewards(Snapshot, *Content);
	// The report, with the optional post-match chat beside it (UX-59–60).
	UHorizontalBox* Below = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UWidget* Report = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	if (Model.bVerified)
	{
		UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Panel, FMargin(Style.Spacing * 2.0f));
		UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Body);
		BuildReport(Snapshot, Model.Report, *Body);
		Report = Panel;
	}
	UHorizontalBoxSlot* ReportSlot = Below->AddChildToHorizontalBox(Report);
	ReportSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BuildPostMatchChat(Snapshot, *Below);
	AddFilling(*Content, *Below);
}

void UVeyraShellScreen::BuildReport(const FVeyraClientSnapshot& Snapshot, const FVeyraMatchReport& Report, UPanelWidget& Parent)
{
	if (!Report.bHasScoreboard)
	{
		// Truthfully pending or missing, never invented (UX-50).
		if (!Report.Pending.IsEmpty())
		{
			AddText(Parent, Report.Pending, RoleOf(EVeyraShellText::Muted));
		}
		return;
	}
	UHorizontalBox* Views = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddKindButton(*Views, EVeyraShellButtonKind::Tab, LOCTEXT("ScoreboardView", "Scoreboard"), [this] { ShowReportView(EVeyraReportView::Scoreboard); }, true,
		ReportView == EVeyraReportView::Scoreboard)->KeepLabelOnOneLine();
	AddKindButton(*Views, EVeyraShellButtonKind::Tab, LOCTEXT("DetailsView", "Detailed Statistics"), [this] { ShowReportView(EVeyraReportView::Details); }, true,
		ReportView == EVeyraReportView::Details)->KeepLabelOnOneLine();
	VeyraShellStyle::AddSpaced(Parent, *Views);
	// The report scrolls within what is left of the screen.
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Body);
	AddFilling(Parent, *Scroll);
	if (ReportView == EVeyraReportView::Scoreboard)
	{
		BuildScoreboard(Snapshot, Report, *Body);
	}
	else
	{
		BuildDetails(Report, *Body);
	}
}

void UVeyraShellScreen::BuildScoreboard(const FVeyraClientSnapshot& Snapshot, const FVeyraMatchReport& Report, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	const uint8 Role = RoleOf(EVeyraShellText::Body);
	const uint8 Column = RoleOf(EVeyraShellText::Column);
	// A square of Texture's Region at Size, its words in a tooltip; an empty, edged square when there is no art.
	const auto AddArt = [this, &Style](UPanelWidget& Row, UTexture2D* Texture, const FBox2f& Region, float Size, const FText& Tip) {
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Size);
		Box->SetHeightOverride(Size);
		UImage* Art = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Art->SetBrush(VeyraShellArt::Brush(Texture, Region, FVector2D(Size), Style.ButtonCornerRadius, VeyraShellLook::Panel(Style.SurfaceRaisedColor),
			Style.HairlineColor, 1.0f));
		Art->SetToolTipText(Tip);
		Box->AddChild(Art);
		UHorizontalBoxSlot* ArtSlot = Cast<UHorizontalBox>(&Row)->AddChildToHorizontalBox(Box);
		ArtSlot->SetVerticalAlignment(VAlign_Center);
		ArtSlot->SetPadding(FMargin(0.0f, 0.0f, Style.Spacing / 3.0f, 0.0f));
	};
	const FBox2f Whole(FVector2f::ZeroVector, FVector2f::UnitVector);
	for (const FVeyraReportTeam& Team : Report.Teams)
	{
		AddText(Parent, Team.Title, RoleOf(EVeyraShellText::Heading));
		AddText(Parent, Team.Summary, RoleOf(EVeyraShellText::Muted));
		// The columns: the Vanguard and its player together beside the portrait, the figures, then the build (ADR-078 §5).
		const float RowInset = FMath::Max(Style.Spacing / 2.0f, Style.PanelChamfer * 0.75f);
		UHorizontalBox* Heads = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		// Inset as the rows are, so each heading stands over its column.
		AddGap(*WidgetTree, *Heads, RowInset);
		AddCell(*Heads, LOCTEXT("ColPlayer", "Vanguard and player"), Style.ReportPortraitSize + Style.Spacing + Style.ReportLabelWidth, Column);
		AddCell(*Heads, LOCTEXT("ColLevel", "Level"), Style.ReportColumnWidth, Column);
		AddCell(*Heads, LOCTEXT("ColKda", "K / D / A"), Style.ReportColumnWidth, Column);
		AddCell(*Heads, LOCTEXT("ColGold", "Gold"), Style.ReportColumnWidth, Column);
		AddCell(*Heads, LOCTEXT("ColLastHits", "Last hits"), Style.ReportLabelWidth, Column);
		AddCell(*Heads, LOCTEXT("ColBuild", "Build"), Style.ReportLabelWidth, Column);
		Parent.AddChild(Heads);
		Parent.AddChild(VeyraShellStyle::MakeRule(*WidgetTree));
		for (const FVeyraReportLine& Line : Team.Lines)
		{
			// The player's own line stands out on a raised row. Every row is inset alike, clear of the raised row's cut corner
			// (ADR-078 §3), so the columns line up.
			UBorder* RowSurface = Line.bYou ? VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(RowInset))
											: VeyraShellStyle::MakeBorder(*WidgetTree, FLinearColor::Transparent, RowInset);
			UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			RowSurface->SetContent(Stack);
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			// The Vanguard's portrait, then its name over the player's.
			UTexture2D* Hero = VeyraShellArt::HeroOf(Line.VanguardId);
			const FBox2f Face = Hero ? VeyraShellArt::Crop(Line.VanguardId, Hero->GetSizeX(), Hero->GetSizeY(), 1.0f, /*bPortrait*/ true) : Whole;
			AddArt(*Row, Hero, Face, Style.ReportPortraitSize, Line.Vanguard);
			AddGap(*WidgetTree, *Row, Style.Spacing - Style.Spacing / 3.0f);
			USizeBox* WhoCell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			WhoCell->SetWidthOverride(Style.ReportLabelWidth);
			UVerticalBox* Who = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			WhoCell->AddChild(Who);
			UTextBlock* VanguardName = VeyraShellStyle::MakeText(*WidgetTree, Line.Vanguard, EVeyraShellText::Heading);
			VanguardName->SetAutoWrapText(false);
			Who->AddChildToVerticalBox(VanguardName);
			// Another human's name opens their player menu (UX-57); the player's own, and a bot's, is only a name.
			const FString PlayerName = Line.Name.ToString();
			if (!Line.bYou && VeyraConductModels::MenuPlayer(Snapshot, PlayerName))
			{
				UVeyraShellButton* Menu = UVeyraShellButton::MakeKindNamed(*WidgetTree, EVeyraShellButtonKind::Quiet, VeyraConductModels::MenuLabel(PlayerName), Line.Name,
					[this, PlayerName] { TogglePlayerMenu(PlayerName); }, true, OpenPlayerMenu == PlayerName);
				Menu->KeepLabelOnOneLine();
				Buttons.Add(Menu);
				Who->AddChildToVerticalBox(Menu)->SetHorizontalAlignment(HAlign_Left);
			}
			else
			{
				UTextBlock* Name = VeyraShellStyle::MakeText(*WidgetTree, Line.Name, EVeyraShellText::Small);
				Name->SetAutoWrapText(false);
				Name->SetColorAndOpacity(FSlateColor(Line.bYou ? Style.AccentColor : Style.MutedTextColor));
				Who->AddChildToVerticalBox(Name);
			}
			Row->AddChildToHorizontalBox(WhoCell)->SetVerticalAlignment(VAlign_Center);
			for (const TPair<const FText*, float>& Figure : { TPair<const FText*, float>(&Line.Level, Style.ReportColumnWidth),
					 TPair<const FText*, float>(&Line.Kda, Style.ReportColumnWidth), TPair<const FText*, float>(&Line.Gold, Style.ReportColumnWidth),
					 TPair<const FText*, float>(&Line.LastHits, Style.ReportLabelWidth) })
			{
				AddCell(*Row, *Figure.Key, Figure.Value, Role);
				Cast<UHorizontalBoxSlot>(Row->GetChildAt(Row->GetChildrenCount() - 1)->Slot)->SetVerticalAlignment(VAlign_Center);
			}
			// The build as art: each item slot, then both Flux Spells; the words in each tooltip, and in the row's.
			for (const FVeyraContentId& Item : Line.ItemIds)
			{
				const FString ItemId = Item.IsValid() ? Item.ToString() : FString();
				AddArt(*Row, ItemId.IsEmpty() ? nullptr : VeyraShellArt::ItemIconOf(ItemId), Whole, Style.ReportIconSize,
					Item.IsValid() ? VeyraContentText::ItemName(Item) : FText::GetEmpty());
			}
			AddGap(*WidgetTree, *Row, Style.Spacing / 2.0f);
			for (const FVeyraContentId& Spell : Line.FluxSpellIds)
			{
				const FString SpellId = Spell.IsValid() ? Spell.ToString() : FString();
				AddArt(*Row, SpellId.IsEmpty() ? nullptr : VeyraShellArt::AbilityIconOf(SpellId), Whole, Style.ReportIconSize,
					Spell.IsValid() ? VeyraContentText::AbilityName(Spell) : FText::GetEmpty());
			}
			Row->SetToolTipText(FText::Format(LOCTEXT("ReportBuild", "{0}      {1}"), Line.Items, Line.FluxSpells));
			Stack->AddChild(Row);
			if (!Line.bYou && OpenPlayerMenu == PlayerName && VeyraConductModels::MenuPlayer(Snapshot, PlayerName))
			{
				BuildPlayerMenu(Snapshot, PlayerName, *Stack);
			}
			VeyraShellStyle::AddSpaced(Parent, *RowSurface);
		}
		AddGap(*WidgetTree, Parent, Style.Spacing);
	}
}

void UVeyraShellScreen::BuildDetails(const FVeyraMatchReport& Report, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	// A column for each player, in the scoreboard's order.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddCell(*Header, FText::GetEmpty(), Style.ReportLabelWidth, RoleOf(EVeyraShellText::Column));
	for (const FText& Column : Report.Columns)
	{
		AddCell(*Header, Column, Style.ReportColumnWidth, RoleOf(EVeyraShellText::Column));
	}
	VeyraShellStyle::AddSpaced(Parent, *Header);
	for (const FVeyraReportGroup& Group : Report.Groups)
	{
		AddText(Parent, Group.Title, RoleOf(EVeyraShellText::Heading));
		Parent.AddChild(VeyraShellStyle::MakeRule(*WidgetTree));
		for (const FVeyraReportRow& Figure : Group.Rows)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			AddCell(*Row, Figure.Label, Style.ReportLabelWidth, RoleOf(EVeyraShellText::Muted));
			for (const FText& Value : Figure.Values)
			{
				AddCell(*Row, Value, Style.ReportColumnWidth, RoleOf(EVeyraShellText::Body));
			}
			Parent.AddChild(Row);
		}
		AddGap(*WidgetTree, Parent, Style.Spacing);
	}
}

UTextBlock* UVeyraShellScreen::AddCell(UPanelWidget& Row, const FText& Text, float Width, uint8 Role)
{
	USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Cell->SetWidthOverride(Width);
	UTextBlock* Block = VeyraShellStyle::MakeText(*WidgetTree, Text, static_cast<EVeyraShellText>(Role));
	Cell->AddChild(Block);
	Row.AddChild(Cell);
	return Block;
}

void UVeyraShellScreen::ShowReportView(EVeyraReportView NewView)
{
	ReportView = NewView;
	Refresh();
}

void UVeyraShellScreen::BuildProblem(const FVeyraClientSnapshot& Snapshot)
{
	if (!Snapshot.Problem.IsSet())
	{
		return;
	}
	const UVeyraShellStyleSettings& Style = ShellStyle();
	UBorder* Banner = VeyraShellStyle::MakeBorder(*WidgetTree, Style.ProblemColor, Style.Spacing);
	Banner->SetBrush(VeyraShellStyle::EdgedFace(Style.ProblemColor, Style.EnemyColor.CopyWithNewOpacity(0.6f)));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Banner->SetContent(Row);
	AddFilling(*Row, *VeyraShellStyle::MakeText(*WidgetTree, VeyraShellModels::DescribeProblem(*Snapshot.Problem), EVeyraShellText::Body));
	// Reconnect-only offers nothing but Reconnect (UX-17), which asks the backend afresh anyway.
	if (Shown != EVeyraShellScreen::ReconnectOnly && Client->CanIssue(EVeyraClientIntent::Retry))
	{
		AddButton(*Row, LOCTEXT("Retry", "Retry"), [this] { Client->Retry(); })->KeepLabelOnOneLine();
	}
	VeyraShellStyle::AddSpaced(*Content, *Banner);
}

void UVeyraShellScreen::BuildSettingsConflict(const FVeyraClientSnapshot& Snapshot)
{
	if (!Snapshot.bSettingsConflict)
	{
		return;
	}
	const UVeyraShellStyleSettings& Style = ShellStyle();
	// Over everything, a picker included: neither copy is overwritten until the player says which (Settings & Accessibility §7).
	UBorder* Scrim = VeyraShellStyle::MakeBorder(*WidgetTree, Style.MenuScrimColor, 0.0f);
	Scrim->SetHorizontalAlignment(HAlign_Center);
	Scrim->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* ScrimSlot = Popup->AddChildToOverlay(Scrim);
	ScrimSlot->SetHorizontalAlignment(HAlign_Fill);
	ScrimSlot->SetVerticalAlignment(VAlign_Fill);
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing * 2.0f));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	USizeBox* PanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelBox->SetWidthOverride(Style.DialogWidth);
	PanelBox->AddChild(Panel);
	Scrim->SetContent(PanelBox);

	AddText(*Rows, LOCTEXT("SettingsConflictTitle", "Your settings changed on another device"), RoleOf(EVeyraShellText::Heading));
	AddText(*Rows,
		LOCTEXT("SettingsConflictDetail", "Your account's settings were saved from another device after this one changed them too. Which settings do you want to keep?"),
		RoleOf(EVeyraShellText::Body));
	UHorizontalBox* Choices = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddKindButton(*Choices, EVeyraShellButtonKind::Primary, SettingsChoiceLabel(/*bThisDevice*/ true), [this] { Client->ResolveSettingsConflict(true); })
		->KeepLabelOnOneLine();
	AddButton(*Choices, SettingsChoiceLabel(/*bThisDevice*/ false), [this] { Client->ResolveSettingsConflict(false); })->KeepLabelOnOneLine();
	Rows->AddChildToVerticalBox(Choices);
}

FText UVeyraShellScreen::SettingsChoiceLabel(bool bThisDevice)
{
	return bThisDevice ? LOCTEXT("KeepThisDevice", "This device") : LOCTEXT("KeepYourAccount", "Your account");
}

UVeyraSettingsSubsystem* UVeyraShellScreen::FindSettings() const
{
	if (UVeyraSettingsSubsystem* Settings = TestSettings.Get())
	{
		return Settings;
	}
	return UVeyraSettingsSubsystem::Get(this);
}

void UVeyraShellScreen::SetSettingsForTests(UVeyraSettingsSubsystem* Settings)
{
	TestSettings = Settings;
}

void UVeyraShellScreen::AddSettingsButton(UPanelWidget& Parent)
{
	AddKindButton(Parent, EVeyraShellButtonKind::Quiet, SettingsLabel(), [this] { OpenSettings(); }, FindSettings() != nullptr)->KeepLabelOnOneLine();
}

FText UVeyraShellScreen::SettingsLabel()
{
	return LOCTEXT("Settings", "Settings");
}

void UVeyraShellScreen::OpenSettings()
{
	UVeyraSettingsSubsystem* Settings = FindSettings();
	const bool bSettingsHere = Shown == EVeyraShellScreen::Shell || Shown == EVeyraShellScreen::Lobby || Shown == EVeyraShellScreen::Results;
	if (SettingsScreen || !Settings || !SettingsLayer || !bSettingsHere)
	{
		return;
	}
	SettingsScreen = WidgetTree->ConstructWidget<UVeyraSettingsScreen>(UVeyraSettingsScreen::StaticClass());
	UOverlaySlot* Layered = SettingsLayer->AddChildToOverlay(SettingsScreen);
	Layered->SetHorizontalAlignment(HAlign_Fill);
	Layered->SetVerticalAlignment(VAlign_Fill);
	SettingsScreen->Show(*Settings, /*bInLiveMatch*/ false, [this] { CloseSettings(); });
}

void UVeyraShellScreen::CloseSettings()
{
	if (SettingsLayer)
	{
		SettingsLayer->ClearChildren();
	}
	SettingsScreen = nullptr;
}

UTextBlock* UVeyraShellScreen::AddText(UPanelWidget& Parent, const FText& Text, uint8 Role)
{
	UTextBlock* Block = VeyraShellStyle::MakeText(*WidgetTree, Text, static_cast<EVeyraShellText>(Role));
	VeyraShellStyle::AddSpaced(Parent, *Block);
	return Block;
}

UVeyraShellButton* UVeyraShellScreen::AddButton(UPanelWidget& Parent, const FText& Label, TFunction<void()> Action, bool bEnabled, bool bSelected)
{
	return AddKindButton(Parent, EVeyraShellButtonKind::Secondary, Label, MoveTemp(Action), bEnabled, bSelected);
}

UVeyraShellButton* UVeyraShellScreen::AddKindButton(UPanelWidget& Parent, EVeyraShellButtonKind Kind, const FText& Label, TFunction<void()> Action, bool bEnabled,
	bool bSelected)
{
	UVeyraShellButton* Button = UVeyraShellButton::MakeKind(*WidgetTree, Kind, Label, MoveTemp(Action), bEnabled, bSelected);
	Buttons.Add(Button);
	// A size box holds exactly one child, which its caller sets.
	if (!Parent.IsA<USizeBox>())
	{
		VeyraShellStyle::AddSpaced(Parent, *Button);
	}
	return Button;
}

void UVeyraShellScreen::ShowPage(EVeyraShellPage NewPage)
{
	Page = NewPage;
	Refresh();
}

TArray<UVeyraShellButton*> UVeyraShellScreen::GetButtons() const
{
	TArray<UVeyraShellButton*> Out;
	for (const TObjectPtr<UVeyraShellButton>& Button : Buttons)
	{
		Out.Add(Button.Get());
	}
	return Out;
}

UVeyraShellButton* UVeyraShellScreen::FindButton(const FText& Label, int32 Occurrence) const
{
	int32 Seen = 0;
	for (const TObjectPtr<UVeyraShellButton>& Button : Buttons)
	{
		if (Button && Button->GetLabel().ToString() == Label.ToString() && Seen++ == Occurrence)
		{
			return Button.Get();
		}
	}
	return nullptr;
}

FString UVeyraShellScreen::DescribeText() const
{
	TArray<FString> Lines;
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([&Lines](UWidget* Widget) {
			if (const UTextBlock* Block = Cast<UTextBlock>(Widget))
			{
				Lines.Add(Block->GetText().ToString());
			}
		});
	}
	return FString::Join(Lines, TEXT("\n"));
}

#undef LOCTEXT_NAMESPACE
