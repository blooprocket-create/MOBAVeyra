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
#include "Components/SizeBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "VeyraUserSettings.h"
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
#include "Shell/VeyraShellSurface.h"
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
		// The friends and party panels as the player left them on this machine (ADR-083 §6).
		if (const UVeyraUserSettings* Device = UVeyraUserSettings::Get())
		{
			bFriendsCollapsed = Device->bFriendsPanelCollapsed;
			bPartyCollapsed = Device->bPartyPanelCollapsed;
		}
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
	if (FriendsBox)
	{
		const UVeyraShellStyleSettings& Style = ShellStyle();
		FriendsBox->SetWidthOverride(PanelSize(FriendsMovedAt, FriendsFrom, bFriendsCollapsed ? Style.FriendsCollapsedWidth : Style.FriendsPanelWidth));
	}
	if (TopBar)
	{
		FitTopBar(static_cast<float>(TopBar->GetCachedGeometry().GetLocalSize().X), static_cast<float>(TopBar->GetDesiredSize().X));
	}
	if (PartyBox)
	{
		// Toward the height its new panel or bar asks for, then free to follow it.
		const UWidget* Panel = PartyBox->GetContent();
		if (IsPartyMoving())
		{
			// Eased only from a height it was laid out at.
			if (const float To = Panel ? static_cast<float>(Panel->GetDesiredSize().Y) : 0.0f; To > 0.0f && PartyFrom > 0.0f)
			{
				PartyBox->SetHeightOverride(PanelSize(PartyMovedAt, PartyFrom, To));
			}
		}
		else if (PartyMovedAt > 0.0)
		{
			PartyBox->ClearHeightOverride();
			PartyMovedAt = 0.0;
		}
	}
}

float UVeyraShellScreen::PanelSize(double MovedAt, float From, float To) const
{
	const float Seconds = ShellStyle().PanelCollapseSeconds;
	if (Seconds <= 0.0f || MovedAt <= 0.0 || VeyraShellLook::Current().bStillAnimation)
	{
		return To;
	}
	const float Through = FMath::Clamp(static_cast<float>((FPlatformTime::Seconds() - MovedAt) / Seconds), 0.0f, 1.0f);
	// Eased out: quick to start, settling at its size.
	return FMath::InterpEaseOut(From, To, Through, 3.0f);
}

void UVeyraShellScreen::FitTopBar(float Has, float Wants)
{
	if (Has <= 0.0f)
	{
		return;
	}
	// One line wants more than it has: the tabs take a line of their own, until the room one line wanted comes back.
	const bool bStack = bTopBarStacked ? Has < TopBarLineWidth : Wants > Has;
	if (bStack == bTopBarStacked)
	{
		return;
	}
	if (bStack)
	{
		TopBarLineWidth = Wants;
	}
	bTopBarStacked = bStack;
	ShownSignature.Reset();
	Refresh();
}

bool UVeyraShellScreen::IsPartyMoving() const
{
	const float Seconds = ShellStyle().PanelCollapseSeconds;
	return PartyBox && PartyMovedAt > 0.0 && Seconds > 0.0f && !VeyraShellLook::Current().bStillAnimation
		&& FPlatformTime::Seconds() - PartyMovedAt < Seconds;
}

void UVeyraShellScreen::SetFriendsCollapsed(bool bCollapsed)
{
	if (bFriendsCollapsed == bCollapsed)
	{
		return;
	}
	const UVeyraShellStyleSettings& Style = ShellStyle();
	FriendsFrom = FriendsBox ? FriendsBox->GetWidthOverride() : (bFriendsCollapsed ? Style.FriendsCollapsedWidth : Style.FriendsPanelWidth);
	FriendsMovedAt = FPlatformTime::Seconds();
	bFriendsCollapsed = bCollapsed;
	if (UVeyraUserSettings* Device = UVeyraUserSettings::Get())
	{
		Device->bFriendsPanelCollapsed = bCollapsed;
		Device->SaveSettings();
	}
	Refresh();
}

void UVeyraShellScreen::SetPartyCollapsed(bool bCollapsed)
{
	if (bPartyCollapsed == bCollapsed)
	{
		return;
	}
	// From the height it has now, even partway through a move.
	PartyFrom = PartyBox ? static_cast<float>(PartyBox->GetDesiredSize().Y) : 0.0f;
	PartyMovedAt = FPlatformTime::Seconds();
	bPartyCollapsed = bCollapsed;
	if (UVeyraUserSettings* Device = UVeyraUserSettings::Get())
	{
		Device->bPartyPanelCollapsed = bCollapsed;
		Device->SaveSettings();
	}
	Refresh();
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
		// Another text size wants another width: the bar measures its one line afresh.
		bTopBarStacked = false;
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
		// The panels open or put away, and Settings on the page (ADR-083 §1, §6).
		FString::Printf(TEXT("panels %d:%d|settings page %d|"), bFriendsCollapsed ? 1 : 0, bPartyCollapsed ? 1 : 0, SettingsScreen && SettingsScreen->IsPage() ? 1 : 0) +
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
	PartyBox = nullptr;
	TopBar = nullptr;
	PageBody = nullptr;
	Shown = VeyraShellModels::ScreenFor(Snapshot.State);
	// Settings belong to the shell, the lobby and the results, never to champion select, Match Found or
	// Reconnect-only (ADR-024 §4); the choice between this device's settings and the account's comes first.
	const bool bSettingsHere = Shown == EVeyraShellScreen::Shell || Shown == EVeyraShellScreen::Lobby || Shown == EVeyraShellScreen::Results;
	if (!bSettingsHere || Snapshot.bSettingsConflict)
	{
		CloseSettings();
	}
	else if (SettingsScreen && SettingsScreen->IsPage() != (Shown == EVeyraShellScreen::Shell))
	{
		PlaceSettings();
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
	// Drifting again as soon as Reduce UI Animation is turned off, as every rebuild follows the setting.
	Motes->Wake();
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
	// While Settings is the page, no page tab is current (ADR-083 §1).
	const bool bOnPages = !(SettingsScreen && SettingsScreen->IsPage());
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
	// Navigation between ordinary pages (UX §1); a lobby holds the player until they leave it. On the bar's line, or on one
	// of their own under it where the bar's line does not fit (ADR-083 §5).
	UHorizontalBox* Tabs = bTopBarStacked ? WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()) : Bar;
	if (bPages)
	{
		AddKindButton(*Tabs, EVeyraShellButtonKind::Tab, LOCTEXT("NavHome", "Home"), [this] { ShowPage(EVeyraShellPage::Home); }, true, bOnPages && Page == EVeyraShellPage::Home)
			->KeepLabelOnOneLine();
		AddKindButton(*Tabs, EVeyraShellButtonKind::Tab, LOCTEXT("NavPlay", "Play"), [this] { ShowPage(EVeyraShellPage::Play); }, true, bOnPages && Page == EVeyraShellPage::Play)
			->KeepLabelOnOneLine();
		// Opening Match History reads its first page afresh, with the filters last used.
		AddKindButton(*Tabs, EVeyraShellButtonKind::Tab, LOCTEXT("NavHistory", "Match History"), [this] {
			ShowPage(EVeyraShellPage::History);
			Client->LoadHistory(Client->GetSnapshot().History.Filter);
		}, true, bOnPages && Page == EVeyraShellPage::History)->KeepLabelOnOneLine();
		// Opening the Collection reads it afresh, with the level and balances (ADR-045 §8).
		AddKindButton(*Tabs, EVeyraShellButtonKind::Tab, LOCTEXT("NavCollection", "Collection"), [this] {
			ShowPage(EVeyraShellPage::Collection);
			Client->LoadCollection();
		}, true, bOnPages && Page == EVeyraShellPage::Collection)->KeepLabelOnOneLine();
		// Opening the Profile page reads the choices afresh, and starts the draft from them (ADR-048 §5).
		AddKindButton(*Tabs, EVeyraShellButtonKind::Tab, VeyraProfileModels::PageLabel(), [this] {
			bProfileDraftReady = false;
			ShowPage(EVeyraShellPage::Profile);
			Client->LoadProfileSettings();
			Client->LoadDisplayName();
		}, true, bOnPages && Page == EVeyraShellPage::Profile)->KeepLabelOnOneLine();
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
	TopBar = Bar;
	Parent.AddChild(Bar);
	if (Tabs != Bar)
	{
		AddGap(*WidgetTree, Parent, Style.Spacing / 2.0f);
		Parent.AddChild(Tabs);
	}
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
	// The page keeps to its own column: what it cannot fit is cut, not drawn over the friends panel (ADR-083 §5).
	Main->SetClipping(EWidgetClipping::ClipToBounds);
	Split->AddChildToHorizontalBox(Main)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddFilling(*Main, *Body);
	PageBody = Body;
	AddGap(*WidgetTree, *Split, ShellStyle().Spacing * 2.0f);
	BuildFriends(Snapshot, *Split);
	if (SettingsScreen && SettingsScreen->IsPage())
	{
		// Settings in place of the page, between the top bar and the panels (ADR-083 §1).
		SettingsScreen->RemoveFromParent();
		AddFilling(*Body, *SettingsScreen);
	}
	else if (Page == EVeyraShellPage::Play)
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
	if (bPartyCollapsed)
	{
		BuildPartyBar(Model, Parent);
		return;
	}
	const UVeyraShellStyleSettings& Style = ShellStyle();
	UBorder* Bar = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.ScreenPadding / 2.0f, Style.Spacing));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Bar->SetContent(Rows);

	// The heading line (ADR-083 §6): the party's size, its mode or its time in the queue, who holds it back; then its
	// privacy, Leave and the way to put it away.
	UHorizontalBox* Heading = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	CentreInLine(*AddText(*Heading, PartySizeText(Model), RoleOf(EVeyraShellText::Eyebrow)));
	AddGap(*WidgetTree, *Heading, Style.Spacing);
	if (Model.bQueued)
	{
		QueueStatus = AddText(*Heading, VeyraShellModels::FormatQueueStatus(Client->GetQueuedSeconds()), RoleOf(EVeyraShellText::Body));
		QueueStatus->SetColorAndOpacity(FSlateColor(Style.AccentColor));
		QueueStatus->SetAutoWrapText(false);
		CentreInLine(*QueueStatus);
	}
	else
	{
		UTextBlock* ModeName = AddText(*Heading, Model.ModeName, RoleOf(EVeyraShellText::Muted));
		ModeName->SetAutoWrapText(false);
		CentreInLine(*ModeName);
	}
	if (Model.bHeld)
	{
		AddGap(*WidgetTree, *Heading, Style.Spacing);
		UTextBlock* Held = AddText(*Heading, Model.Status, RoleOf(EVeyraShellText::Body));
		Held->SetColorAndOpacity(FSlateColor(Style.NotReadyColor));
		Held->SetAutoWrapText(false);
		CentreInLine(*Held);
	}
	AddStretch(*WidgetTree, *Heading);
	// The leader's privacy (Parties & Social Bible §1), named for what it is and set by a press; the rest read it.
	const bool bPublic = Model.PrivacyTarget == VeyraBackendProtocol::EPartyPrivacy::Private;
	const FText PrivacyShown = bPublic ? LOCTEXT("PartyIsPublic", "Public") : LOCTEXT("PartyIsPrivate", "Private");
	if (Model.bOffersPrivacy)
	{
		const VeyraBackendProtocol::EPartyPrivacy Target = Model.PrivacyTarget;
		UVeyraShellButton* Privacy = AddNamedButton(*Heading, EVeyraShellButtonKind::Quiet, Model.PrivacyLabel, PrivacyShown,
			[this, Target] { Client->SetPartyPrivacy(Target); }, Model.bCanSetPrivacy);
		Privacy->SetToolTipText(Model.Privacy);
		CentreInLine(*Privacy);
	}
	else
	{
		UTextBlock* Privacy = AddText(*Heading, PrivacyShown, RoleOf(EVeyraShellText::Muted));
		Privacy->SetToolTipText(Model.Privacy);
		CentreInLine(*Privacy);
	}
	CentreInLine(*AddNamedButton(*Heading, EVeyraShellButtonKind::Quiet, VeyraShellModels::LeavePartyLabel(), LOCTEXT("LeavePartyShown", "Leave"), [this] {
		Client->LeaveParty();
		CloseCard();
	}, Model.bCanLeave));
	UVeyraShellButton* Collapse = AddNamedButton(*Heading, EVeyraShellButtonKind::Quiet, CollapsePartyLabel(), LOCTEXT("CollapsePartyShown", "›"), [this] { SetPartyCollapsed(true); });
	TurnLabel(*Collapse, 90.0f);
	CentreInLine(*Collapse);
	Rows->AddChildToVerticalBox(Heading);

	// The places, side by side and alike (UX-10): each member's, then the open ones up to the party's size; then Find Match
	// or Cancel, the leader's (Parties & Social Bible §2).
	UHorizontalBox* Places = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const int32 PlaceCount = FMath::Max(Model.MaxSize, Model.Cards.Num());
	const FVeyraPartyMemberModel* OpenMember = nullptr;
	for (int32 Index = 0; Index < PlaceCount; ++Index)
	{
		const FVeyraPartyMemberModel* Card = Model.Cards.IsValidIndex(Index) ? &Model.Cards[Index] : nullptr;
		if (Card && OpenCardId == MemberCardKey(Card->AccountId))
		{
			OpenMember = Card;
		}
		USizeBox* Place = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Place->SetHeightOverride(Style.PartySlotHeight);
		Place->AddChild(Card ? BuildPartyPlace(Model, *Card) : BuildOpenPlace());
		UHorizontalBoxSlot* PlaceSlot = Places->AddChildToHorizontalBox(Place);
		PlaceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PlaceSlot->SetPadding(FMargin(0.0f, 0.0f, Style.Spacing / 2.0f, 0.0f));
	}
	if (Model.bOffersFindMatch)
	{
		CentreInLine(AddKindButton(*Places, EVeyraShellButtonKind::Primary, LOCTEXT("FindMatch", "Find Match"), [this] { Client->FindMatch(); }, Model.bCanFindMatch)
				->KeepLabelOnOneLine());
	}
	if (Model.bOffersCancel)
	{
		CentreInLine(AddKindButton(*Places, EVeyraShellButtonKind::Quiet, LOCTEXT("CancelQueue", "Cancel"), [this] { Client->CancelQueue(); }, Model.bCanCancel)
				->KeepLabelOnOneLine());
	}
	Rows->AddChildToVerticalBox(Places)->SetPadding(FMargin(0.0f, Style.Spacing / 2.0f, 0.0f, 0.0f));

	// The member card the leader opened: its actions under the places. Make Party Leader asks first, naming the
	// recipient (UX-11).
	if (OpenMember && OpenMember->bOffersActions)
	{
		const FString Id = OpenMember->AccountId;
		UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (Confirm == EVeyraShellConfirm::PartyLeader && ConfirmId == Id)
		{
			UTextBlock* Prompt = AddText(*Actions, VeyraShellModels::ConfirmLeaderPrompt(OpenMember->Name), RoleOf(EVeyraShellText::Muted));
			Prompt->SetAutoWrapText(false);
			CentreInLine(*Prompt);
			AddNamedButton(*Actions, EVeyraShellButtonKind::Primary, VeyraShellModels::ConfirmLeaderLabel(OpenMember->Name), LOCTEXT("ConfirmLeader", "Confirm"),
				[this, Id] {
					Client->TransferPartyLeader(Id);
					CloseCard();
				},
				OpenMember->bCanMakeLeader);
			AddNamedButton(*Actions, EVeyraShellButtonKind::Quiet, VeyraShellModels::CancelConfirmLabel(), LOCTEXT("CancelLeader", "Cancel"),
				[this] { AskToConfirm(EVeyraShellConfirm::None, FString()); });
		}
		else
		{
			CentreInLine(*AddText(*Actions, FText::FromString(OpenMember->Name), RoleOf(EVeyraShellText::Body)));
			AddNamedButton(*Actions, EVeyraShellButtonKind::Secondary, VeyraShellModels::MakeLeaderLabel(OpenMember->Name), LOCTEXT("MakeLeader", "Make Party Leader"),
				[this, Id] { AskToConfirm(EVeyraShellConfirm::PartyLeader, Id); }, OpenMember->bCanMakeLeader);
			AddNamedButton(*Actions, EVeyraShellButtonKind::Quiet, VeyraShellModels::RemoveFromPartyLabel(OpenMember->Name), LOCTEXT("RemoveMember", "Remove"),
				[this, Id] {
					Client->KickFromParty(Id);
					CloseCard();
				},
				OpenMember->bCanRemove);
		}
		Rows->AddChildToVerticalBox(Actions)->SetPadding(FMargin(0.0f, Style.Spacing / 2.0f, 0.0f, 0.0f));
	}
	PlacePartyPanel(*Bar, Parent);
}

void UVeyraShellScreen::PlacePartyPanel(UWidget& Panel, UPanelWidget& Parent)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetClipping(EWidgetClipping::ClipToBounds);
	Box->AddChild(&Panel);
	if (USizeBoxSlot* PanelSlot = Cast<USizeBoxSlot>(Panel.Slot))
	{
		// Settled on the page's bottom edge, so it rises as it opens and sinks as it closes.
		PanelSlot->SetVerticalAlignment(VAlign_Bottom);
	}
	PartyBox = Box;
	if (IsPartyMoving() && PartyFrom > 0.0f)
	{
		// A rebuild partway through a move starts where the move is.
		Box->SetHeightOverride(PartyFrom);
	}
	AddGap(*WidgetTree, Parent, ShellStyle().Spacing);
	Parent.AddChild(Box);
}

UWidget* UVeyraShellScreen::BuildPartyPlace(const FVeyraPartyModel& Model, const FVeyraPartyMemberModel& Card)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	// A member's place (ADR-083 §6): their initial, their name with You and Leader under it, and their mark. The player's
	// own is edged in the frame's colour.
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrush(VeyraShellStyle::EdgedFace(Style.SurfaceColor, Card.bYou ? Style.FrameColor : Style.HairlineColor));
	Frame->SetPadding(FMargin(Style.Spacing, 0.0f));
	Frame->SetVerticalAlignment(VAlign_Center);
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Line);
	AddCentred(*Line, *VeyraShellStyle::MakeInitialTile(*WidgetTree, FText::FromString(Card.Name), Style.AvatarSize * 0.75f));
	AddGap(*WidgetTree, *Line, Style.Spacing / 2.0f);
	UVerticalBox* Who = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	// The name, which for the leader opens another member's card (UX-10), set as a name rather than a command; a plain
	// name is inset as a button's is, so every place reads level.
	const FMargin Inset(Style.ButtonPadding * 2.0f, 0.0f, 0.0f, 0.0f);
	if (Card.bOffersActions)
	{
		UVeyraShellButton* NameButton = AddNamedButton(*Who, EVeyraShellButtonKind::Quiet, VeyraShellModels::PartyMemberLabel(Card.Name), FText::FromString(Card.Name),
			[this, Key = MemberCardKey(Card.AccountId)] { OpenCard(Key); }, true, OpenCardId == MemberCardKey(Card.AccountId));
		NameButton->SetToolTipText(LOCTEXT("MemberCardTip", "Make Party Leader or Remove"));
		Cast<UVerticalBoxSlot>(NameButton->Slot)->SetHorizontalAlignment(HAlign_Left);
		NameButton->ShowLabelAsName(VeyraShellStyle::FontFor(EVeyraShellText::Body), Style.TextColor);
	}
	else
	{
		UTextBlock* Name = VeyraShellStyle::MakeText(*WidgetTree, FText::FromString(Card.Name), EVeyraShellText::Body);
		Name->SetAutoWrapText(false);
		Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Who->AddChildToVerticalBox(Name)->SetPadding(Inset);
	}
	const FText Tag = Card.bYou && Card.bLeader ? LOCTEXT("PlaceYouLeader", "You · Leader")
		: Card.bYou							   ? LOCTEXT("PlaceYou", "You")
		: Card.bLeader						   ? LOCTEXT("PlaceLeader", "Leader")
											   : FText::GetEmpty();
	if (!Tag.IsEmpty())
	{
		UTextBlock* TagText = VeyraShellStyle::MakeText(*WidgetTree, Tag, EVeyraShellText::Muted);
		TagText->SetAutoWrapText(false);
		if (Card.bLeader)
		{
			TagText->SetColorAndOpacity(FSlateColor(Style.FrameColor));
		}
		Who->AddChildToVerticalBox(TagText)->SetPadding(Inset);
	}
	UHorizontalBoxSlot* WhoSlot = Line->AddChildToHorizontalBox(Who);
	WhoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	WhoSlot->SetVerticalAlignment(VAlign_Center);
	// Their mark: a tick while Ready, a cross while not. The player's own sets the other by a press, under its usual name.
	const FText Mark = Card.bReady ? LOCTEXT("MarkReady", "✓") : LOCTEXT("MarkNotReady", "✗");
	const FText MarkTip = Card.bReady ? LOCTEXT("MarkReadyTip", "Ready") : LOCTEXT("MarkNotReadyTip", "Not Ready");
	UTextBlock* MarkText = nullptr;
	if (Card.bYou)
	{
		const bool bReady = Model.bReadyTarget;
		UVeyraShellButton* Toggle = AddNamedButton(*Line, EVeyraShellButtonKind::Quiet, Model.ReadyLabel, Mark, [this, bReady] { Client->SetReady(bReady); }, Model.bCanReady);
		// Why it cannot be pressed yet, when it cannot.
		Toggle->SetToolTipText(!Model.ReadyWaits.IsEmpty() ? FText::Format(LOCTEXT("MarkYouWaitsTip", "{0}. {1}"), MarkTip, Model.ReadyWaits)
				: FText::Format(Card.bReady ? LOCTEXT("MarkYouReadyTip", "{0}: press to unready") : LOCTEXT("MarkYouNotReadyTip", "{0}: press to ready up"), MarkTip));
		CentreInLine(*Toggle);
		MarkText = Cast<UTextBlock>(Toggle->GetContent());
	}
	else
	{
		MarkText = AddText(*Line, Mark, RoleOf(EVeyraShellText::Body));
		MarkText->SetToolTipText(MarkTip);
		CentreInLine(*MarkText);
	}
	if (MarkText)
	{
		MarkText->SetFont(VeyraShellStyle::FontFor(EVeyraShellText::Heading));
		MarkText->SetColorAndOpacity(FSlateColor(Card.bReady ? Style.ReadyColor : Style.NotReadyColor));
	}
	return Frame;
}

UWidget* UVeyraShellScreen::BuildOpenPlace()
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	// An open place (ADR-083 §6): hollow and dashed, and a way to the friends to invite into it.
	UVeyraShellSurface* Frame = WidgetTree->ConstructWidget<UVeyraShellSurface>(UVeyraShellSurface::StaticClass());
	Frame->SetBrush(FSlateNoResource());
	Frame->SetDash(FVeyraShellDash{ Style.HairlineColor, Style.OpenPlaceDashLength, Style.OpenPlaceDashGap });
	Frame->SetPadding(FMargin(0.0f));
	Frame->SetHorizontalAlignment(HAlign_Center);
	Frame->SetVerticalAlignment(VAlign_Center);
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Line);
	AddNamedButton(*Line, EVeyraShellButtonKind::Quiet, InviteFriendsLabel(), LOCTEXT("OpenPlaceShown", "+ Invite"), [this] {
		SetFriendsCollapsed(false);
		if (FriendNameBox)
		{
			FriendNameBox->SetKeyboardFocus();
		}
	})->KeepLabelOnOneLine();
	return Frame;
}

FText UVeyraShellScreen::PartySizeText(const FVeyraPartyModel& Model)
{
	return Model.MaxSize > 0 ? FText::Format(LOCTEXT("PartySizeOf", "Party · {0}/{1}"), FText::AsNumber(Model.Cards.Num()), FText::AsNumber(Model.MaxSize))
							 : FText::Format(LOCTEXT("PartyTitleCount", "Party · {0}"), FText::AsNumber(Model.Cards.Num()));
}

void UVeyraShellScreen::CentreInLine(UWidget& Widget)
{
	if (UHorizontalBoxSlot* Line = Cast<UHorizontalBoxSlot>(Widget.Slot))
	{
		Line->SetVerticalAlignment(VAlign_Center);
	}
}

void UVeyraShellScreen::AddCentred(UHorizontalBox& Line, UWidget& Widget)
{
	Line.AddChildToHorizontalBox(&Widget)->SetVerticalAlignment(VAlign_Center);
}

void UVeyraShellScreen::TurnLabel(UVeyraShellButton& Button, float Degrees)
{
	// The font's own angle, turned to point down or up: it has no arrow of its own that way.
	if (UWidget* Label = Button.GetContent())
	{
		Label->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Label->SetRenderTransformAngle(Degrees);
	}
}

void UVeyraShellScreen::BuildPartyBar(const FVeyraPartyModel& Model, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	UBorder* Bar = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.ScreenPadding / 2.0f, 0.0f));
	USizeBox* Height = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Height->SetHeightOverride(Style.PartyCollapsedHeight);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Height->AddChild(Row);
	Bar->SetContent(Height);
	// Put away (ADR-083 §6): the party's size, each member's initial with their mark, then its queue or its mode.
	CentreInLine(*AddText(*Row, PartySizeText(Model), RoleOf(EVeyraShellText::Eyebrow)));
	AddGap(*WidgetTree, *Row, Style.Spacing);
	for (const FVeyraPartyMemberModel& Card : Model.Cards)
	{
		AddCentred(*Row, *VeyraShellStyle::MakeInitialTile(*WidgetTree, FText::FromString(Card.Name), Style.AvatarSize * 0.75f));
		UTextBlock* Mark = AddText(*Row, Card.bReady ? LOCTEXT("MarkReady", "✓") : LOCTEXT("MarkNotReady", "✗"), RoleOf(EVeyraShellText::Body));
		Mark->SetFont(VeyraShellStyle::FontFor(EVeyraShellText::Heading));
		Mark->SetColorAndOpacity(FSlateColor(Card.bReady ? Style.ReadyColor : Style.NotReadyColor));
		Mark->SetToolTipText(FText::Format(LOCTEXT("BarMarkTip", "{0}: {1}"), FText::FromString(Card.Name),
			Card.bReady ? LOCTEXT("MarkReadyTip", "Ready") : LOCTEXT("MarkNotReadyTip", "Not Ready")));
		CentreInLine(*Mark);
		AddGap(*WidgetTree, *Row, Style.Spacing);
	}
	if (Model.bQueued)
	{
		QueueStatus = AddText(*Row, VeyraShellModels::FormatQueueStatus(Client->GetQueuedSeconds()), RoleOf(EVeyraShellText::Body));
		QueueStatus->SetColorAndOpacity(FSlateColor(Style.AccentColor));
		QueueStatus->SetAutoWrapText(false);
		CentreInLine(*QueueStatus);
	}
	else
	{
		UTextBlock* Line = AddText(*Row, Model.bHeld ? Model.Status : Model.ModeName, RoleOf(EVeyraShellText::Muted));
		Line->SetAutoWrapText(false);
		CentreInLine(*Line);
	}
	AddStretch(*WidgetTree, *Row);
	UVeyraShellButton* Open = AddNamedButton(*Row, EVeyraShellButtonKind::Quiet, OpenPartyLabel(), LOCTEXT("OpenPartyShown", "›"), [this] { SetPartyCollapsed(false); });
	TurnLabel(*Open, -90.0f);
	CentreInLine(*Open);
	PlacePartyPanel(*Bar, Parent);
}

FText UVeyraShellScreen::CollapseFriendsLabel()
{
	return LOCTEXT("CollapseFriends", "Collapse Friends");
}

FText UVeyraShellScreen::OpenFriendsLabel()
{
	return LOCTEXT("OpenFriends", "Open Friends");
}

FText UVeyraShellScreen::CollapsePartyLabel()
{
	return LOCTEXT("CollapseParty", "Collapse Party");
}

FText UVeyraShellScreen::OpenPartyLabel()
{
	return LOCTEXT("OpenParty", "Open Party");
}

FText UVeyraShellScreen::InviteFriendsLabel()
{
	return LOCTEXT("InviteFriends", "Invite Friends");
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
				// A name, as the other rows' are (ADR-083 §6).
				Menu->ShowLabelAsName(VeyraShellStyle::FontFor(EVeyraShellText::Small), Style.TextColor);
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
	const bool bShown = SettingsScreen && SettingsScreen->IsPage();
	AddKindButton(Parent, bShown ? EVeyraShellButtonKind::Tab : EVeyraShellButtonKind::Quiet, SettingsLabel(), [this] { OpenSettings(); },
		FindSettings() != nullptr, bShown)->KeepLabelOnOneLine();
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
	PlaceSettings();
}

void UVeyraShellScreen::PlaceSettings()
{
	UVeyraSettingsSubsystem* Settings = FindSettings();
	if (!SettingsScreen || !Settings)
	{
		return;
	}
	// A page of the shell, or a window over the lobby and the results (ADR-083 §1).
	const bool bPage = Shown == EVeyraShellScreen::Shell;
	SettingsScreen->RemoveFromParent();
	if (!bPage)
	{
		UOverlaySlot* Layered = SettingsLayer->AddChildToOverlay(SettingsScreen);
		Layered->SetHorizontalAlignment(HAlign_Fill);
		Layered->SetVerticalAlignment(VAlign_Fill);
	}
	SettingsScreen->Show(*Settings, /*bInLiveMatch*/ false, [this] {
		const bool bWasPage = SettingsScreen && SettingsScreen->IsPage();
		CloseSettings();
		if (bWasPage)
		{
			Refresh();
		}
	}, bPage);
	if (bPage)
	{
		Refresh();
	}
}

void UVeyraShellScreen::CloseSettings()
{
	if (SettingsLayer)
	{
		SettingsLayer->ClearChildren();
	}
	if (SettingsScreen)
	{
		SettingsScreen->RemoveFromParent();
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
	// Another page leaves the Settings page (ADR-083 §1).
	if (SettingsScreen && SettingsScreen->IsPage())
	{
		CloseSettings();
	}
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
