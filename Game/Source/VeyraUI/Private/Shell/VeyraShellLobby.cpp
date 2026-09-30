// Copyright © 2026 Wayfinder Studios. All rights reserved.

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
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "Shell/VeyraShellArt.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"

#define LOCTEXT_NAMESPACE "VeyraShell"

// The custom lobby (ADR-021) and the friends panel, laid out as League's custom lobby and social panel.
namespace
{
	using VeyraShellStyle::EVeyraShellSurface;
	using VeyraShellStyle::EVeyraShellText;
	using VeyraBackendProtocol::ELobbySeatKind;

	const UVeyraShellStyleSettings& LobbyStyle()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	uint8 LobbyRole(EVeyraShellText Role)
	{
		return static_cast<uint8>(Role);
	}

	/** Adds Child to a horizontal or vertical box so that it takes the room left. */
	void AddLobbyFilling(UPanelWidget& Parent, UWidget& Child)
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

	void AddLobbyGap(UWidgetTree& Tree, UPanelWidget& Parent, float Size)
	{
		USpacer* Gap = Tree.ConstructWidget<USpacer>(USpacer::StaticClass());
		Gap->SetSize(FVector2D(Size, Size));
		Parent.AddChild(Gap);
	}

	/** A round portrait of VanguardId at Size, ringed in Ring; an empty disc when there is no Vanguard or art. */
	UImage* MakeLobbyPortrait(UWidgetTree& Tree, const FString& VanguardId, float Size, const FLinearColor& Ring)
	{
		const UVeyraShellStyleSettings& Style = LobbyStyle();
		UTexture2D* Hero = VeyraShellArt::HeroOf(VanguardId);
		const FBox2f Region = Hero ? VeyraShellArt::Crop(VanguardId, Hero->GetSizeX(), Hero->GetSizeY(), 1.0f, /*bPortrait*/ true)
								   : FBox2f(FVector2f::ZeroVector, FVector2f::UnitVector);
		UImage* Portrait = Tree.ConstructWidget<UImage>(UImage::StaticClass());
		Portrait->SetBrush(VeyraShellArt::Brush(Hero, Region, FVector2D(Size), -1.0f, Style.SurfaceColor, Ring, Style.FrameWidth));
		return Portrait;
	}
}

void UVeyraShellScreen::BuildLobby(const FVeyraClientSnapshot& Snapshot)
{
	const UVeyraShellStyleSettings& Style = LobbyStyle();
	const FVeyraLobbyModel Model = VeyraShellModels::DescribeLobby(Snapshot, Client->CanIssue(EVeyraClientIntent::SetLobbyBot),
		Client->CanIssue(EVeyraClientIntent::LaunchLobby), Client->CanIssue(EVeyraClientIntent::LeaveLobby));
	ShowShowcase(Style.ModeArtOf(TEXT("custom_game")));
	BuildTopBar(Snapshot, *Content, /*bPages*/ false);
	if (const FText Notice = VeyraShellModels::DescribeNotice(Snapshot.Notice); !Notice.IsEmpty())
	{
		UBorder* Banner = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing));
		Banner->SetContent(VeyraShellStyle::MakeText(*WidgetTree, Notice, EVeyraShellText::Body));
		VeyraShellStyle::AddSpaced(*Content, *Banner);
	}

	UHorizontalBox* Split = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddLobbyFilling(*Content, *Split);
	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Split->AddChildToHorizontalBox(Main)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AddText(*Main, LOCTEXT("LobbyEyebrow", "Custom Game"), LobbyRole(EVeyraShellText::Eyebrow));
	AddText(*Main, Model.Title, LobbyRole(EVeyraShellText::Title));

	// Both sides, as League's custom lobby shows its two teams side by side.
	UHorizontalBox* Sides = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Sides->AddChildToHorizontalBox(&MakeLobbySide(VeyraShellModels::SideName(TEXT("A")), Model.SideA))->SetVerticalAlignment(VAlign_Top);
	AddLobbyGap(*WidgetTree, *Sides, Style.Spacing * 2.0f);
	Sides->AddChildToHorizontalBox(&MakeLobbySide(VeyraShellModels::SideName(TEXT("B")), Model.SideB))->SetVerticalAlignment(VAlign_Top);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Scroll->AddChild(Sides);
	AddLobbyFilling(*Main, *Scroll);

	BuildLobbyRules(Model, *Main);

	// The one way forward is the host's; everyone may leave.
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddText(*Footer, Model.Status, LobbyRole(EVeyraShellText::Muted))->SetAutoWrapText(false);
	AddLobbyFilling(*Footer, *WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()));
	AddKindButton(*Footer, EVeyraShellButtonKind::Quiet, LOCTEXT("LeaveLobby", "Leave Lobby"), [this] { Client->LeaveLobby(); }, Model.bCanLeave)
		->KeepLabelOnOneLine();
	if (Model.bHost)
	{
		AddKindButton(*Footer, EVeyraShellButtonKind::Primary, LOCTEXT("StartGame", "Start Game"), [this] { Client->LaunchLobby(); }, Model.bCanStart)
			->KeepLabelOnOneLine();
	}
	Main->AddChildToVerticalBox(Footer);

	AddLobbyGap(*WidgetTree, *Split, Style.Spacing * 2.0f);
	BuildFriends(Snapshot, *Split);

	if (BotPickerIndex != INDEX_NONE)
	{
		BuildBotPicker(Snapshot);
	}
}

UWidget& UVeyraShellScreen::MakeLobbySide(const FText& Title, const TArray<FVeyraLobbySeatModel>& Seats)
{
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Width->SetWidthOverride(LobbyStyle().LobbySeatWidth);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Width->AddChild(Column);
	AddText(*Column, Title, LobbyRole(EVeyraShellText::Eyebrow));
	for (const FVeyraLobbySeatModel& Seat : Seats)
	{
		VeyraShellStyle::AddSpaced(*Column, MakeLobbySeat(Seat));
	}
	return *Width;
}

UWidget& UVeyraShellScreen::MakeLobbySeat(const FVeyraLobbySeatModel& Seat)
{
	const UVeyraShellStyleSettings& Style = LobbyStyle();
	const bool bEmpty = Seat.Kind == ELobbySeatKind::Empty;
	UBorder* Row = VeyraShellStyle::MakeSurface(*WidgetTree, bEmpty ? EVeyraShellSurface::Panel : EVeyraShellSurface::Raised, FMargin(Style.Spacing / 2.0f));
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetContent(Line);

	// The side's colour rings its seats, and the gold frame the player's own (Art Bible §4.3).
	const FLinearColor Ring = bEmpty ? Style.HairlineColor : Seat.bYou ? Style.FrameColor : Seat.Side == TEXT("A") ? Style.AllyColor : Style.EnemyColor;
	UHorizontalBoxSlot* PortraitSlot = Line->AddChildToHorizontalBox(MakeLobbyPortrait(*WidgetTree, Seat.VanguardId, Style.AbilityIconSize, Ring));
	PortraitSlot->SetVerticalAlignment(VAlign_Center);
	PortraitSlot->SetPadding(FMargin(0.0f, 0.0f, Style.Spacing, 0.0f));

	// A long name is cut at the seat's actions rather than running under them.
	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Texts->SetClipping(EWidgetClipping::ClipToBounds);
	UTextBlock* Name = VeyraShellStyle::MakeText(*WidgetTree, Seat.Name, bEmpty ? EVeyraShellText::Muted : EVeyraShellText::Heading);
	Name->SetAutoWrapText(false);
	Texts->AddChildToVerticalBox(Name);
	if (!Seat.Detail.IsEmpty())
	{
		UTextBlock* Detail = VeyraShellStyle::MakeText(*WidgetTree, Seat.Detail, EVeyraShellText::Small);
		Detail->SetAutoWrapText(false);
		Texts->AddChildToVerticalBox(Detail);
	}
	AddLobbyFilling(*Line, *Texts);

	// The host's actions on this seat, each named for it.
	const FString Side = Seat.Side;
	const int32 Index = Seat.Index;
	switch (Seat.Kind)
	{
	case ELobbySeatKind::Empty:
		if (Seat.bCanSetBot)
		{
			AddNamedButton(*Line, EVeyraShellButtonKind::Secondary, VeyraShellModels::AddBotLabel(Side, Index), LOCTEXT("AddBot", "Add Bot"),
				[this, Side, Index] { OpenBotPicker(Side, Index, FString()); }, true, BotPickerSide == Side && BotPickerIndex == Index);
		}
		break;
	case ELobbySeatKind::Bot:
		if (Seat.bCanSetBot)
		{
			AddNamedButton(*Line, EVeyraShellButtonKind::Quiet, VeyraShellModels::ChangeBotLabel(Side, Index), LOCTEXT("ChangeBot", "Change"),
				[this, Side, Index, Difficulty = Seat.Difficulty] { OpenBotPicker(Side, Index, Difficulty); }, true, BotPickerSide == Side && BotPickerIndex == Index);
		}
		if (Seat.bCanRemoveBot)
		{
			AddNamedButton(*Line, EVeyraShellButtonKind::Quiet, VeyraShellModels::RemoveBotLabel(Side, Index), LOCTEXT("RemoveBot", "Remove"),
				[this, Side, Index] { Client->RemoveLobbyBot(Side, Index); });
		}
		break;
	case ELobbySeatKind::Human:
		if (Seat.bCanSwitchSide)
		{
			AddNamedButton(*Line, EVeyraShellButtonKind::Quiet, VeyraShellModels::SwitchSideLabel(Seat.PlayerName, Seat.SwitchToSide), LOCTEXT("SwitchSide", "Swap"),
				[this, AccountId = Seat.AccountId, ToSide = Seat.SwitchToSide, ToIndex = Seat.SwitchToIndex] { Client->MoveInLobby(AccountId, ToSide, ToIndex); });
		}
		if (Seat.bCanKick)
		{
			AddNamedButton(*Line, EVeyraShellButtonKind::Quiet, VeyraShellModels::KickLabel(Seat.PlayerName), LOCTEXT("KickPlayer", "Remove"),
				[this, AccountId = Seat.AccountId] { Client->KickFromLobby(AccountId); });
		}
		break;
	}
	return *Row;
}

void UVeyraShellScreen::BuildLobbyRules(const FVeyraLobbyModel& Model, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = LobbyStyle();
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Panel, FMargin(Style.Spacing));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	AddText(*Rows, LOCTEXT("LobbyRulesEyebrow", "Session rules"), LobbyRole(EVeyraShellText::Eyebrow));

	// Victory on or off (Custom Matches Bible §4): the host's to switch, and only with a Vanguard on each side.
	UHorizontalBox* Victory = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddLobbyFilling(*Victory, *VeyraShellStyle::MakeText(*WidgetTree, Model.Victory, EVeyraShellText::Body));
	if (Model.bHost)
	{
		const bool bTurnOn = !Model.bVictoryEnabled;
		AddKindButton(*Victory, EVeyraShellButtonKind::Secondary, bTurnOn ? LOCTEXT("VictoryOn", "Turn Victory On") : LOCTEXT("VictoryOff", "Turn Victory Off"),
			[this, bTurnOn] {
				const TOptional<VeyraBackendProtocol::FLobby>& Lobby = Client->GetSnapshot().Lobby;
				Client->SetLobbySettings(bTurnOn, Lobby.IsSet() ? Lobby->StartingGold : TOptional<double>());
			}, Model.bCanToggleVictory)->KeepLabelOnOneLine();
	}
	Rows->AddChildToVerticalBox(Victory);

	AddText(*Rows, Model.StartingGold, LobbyRole(EVeyraShellText::Body));
	if (Model.bHost)
	{
		// One row, like tabs: the chosen one lit, and choosing it again changes nothing.
		UHorizontalBox* Choices = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		for (const FVeyraGoldChoiceModel& Choice : Model.GoldChoices)
		{
			AddKindButton(*Choices, EVeyraShellButtonKind::Tab, Choice.Label, [this, Gold = Choice.Gold] {
				const TOptional<VeyraBackendProtocol::FLobby>& Lobby = Client->GetSnapshot().Lobby;
				Client->SetLobbySettings(Lobby.IsSet() && Lobby->bVictoryEnabled, Gold);
			}, Model.bCanSetGold, Choice.bChosen)->KeepLabelOnOneLine();
		}
		Rows->AddChildToVerticalBox(Choices);
	}
	VeyraShellStyle::AddSpaced(Parent, *Panel);
}

void UVeyraShellScreen::BuildBotPicker(const FVeyraClientSnapshot& Snapshot)
{
	const UVeyraShellStyleSettings& Style = LobbyStyle();
	const FVeyraBotPickerModel Model = VeyraShellModels::DescribeBotPicker(Snapshot, BotPickerSide, BotPickerIndex);
	if (Model.Vanguards.IsEmpty() || Model.Difficulties.IsEmpty())
	{
		BotPickerIndex = INDEX_NONE;
		return;
	}
	if (!Model.Difficulties.ContainsByPredicate([this](const TPair<FString, FText>& Difficulty) { return Difficulty.Key == BotDifficulty; }))
	{
		BotDifficulty = Model.Difficulties[0].Key;
	}
	// Over everything, as the Flux Spell picker is, until a choice or Close.
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
	PanelBox->SetWidthOverride(Style.PickerWidth);
	PanelBox->AddChild(Panel);
	Scrim->SetContent(PanelBox);

	AddText(*Rows, Model.Title, LobbyRole(EVeyraShellText::Heading));
	// The difficulty first (§3), then the Vanguard, which seats the bot.
	UHorizontalBox* Difficulties = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (const TPair<FString, FText>& Difficulty : Model.Difficulties)
	{
		AddKindButton(*Difficulties, EVeyraShellButtonKind::Tab, Difficulty.Value, [this, Id = Difficulty.Key] {
			BotDifficulty = Id;
			Refresh();
		}, true, Difficulty.Key == BotDifficulty)->KeepLabelOnOneLine();
	}
	Rows->AddChildToVerticalBox(Difficulties);
	UWrapBox* Choices = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	Choices->SetExplicitWrapSize(true);
	Choices->SetWrapSize(Style.PickerWidth - Style.Spacing * 4.0f);
	const bool bCanSet = Client->CanIssue(EVeyraClientIntent::SetLobbyBot);
	for (const FVeyraBotChoiceModel& Choice : Model.Vanguards)
	{
		UVerticalBox* Tile = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UVerticalBoxSlot* PortraitSlot = Tile->AddChildToVerticalBox(MakeLobbyPortrait(*WidgetTree, Choice.VanguardId, Style.RosterTileSize, Choice.bChosen ? Style.FrameColor : Style.HairlineColor));
		PortraitSlot->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* Name = VeyraShellStyle::MakeText(*WidgetTree, Choice.Name, EVeyraShellText::Small);
		Name->SetAutoWrapText(false);
		Name->SetJustification(ETextJustify::Center);
		Tile->AddChildToVerticalBox(Name)->SetHorizontalAlignment(HAlign_Center);
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Style.RosterTileSize + Style.Spacing * 2.0f);
		Box->AddChild(Tile);
		const FString Side = BotPickerSide;
		const int32 Index = BotPickerIndex;
		AddContentButton(*Choices, VeyraShellModels::BotChoiceLabel(Choice.VanguardId), *Box, [this, Side, Index, Vanguard = Choice.VanguardId] {
			// Closed first, so the rebuild the choice starts shows the lobby without the picker.
			BotPickerIndex = INDEX_NONE;
			Client->SetLobbyBot(Side, Index, Vanguard, BotDifficulty);
			Refresh();
		}, bCanSet && !Choice.bTaken, Choice.bChosen);
	}
	VeyraShellStyle::AddSpaced(*Rows, *Choices);
	AddButton(*Rows, LOCTEXT("CloseBotPicker", "Close"), [this] {
		BotPickerIndex = INDEX_NONE;
		Refresh();
	});
}

void UVeyraShellScreen::OpenBotPicker(const FString& Side, int32 Index, const FString& Difficulty)
{
	const bool bOpenHere = BotPickerSide == Side && BotPickerIndex == Index;
	BotPickerSide = Side;
	BotPickerIndex = bOpenHere ? INDEX_NONE : Index;
	if (!Difficulty.IsEmpty())
	{
		BotDifficulty = Difficulty;
	}
	Refresh();
}

void UVeyraShellScreen::BuildFriends(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = LobbyStyle();
	const bool bInLobby = Snapshot.State == EVeyraClientState::Lobby;
	const FVeyraFriendsModel Model = VeyraShellModels::DescribeFriends(Snapshot, Client->CanIssue(EVeyraClientIntent::SendFriendRequest),
		Client->CanIssue(EVeyraClientIntent::AnswerFriendRequest), Client->CanIssue(EVeyraClientIntent::AcceptLobbyInvite),
		Client->CanIssue(EVeyraClientIntent::DeclineLobbyInvite), Client->CanIssue(EVeyraClientIntent::InviteToLobby));

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Width->SetWidthOverride(Style.FriendsPanelWidth);
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Panel, FMargin(Style.Spacing));
	Width->AddChild(Panel);
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);
	if (UHorizontalBox* Row = Cast<UHorizontalBox>(&Parent))
	{
		Row->AddChildToHorizontalBox(Width)->SetVerticalAlignment(VAlign_Fill);
	}
	else
	{
		Parent.AddChild(Width);
	}
	AddText(*Rows, LOCTEXT("FriendsEyebrow", "Friends"), LobbyRole(EVeyraShellText::Eyebrow));

	// Add a friend by name (Parties & Social Bible §1). The field keeps what was typed across rebuilds.
	FriendNameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	FEditableTextBoxStyle FieldStyle = FriendNameBox->GetWidgetStyle();
	const FSlateRoundedBoxBrush Field(Style.SurfaceRaisedColor, Style.ButtonCornerRadius, Style.HairlineColor, 1.0f);
	const FSlateRoundedBoxBrush Focused(Style.SurfaceRaisedColor, Style.ButtonCornerRadius, Style.AccentColor, 1.0f);
	FieldStyle.SetBackgroundImageNormal(Field);
	FieldStyle.SetBackgroundImageHovered(Field);
	FieldStyle.SetBackgroundImageFocused(Focused);
	FieldStyle.SetBackgroundImageReadOnly(Field);
	FieldStyle.SetForegroundColor(FSlateColor(Style.TextColor));
	FieldStyle.SetFocusedForegroundColor(FSlateColor(Style.TextColor));
	FieldStyle.SetPadding(FMargin(Style.ButtonPadding));
	FieldStyle.SetFont(VeyraShellStyle::FontFor(EVeyraShellText::Body));
	FriendNameBox->SetWidgetStyle(FieldStyle);
	FriendNameBox->SetHintText(LOCTEXT("FriendNameHint", "A player's name"));
	FriendNameBox->SetText(FText::FromString(FriendNameDraft));
	FriendNameBox->SetIsEnabled(Model.bCanAdd);
	FriendNameBox->OnTextChanged.AddUniqueDynamic(this, &UVeyraShellScreen::HandleFriendNameChanged);
	FriendNameBox->OnTextCommitted.AddUniqueDynamic(this, &UVeyraShellScreen::HandleFriendNameCommitted);
	VeyraShellStyle::AddSpaced(*Rows, *FriendNameBox);
	// Enabled whatever the field holds, so typing never rebuilds the panel; an empty name sends nothing.
	AddKindButton(*Rows, EVeyraShellButtonKind::Secondary, LOCTEXT("AddFriend", "Add Friend"), [this] { SubmitFriendName(); }, Model.bCanAdd)
		->KeepLabelOnOneLine();
	if (!Model.Feedback.IsEmpty())
	{
		AddText(*Rows, Model.Feedback, LobbyRole(EVeyraShellText::Muted));
	}

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UVerticalBox* Lists = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Lists);
	AddLobbyFilling(*Rows, *Scroll);

	// Invitations and requests first: they wait for an answer.
	for (const FVeyraSocialRequestModel& Invitation : Model.Invitations)
	{
		UBorder* Card = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing / 2.0f));
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->SetContent(Lines);
		AddText(*Lines, Invitation.Line, LobbyRole(EVeyraShellText::Body));
		UHorizontalBox* Answers = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const FString Name = Invitation.Name.ToString();
		AddNamedButton(*Answers, EVeyraShellButtonKind::Primary, VeyraShellModels::JoinLobbyLabel(Name), LOCTEXT("JoinLobby", "Join"),
			[this, Id = Invitation.Id] { Client->AcceptLobbyInvite(Id); }, Model.bCanJoin);
		AddNamedButton(*Answers, EVeyraShellButtonKind::Quiet, VeyraShellModels::DeclineInviteLabel(Name), LOCTEXT("DeclineInvite", "Decline"),
			[this, Id = Invitation.Id] { Client->DeclineLobbyInvite(Id); }, Model.bCanAnswerInvitations);
		Lines->AddChildToVerticalBox(Answers);
		VeyraShellStyle::AddSpaced(*Lists, *Card);
	}
	for (const FVeyraSocialRequestModel& Request : Model.Requests)
	{
		UBorder* Card = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing / 2.0f));
		UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->SetContent(Lines);
		AddText(*Lines, Request.Line, LobbyRole(EVeyraShellText::Body));
		UHorizontalBox* Answers = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const FString Name = Request.Name.ToString();
		AddNamedButton(*Answers, EVeyraShellButtonKind::Secondary, VeyraShellModels::AcceptRequestLabel(Name), LOCTEXT("AcceptRequest", "Accept"),
			[this, Id = Request.Id] { Client->AnswerFriendRequest(Id, /*bAccept*/ true); }, Model.bCanAnswerRequests);
		AddNamedButton(*Answers, EVeyraShellButtonKind::Quiet, VeyraShellModels::DeclineRequestLabel(Name), LOCTEXT("DeclineRequest", "Decline"),
			[this, Id = Request.Id] { Client->AnswerFriendRequest(Id, /*bAccept*/ false); }, Model.bCanAnswerRequests);
		Lines->AddChildToVerticalBox(Answers);
		VeyraShellStyle::AddSpaced(*Lists, *Card);
	}

	if (Model.bLoaded && Model.Friends.IsEmpty())
	{
		AddText(*Lists, LOCTEXT("NoFriends", "No friends yet. Add a player by their name."), LobbyRole(EVeyraShellText::Muted));
	}
	for (const FVeyraFriendModel& Friend : Model.Friends)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Name = VeyraShellStyle::MakeText(*WidgetTree, Friend.Name, EVeyraShellText::Body);
		Name->SetAutoWrapText(false);
		AddLobbyFilling(*Line, *Name);
		// In the lobby its host invites friends (Custom Matches Bible §1).
		if (Friend.bOffersInvite)
		{
			AddNamedButton(*Line, EVeyraShellButtonKind::Secondary, VeyraShellModels::InviteLabel(Friend.Name.ToString()), LOCTEXT("InviteFriend", "Invite"),
				[this, Id = Friend.AccountId] { Client->InviteToLobby(Id); }, Friend.bCanInvite);
		}
		VeyraShellStyle::AddSpaced(*Lists, *Line);
	}
	for (const FText& Pending : Model.Pending)
	{
		AddText(*Lists, Pending, LobbyRole(EVeyraShellText::Muted));
	}
	if (!bInLobby && !Model.Friends.IsEmpty())
	{
		AddText(*Lists, LOCTEXT("FriendsCustomHint", "Open a Custom Game from Play to invite them."), LobbyRole(EVeyraShellText::Small));
	}
}

void UVeyraShellScreen::SetFriendNameDraft(const FString& Name)
{
	FriendNameDraft = Name;
	if (FriendNameBox)
	{
		FriendNameBox->SetText(FText::FromString(Name));
	}
}

void UVeyraShellScreen::SubmitFriendName()
{
	if (Client && Client->SendFriendRequest(FriendNameDraft))
	{
		FriendNameDraft.Reset();
		if (FriendNameBox)
		{
			FriendNameBox->SetText(FText::GetEmpty());
		}
	}
}

void UVeyraShellScreen::HandleFriendNameChanged(const FText& Text)
{
	FriendNameDraft = Text.ToString();
}

void UVeyraShellScreen::HandleFriendNameCommitted(const FText& Text, ETextCommit::Type Method)
{
	FriendNameDraft = Text.ToString();
	if (Method == ETextCommit::OnEnter)
	{
		SubmitFriendName();
	}
}

UVeyraShellButton* UVeyraShellScreen::AddNamedButton(UPanelWidget& Parent, EVeyraShellButtonKind Kind, const FText& Label, const FText& ShownText,
	TFunction<void()> Action, bool bEnabled, bool bSelected)
{
	UVeyraShellButton* Button = UVeyraShellButton::MakeKindNamed(*WidgetTree, Kind, Label, ShownText, MoveTemp(Action), bEnabled, bSelected);
	Button->KeepLabelOnOneLine();
	Buttons.Add(Button);
	VeyraShellStyle::AddSpaced(Parent, *Button);
	return Button;
}

#undef LOCTEXT_NAMESPACE
