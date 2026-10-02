// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Blueprint/UserWidget.h"
#include "Components/ActorTestSpawner.h"
#include "Components/EditableTextBox.h"
#include "Shell/VeyraChatModels.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraChatScreenTests
{
	using namespace VeyraClientFlowTests;
	using VeyraBackendProtocol::EChatKind;

	FVeyraChatEntry ChatEntry(EChatKind Kind, const FString& Sender, const FString& Name, const FString& Text, int64 Seq)
	{
		FVeyraChatEntry Entry;
		Entry.Kind = Kind;
		Entry.SenderId = Sender;
		Entry.SenderName = Name;
		Entry.Text = Text;
		Entry.Seq = Seq;
		return Entry;
	}

	/** The coordinator's snapshot in the shell: DevTwo a friend, and in a party or not. */
	FVeyraClientSnapshot ChatSnapshot(bool bInParty)
	{
		FVeyraClientSnapshot Snapshot;
		Snapshot.State = EVeyraClientState::Shell;
		Snapshot.AccountId = AccountId;
		Snapshot.Social.bLoaded = true;
		Snapshot.Social.Friends.Friends.Add({ FriendId, TEXT("DevTwo") });
		if (bInParty)
		{
			FString Problem;
			VeyraBackendProtocol::ParseParty(PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ true), Snapshot.Party, Problem);
		}
		return Snapshot;
	}

	// Veyra.UI.ChatPanelScreen.*: the sidebar's Party Chat and direct conversations (ADR-046 §6; UX-3), from the
	// coordinator's snapshot, and typed into and clicked as the player would.
	TEST_CLASS(ChatPanelScreen, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		FClientFlowTestRig Rig;
		UVeyraShellScreen* Screen = nullptr;

		AFTER_EACH()
		{
			if (Screen)
			{
				Screen->Unbind();
			}
		}

		/** The shell's screen over the coordinator, DevTwo a friend, in a party of two. */
		bool ShowShell()
		{
			if (!Rig.ReachShell())
			{
				return false;
			}
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			return Rig.ReadSocial() && Rig.Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ true));
		}

		bool Press(const FText& Label)
		{
			UVeyraShellButton* Found = Screen->FindButton(Label);
			if (!Found || !Found->GetIsEnabled())
			{
				return false;
			}
			Found->Press();
			return true;
		}

		TEST_METHOD(TheSidebarShowsPartyChatOrTheOpenConversation)
		{
			FVeyraClientSnapshot Snapshot = ChatSnapshot(/*bInParty*/ false);
			ASSERT_THAT(IsFalse(VeyraChatModels::DescribeSidebar(Snapshot, true).bVisible, TEXT("no party and no conversation open")));

			Snapshot = ChatSnapshot(/*bInParty*/ true);
			Snapshot.Chat.Party.Lines.Add(ChatEntry(EChatKind::Party, FriendId, TEXT("DevTwo"), TEXT("ready?"), 3));
			FVeyraChatEntry Mine = ChatEntry(EChatKind::Party, AccountId, TEXT("DevOne"), TEXT("yes"), 0);
			Mine.bPending = true;
			Snapshot.Chat.Party.Lines.Add(Mine);
			FVeyraChatPanelModel Panel = VeyraChatModels::DescribeSidebar(Snapshot, true);
			ASSERT_THAT(IsTrue(Panel.bVisible && Panel.Kind == EChatKind::Party && Panel.bCanSend && !Panel.bCanClose));
			ASSERT_THAT(AreEqual(Panel.Title.ToString(), FString(TEXT("Party Chat"))));
			ASSERT_THAT(IsTrue(Panel.Lines.Num() == 2 && Panel.Lines[0].Sender.ToString() == TEXT("DevTwo") && Panel.Lines[1].Sender.ToString() == TEXT("You")));
			ASSERT_THAT(AreEqual(Panel.Lines[1].Status.ToString(), FString(TEXT("Sending…"))));

			// An open direct conversation takes the panel, and closes back to Party Chat.
			Snapshot.Chat.OpenDirect = FriendId;
			FVeyraChatEntry Refused = ChatEntry(EChatKind::Direct, AccountId, TEXT("DevOne"), TEXT("hi"), 0);
			Refused.Failure = TEXT("rate_limited");
			Snapshot.Chat.Direct.FindOrAdd(FriendId).Lines.Add(Refused);
			Panel = VeyraChatModels::DescribeSidebar(Snapshot, true);
			ASSERT_THAT(IsTrue(Panel.Kind == EChatKind::Direct && Panel.Target == FriendId && Panel.bCanClose && Panel.Title.ToString() == TEXT("DevTwo")));
			ASSERT_THAT(IsTrue(Panel.Lines.Num() == 1 && Panel.Lines[0].bFailed));
			ASSERT_THAT(AreEqual(Panel.Lines[0].Status.ToString(), FString(TEXT("Not sent: too many messages at once. Wait a moment."))));
			ASSERT_THAT(IsFalse(VeyraChatModels::DescribeSidebar(Snapshot, /*bCanSend*/ false).bCanSend));
		}

		TEST_METHOD(AFriendsUnreadMessagesShowOnTheirLine)
		{
			FVeyraClientSnapshot Snapshot = ChatSnapshot(/*bInParty*/ false);
			Snapshot.Chat.Direct.FindOrAdd(FriendId).Unread = 2;
			FVeyraSocialPermissions Permissions;
			Permissions.bCanMessage = true;
			const FVeyraFriendsModel Model = VeyraShellModels::DescribeFriends(Snapshot, true, true, true, true, true, Permissions);
			ASSERT_THAT(IsTrue(Model.Friends.Num() == 1 && Model.Friends[0].Unread == 2 && Model.Friends[0].bCanMessage));
		}

		TEST_METHOD(TypingAndSendingReachesTheParty)
		{
			ASSERT_THAT(IsTrue(ShowShell()));
			ASSERT_THAT(IsNotNull(Screen->GetChatBox(), TEXT("the party's chat shows below the friends")));
			Screen->SetChatDraft(TEXT("hello party"));
			ASSERT_THAT(IsTrue(Press(FText::FromString(TEXT("Send to Party Chat")))));
			const FFlowTestBackend::FRequest* Request = Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/chat/party"));
			ASSERT_THAT(IsTrue(Request && Request->Body.Contains(TEXT("\"text\":\"hello party\""))));
			ASSERT_THAT(IsTrue(Screen->GetChatBox() && Screen->GetChatBox()->GetText().IsEmpty(), TEXT("the composer empties once sent")));
			ASSERT_THAT(IsTrue(Rig.Flow->GetSnapshot().Chat.Party.Lines.Num() == 1 && Rig.Flow->GetSnapshot().Chat.Party.Lines[0].bPending));
			// An empty composer sends nothing.
			ASSERT_THAT(IsTrue(Press(FText::FromString(TEXT("Send to Party Chat")))));
			ASSERT_THAT(AreEqual(Rig.Flow->GetSnapshot().Chat.Party.Lines.Num(), 1));
		}

		TEST_METHOD(AFriendsCardOpensTheirConversationAndCloseReturnsToTheParty)
		{
			ASSERT_THAT(IsTrue(ShowShell()));
			ASSERT_THAT(IsTrue(Press(VeyraShellModels::FriendCardLabel(TEXT("DevTwo")))));
			ASSERT_THAT(IsTrue(Press(FText::FromString(TEXT("Message DevTwo")))));
			ASSERT_THAT(AreEqual(Rig.Flow->GetSnapshot().Chat.OpenDirect, FString(FriendId)));
			Screen->SetChatDraft(TEXT("psst"));
			ASSERT_THAT(IsTrue(Press(FText::FromString(TEXT("Send to DevTwo")))));
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("POST"), FString(TEXT("/v1/me/chat/direct/")) + FriendId)));
			ASSERT_THAT(IsTrue(Press(FText::FromString(TEXT("Close the conversation with DevTwo")))));
			ASSERT_THAT(IsTrue(Rig.Flow->GetSnapshot().Chat.OpenDirect.IsEmpty() && Screen->FindButton(FText::FromString(TEXT("Send to Party Chat"))) != nullptr));
		}

		TEST_METHOD(EachConversationKeepsItsOwnDraft)
		{
			ASSERT_THAT(IsTrue(ShowShell()));
			Screen->SetChatDraft(TEXT("for the party"));
			ASSERT_THAT(IsTrue(Rig.Flow->OpenDirectChat(FriendId)));
			ASSERT_THAT(IsTrue(Screen->GetChatBox() && Screen->GetChatBox()->GetText().IsEmpty(), TEXT("DevTwo's conversation has its own")));
			ASSERT_THAT(IsTrue(Rig.Flow->CloseDirectChat()));
			ASSERT_THAT(AreEqual(Screen->GetChatBox()->GetText().ToString(), FString(TEXT("for the party"))));
		}
	};

	// Veyra.UI.SelectChatScreen.*: champion select's one compact chat panel (UX-33–34): the team's lines and the
	// party's in one display, Team by default and Party with /p, collapsible.
	TEST_CLASS(SelectChatScreen, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		FClientFlowTestRig Rig;
		UVeyraShellScreen* Screen = nullptr;

		AFTER_EACH()
		{
			if (Screen)
			{
				Screen->Unbind();
			}
		}

		bool ShowSelect()
		{
			if (!Rig.ReachSelect())
			{
				return false;
			}
			Screen = CreateWidget<UVeyraShellScreen>(&Spawner.GetWorld());
			Screen->Bind(*Rig.Flow);
			return true;
		}

		bool Press(const FString& Label)
		{
			UVeyraShellButton* Found = Screen->FindButton(FText::FromString(Label));
			if (!Found || !Found->GetIsEnabled())
			{
				return false;
			}
			Found->Press();
			return true;
		}

		TEST_METHOD(TheTeamsAndThePartysLinesShareOneDisplayInOrder)
		{
			FVeyraClientSnapshot Snapshot = ChatSnapshot(/*bInParty*/ true);
			Snapshot.State = EVeyraClientState::Selecting;
			Snapshot.Chat.Select.Lines.Add(ChatEntry(EChatKind::Select, FriendId, TEXT("DevTwo"), TEXT("mid?"), 5));
			Snapshot.Chat.Party.Lines.Add(ChatEntry(EChatKind::Party, FriendId, TEXT("DevTwo"), TEXT("duo bot"), 4));
			FVeyraChatEntry Mine = ChatEntry(EChatKind::Select, AccountId, TEXT("DevOne"), TEXT("ok"), 0);
			Mine.bPending = true;
			Snapshot.Chat.Select.Lines.Add(Mine);
			const FVeyraChatPanelModel Panel = VeyraChatModels::DescribeSelectChat(Snapshot, true);
			ASSERT_THAT(IsTrue(Panel.bVisible && Panel.Kind == EChatKind::Select && Panel.bShowsRecipient && Panel.Title.ToString() == TEXT("Team Chat")));
			ASSERT_THAT(AreEqual(Panel.Lines.Num(), 3));
			ASSERT_THAT(IsTrue(Panel.Lines[0].bParty && Panel.Lines[0].Text.ToString() == TEXT("duo bot"), TEXT("the party's line, marked, by its sequence")));
			ASSERT_THAT(IsTrue(!Panel.Lines[1].bParty && Panel.Lines[1].Text.ToString() == TEXT("mid?")));
			ASSERT_THAT(IsTrue(Panel.Lines[2].bOwn && !Panel.Lines[2].Status.IsEmpty(), TEXT("the unanswered line last")));
		}

		TEST_METHOD(SlashPAddressesThePartyAndAnythingElseTheTeam)
		{
			FString Text;
			ASSERT_THAT(IsTrue(VeyraChatModels::SelectRecipient(TEXT("/p duo bot"), Text) == EChatKind::Party && Text == TEXT("duo bot")));
			ASSERT_THAT(IsTrue(VeyraChatModels::SelectRecipient(TEXT("  /P   go  "), Text) == EChatKind::Party && Text == TEXT("go")));
			ASSERT_THAT(IsTrue(VeyraChatModels::SelectRecipient(TEXT("/party"), Text) == EChatKind::Select && Text == TEXT("/party"), TEXT("not the command")));
			ASSERT_THAT(IsTrue(VeyraChatModels::SelectRecipient(TEXT("mid?"), Text) == EChatKind::Select && Text == TEXT("mid?")));
			ASSERT_THAT(AreEqual(VeyraChatModels::RecipientLabel(EChatKind::Party).ToString(), FString(TEXT("Party"))));
			ASSERT_THAT(AreEqual(VeyraChatModels::RecipientLabel(EChatKind::Select).ToString(), FString(TEXT("Team"))));
		}

		TEST_METHOD(TheComposerSendsToTheTeamOrWithSlashPToTheParty)
		{
			ASSERT_THAT(IsTrue(ShowSelect()));
			ASSERT_THAT(AreEqual(Screen->GetChatRecipient().ToString(), FString(TEXT("Team"))));
			Screen->SetChatDraft(TEXT("/p duo bot"));
			ASSERT_THAT(AreEqual(Screen->GetChatRecipient().ToString(), FString(TEXT("Party")), TEXT("the recipient follows the draft")));
			ASSERT_THAT(IsTrue(Press(TEXT("Send to Team Chat"))));
			const FFlowTestBackend::FRequest* Party = Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/chat/party"));
			ASSERT_THAT(IsTrue(Party && Party->Body.Contains(TEXT("\"text\":\"duo bot\"")), TEXT("/p is taken off")));
			ASSERT_THAT(AreEqual(Screen->GetChatRecipient().ToString(), FString(TEXT("Team")), TEXT("back to the team once sent")));
			Screen->SetChatDraft(TEXT("mid?"));
			ASSERT_THAT(IsTrue(Press(TEXT("Send to Team Chat"))));
			ASSERT_THAT(IsNotNull(Rig.Backend.Find(TEXT("POST"), TEXT("/v1/me/chat/select"))));
		}

		TEST_METHOD(ThePanelCollapsesAndComesBack)
		{
			ASSERT_THAT(IsTrue(ShowSelect()));
			ASSERT_THAT(IsTrue(Press(TEXT("Hide Chat"))));
			ASSERT_THAT(IsTrue(Screen->FindButton(FText::FromString(TEXT("Send to Team Chat"))) == nullptr && Screen->GetChatBox() == nullptr));
			ASSERT_THAT(IsTrue(Press(TEXT("Show Chat"))));
			ASSERT_THAT(IsNotNull(Screen->FindButton(FText::FromString(TEXT("Send to Team Chat")))));
		}
	};
}

#endif
