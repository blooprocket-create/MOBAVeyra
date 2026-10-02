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
	// Text in a scroll box wraps at a set width: measured against the box alone, a long line would overlap the next.
	// The panel sits in the sidebar's width, less its own and its lines' padding on each side.
	const float WrapAt = Style.FriendsPanelWidth - Style.Spacing * 4.0f;
	const auto AddLine = [this, Lines, WrapAt](const FText& Text, EVeyraShellText Role) {
		UTextBlock* Block = AddText(*Lines, Text, ChatRole(Role));
		Block->SetAutoWrapText(false);
		Block->SetWrapTextAt(WrapAt);
	};
	if (Model.Lines.IsEmpty())
	{
		AddLine(Model.Empty, EVeyraShellText::Muted);
	}
	for (const FVeyraChatLineModel& Line : Model.Lines)
	{
		// A party line in a panel that mixes conversations says so (UX-34).
		const FText Said = Line.bParty ? FText::Format(LOCTEXT("ChatPartyLine", "[Party] {0}: {1}"), Line.Sender, Line.Text)
									   : FText::Format(LOCTEXT("ChatLine", "{0}: {1}"), Line.Sender, Line.Text);
		AddLine(Said, Line.bOwn ? EVeyraShellText::Muted : EVeyraShellText::Body);
		if (!Line.Status.IsEmpty())
		{
			AddLine(Line.Status, EVeyraShellText::Small);
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
	Composer->AddChildToHorizontalBox(ChatBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	// Enabled whatever the field holds, so typing never rebuilds the panel; an empty message sends nothing.
	AddNamedButton(*Composer, EVeyraShellButtonKind::Secondary, FText::Format(LOCTEXT("SendChatLabel", "Send to {0}"), Model.Title),
		LOCTEXT("SendChat", "Send"), [this] { SubmitChat(); }, Model.bCanSend);
	VeyraShellStyle::AddSpaced(Parent, *Composer);
}

void UVeyraShellScreen::SubmitChat()
{
	const FString Draft = ChatDrafts.FindRef(ChatBoxKey);
	if (Client && !Draft.TrimStartAndEnd().IsEmpty() && Client->SendChatMessage(ChatBoxKind, ChatBoxTarget, Draft))
	{
		ChatDrafts.Remove(ChatBoxKey);
		if (ChatBox)
		{
			ChatBox->SetText(FText::GetEmpty());
		}
	}
}

void UVeyraShellScreen::SetChatDraft(const FString& Text)
{
	ChatDrafts.Add(ChatBoxKey, Text);
	if (ChatBox)
	{
		ChatBox->SetText(FText::FromString(Text));
	}
}

void UVeyraShellScreen::HandleChatChanged(const FText& Text)
{
	ChatDrafts.Add(ChatBoxKey, Text.ToString());
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
