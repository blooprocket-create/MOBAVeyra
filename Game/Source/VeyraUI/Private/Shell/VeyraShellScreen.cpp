// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraShellScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Client/VeyraClientIntents.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"

#define LOCTEXT_NAMESPACE "VeyraShell"

namespace
{
	using VeyraShellStyle::EVeyraShellText;
}

bool UVeyraShellScreen::Initialize()
{
	const bool bFirst = Super::Initialize();
	if (bFirst && WidgetTree && !WidgetTree->RootWidget)
	{
		const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
		// In layers: the background, a Vanguard's art over it in champion select, the content, and
		// champion select's picker over everything.
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
		UBorder* Frame = VeyraShellStyle::MakeBorder(*WidgetTree, FLinearColor::Transparent, Style.ScreenPadding);
		Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Frame->SetContent(Content);
		Fill(Frame);
		Popup = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		// Empty, it lets clicks through to the content under it.
		Popup->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Fill(Popup);
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
	const FString Signature = FString::Printf(TEXT("page %d|spells %d|abilities %d|report %d|"), static_cast<int32>(Page), OpenSpellSlot, bShowAbilities ? 1 : 0,
								  static_cast<int32>(ReportView)) +
		VeyraShellModels::Signature(Snapshot);
	if (Signature == ShownSignature)
	{
		return;
	}
	ShownSignature = Signature;
	Rebuild(Snapshot);
}

void UVeyraShellScreen::Rebuild(const FVeyraClientSnapshot& Snapshot)
{
	Content->ClearChildren();
	Popup->ClearChildren();
	Buttons.Reset();
	PickBars.Reset();
	Countdown = nullptr;
	QueueStatus = nullptr;
	Shown = VeyraShellModels::ScreenFor(Snapshot.State);
	if (Shown != EVeyraShellScreen::ChampionSelect)
	{
		// The picker and the abilities belong to one select; the art to champion select alone.
		OpenSpellSlot = INDEX_NONE;
		bShowAbilities = false;
		ShowBackdrop(nullptr);
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
}

void UVeyraShellScreen::BuildStatus(const FVeyraClientSnapshot& Snapshot)
{
	const FVeyraStatusModel Model = VeyraShellModels::DescribeStatus(Snapshot);
	AddText(*Content, Model.Title, static_cast<uint8>(EVeyraShellText::Title));
	AddText(*Content, Model.Detail, static_cast<uint8>(EVeyraShellText::Body));
	// Match Starting names the confirmed Vanguard (UX-40).
	const VeyraBackendProtocol::FSelectSeat* You = Snapshot.Select.FindYou();
	if ((Snapshot.State == EVeyraClientState::MatchStarting || Snapshot.State == EVeyraClientState::Connecting) && You && !You->Locked.IsEmpty())
	{
		AddText(*Content, FText::Format(LOCTEXT("StatusVanguard", "Your Vanguard: {0}"), VeyraShellModels::VanguardNameOf(You->Locked)), static_cast<uint8>(EVeyraShellText::Muted));
	}
}

void UVeyraShellScreen::BuildStopped(const FVeyraClientSnapshot& Snapshot)
{
	const FVeyraStatusModel Model = VeyraShellModels::DescribeStatus(Snapshot);
	AddText(*Content, Model.Title, static_cast<uint8>(EVeyraShellText::Title));
	AddText(*Content, Model.Detail, static_cast<uint8>(EVeyraShellText::Body));
	AddButton(*Content, LOCTEXT("Quit", "Quit"), [this] { Client->Quit(); });
}

void UVeyraShellScreen::BuildStarterChoice(const FVeyraClientSnapshot& Snapshot)
{
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	AddText(*Content, LOCTEXT("StarterTitle", "Choose Your Starter"), static_cast<uint8>(EVeyraShellText::Title));
	AddText(*Content, LOCTEXT("StarterDetail", "The tutorial is coming later. Choose a starter Vanguard: it is yours to keep."),
		static_cast<uint8>(EVeyraShellText::Body));
	UWrapBox* Cards = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	const bool bCanChoose = Client->CanIssue(EVeyraClientIntent::ChooseStarter);
	for (const FString& Starter : Snapshot.Starters)
	{
		USizeBox* Card = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Card->SetWidthOverride(Style.CardWidth);
		Card->SetHeightOverride(Style.CardHeight);
		Card->AddChild(AddButton(*Card, VeyraShellModels::VanguardNameOf(Starter), [this, Starter] { Client->ChooseStarter(Starter); }, bCanChoose));
		VeyraShellStyle::AddSpaced(*Cards, *Card);
	}
	VeyraShellStyle::AddSpaced(*Content, *Cards);
}

void UVeyraShellScreen::BuildShell(const FVeyraClientSnapshot& Snapshot)
{
	// Navigation between ordinary pages (UX §1). Only Home and Play exist so far.
	UHorizontalBox* Pages = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddButton(*Pages, LOCTEXT("NavHome", "Home"), [this] { ShowPage(EVeyraShellPage::Home); }, true, Page == EVeyraShellPage::Home);
	AddButton(*Pages, LOCTEXT("NavPlay", "Play"), [this] { ShowPage(EVeyraShellPage::Play); }, true, Page == EVeyraShellPage::Play);
	AddButton(*Pages, LOCTEXT("Quit", "Quit"), [this] { Client->Quit(); });
	VeyraShellStyle::AddSpaced(*Content, *Pages);
	AddText(*Content, FText::Format(LOCTEXT("SignedInAs", "Signed in as {0}"), FText::FromString(Snapshot.DisplayName)), static_cast<uint8>(EVeyraShellText::Muted));
	if (const FText Notice = VeyraShellModels::DescribeNotice(Snapshot.Notice); !Notice.IsEmpty())
	{
		AddText(*Content, Notice, static_cast<uint8>(EVeyraShellText::Body));
	}
	BuildParty(Snapshot, *Content);
	if (Page == EVeyraShellPage::Play)
	{
		BuildPlay(Snapshot, *Content);
	}
	else
	{
		BuildHome(Snapshot, *Content);
	}
}

void UVeyraShellScreen::BuildParty(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	const FVeyraPartyModel Model = VeyraShellModels::DescribeParty(Snapshot, Client->CanIssue(EVeyraClientIntent::SetReady),
		Client->CanIssue(EVeyraClientIntent::FindMatch), Client->CanIssue(EVeyraClientIntent::CancelQueue));
	if (!Model.bShown)
	{
		return;
	}
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	UBorder* Panel = VeyraShellStyle::MakeBorder(*WidgetTree, Style.PanelColor, Style.Spacing);
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	AddText(*Rows, LOCTEXT("PartyTitle", "Party"), static_cast<uint8>(EVeyraShellText::Heading));
	AddText(*Rows, Model.Mode, static_cast<uint8>(EVeyraShellText::Body));
	for (const FText& Member : Model.Members)
	{
		AddText(*Rows, Member, static_cast<uint8>(EVeyraShellText::Body));
	}
	if (Model.bQueued)
	{
		QueueStatus = AddText(*Rows, VeyraShellModels::FormatQueueStatus(Client->GetQueuedSeconds()), static_cast<uint8>(EVeyraShellText::Heading));
	}
	else if (!Model.Status.IsEmpty())
	{
		AddText(*Rows, Model.Status, static_cast<uint8>(EVeyraShellText::Muted));
	}

	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (!Model.bQueued)
	{
		const bool bReady = Model.bReadyTarget;
		AddButton(*Actions, Model.ReadyLabel, [this, bReady] { Client->SetReady(bReady); }, Model.bCanReady);
	}
	if (Model.bOffersFindMatch)
	{
		AddButton(*Actions, LOCTEXT("FindMatch", "Find Match"), [this] { Client->FindMatch(); }, Model.bCanFindMatch);
	}
	if (Model.bOffersCancel)
	{
		AddButton(*Actions, LOCTEXT("CancelQueue", "Cancel"), [this] { Client->CancelQueue(); }, Model.bCanCancel);
	}
	VeyraShellStyle::AddSpaced(*Rows, *Actions);
	VeyraShellStyle::AddSpaced(Parent, *Panel);
}

void UVeyraShellScreen::BuildMatchFound(const FVeyraClientSnapshot& Snapshot)
{
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	const bool bCanAnswer = Client->CanIssue(EVeyraClientIntent::AcceptMatch);
	const FVeyraMatchFoundModel Model = VeyraShellModels::DescribeMatchFound(Snapshot, Client->GetRemainingAcceptSeconds(), bCanAnswer);
	UBorder* Panel = VeyraShellStyle::MakeBorder(*WidgetTree, Style.PanelColor, Style.ScreenPadding);
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	AddText(*Rows, Model.Title, static_cast<uint8>(EVeyraShellText::Title));
	AddText(*Rows, Model.Mode, static_cast<uint8>(EVeyraShellText::Heading));
	Countdown = AddText(*Rows, Model.Countdown, static_cast<uint8>(EVeyraShellText::Countdown));
	AddText(*Rows, Model.Progress, static_cast<uint8>(EVeyraShellText::Body));
	AddText(*Rows, Model.Phase, static_cast<uint8>(EVeyraShellText::Muted));
	UHorizontalBox* Answers = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddButton(*Answers, LOCTEXT("Accept", "Accept"), [this] { Client->AcceptMatch(); }, Model.bCanAnswer);
	AddButton(*Answers, LOCTEXT("Decline", "Decline"), [this] { Client->DeclineMatch(); }, Model.bCanAnswer);
	VeyraShellStyle::AddSpaced(*Rows, *Answers);
	VeyraShellStyle::AddSpaced(*Content, *Panel);
}

void UVeyraShellScreen::BuildHome(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	AddText(Parent, LOCTEXT("HomeTitle", "Home"), static_cast<uint8>(EVeyraShellText::Title));
	AddText(Parent, FText::Format(LOCTEXT("HomeWelcome", "Welcome, {0}."), FText::FromString(Snapshot.DisplayName)), static_cast<uint8>(EVeyraShellText::Body));
	AddButton(Parent, LOCTEXT("HomePlay", "Play"), [this] { ShowPage(EVeyraShellPage::Play); });
}

void UVeyraShellScreen::BuildPlay(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	AddText(Parent, LOCTEXT("PlayTitle", "Play"), static_cast<uint8>(EVeyraShellText::Title));

	// A mode card: its name, its team format and whether it is available (UX-12). Choosing one makes
	// the player a party of one, or changes the mode of the party they lead (UX-6).
	const bool bCanSelectMode = Client->CanIssue(EVeyraClientIntent::SelectMode);
	UWrapBox* Modes = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	for (const FVeyraModeCardModel& ModeCard : VeyraShellModels::DescribeModes(Snapshot))
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(Style.CardWidth);
		UBorder* Card = VeyraShellStyle::MakeBorder(*WidgetTree, Style.PanelColor, Style.Spacing);
		UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->SetContent(Rows);
		Size->AddChild(Card);
		const FString ModeId = ModeCard.ModeId;
		AddButton(*Rows, ModeCard.Name, [this, ModeId] { Client->SelectMode(ModeId); }, bCanSelectMode && ModeCard.bAvailable, ModeCard.bSelected);
		AddText(*Rows, ModeCard.Format, static_cast<uint8>(EVeyraShellText::Body));
		if (!ModeCard.Availability.IsEmpty())
		{
			AddText(*Rows, ModeCard.Availability, static_cast<uint8>(EVeyraShellText::Muted));
		}
		VeyraShellStyle::AddSpaced(*Modes, *Size);
	}
	VeyraShellStyle::AddSpaced(Parent, *Modes);

	// Custom practice is not a matchmade mode: it starts at once, with no party or queue (ADR-010 §7).
	UBorder* Card = VeyraShellStyle::MakeBorder(*WidgetTree, Style.PanelColor, Style.Spacing);
	UVerticalBox* Custom = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Custom);
	AddText(*Custom, LOCTEXT("CustomTitle", "Custom"), static_cast<uint8>(EVeyraShellText::Heading));
	AddText(*Custom, LOCTEXT("CustomDetail", "Your own match. Practice puts you alone on the battleground to try a Vanguard; it ends when you end it."),
		static_cast<uint8>(EVeyraShellText::Body));
	AddText(*Custom, LOCTEXT("CustomFormat", "Practice: 1 player. Invites, team slots and bots are coming later."), static_cast<uint8>(EVeyraShellText::Muted));
	AddButton(*Custom, LOCTEXT("Practice", "Practice"), [this] { Client->StartPractice(); }, Client->CanIssue(EVeyraClientIntent::StartPractice));
	VeyraShellStyle::AddSpaced(Parent, *Card);
}

void UVeyraShellScreen::BuildReconnectOnly(const FVeyraClientSnapshot& Snapshot)
{
	AddText(*Content, LOCTEXT("ReconnectTitle", "Match in Progress"), static_cast<uint8>(EVeyraShellText::Title));
	AddText(*Content, LOCTEXT("ReconnectDetail", "Your match is still running. Until it ends, Reconnect is the only thing you can do."),
		static_cast<uint8>(EVeyraShellText::Body));
	if (const FText Notice = VeyraShellModels::DescribeNotice(Snapshot.Notice); !Notice.IsEmpty())
	{
		AddText(*Content, Notice, static_cast<uint8>(EVeyraShellText::Body));
	}
	// ADR-010 §11.5: said plainly, since the server still refuses rejoining (an ADR-007 open item).
	AddText(*Content, LOCTEXT("ReconnectCaveat", "Rejoining a match you have already joined is not supported yet, so its server may turn you away."),
		static_cast<uint8>(EVeyraShellText::Muted));
	AddButton(*Content, LOCTEXT("Reconnect", "Reconnect"), [this] { Client->Reconnect(); }, Client->CanIssue(EVeyraClientIntent::Reconnect));
}

void UVeyraShellScreen::BuildResults(const FVeyraClientSnapshot& Snapshot)
{
	const FVeyraResultsModel Model = VeyraShellModels::DescribeResults(Snapshot);
	AddText(*Content, Model.Headline, static_cast<uint8>(EVeyraShellText::Title));
	for (const FText& Line : Model.Lines)
	{
		AddText(*Content, Line, static_cast<uint8>(EVeyraShellText::Body));
	}
	AddButton(*Content, LOCTEXT("Continue", "Continue"), [this] { Client->ContinueFromResults(); }, Client->CanIssue(EVeyraClientIntent::ContinueFromResults));
	if (Model.bVerified)
	{
		BuildReport(Model.Report, *Content);
	}
}

void UVeyraShellScreen::BuildReport(const FVeyraMatchReport& Report, UPanelWidget& Parent)
{
	if (!Report.bHasScoreboard)
	{
		// Truthfully pending or missing, never invented (UX-50).
		if (!Report.Pending.IsEmpty())
		{
			AddText(Parent, Report.Pending, static_cast<uint8>(EVeyraShellText::Muted));
		}
		return;
	}
	UHorizontalBox* Views = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddButton(*Views, LOCTEXT("ScoreboardView", "Scoreboard"), [this] { ShowReportView(EVeyraReportView::Scoreboard); }, true,
		ReportView == EVeyraReportView::Scoreboard);
	AddButton(*Views, LOCTEXT("DetailsView", "Detailed Statistics"), [this] { ShowReportView(EVeyraReportView::Details); }, true,
		ReportView == EVeyraReportView::Details);
	VeyraShellStyle::AddSpaced(Parent, *Views);
	// The report scrolls within what is left of the screen.
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Body);
	if (UVerticalBox* Column = Cast<UVerticalBox>(&Parent))
	{
		Column->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	else
	{
		Parent.AddChild(Scroll);
	}
	if (ReportView == EVeyraReportView::Scoreboard)
	{
		BuildScoreboard(Report, *Body);
	}
	else
	{
		BuildDetails(Report, *Body);
	}
}

void UVeyraShellScreen::BuildScoreboard(const FVeyraMatchReport& Report, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	for (const FVeyraReportTeam& Team : Report.Teams)
	{
		AddText(Parent, Team.Title, static_cast<uint8>(EVeyraShellText::Heading));
		AddText(Parent, Team.Summary, static_cast<uint8>(EVeyraShellText::Muted));
		for (const FVeyraReportLine& Line : Team.Lines)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			const uint8 Role = static_cast<uint8>(EVeyraShellText::Body);
			AddCell(*Row, Line.Vanguard, Style.ReportColumnWidth, Role);
			UTextBlock* Name = AddCell(*Row, Line.Name, Style.ReportLabelWidth, Role);
			if (Line.bYou)
			{
				Name->SetColorAndOpacity(Style.AccentColor);
			}
			AddCell(*Row, Line.Level, Style.ReportColumnWidth, Role);
			AddCell(*Row, Line.Kda, Style.ReportColumnWidth, Role);
			AddCell(*Row, Line.Gold, Style.ReportColumnWidth, Role);
			AddCell(*Row, Line.LastHits, Style.ReportLabelWidth, Role);
			Parent.AddChild(Row);
			UHorizontalBox* Build = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			AddCell(*Build, FText::GetEmpty(), Style.ReportColumnWidth, static_cast<uint8>(EVeyraShellText::Small));
			AddText(*Build, Line.Items, static_cast<uint8>(EVeyraShellText::Small));
			AddText(*Build, Line.FluxSpells, static_cast<uint8>(EVeyraShellText::Small));
			VeyraShellStyle::AddSpaced(Parent, *Build);
		}
	}
}

void UVeyraShellScreen::BuildDetails(const FVeyraMatchReport& Report, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	// A column for each player, in the scoreboard's order.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddCell(*Header, FText::GetEmpty(), Style.ReportLabelWidth, static_cast<uint8>(EVeyraShellText::Small));
	for (const FText& Column : Report.Columns)
	{
		AddCell(*Header, Column, Style.ReportColumnWidth, static_cast<uint8>(EVeyraShellText::Small));
	}
	VeyraShellStyle::AddSpaced(Parent, *Header);
	for (const FVeyraReportGroup& Group : Report.Groups)
	{
		AddText(Parent, Group.Title, static_cast<uint8>(EVeyraShellText::Heading));
		for (const FVeyraReportRow& Figure : Group.Rows)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			AddCell(*Row, Figure.Label, Style.ReportLabelWidth, static_cast<uint8>(EVeyraShellText::Muted));
			for (const FText& Value : Figure.Values)
			{
				AddCell(*Row, Value, Style.ReportColumnWidth, static_cast<uint8>(EVeyraShellText::Body));
			}
			Parent.AddChild(Row);
		}
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
	const UVeyraShellStyleSettings& Style = *GetDefault<UVeyraShellStyleSettings>();
	UBorder* Banner = VeyraShellStyle::MakeBorder(*WidgetTree, Style.ProblemColor, Style.Spacing);
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Banner->SetContent(Row);
	AddText(*Row, VeyraShellModels::DescribeProblem(*Snapshot.Problem), static_cast<uint8>(EVeyraShellText::Body));
	// Reconnect-only offers nothing but Reconnect (UX-17), which asks the backend afresh anyway.
	if (Shown != EVeyraShellScreen::ReconnectOnly && Client->CanIssue(EVeyraClientIntent::Retry))
	{
		AddButton(*Row, LOCTEXT("Retry", "Retry"), [this] { Client->Retry(); });
	}
	VeyraShellStyle::AddSpaced(*Content, *Banner);
}

UTextBlock* UVeyraShellScreen::AddText(UPanelWidget& Parent, const FText& Text, uint8 Role)
{
	UTextBlock* Block = VeyraShellStyle::MakeText(*WidgetTree, Text, static_cast<EVeyraShellText>(Role));
	VeyraShellStyle::AddSpaced(Parent, *Block);
	return Block;
}

UVeyraShellButton* UVeyraShellScreen::AddButton(UPanelWidget& Parent, const FText& Label, TFunction<void()> Action, bool bEnabled, bool bSelected)
{
	UVeyraShellButton* Button = UVeyraShellButton::Make(*WidgetTree, Label, MoveTemp(Action), bEnabled, bSelected);
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
