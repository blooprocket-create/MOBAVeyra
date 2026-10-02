// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Client/VeyraClientIntents.h"
#include "Components/Border.h"
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
#include "Shell/VeyraShellArt.h"
#include "Shell/VeyraShellButton.h"
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
		Gradients = { FromLeft, FromBottom, FromTop };
		ScrimLeft = VeyraShellStyle::MakeGradient(*WidgetTree, FromLeft, Style.ScrimColor);
		ScrimBottom = VeyraShellStyle::MakeGradient(*WidgetTree, FromBottom, Style.ScrimColor);
		ScrimTop = VeyraShellStyle::MakeGradient(*WidgetTree, FromTop, Style.ScrimColor);
		for (UImage* Scrim : { ScrimLeft.Get(), ScrimBottom.Get(), ScrimTop.Get() })
		{
			Scrim->SetVisibility(ESlateVisibility::Collapsed);
			Fill(Scrim);
		}
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
	Unbind();
	Client = &InClient;
	ChangedHandle = Client->OnChanged().AddUObject(this, &UVeyraShellScreen::Refresh);
	ShownSignature.Reset();
	Refresh();
}

void UVeyraShellScreen::Unbind()
{
	if (Client)
	{
		Client->OnChanged().Remove(ChangedHandle);
	}
	ChangedHandle.Reset();
	Client = nullptr;
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
	// A draft turn of the player's own asks for their attention once, as it begins (UX-31, UX-32).
	if (const FString Turn = VeyraShellModels::PlayersTurn(Snapshot); Turn != AttendedTurn)
	{
		AttendedTurn = Turn;
		if (!Turn.IsEmpty())
		{
			DrawTurnAttention();
		}
	}
	const FString Signature = FString::Printf(TEXT("page %d|spells %d|abilities %d|report %d|bots %s%d:%s|card %s|confirm %d:%s|select chat %d|"),
								  static_cast<int32>(Page), OpenSpellSlot, bShowAbilities ? 1 : 0, static_cast<int32>(ReportView), *BotPickerSide, BotPickerIndex,
								  *BotDifficulty, *OpenCardId, static_cast<int32>(Confirm), *ConfirmId, bSelectChatHidden ? 1 : 0) +
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
	const TSharedPtr<SWidget> Widget = GetCachedWidget();
	if (FSlateApplication::IsInitialized() && Widget.IsValid())
	{
		if (const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()))
		{
			Window->BringToFront(/*bForce*/ true);
			if (!Window->IsActive())
			{
				Window->DrawAttention(FWindowDrawAttentionParameters(EWindowDrawAttentionRequestType::UntilActivated));
			}
		}
	}
	// One brief, distinct cue, made from the style's tones as it plays: each fades in and out over its
	// length, so it neither clicks nor rings on (UX-32).
	const UVeyraShellStyleSettings& Style = ShellStyle();
	UWorld* World = GetWorld();
	if (!World || !FApp::CanEverRenderAudio() || Style.TurnCueTonesHz.IsEmpty())
	{
		return;
	}
	constexpr int32 SampleRate = UVeyraShellStyleSettings::TurnCueSampleRate;
	const int32 ToneSamples = FMath::Max(1, FMath::RoundToInt32(Style.TurnCueToneSeconds * SampleRate));
	TArray<int16> Samples;
	Samples.Reserve(ToneSamples * Style.TurnCueTonesHz.Num());
	for (const float Hz : Style.TurnCueTonesHz)
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
	UGameplayStatics::PlaySound2D(World, Cue, Style.TurnCueVolume);
}

void UVeyraShellScreen::Rebuild(const FVeyraClientSnapshot& Snapshot)
{
	// A player typing a friend's name keeps typing across a rebuild, into the field that replaces it.
	const bool bRefocusFriendName = FriendNameBox && FriendNameBox->HasKeyboardFocus();
	FriendNameBox = nullptr;
	// The same for a player typing a chat message while lines arrive (ADR-046 §6).
	const bool bRefocusChat = ChatBox && ChatBox->HasKeyboardFocus();
	ChatBox = nullptr;
	// And for a player writing a report's details.
	const bool bRefocusReport = ReportDetailsBox && ReportDetailsBox->HasKeyboardFocus();
	ReportDetailsBox = nullptr;
	ReportDetailsCount = nullptr;
	ChatScroll = nullptr;
	ChatRecipient = nullptr;
	Content->ClearChildren();
	Popup->ClearChildren();
	Buttons.Reset();
	PickBars.Reset();
	Countdown = nullptr;
	QueueStatus = nullptr;
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
	for (UImage* Scrim : { ScrimLeft.Get(), ScrimBottom.Get(), ScrimTop.Get() })
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
	BuildProblem(Snapshot);
	BuildSettingsConflict(Snapshot);
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
	AddText(*Content, LOCTEXT("StarterEyebrow", "Your first Vanguard"), RoleOf(EVeyraShellText::Eyebrow));
	AddText(*Content, LOCTEXT("StarterTitle", "Choose Your Starter"), RoleOf(EVeyraShellText::Display));
	AddText(*Content, LOCTEXT("StarterDetail", "The tutorial is coming later. Choose a starter Vanguard: it is yours to keep."), RoleOf(EVeyraShellText::Muted));
	UHorizontalBox* Cards = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const bool bCanChoose = Client->CanIssue(EVeyraClientIntent::ChooseStarter);
	for (const FString& Starter : Snapshot.Starters)
	{
		const FVeyraContentId Id = FVeyraContentId::FromText(Starter).Get(FVeyraContentId());
		const TArray<TPair<FText, uint8>> Plate = {
			{ VeyraShellModels::VanguardNameOf(Starter), RoleOf(EVeyraShellText::Heading) },
			{ VeyraContentText::VanguardTitle(Id), RoleOf(EVeyraShellText::Muted) },
		};
		AddArtCard(*Cards, VeyraShellModels::VanguardNameOf(Starter), Starter, Plate, [this, Starter] { Client->ChooseStarter(Starter); }, bCanChoose, false);
	}
	VeyraShellStyle::AddSpaced(*Content, *Cards);
}

void UVeyraShellScreen::BuildTopBar(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent, bool bPages)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	UHorizontalBox* Bar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	// The name, in the display tier at a smaller size and wider tracking.
	UTextBlock* Mark = VeyraShellStyle::MakeText(*WidgetTree, LOCTEXT("Wordmark", "Veyra"), EVeyraShellText::Display);
	FSlateFontInfo MarkFont = VeyraShellStyle::FontFor(EVeyraShellText::Display);
	MarkFont.Size = Style.TitleFontSize;
	MarkFont.LetterSpacing = Style.EyebrowLetterSpacing * 2;
	Mark->SetFont(MarkFont);
	Mark->SetAutoWrapText(false);
	VeyraShellStyle::AddSpaced(*Bar, *Mark);
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
	}
	AddStretch(*WidgetTree, *Bar);
	AddProgressionReadout(Snapshot, *Bar);
	UTextBlock* Player = AddText(*Bar, FText::Format(LOCTEXT("SignedInAs", "Signed in as {0}"), FText::FromString(Snapshot.DisplayName)), RoleOf(EVeyraShellText::Muted));
	Player->SetAutoWrapText(false);
	AddSettingsButton(*Bar);
	AddKindButton(*Bar, EVeyraShellButtonKind::Quiet, LOCTEXT("Quit", "Quit"), [this] { Client->Quit(); })->KeepLabelOnOneLine();
	Parent.AddChild(Bar);
	AddGap(*WidgetTree, Parent, Style.Spacing);
	Parent.AddChild(VeyraShellStyle::MakeRule(*WidgetTree));
	AddGap(*WidgetTree, Parent, Style.ScreenPadding);
}

void UVeyraShellScreen::BuildShell(const FVeyraClientSnapshot& Snapshot)
{
	BuildTopBar(Snapshot, *Content);
	if (const FText Notice = VeyraShellModels::DescribeNotice(Snapshot.Notice); !Notice.IsEmpty())
	{
		UBorder* Banner = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(ShellStyle().Spacing));
		Banner->SetContent(VeyraShellStyle::MakeText(*WidgetTree, Notice, EVeyraShellText::Body));
		VeyraShellStyle::AddSpaced(*Content, *Banner);
	}
	// The page, and the friends panel down the right (Art Bible §7).
	UHorizontalBox* Split = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddFilling(*Content, *Split);
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Split->AddChildToHorizontalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
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
	else
	{
		BuildHome(Snapshot, *Body);
	}
	BuildParty(Snapshot, *Content);
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
		AddText(Parent, Model.Opened->Headline, RoleOf(EVeyraShellText::Title));
		for (const FText& Line : Model.Opened->Lines)
		{
			AddText(Parent, Line, RoleOf(EVeyraShellText::Muted));
		}
		BuildReport(Snapshot, Model.Opened->Report, Parent);
		return;
	}
	AddText(Parent, LOCTEXT("HistoryEyebrow", "Your record"), RoleOf(EVeyraShellText::Eyebrow));
	AddText(Parent, LOCTEXT("HistoryTitle", "Match History"), RoleOf(EVeyraShellText::Display));
	AddHistoryFilter(Parent, Model.Vanguards, [](VeyraBackendProtocol::FHistoryFilter& Filter, const FString& Value) { Filter.VanguardId = Value; });
	AddHistoryFilter(Parent, Model.Modes, [](VeyraBackendProtocol::FHistoryFilter& Filter, const FString& Value) { Filter.Mode = Value; });
	AddHistoryFilter(Parent, Model.Outcomes, [](VeyraBackendProtocol::FHistoryFilter& Filter, const FString& Value) { Filter.Outcome = Value; });
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
		UBorder* Card = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Panel, FMargin(Style.Spacing));
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Card->SetContent(Line);
		const FString MatchId = Row.MatchId;
		AddButton(*Line, LOCTEXT("OpenMatch", "Open"), [this, MatchId] { Client->OpenHistoryMatch(MatchId); }, bCanOpen)->KeepLabelOnOneLine();
		AddText(*Line, Row.Summary, RoleOf(EVeyraShellText::Body))->SetAutoWrapText(false);
		VeyraShellStyle::AddSpaced(*List, *Card);
	}
	// Older records come in batches (UX-67).
	if (Model.bOffersLoadMore)
	{
		AddButton(*List, LOCTEXT("LoadMore", "Load More"), [this] { Client->LoadMoreHistory(); });
	}
}

void UVeyraShellScreen::AddHistoryFilter(UPanelWidget& Parent, const TArray<FVeyraHistoryOption>& Options,
	TFunction<void(VeyraBackendProtocol::FHistoryFilter&, const FString&)> Apply)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const bool bCanLoad = Client->CanIssue(EVeyraClientIntent::LoadHistory);
	for (const FVeyraHistoryOption& Option : Options)
	{
		const FString Value = Option.Value;
		AddKindButton(*Row, EVeyraShellButtonKind::Tab, Option.Label, [this, Value, Apply] {
			VeyraBackendProtocol::FHistoryFilter Filter = Client->GetSnapshot().History.Filter;
			Apply(Filter, Value);
			Client->LoadHistory(Filter);
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
	UVerticalBox* Members = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (const FVeyraPartyMemberModel& Card : Model.Cards)
	{
		if (!Card.bOffersActions)
		{
			AddText(*Members, Card.Line, RoleOf(EVeyraShellText::Body))->SetAutoWrapText(false);
			continue;
		}
		const FString Id = Card.AccountId;
		const FString Key = MemberCardKey(Id);
		const bool bOpen = OpenCardId == Key;
		AddNamedButton(*Members, EVeyraShellButtonKind::Quiet, VeyraShellModels::PartyMemberLabel(Card.Name), Card.Line, [this, Key] { OpenCard(Key); }, true, bOpen);
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
		VeyraShellStyle::AddSpaced(*Members, *Actions);
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
	const auto Centre = [](UTextBlock* Text) {
		Text->SetJustification(ETextJustify::Center);
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
	// Looking into the world, with Play the clear way in (Art Bible §6.1).
	ShowShowcase(FeaturedVanguard);
	UHorizontalBox* Split = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddFilling(Parent, *Split);

	USizeBox* ColumnWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ColumnWidth->SetWidthOverride(Style.HomeColumnWidth);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ColumnWidth->AddChild(Column);
	Split->AddChildToHorizontalBox(ColumnWidth)->SetVerticalAlignment(VAlign_Fill);
	AddStretch(*WidgetTree, *Column);
	AddText(*Column, LOCTEXT("HomeEyebrow", "The Meridian Crucible"), RoleOf(EVeyraShellText::Eyebrow));
	AddText(*Column, FText::Format(LOCTEXT("HomeWelcome", "Welcome, {0}."), FText::FromString(Snapshot.DisplayName)), RoleOf(EVeyraShellText::Display));
	AddText(*Column, LOCTEXT("HomeLead", "Every Vanguard answers the Crucible's call. Choose a mode, then the Vanguard you will be."),
		RoleOf(EVeyraShellText::Body));
	AddGap(*WidgetTree, *Column, Style.Spacing);
	AddKindButton(*Column, EVeyraShellButtonKind::Primary, LOCTEXT("HomePlay", "Play"), [this] { ShowPage(EVeyraShellPage::Play); });
	AddStretch(*WidgetTree, *Column);
	AddStretch(*WidgetTree, *Column);

	// The Vanguard the art shows, named in the corner.
	AddStretch(*WidgetTree, *Split);
	UVerticalBox* Caption = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const FVeyraContentId Featured = FVeyraContentId::FromText(FeaturedVanguard).Get(FVeyraContentId());
	AddText(*Caption, LOCTEXT("HomeFeatured", "Featured Vanguard"), RoleOf(EVeyraShellText::Eyebrow))->SetJustification(ETextJustify::Right);
	AddText(*Caption, VeyraShellModels::VanguardNameOf(FeaturedVanguard), RoleOf(EVeyraShellText::Title))->SetJustification(ETextJustify::Right);
	AddText(*Caption, VeyraContentText::VanguardTitle(Featured), RoleOf(EVeyraShellText::Muted))->SetJustification(ETextJustify::Right);
	UHorizontalBoxSlot* CaptionSlot = Split->AddChildToHorizontalBox(Caption);
	CaptionSlot->SetVerticalAlignment(VAlign_Bottom);
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
	TFunction<void()> Action, bool bEnabled, bool bSelected)
{
	const UVeyraShellStyleSettings& Style = ShellStyle();
	const FVector2D Size(Style.ModeCardWidth, Style.ModeCardHeight);
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
	AddSettingsButton(*Header);
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
	for (const FVeyraReportTeam& Team : Report.Teams)
	{
		AddText(Parent, Team.Title, RoleOf(EVeyraShellText::Heading));
		AddText(Parent, Team.Summary, RoleOf(EVeyraShellText::Muted));
		UHorizontalBox* Heads = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddCell(*Heads, LOCTEXT("ColVanguard", "Vanguard"), Style.ReportColumnWidth, Column);
		AddCell(*Heads, LOCTEXT("ColPlayer", "Player"), Style.ReportLabelWidth, Column);
		AddCell(*Heads, LOCTEXT("ColLevel", "Level"), Style.ReportColumnWidth, Column);
		AddCell(*Heads, LOCTEXT("ColKda", "K / D / A"), Style.ReportColumnWidth, Column);
		AddCell(*Heads, LOCTEXT("ColGold", "Gold"), Style.ReportColumnWidth, Column);
		AddCell(*Heads, LOCTEXT("ColLastHits", "Last hits"), Style.ReportLabelWidth, Column);
		Parent.AddChild(Heads);
		Parent.AddChild(VeyraShellStyle::MakeRule(*WidgetTree));
		for (const FVeyraReportLine& Line : Team.Lines)
		{
			// The player's own line stands out on a raised row.
			UBorder* RowSurface = Line.bYou ? VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(0.0f, Style.Spacing / 2.0f))
											: VeyraShellStyle::MakeBorder(*WidgetTree, FLinearColor::Transparent, Style.Spacing / 2.0f);
			UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			RowSurface->SetContent(Stack);
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			AddCell(*Row, Line.Vanguard, Style.ReportColumnWidth, RoleOf(EVeyraShellText::Heading));
			// Another human's name opens their player menu (UX-57); the player's own, and a bot's, is only a name.
			const FString PlayerName = Line.Name.ToString();
			if (!Line.bYou && VeyraConductModels::MenuPlayer(Snapshot, PlayerName))
			{
				USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				Cell->SetWidthOverride(Style.ReportLabelWidth);
				Row->AddChild(Cell);
				AddNamedButton(*Cell, EVeyraShellButtonKind::Quiet, VeyraConductModels::MenuLabel(PlayerName), Line.Name, [this, PlayerName] { TogglePlayerMenu(PlayerName); },
					true, OpenPlayerMenu == PlayerName);
			}
			else
			{
				UTextBlock* Name = AddCell(*Row, Line.Name, Style.ReportLabelWidth, Role);
				if (Line.bYou)
				{
					Name->SetColorAndOpacity(Style.AccentColor);
				}
			}
			AddCell(*Row, Line.Level, Style.ReportColumnWidth, Role);
			AddCell(*Row, Line.Kda, Style.ReportColumnWidth, Role);
			AddCell(*Row, Line.Gold, Style.ReportColumnWidth, Role);
			AddCell(*Row, Line.LastHits, Style.ReportLabelWidth, Role);
			Stack->AddChild(Row);
			UHorizontalBox* Build = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			AddCell(*Build, FText::GetEmpty(), Style.ReportColumnWidth, RoleOf(EVeyraShellText::Small));
			// One line, so neither half wraps in a column of its own.
			UTextBlock* Loadout = AddText(*Build, FText::Format(LOCTEXT("ReportBuild", "{0}      {1}"), Line.Items, Line.FluxSpells), RoleOf(EVeyraShellText::Small));
			Loadout->SetAutoWrapText(false);
			Loadout->SetColorAndOpacity(FSlateColor(Style.MutedTextColor));
			Stack->AddChild(Build);
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
	Banner->SetBrush(FSlateRoundedBoxBrush(Style.ProblemColor, Style.PanelCornerRadius, Style.EnemyColor.CopyWithNewOpacity(0.6f), 1.0f));
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
