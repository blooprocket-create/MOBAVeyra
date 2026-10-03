// Copyright © 2026 Wayfinder Studios. All rights reserved.

// The player menu on a match's report and its report form (ADR-047 §5). The backend decides every report,
// commendation, friend request and invitation; the menu only asks, and names players as the match recorded them.

#include "Blueprint/WidgetTree.h"
#include "Client/VeyraClientIntents.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WrapBox.h"
#include "Shell/VeyraConductModels.h"
#include "Shell/VeyraProfileModels.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"

#define LOCTEXT_NAMESPACE "VeyraShell"

namespace
{
	using VeyraShellStyle::EVeyraShellSurface;
	using VeyraShellStyle::EVeyraShellText;

	const UVeyraShellStyleSettings& ConductStyle()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	uint8 ConductRole(EVeyraShellText Role)
	{
		return static_cast<uint8>(Role);
	}
}

void UVeyraShellScreen::TogglePlayerMenu(const FString& Name)
{
	OpenPlayerMenu = OpenPlayerMenu == Name ? FString() : Name;
	ReportFormName.Reset();
	BlockConfirmName.Reset();
	ReportReason.Reset();
	ReportDetailsDraft.Reset();
	Refresh();
}

void UVeyraShellScreen::BuildPlayerMenu(const FVeyraClientSnapshot& Snapshot, const FString& Name, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = ConductStyle();
	FVeyraPlayerMenuPermissions Can;
	Can.bCanAddFriend = Client->CanIssue(EVeyraClientIntent::SendFriendRequest);
	Can.bCanInvite = Client->CanIssue(EVeyraClientIntent::InviteToParty);
	Can.bCanCommend = Client->CanIssue(EVeyraClientIntent::CommendTeammate);
	Can.bCanReport = Client->CanIssue(EVeyraClientIntent::ReportPlayer);
	Can.bCanViewProfile = Client->CanIssue(EVeyraClientIntent::OpenProfile);
	Can.bCanBlock = Client->CanIssue(EVeyraClientIntent::BlockByName);
	const FVeyraPlayerMenuModel Model = VeyraConductModels::DescribeMenu(Snapshot, Name, Can);
	UBorder* Card = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing));
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Rows);
	for (const FText& Note : Model.Notes)
	{
		AddText(*Rows, Note, ConductRole(EVeyraShellText::Body));
	}
	if (ReportFormName == Name && Model.bOffersReport)
	{
		BuildReportForm(Snapshot, Name, *Rows);
		VeyraShellStyle::AddSpaced(Parent, *Card);
		return;
	}
	UWrapBox* Actions = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	Actions->SetInnerSlotPadding(FVector2D(Style.Spacing, 0.0f));
	if (Model.bOffersAddFriend)
	{
		AddNamedButton(*Actions, EVeyraShellButtonKind::Secondary, VeyraConductModels::AddFriendLabel(Name), LOCTEXT("MenuAddFriend", "Add Friend"),
			[this, Name] { Client->SendFriendRequest(Name); });
	}
	if (Model.bOffersInvite)
	{
		const FString AccountId = Model.FriendAccountId;
		AddNamedButton(*Actions, EVeyraShellButtonKind::Secondary, VeyraConductModels::InviteLabel(Name), LOCTEXT("MenuInvite", "Invite to Party"),
			[this, AccountId] { Client->InviteToParty(AccountId); });
	}
	if (Model.bOffersCommend)
	{
		AddNamedButton(*Actions, EVeyraShellButtonKind::Secondary, VeyraConductModels::CommendLabel(Name), LOCTEXT("MenuCommend", "Commend"),
			[this, Name] { Client->CommendTeammate(Name); });
	}
	// Their profile, over the screen (ADR-048 §5).
	if (Model.bOffersProfile)
	{
		AddNamedButton(*Actions, EVeyraShellButtonKind::Secondary, VeyraProfileModels::MenuProfileLabel(Name), LOCTEXT("MenuProfile", "Profile"),
			[this, Name] { Client->OpenProfile(Name); });
	}
	if (Model.bOffersReport)
	{
		AddNamedButton(*Actions, EVeyraShellButtonKind::Quiet, VeyraConductModels::ReportLabel(Name), LOCTEXT("MenuReport", "Report"), [this, Name] {
			ReportFormName = Name;
			ReportReason.Reset();
			ReportDetailsDraft.Reset();
			Refresh();
		});
	}
	// Block, from the player list (Parties & Social Bible §6; ADR-060 §5); it asks first, as the friends card does.
	const bool bConfirmingBlock = Model.bOffersBlock && BlockConfirmName == Name;
	if (Model.bOffersBlock && !bConfirmingBlock)
	{
		AddNamedButton(*Actions, EVeyraShellButtonKind::Quiet, VeyraShellModels::BlockLabel(Name), LOCTEXT("MenuBlock", "Block"), [this, Name] {
			BlockConfirmName = Name;
			Refresh();
		});
	}
	if (Actions->GetChildrenCount() > 0)
	{
		VeyraShellStyle::AddSpaced(*Rows, *Actions);
	}
	if (bConfirmingBlock)
	{
		AddText(*Rows, VeyraShellModels::ConfirmBlockPrompt(Name), ConductRole(EVeyraShellText::Muted));
		UHorizontalBox* Answers = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddNamedButton(*Answers, EVeyraShellButtonKind::Primary, VeyraShellModels::ConfirmBlockLabel(Name), LOCTEXT("MenuConfirmBlock", "Block"),
			[this, Name] {
				BlockConfirmName.Reset();
				Client->BlockByName(Name);
				Refresh();
			},
			Client->CanIssue(EVeyraClientIntent::BlockByName));
		AddNamedButton(*Answers, EVeyraShellButtonKind::Quiet, VeyraShellModels::CancelConfirmLabel(), LOCTEXT("MenuCancelBlock", "Cancel"), [this] {
			BlockConfirmName.Reset();
			Refresh();
		});
		VeyraShellStyle::AddSpaced(*Rows, *Answers);
	}
	VeyraShellStyle::AddSpaced(Parent, *Card);
}

void UVeyraShellScreen::BuildReportForm(const FVeyraClientSnapshot& Snapshot, const FString& Name, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = ConductStyle();
	const FVeyraReportFormModel Form = VeyraConductModels::DescribeReportForm(Snapshot);
	AddText(Parent, FText::Format(LOCTEXT("ReportFormTitle", "Report {0}"), FText::FromString(Name)), ConductRole(EVeyraShellText::Heading));
	// The reporter never learns of other reports or of any outcome (ADR-047 §2).
	AddText(Parent, LOCTEXT("ReportFormNote", "Reports are reviewed privately; you will not hear what comes of one."), ConductRole(EVeyraShellText::Muted));
	UWrapBox* Reasons = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	Reasons->SetInnerSlotPadding(FVector2D(Style.Spacing, 0.0f));
	for (const FString& Reason : Form.Reasons)
	{
		AddNamedButton(*Reasons, EVeyraShellButtonKind::Tab, VeyraConductModels::ReasonButtonLabel(Name, Reason), VeyraConductModels::ReasonLabel(Reason),
			[this, Reason] {
				ReportReason = Reason;
				Refresh();
			},
			true, ReportReason == Reason);
	}
	VeyraShellStyle::AddSpaced(Parent, *Reasons);
	ReportDetailsBox = MakeTextField(LOCTEXT("ReportDetailsHint", "Details (optional)"), ReportDetailsDraft, true);
	ReportDetailsBox->OnTextChanged.AddUniqueDynamic(this, &UVeyraShellScreen::HandleReportDetailsChanged);
	VeyraShellStyle::AddSpaced(Parent, *ReportDetailsBox);
	ReportDetailsCount = AddText(Parent, VeyraConductModels::DetailsCount(VeyraConductModels::CharacterCount(ReportDetailsDraft), Form.DetailsMaxCharacters), ConductRole(EVeyraShellText::Small));
	UHorizontalBox* Answers = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddNamedButton(*Answers, EVeyraShellButtonKind::Primary, VeyraConductModels::SubmitReportLabel(Name), LOCTEXT("SubmitReport", "Submit"),
		[this, Name] {
			const FString Reason = ReportReason;
			const FString Details = ReportDetailsDraft.TrimStartAndEnd();
			ReportFormName.Reset();
			ReportReason.Reset();
			ReportDetailsDraft.Reset();
			Client->ReportPlayer(Name, Reason, Details);
			Refresh();
		},
		!ReportReason.IsEmpty() && Client->CanIssue(EVeyraClientIntent::ReportPlayer));
	AddNamedButton(*Answers, EVeyraShellButtonKind::Quiet, VeyraConductModels::CancelReportLabel(Name), LOCTEXT("CancelReport", "Cancel"), [this] {
		ReportFormName.Reset();
		ReportReason.Reset();
		ReportDetailsDraft.Reset();
		Refresh();
	});
	VeyraShellStyle::AddSpaced(Parent, *Answers);
}

void UVeyraShellScreen::HandleReportDetailsChanged(const FText& Text)
{
	// The field holds no more than the backend accepts, counted as it counts: by character, not UTF-16 unit.
	const int32 Max = Client ? Client->GetSnapshot().Conduct.Record.DetailsMaxCharacters : 0;
	FString Draft = Text.ToString();
	if (VeyraConductModels::CharacterCount(Draft) > Max)
	{
		Draft = VeyraConductModels::LeftCharacters(Draft, Max);
		if (ReportDetailsBox)
		{
			ReportDetailsBox->SetText(FText::FromString(Draft));
		}
	}
	ReportDetailsDraft = Draft;
	if (ReportDetailsCount)
	{
		ReportDetailsCount->SetText(VeyraConductModels::DetailsCount(VeyraConductModels::CharacterCount(Draft), Max));
	}
}

void UVeyraShellScreen::SetReportDetailsDraft(const FString& Text)
{
	if (ReportDetailsBox)
	{
		ReportDetailsBox->SetText(FText::FromString(Text));
	}
	HandleReportDetailsChanged(FText::FromString(Text));
}

#undef LOCTEXT_NAMESPACE
