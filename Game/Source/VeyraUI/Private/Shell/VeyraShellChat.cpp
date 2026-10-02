// Copyright © 2026 Wayfinder Studios. All rights reserved.

// The chat panels (ADR-046 §6): the sidebar's Party Chat and direct conversations (UX-3). The backend decides
// who reads each line and the flow keeps the conversations; these panels show them and send what the
// player types.

#include "Blueprint/WidgetTree.h"
#include "Client/VeyraClientIntents.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Shell/VeyraChatModels.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellStyle.h"
#include "Shell/VeyraShellStyleSettings.h"

#define LOCTEXT_NAMESPACE "VeyraShell"

namespace
{
	using VeyraBackendProtocol::EChatKind;
	using VeyraShellStyle::EVeyraShellSurface;
	using VeyraShellStyle::EVeyraShellText;

	const UVeyraShellStyleSettings& ChatStyle()
	{
		return *GetDefault<UVeyraShellStyleSettings>();
	}

	uint8 ChatRole(EVeyraShellText Role)
	{
		return static_cast<uint8>(Role);
	}

	/** Each conversation's own draft: the party's, a friend's, the select's or the post-match chat's. */
	FString ChatDraftKey(EChatKind Kind, const FString& Target)
	{
		return FString(VeyraBackendProtocol::ChatKindName(Kind)) + TEXT(":") + Target;
	}

	/** Champion select's recipient as Draft stands: changed in place as the player types, never by a rebuild. */
	void ShowChatRecipient(UTextBlock* Recipient, const FString& Draft)
	{
		if (Recipient)
		{
			FString Text;
			Recipient->SetText(VeyraChatModels::RecipientLabel(VeyraChatModels::SelectRecipient(Draft, Text)));
		}
	}
}

void UVeyraShellScreen::BuildSidebarChat(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	const FVeyraChatPanelModel Model = VeyraChatModels::DescribeSidebar(Snapshot, Client->CanIssue(EVeyraClientIntent::SendChatMessage));
	if (Model.bVisible)
	{
		BuildChatPanel(Model, Parent);
	}
}

void UVeyraShellScreen::BuildChatPanel(const FVeyraChatPanelModel& Model, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = ChatStyle();
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* Title = VeyraShellStyle::MakeText(*WidgetTree, Model.Title, EVeyraShellText::Eyebrow);
	Title->SetAutoWrapText(false);
	Header->AddChildToHorizontalBox(Title)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	if (Model.bCanClose)
	{
		// A direct conversation closes back to Party Chat.
		AddNamedButton(*Header, EVeyraShellButtonKind::Quiet, FText::Format(LOCTEXT("CloseChatLabel", "Close the conversation with {0}"), Model.Title),
			LOCTEXT("CloseChat", "Close"), [this] { Client->CloseDirectChat(); });
	}
	VeyraShellStyle::AddSpaced(Parent, *Header);

	USizeBox* Height = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Height->SetHeightOverride(Style.ChatLinesHeight);
	UBorder* Surface = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Raised, FMargin(Style.Spacing / 2.0f));
	Height->AddChild(Surface);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Surface->SetContent(Scroll);
	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Lines);
	if (Model.Lines.IsEmpty())
	{
		AddText(*Lines, Model.Empty, ChatRole(EVeyraShellText::Muted));
	}
	for (const FVeyraChatLineModel& Line : Model.Lines)
	{
		// A party line in a panel that mixes conversations says so (UX-34).
		const FText Said = Line.bParty ? FText::Format(LOCTEXT("ChatPartyLine", "[Party] {0}: {1}"), Line.Sender, Line.Text)
									   : FText::Format(LOCTEXT("ChatLine", "{0}: {1}"), Line.Sender, Line.Text);
		AddText(*Lines, Said, ChatRole(Line.bOwn ? EVeyraShellText::Muted : EVeyraShellText::Body));
		if (!Line.Status.IsEmpty())
		{
			AddText(*Lines, Line.Status, ChatRole(EVeyraShellText::Small));
		}
	}
	// The newest lines show first: scrolled once the panel is laid out, on the next frame.
	ChatScroll = Scroll;
	VeyraShellStyle::AddSpaced(Parent, *Height);

	ChatBoxKind = Model.Kind;
	ChatBoxTarget = Model.Target;
	ChatBoxKey = ChatDraftKey(Model.Kind, Model.Target);
	ChatBox = MakeTextField(Model.Hint, ChatDrafts.FindRef(ChatBoxKey), Model.bCanSend);
	ChatBox->OnTextChanged.AddUniqueDynamic(this, &UVeyraShellScreen::HandleChatChanged);
	ChatBox->OnTextCommitted.AddUniqueDynamic(this, &UVeyraShellScreen::HandleChatCommitted);
	UHorizontalBox* Composer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ChatRecipient = nullptr;
	if (Model.bShowsRecipient)
	{
		// Team by default, Party while /p leads the draft (UX-34).
		ChatRecipient = VeyraShellStyle::MakeText(*WidgetTree, FText::GetEmpty(), EVeyraShellText::Eyebrow);
		ChatRecipient->SetAutoWrapText(false);
		ShowChatRecipient(ChatRecipient, ChatDrafts.FindRef(ChatBoxKey));
		Composer->AddChildToHorizontalBox(ChatRecipient)->SetVerticalAlignment(VAlign_Center);
	}
	Composer->AddChildToHorizontalBox(ChatBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	// Enabled whatever the field holds, so typing never rebuilds the panel; an empty message sends nothing.
	AddNamedButton(*Composer, EVeyraShellButtonKind::Secondary, FText::Format(LOCTEXT("SendChatLabel", "Send to {0}"), Model.Title),
		LOCTEXT("SendChat", "Send"), [this] { SubmitChat(); }, Model.bCanSend);
	VeyraShellStyle::AddSpaced(Parent, *Composer);
	if (!Model.Notice.IsEmpty())
	{
		AddText(Parent, Model.Notice, ChatRole(EVeyraShellText::Small));
	}
}

void UVeyraShellScreen::BuildPostMatchChat(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	FVeyraChatPanelModel Model = VeyraChatModels::DescribePostMatch(Snapshot, Client->CanIssue(EVeyraClientIntent::SendChatMessage));
	if (!Model.bVisible)
	{
		return;
	}
	Model.Notice = ChatNotice;
	const UVeyraShellStyleSettings& Style = ChatStyle();
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Width->SetWidthOverride(Style.FriendsPanelWidth);
	UBorder* Panel = VeyraShellStyle::MakeSurface(*WidgetTree, EVeyraShellSurface::Panel, FMargin(Style.Spacing));
	Width->AddChild(Panel);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);
	BuildChatPanel(Model, *Column);
	if (UHorizontalBox* Row = Cast<UHorizontalBox>(&Parent))
	{
		Row->AddChildToHorizontalBox(Width)->SetVerticalAlignment(VAlign_Top);
	}
	else
	{
		VeyraShellStyle::AddSpaced(Parent, *Width);
	}
}

void UVeyraShellScreen::BuildSelectChat(const FVeyraClientSnapshot& Snapshot, UPanelWidget& Parent)
{
	const UVeyraShellStyleSettings& Style = ChatStyle();
	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Width->SetWidthOverride(Style.FriendsPanelWidth);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Width->AddChild(Column);
	// Collapsible, and never over the picks, bans, trades or the countdown (UX-33).
	AddKindButton(*Column, EVeyraShellButtonKind::Quiet, bSelectChatHidden ? LOCTEXT("ShowSelectChat", "Show Chat") : LOCTEXT("HideSelectChat", "Hide Chat"),
		[this] {
			bSelectChatHidden = !bSelectChatHidden;
			Refresh();
		})
		->KeepLabelOnOneLine();
	if (!bSelectChatHidden)
	{
		BuildChatPanel(VeyraChatModels::DescribeSelectChat(Snapshot, Client->CanIssue(EVeyraClientIntent::SendChatMessage)), *Column);
	}
	VeyraShellStyle::AddSpaced(Parent, *Width);
}

void UVeyraShellScreen::SubmitChat()
{
	const FString Draft = ChatDrafts.FindRef(ChatBoxKey);
	FString Text = Draft;
	VeyraBackendProtocol::EChatKind Kind = ChatBoxKind;
	// Champion select's one composer addresses the team, or the party with /p (UX-33).
	if (ChatBoxKind == EChatKind::Select)
	{
		Kind = VeyraChatModels::SelectRecipient(Draft, Text);
	}
	// The post-match chat takes /mute and /unmute, as a match does (ADR-046 §5).
	if (ChatBoxKind == EChatKind::PostMatch && Client)
	{
		const FVeyraPostMatchCommand Command = VeyraChatModels::ParsePostMatch(Draft, Client->GetSnapshot().Chat, Client->GetSnapshot().AccountId);
		if (Command.Kind != EVeyraPostMatchCommandKind::Send)
		{
			if (Command.Kind != EVeyraPostMatchCommandKind::NoSuchSpeaker)
			{
				Client->MutePostMatchChat(Command.AccountId, Command.Kind == EVeyraPostMatchCommandKind::Mute);
			}
			ChatNotice = VeyraChatModels::PostMatchNotice(Command);
			ChatDrafts.Remove(ChatBoxKey);
			ShownSignature.Reset();
			Refresh();
			return;
		}
	}
	if (Client && !Text.TrimStartAndEnd().IsEmpty() && Client->SendChatMessage(Kind, ChatBoxTarget, Text))
	{
		// The recipient goes back to the team once sent (UX-34).
		ChatDrafts.Remove(ChatBoxKey);
		ShowChatRecipient(ChatRecipient, FString());
		if (ChatBox)
		{
			ChatBox->SetText(FText::GetEmpty());
		}
	}
}

FText UVeyraShellScreen::GetChatRecipient() const
{
	return ChatRecipient ? ChatRecipient->GetText() : FText::GetEmpty();
}

void UVeyraShellScreen::SetChatDraft(const FString& Text)
{
	ChatDrafts.Add(ChatBoxKey, Text);
	ShowChatRecipient(ChatRecipient, Text);
	if (ChatBox)
	{
		ChatBox->SetText(FText::FromString(Text));
	}
}

void UVeyraShellScreen::HandleChatChanged(const FText& Text)
{
	ChatDrafts.Add(ChatBoxKey, Text.ToString());
	ShowChatRecipient(ChatRecipient, Text.ToString());
}

void UVeyraShellScreen::HandleChatCommitted(const FText& Text, ETextCommit::Type Method)
{
	ChatDrafts.Add(ChatBoxKey, Text.ToString());
	if (Method == ETextCommit::OnEnter)
	{
		SubmitChat();
		// Enter sends and keeps the composer, so the player can go on typing.
		if (ChatBox)
		{
			ChatBox->SetKeyboardFocus();
		}
	}
}

#undef LOCTEXT_NAMESPACE
