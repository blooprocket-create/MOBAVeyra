// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "Backend/VeyraChatProtocol.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/Services/VeyraClientFlowTestRig.h"

namespace VeyraClientFlowTests
{
	using VeyraBackendProtocol::EChatKind;

	// Fixture answers, independent of the committed backend configuration.
	inline FString ChatMessageJson(int64 Seq, const TCHAR* Kind, const FString& Conversation, const TCHAR* Sender, const TCHAR* SenderName,
		const TCHAR* Recipient, const FString& Text, const FString& ClientId)
	{
		return FString::Printf(TEXT("{\"seq\":%lld,\"kind\":\"%s\",\"conversation\":\"%s\",\"sender\":{\"id\":\"%s\",\"displayName\":\"%s\"},")
								   TEXT("\"recipientId\":%s,\"text\":\"%s\",\"sentAt\":\"2026-10-02T12:00:00Z\",\"clientId\":\"%s\"}"),
			static_cast<long long>(Seq), Kind, *Conversation, Sender, SenderName, *Quoted(Recipient), *Text, *ClientId);
	}

	/** DevTwo's party line. */
	inline FString PartyLine(int64 Seq, const FString& Text, const FString& Party = PartyId)
	{
		return ChatMessageJson(Seq, TEXT("party"), Party, FriendId, TEXT("DevTwo"), TEXT(""), Text, FString::Printf(TEXT("their-%04lld"), static_cast<long long>(Seq)));
	}

	/** DevTwo's direct message to the player. */
	inline FString DirectLine(int64 Seq, const FString& Text)
	{
		return ChatMessageJson(Seq, TEXT("direct"), FString(AccountId) + TEXT("|") + FriendId, FriendId, TEXT("DevTwo"), AccountId, Text,
			FString::Printf(TEXT("their-%04lld"), static_cast<long long>(Seq)));
	}

	inline FString ChatPage(const TArray<FString>& Messages, int64 Next, bool bMore = false)
	{
		return FString::Printf(TEXT("{\"messages\":[%s],\"next\":%lld,\"more\":%s}"), *FString::Join(Messages, TEXT(",")), static_cast<long long>(Next),
			bMore ? TEXT("true") : TEXT("false"));
	}

	inline FString ChatSentAnswer(const FString& Message) { return FString::Printf(TEXT("{\"message\":%s}"), *Message); }

	inline FString ChatAfter(int64 Cursor) { return FString::Printf(TEXT("/v1/me/chat?after=%lld"), static_cast<long long>(Cursor)); }

	inline FString ClientIdOf(const FString& Body)
	{
		TSharedPtr<FJsonObject> Root;
		FString Id;
		return FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Body), Root) && Root->TryGetStringField(TEXT("clientId"), Id) ? Id : FString();
	}

	inline FString DirectPath() { return FString(TEXT("/v1/me/chat/direct/")) + FriendId; }

	// Veyra.Services.ChatWire.*: chat on the wire (ADR-046 §4).
	TEST_CLASS(ChatWire, "Veyra.Services")
	{
		TEST_METHOD(APageReadsItsMessagesInOrder)
		{
			VeyraBackendProtocol::FChatPage Page;
			FString Problem;
			ASSERT_THAT(IsTrue(VeyraBackendProtocol::ParseChatPage(ChatPage({ PartyLine(3, TEXT("ready?")), DirectLine(5, TEXT("psst")) }, 6, true), Page, Problem), Problem));
			ASSERT_THAT(IsTrue(Page.Next == 6 && Page.bMore && Page.Messages.Num() == 2));
			ASSERT_THAT(IsTrue(Page.Messages[0].Kind == EChatKind::Party && Page.Messages[0].Conversation == PartyId && Page.Messages[0].RecipientId.IsEmpty()
				&& Page.Messages[0].SenderName == TEXT("DevTwo")));
			ASSERT_THAT(IsTrue(Page.Messages[1].Kind == EChatKind::Direct && Page.Messages[1].RecipientId == AccountId && Page.Messages[1].Text == TEXT("psst")));
		}

		TEST_METHOD(AMalformedPageIsRefused)
		{
			VeyraBackendProtocol::FChatPage Page;
			FString Problem;
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseChatPage(TEXT("{\"messages\":[]}"), Page, Problem), TEXT("no cursor")));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseChatPage(ChatPage({ PartyLine(5, TEXT("a")), PartyLine(4, TEXT("b")) }, 5), Page, Problem), TEXT("out of order")));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseChatPage(ChatPage({ PartyLine(9, TEXT("a")) }, 5), Page, Problem), TEXT("past its cursor")));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseChatPage(ChatPage({ ChatMessageJson(1, TEXT("shout"), TEXT("x"), FriendId, TEXT("DevTwo"), TEXT(""), TEXT("a"), TEXT("their-0001")) }, 1),
				Page, Problem), TEXT("an unknown kind")));
			ASSERT_THAT(IsFalse(VeyraBackendProtocol::ParseChatPage(ChatPage({ ChatMessageJson(1, TEXT("direct"), TEXT("x"), FriendId, TEXT("DevTwo"), TEXT(""), TEXT("a"), TEXT("their-0001")) }, 1),
				Page, Problem), TEXT("a direct message without its recipient")));
		}

		TEST_METHOD(ASendCarriesItsClientIdAndText)
		{
			const FString Body = VeyraBackendProtocol::BuildChatBody(TEXT("client-0001"), TEXT("say \"hi\""));
			TSharedPtr<FJsonObject> Root;
			FString Text;
			ASSERT_THAT(IsTrue(FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Body), Root) && Root->TryGetStringField(TEXT("text"), Text)));
			ASSERT_THAT(IsTrue(Text == TEXT("say \"hi\"") && ClientIdOf(Body) == TEXT("client-0001")));
		}
	};

	// Veyra.Services.ChatFlow.*: Party Chat, direct messages, select and post-match chat in the flow (ADR-046 §6),
	// on the fake backend.
	TEST_CLASS(ChatFlow, "Veyra.Services")
	{
		FClientFlowTestRig Rig;
		FFlowTestBackend& Backend = Rig.Backend;
		TUniquePtr<FVeyraClientFlow>& Flow = Rig.Flow;

		const FVeyraClientSnapshot& Snapshot() const { return Flow->GetSnapshot(); }
		const FVeyraChat& Chat() const { return Snapshot().Chat; }

		/** The shell with DevTwo a friend, and chat's first read answered with Messages. */
		bool ReachChat(const TArray<FString>& Messages = {}, int64 Next = 0)
		{
			if (!Rig.ReachShell() || !Rig.ReadSocial())
			{
				return false;
			}
			Rig.Advance(0.0);
			return Backend.Answer(TEXT("GET"), TEXT("/v1/me/chat"), 200, ChatPage(Messages, Next)) && Chat().bLoaded;
		}

		TEST_METHOD(TheFirstReadIsHistoryAndLaterReadsFollowTheCursor)
		{
			ASSERT_THAT(IsTrue(ReachChat({ PartyLine(3, TEXT("ready?")), DirectLine(4, TEXT("earlier")) }, 4)));
			ASSERT_THAT(IsTrue(Chat().Party.Key == PartyId && Chat().Party.Lines.Num() == 1 && Chat().Party.Lines[0].Text == TEXT("ready?")));
			const FVeyraChatConversation* Direct = Chat().Direct.Find(FriendId);
			ASSERT_THAT(IsTrue(Direct && Direct->Lines.Num() == 1 && Direct->Lines[0].With == FriendId && Direct->Lines[0].bHistory));
			ASSERT_THAT(IsTrue(Direct->Unread == 0 && Chat().LastDirectFrom.IsEmpty(), TEXT("history is not news")));
			// The next read follows the cursor, on chat's own interval.
			Rig.Advance(0.5);
			ASSERT_THAT(IsNull(Backend.Find(TEXT("GET"), ChatAfter(4)), TEXT("not before the interval")));
			Rig.Advance(0.5);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ChatAfter(4), 200, ChatPage({ DirectLine(5, TEXT("psst")), PartyLine(6, TEXT("go")) }, 6))));
			Direct = Chat().Direct.Find(FriendId);
			ASSERT_THAT(IsTrue(Direct->Lines.Num() == 2 && Direct->Unread == 1 && Chat().LastDirectFrom == FriendId));
			ASSERT_THAT(AreEqual(Chat().Party.Lines.Num(), 2));
			// A full page asks again at once.
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ChatAfter(6), 200, ChatPage({}, 9, /*bMore*/ true))));
			Rig.Advance(0.0);
			ASSERT_THAT(IsNotNull(Backend.Find(TEXT("GET"), ChatAfter(9))));
		}

		TEST_METHOD(ChatIsReadInEveryStateButReconnectOnly)
		{
			ASSERT_THAT(IsTrue(Rig.ReachReconnectOnly()));
			Rig.Advance(2.0);
			ASSERT_THAT(IsNull(Backend.Find(TEXT("GET"), TEXT("/v1/me/chat")), TEXT("Reconnect-only offers nothing else")));
			ASSERT_THAT(IsFalse(Flow->CanIssue(EVeyraClientIntent::SendChatMessage)));
			// Rejoined, chat resumes, a match included.
			ASSERT_THAT(IsTrue(Flow->Reconnect()));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/match"), 200, ReadyMatch())));
			Flow->NotifyWorld(EVeyraClientWorld::Match);
			ASSERT_THAT(IsTrue(Rig.State() == EVeyraClientState::InMatch));
			Rig.Advance(0.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/me/chat"), 200, ChatPage({ PartyLine(2, TEXT("welcome back")) }, 2))));
			ASSERT_THAT(IsTrue(Chat().Party.Lines.Num() == 1 && Flow->CanIssue(EVeyraClientIntent::SendChatMessage)));
		}

		TEST_METHOD(AMessageShowsPendingUntilItsAnswerConfirmsIt)
		{
			ASSERT_THAT(IsTrue(ReachChat()));
			ASSERT_THAT(IsTrue(Flow->SendChatMessage(EChatKind::Direct, FriendId, TEXT("  hi there  "))));
			const FFlowTestBackend::FRequest* Request = Backend.Find(TEXT("POST"), DirectPath());
			ASSERT_THAT(IsNotNull(Request));
			const FString ClientId = ClientIdOf(Request->Body);
			ASSERT_THAT(IsTrue(ClientId.Len() == 36 && Request->Body.Contains(TEXT("\"text\":\"hi there\""))));
			const FVeyraChatConversation* Direct = Chat().Direct.Find(FriendId);
			ASSERT_THAT(IsTrue(Direct && Direct->Lines.Num() == 1 && Direct->Lines[0].bPending && Direct->Lines[0].Seq == 0 && Direct->Lines[0].SenderId == AccountId));
			const FString Stored = ChatMessageJson(7, TEXT("direct"), FString(AccountId) + TEXT("|") + FriendId, AccountId, TEXT("DevOne"), FriendId, TEXT("hi there"), ClientId);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), DirectPath(), 200, ChatSentAnswer(Stored))));
			Direct = Chat().Direct.Find(FriendId);
			ASSERT_THAT(IsTrue(Direct->Lines.Num() == 1 && !Direct->Lines[0].bPending && Direct->Lines[0].Seq == 7 && Direct->Unread == 0));
			// The next read repeating it shows it once.
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ChatAfter(0), 200, ChatPage({ Stored }, 7))));
			ASSERT_THAT(AreEqual(Chat().Direct.Find(FriendId)->Lines.Num(), 1));
			ASSERT_THAT(IsFalse(Flow->SendChatMessage(EChatKind::Direct, TEXT("not-a-friend"), TEXT("hi")), TEXT("only friends")));
			ASSERT_THAT(IsFalse(Flow->SendChatMessage(EChatKind::Party, FString(), TEXT("   ")), TEXT("nothing to say")));
			ASSERT_THAT(IsFalse(Flow->SendChatMessage(EChatKind::Select, FString(), TEXT("mid?")), TEXT("no select")));
		}

		TEST_METHOD(ALostSendTriesAgainWithItsIdAndARefusalMarksTheLine)
		{
			ASSERT_THAT(IsTrue(ReachChat()));
			ASSERT_THAT(IsFalse(Flow->SendChatMessage(EChatKind::Party, FString(), TEXT("hello")), TEXT("no party known to write to")));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, PartyOfTwoBody(TEXT("idle"), /*bYouLead*/ true))));
			// The send names the party the player writes to, so it never reaches a party they joined since (ADR-046 §3).
			const FString PartyPath = FString(TEXT("/v1/me/chat/party/")) + PartyId;
			ASSERT_THAT(IsTrue(Flow->SendChatMessage(EChatKind::Party, FString(), TEXT("hello"))));
			const FString ClientId = ClientIdOf(Backend.Find(TEXT("POST"), PartyPath)->Body);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), PartyPath, 0)));
			Rig.Advance(1.0);
			ASSERT_THAT(AreEqual(ClientIdOf(Backend.Find(TEXT("POST"), PartyPath)->Body), ClientId, TEXT("the same ID again")));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), PartyPath, 0)));
			ASSERT_THAT(IsTrue(Chat().Party.Lines.Last().Failure == TEXT("not_sent") && !Chat().Party.Lines.Last().bPending));
			// A refusal marks the line, never the screen.
			ASSERT_THAT(IsTrue(Flow->SendChatMessage(EChatKind::Party, FString(), TEXT("anyone?"))));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), PartyPath, 409, ErrorBody(TEXT("conversation_changed")))));
			ASSERT_THAT(IsTrue(Chat().Party.Lines.Last().Failure == TEXT("conversation_changed") && !Snapshot().Problem.IsSet()));
			// A request in flight elsewhere never holds chat up.
			ASSERT_THAT(IsTrue(Flow->LoadCollection() && Snapshot().bBusy));
			ASSERT_THAT(IsTrue(Flow->SendChatMessage(EChatKind::Party, FString(), TEXT("still here"))));
		}

		TEST_METHOD(OpeningADirectConversationClearsItsUnreadCount)
		{
			ASSERT_THAT(IsTrue(ReachChat({}, 0)));
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ChatAfter(0), 200, ChatPage({ DirectLine(1, TEXT("one")), DirectLine(2, TEXT("two")) }, 2))));
			ASSERT_THAT(AreEqual(Chat().Direct.Find(FriendId)->Unread, 2));
			ASSERT_THAT(IsTrue(Flow->OpenDirectChat(FriendId)));
			ASSERT_THAT(IsTrue(Chat().OpenDirect == FriendId && Chat().Direct.Find(FriendId)->Unread == 0));
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ChatAfter(2), 200, ChatPage({ DirectLine(3, TEXT("three")) }, 3))));
			ASSERT_THAT(AreEqual(Chat().Direct.Find(FriendId)->Unread, 0, TEXT("read as it arrives while open")));
			ASSERT_THAT(IsTrue(Flow->CloseDirectChat() && Chat().OpenDirect.IsEmpty()));
			ASSERT_THAT(IsFalse(Flow->OpenDirectChat(TEXT("not-a-friend"))));
		}

		TEST_METHOD(AConversationKeepsItsNewestLines)
		{
			TArray<FString> Lines;
			for (int64 Seq = 1; Seq <= FClientFlowTestRig::ChatKeepMessages + 2; ++Seq)
			{
				Lines.Add(PartyLine(Seq, FString::Printf(TEXT("line %lld"), static_cast<long long>(Seq))));
			}
			ASSERT_THAT(IsTrue(ReachChat(Lines, Lines.Num())));
			ASSERT_THAT(AreEqual(Chat().Party.Lines.Num(), FClientFlowTestRig::ChatKeepMessages));
			ASSERT_THAT(AreEqual(Chat().Party.Lines[0].Seq, int64(3)));
		}

		TEST_METHOD(LeavingThePartyEndsItsChatAndANewPartyStartsAfresh)
		{
			ASSERT_THAT(IsTrue(ReachChat({ PartyLine(1, TEXT("old party")) }, 1)));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), TEXT("/v1/party"), 200, NoParty)));
			ASSERT_THAT(IsTrue(Chat().Party.Lines.IsEmpty() && Chat().Party.Key.IsEmpty()));
			// A line from another party is a new party's.
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ChatAfter(1), 200, ChatPage({ PartyLine(2, TEXT("new party"), FriendPartyId) }, 2))));
			ASSERT_THAT(IsTrue(Chat().Party.Key == FriendPartyId && Chat().Party.Lines.Num() == 1));
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ChatAfter(2), 200, ChatPage({ PartyLine(3, TEXT("third party")) }, 3))));
			ASSERT_THAT(IsTrue(Chat().Party.Key == PartyId && Chat().Party.Lines.Num() == 1 && Chat().Party.Lines[0].Text == TEXT("third party")));
		}

		TEST_METHOD(ThePostMatchChatOpensOnTheResultsAndEndsOnContinue)
		{
			ASSERT_THAT(IsTrue(Rig.ReachResults()));
			ASSERT_THAT(IsTrue(Chat().PostMatch.Key == MatchId && !Chat().bPostMatchJoined));
			const FString PostMatchPath = FString(TEXT("/v1/me/chat/matches/")) + MatchId;
			ASSERT_THAT(IsTrue(Flow->SendChatMessage(EChatKind::PostMatch, FString(), TEXT("gg"))));
			const FString ClientId = ClientIdOf(Backend.Find(TEXT("POST"), PostMatchPath)->Body);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), PostMatchPath, 200,
				ChatSentAnswer(ChatMessageJson(8, TEXT("postmatch"), MatchId, AccountId, TEXT("DevOne"), TEXT(""), TEXT("gg"), ClientId)))));
			ASSERT_THAT(IsTrue(Chat().bPostMatchJoined && Chat().PostMatch.Lines.Num() == 1));
			// Another participant speaks, and the player mutes them.
			Rig.Advance(1.0);
			const FFlowTestBackend::FRequest* Read = Backend.Pending.FindByPredicate([](const FFlowTestBackend::FRequest& Request) { return Request.Path.StartsWith(TEXT("/v1/me/chat")) && Request.Verb == TEXT("GET"); });
			ASSERT_THAT(IsNotNull(Read));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), Read->Path, 200,
				ChatPage({ ChatMessageJson(9, TEXT("postmatch"), MatchId, FriendId, TEXT("DevTwo"), TEXT(""), TEXT("wp"), TEXT("their-0009")) }, 9))));
			ASSERT_THAT(AreEqual(Chat().PostMatch.Lines.Num(), 2));
			ASSERT_THAT(IsFalse(Flow->MutePostMatchChat(AccountId, true), TEXT("not oneself")));
			ASSERT_THAT(IsTrue(Flow->MutePostMatchChat(FriendId, true)));
			ASSERT_THAT(IsTrue(Chat().PostMatchMuted.Contains(FriendId) && Backend.Find(TEXT("PUT"), PostMatchPath + TEXT("/mutes/") + FriendId) != nullptr));
			// Continue leaves it for good.
			ASSERT_THAT(IsTrue(Flow->ContinueFromResults()));
			ASSERT_THAT(IsNotNull(Backend.Find(TEXT("DELETE"), PostMatchPath)));
			ASSERT_THAT(IsTrue(Chat().PostMatch.Key.IsEmpty() && Chat().PostMatch.Lines.IsEmpty() && !Chat().bPostMatchJoined));
		}

		TEST_METHOD(AReadThatBringsThePlayersFirstPostMatchLineOptsThemIn)
		{
			ASSERT_THAT(IsTrue(Rig.ReachResults()));
			const FString PostMatchPath = FString(TEXT("/v1/me/chat/matches/")) + MatchId;
			ASSERT_THAT(IsTrue(Flow->SendChatMessage(EChatKind::PostMatch, FString(), TEXT("gg"))));
			const FString ClientId = ClientIdOf(Backend.Find(TEXT("POST"), PostMatchPath)->Body);
			const FString Stored = ChatMessageJson(8, TEXT("postmatch"), MatchId, AccountId, TEXT("DevOne"), TEXT(""), TEXT("gg"), ClientId);
			// The read delivers the stored line before the send's answer arrives.
			Rig.Advance(1.0);
			const FFlowTestBackend::FRequest* Read = Backend.Pending.FindByPredicate([](const FFlowTestBackend::FRequest& Request) { return Request.Path.StartsWith(TEXT("/v1/me/chat")) && Request.Verb == TEXT("GET"); });
			ASSERT_THAT(IsNotNull(Read));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), Read->Path, 200, ChatPage({ Stored }, 8))));
			ASSERT_THAT(IsTrue(Chat().bPostMatchJoined && Chat().PostMatch.Lines.Num() == 1 && !Chat().PostMatch.Lines[0].bPending));
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), PostMatchPath, 200, ChatSentAnswer(Stored))));
			ASSERT_THAT(IsTrue(Chat().bPostMatchJoined && Chat().PostMatch.Lines.Num() == 1));
		}

		TEST_METHOD(TheSelectChatIsTheSelectsOwn)
		{
			ASSERT_THAT(IsTrue(Rig.ReachSelect()));
			ASSERT_THAT(IsTrue(Flow->SendChatMessage(EChatKind::Select, FString(), TEXT("mid?"))));
			// The send names the select it was written in.
			const FString SelectChatPath = FString(TEXT("/v1/me/chat/select/")) + SelectId;
			const FString ClientId = ClientIdOf(Backend.Find(TEXT("POST"), SelectChatPath)->Body);
			const FString Key = FString(SelectId) + TEXT("|A");
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("POST"), SelectChatPath, 200,
				ChatSentAnswer(ChatMessageJson(4, TEXT("select"), Key, AccountId, TEXT("DevOne"), TEXT(""), TEXT("mid?"), ClientId)))));
			ASSERT_THAT(IsTrue(Chat().Select.Key == Key && Chat().Select.Lines.Num() == 1 && !Chat().Select.Lines[0].bPending));
			ASSERT_THAT(IsFalse(Flow->SendChatMessage(EChatKind::PostMatch, FString(), TEXT("gg")), TEXT("no results yet")));
		}

		TEST_METHOD(AnEndedSessionDropsChat)
		{
			ASSERT_THAT(IsTrue(ReachChat({ PartyLine(1, TEXT("hi")) }, 1)));
			TestRunner->AddExpectedMessagePlain(TEXT("VeyraClientFlow: the backend ended the game session."), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			Rig.Advance(1.0);
			ASSERT_THAT(IsTrue(Backend.Answer(TEXT("GET"), ChatAfter(1), 401, ErrorBody(TEXT("invalid_credentials")))));
			ASSERT_THAT(IsTrue(Rig.State() == EVeyraClientState::SessionEnded && Chat().Party.Lines.IsEmpty() && !Chat().bLoaded));
			Rig.Advance(2.0);
			ASSERT_THAT(IsNull(Backend.Find(TEXT("GET"), TEXT("/v1/me/chat"))));
		}
	};
}

#endif
