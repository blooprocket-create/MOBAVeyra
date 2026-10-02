// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Chat/VeyraChatCommands.h"
#include "Chat/VeyraChatComposer.h"
#include "Components/ActorTestSpawner.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Hud/VeyraChatLogModel.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraUIInputSettings.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"

namespace VeyraChatTests
{
	// Veyra.UI.ChatCommands.*: what a line typed in the composer asks for (ADR-029 §3, §5, §8).
	TEST_CLASS(ChatCommands, "Veyra.UI")
	{
		TEST_METHOD(AMessageGoesOnTheChosenChannelAndSlashAllSendsOneToAll)
		{
			const FVeyraChatCommand Team = VeyraChatCommands::Parse(TEXT("  push mid "), EVeyraChatChannel::Team);
			ASSERT_THAT(IsTrue(Team.Kind == EVeyraChatCommandKind::Send && Team.Channel == EVeyraChatChannel::Team && Team.Text == TEXT("push mid")));
			const FVeyraChatCommand Chosen = VeyraChatCommands::Parse(TEXT("gg"), EVeyraChatChannel::All);
			ASSERT_THAT(IsTrue(Chosen.Kind == EVeyraChatCommandKind::Send && Chosen.Channel == EVeyraChatChannel::All, TEXT("the channel Tab chose")));
			const FVeyraChatCommand All = VeyraChatCommands::Parse(TEXT("/ALL  well played"), EVeyraChatChannel::Team);
			ASSERT_THAT(IsTrue(All.Kind == EVeyraChatCommandKind::Send && All.Channel == EVeyraChatChannel::All && All.Text == TEXT("well played")));
			ASSERT_THAT(IsTrue(VeyraChatCommands::Parse(TEXT("/all"), EVeyraChatChannel::Team).Kind == EVeyraChatCommandKind::Nothing, TEXT("nothing after it")));
			ASSERT_THAT(IsTrue(VeyraChatCommands::Parse(TEXT("   "), EVeyraChatChannel::Team).Kind == EVeyraChatCommandKind::Nothing));
		}

		TEST_METHOD(MuteAndUnmuteNameAParticipantAndOtherCommandsAreUnknown)
		{
			const FVeyraChatCommand Mute = VeyraChatCommands::Parse(TEXT("/mute Dusk Walker"), EVeyraChatChannel::Team);
			ASSERT_THAT(IsTrue(Mute.Kind == EVeyraChatCommandKind::Mute && Mute.Name == TEXT("Dusk Walker"), TEXT("a name may have spaces")));
			const FVeyraChatCommand Unmute = VeyraChatCommands::Parse(TEXT("/unmute Nyx"), EVeyraChatChannel::All);
			ASSERT_THAT(IsTrue(Unmute.Kind == EVeyraChatCommandKind::Unmute && Unmute.Name == TEXT("Nyx")));
			ASSERT_THAT(IsTrue(VeyraChatCommands::Parse(TEXT("/mute"), EVeyraChatChannel::Team).Kind == EVeyraChatCommandKind::Nothing, TEXT("no one named")));
			const FVeyraChatCommand Unknown = VeyraChatCommands::Parse(TEXT("/muted Nyx"), EVeyraChatChannel::Team);
			ASSERT_THAT(IsTrue(Unknown.Kind == EVeyraChatCommandKind::Unknown && Unknown.Name == TEXT("/muted")));
		}

		TEST_METHOD(PartyReplyAndMessageGoToTheBackendsConversations)
		{
			const FVeyraChatCommand Party = VeyraChatCommands::Parse(TEXT("/p  group mid "), EVeyraChatChannel::All);
			ASSERT_THAT(IsTrue(Party.Kind == EVeyraChatCommandKind::Party && Party.Text == TEXT("group mid")));
			const FVeyraChatCommand Reply = VeyraChatCommands::Parse(TEXT("/R on my way"), EVeyraChatChannel::Team);
			ASSERT_THAT(IsTrue(Reply.Kind == EVeyraChatCommandKind::Reply && Reply.Text == TEXT("on my way")));
			const FVeyraChatCommand Message = VeyraChatCommands::Parse(TEXT("/msg DevTwo  are you free after? "), EVeyraChatChannel::Team);
			ASSERT_THAT(IsTrue(Message.Kind == EVeyraChatCommandKind::Message && Message.Name == TEXT("DevTwo") && Message.Text == TEXT("are you free after?")));
			ASSERT_THAT(IsTrue(VeyraChatCommands::Parse(TEXT("/msg DevTwo"), EVeyraChatChannel::Team).Kind == EVeyraChatCommandKind::Nothing, TEXT("nothing to say")));
			ASSERT_THAT(IsTrue(VeyraChatCommands::Parse(TEXT("/p"), EVeyraChatChannel::Team).Kind == EVeyraChatCommandKind::Nothing));
			ASSERT_THAT(IsTrue(VeyraChatCommands::Parse(TEXT("/party on"), EVeyraChatChannel::Team).Kind == EVeyraChatCommandKind::Unknown, TEXT("not /p")));
		}

		TEST_METHOD(AParticipantIsFoundByTheirWholeNameIgnoringCase)
		{
			const TArray<FVeyraChatParticipant> Participants = { { 3, TEXT("Nyx") }, { 7, TEXT("Dusk Walker") }, { 9, TEXT("nyx2") } };
			ASSERT_THAT(IsTrue(VeyraChatCommands::FindPlayer(TEXT(" NYX "), Participants) == TOptional<int32>(3)));
			ASSERT_THAT(IsTrue(VeyraChatCommands::FindPlayer(TEXT("dusk walker"), Participants) == TOptional<int32>(7)));
			ASSERT_THAT(IsFalse(VeyraChatCommands::FindPlayer(TEXT("Dusk"), Participants).IsSet(), TEXT("not part of a name")));
			const TArray<FVeyraChatParticipant> Twins = { { 1, TEXT("Nyx") }, { 2, TEXT("nyx") } };
			ASSERT_THAT(IsFalse(VeyraChatCommands::FindPlayer(TEXT("Nyx"), Twins).IsSet(), TEXT("nor one of two")));
		}
	};

	// Veyra.UI.ChatLog.*: the HUD's chat log (ADR-029 §5; SET-66, SET-67). Fixture values, not the committed presentation.
	TEST_CLASS(ChatLog, "Veyra.UI")
	{
		FVeyraChatLogPreferences Preferences;

		BEFORE_EACH()
		{
			Preferences.Lines = 2;
			Preferences.FadeSeconds = 10.0;
			Preferences.FadeOutSeconds = 2.0;
			Preferences.bTimestamps = false;
		}

		static FVeyraReceivedChat Said(EVeyraTeam Side, EVeyraChatChannel Channel, const TCHAR* Text, double At)
		{
			FVeyraReceivedChat Line;
			Line.Message.SenderId = Side == EVeyraTeam::A ? 1 : 2;
			Line.Message.SenderName = Side == EVeyraTeam::A ? TEXT("Nyx") : TEXT("Rook");
			Line.Message.SenderTeam = Side;
			Line.Message.Channel = Channel;
			Line.Message.Text = Text;
			Line.ReceivedAt = At;
			Line.MatchSeconds = 312.0;
			return Line;
		}

		static FString NoVanguard(int32 /*PlayerId*/) { return FString(); }

		TEST_METHOD(OnlyTheNewestShowAndEachFadesUnlessThePlayerComposes)
		{
			const TArray<FVeyraReceivedChat> Chat = { Said(EVeyraTeam::A, EVeyraChatChannel::Team, TEXT("one"), 0.0),
				Said(EVeyraTeam::A, EVeyraChatChannel::Team, TEXT("two"), 5.0), Said(EVeyraTeam::B, EVeyraChatChannel::All, TEXT("three"), 9.0) };
			const TArray<FVeyraChatLine> Fresh = VeyraChatLog::Describe(Chat, 9.0, false, Preferences, EVeyraTeam::A, &NoVanguard);
			ASSERT_THAT(AreEqual(2, Fresh.Num()));
			ASSERT_THAT(IsTrue(Fresh[0].Text == TEXT("two") && Fresh[1].Text == TEXT("three"), TEXT("the newest two, oldest first")));

			const TArray<FVeyraChatLine> Fading = VeyraChatLog::Describe(Chat, 16.0, false, Preferences, EVeyraTeam::A, &NoVanguard);
			ASSERT_THAT(IsTrue(Fading.Num() == 2 && FMath::IsNearlyEqual(Fading[0].Opacity, 0.5) && Fading[1].Opacity == 1.0, TEXT("half faded a second past its time")));
			const TArray<FVeyraChatLine> Gone = VeyraChatLog::Describe(Chat, 30.0, false, Preferences, EVeyraTeam::A, &NoVanguard);
			ASSERT_THAT(IsTrue(Gone.IsEmpty()));
			const TArray<FVeyraChatLine> Composing = VeyraChatLog::Describe(Chat, 30.0, true, Preferences, EVeyraTeam::A, &NoVanguard);
			ASSERT_THAT(IsTrue(Composing.Num() == 2 && Composing[0].Opacity == 1.0, TEXT("all back while the player types")));
		}

		TEST_METHOD(EachLineNamesItsChannelSenderAndSide)
		{
			const TArray<FVeyraReceivedChat> Chat = { Said(EVeyraTeam::A, EVeyraChatChannel::Team, TEXT("b"), 0.0), Said(EVeyraTeam::B, EVeyraChatChannel::All, TEXT("gl hf"), 0.0) };
			const TArray<FVeyraChatLine> Lines = VeyraChatLog::Describe(Chat, 0.0, false, Preferences, EVeyraTeam::A, [](int32 PlayerId) {
				return PlayerId == 2 ? FString(TEXT("Cairn")) : FString();
			});
			ASSERT_THAT(IsTrue(Lines[0].Side == EVeyraChatLineSide::Ally && Lines[0].Prefix == TEXT("[Team] ") && Lines[0].Sender == TEXT("Nyx: ")));
			ASSERT_THAT(IsTrue(Lines[1].Side == EVeyraChatLineSide::Enemy && Lines[1].Prefix == TEXT("[All] ") && Lines[1].Sender == TEXT("Rook (Cairn): ")));

			Preferences.bTimestamps = true;
			const TArray<FVeyraChatLine> Stamped = VeyraChatLog::Describe(Chat, 0.0, false, Preferences, EVeyraTeam::A, &NoVanguard);
			ASSERT_THAT(AreEqual(FString(TEXT("[05:12] [Team] ")), Stamped[0].Prefix));
		}

		TEST_METHOD(NoticesSayWhatHappened)
		{
			FVeyraReceivedChat Refused;
			Refused.Notice = EVeyraChatNotice::Refused;
			Refused.Refusal = EVeyraChatRefusal::TooMany;
			FVeyraReceivedChat Muted;
			Muted.Notice = EVeyraChatNotice::Muted;
			Muted.Message.SenderName = TEXT("Rook");
			FVeyraReceivedChat Unknown;
			Unknown.Notice = EVeyraChatNotice::UnknownCommand;
			Unknown.Message.Text = TEXT("/shrug");
			const TArray<FVeyraReceivedChat> Chat = { Refused, Muted };
			const TArray<FVeyraChatLine> Lines = VeyraChatLog::Describe(Chat, 0.0, false, Preferences, EVeyraTeam::A, &NoVanguard);
			ASSERT_THAT(IsTrue(Lines[0].Side == EVeyraChatLineSide::Notice && Lines[0].Prefix.IsEmpty() && Lines[0].Text.Contains(TEXT("too quickly"))));
			ASSERT_THAT(IsTrue(Lines[1].Text.Contains(TEXT("muted Rook"))));
			ASSERT_THAT(IsTrue(VeyraChatLog::NoticeText(Unknown).ToString().StartsWith(TEXT("/shrug"))));
		}

		TEST_METHOD(ThePartysAndFriendsLinesMixInByArrivalMarkedAsTheirs)
		{
			Preferences.Lines = 3;
			const TArray<FVeyraReceivedChat> Chat = { Said(EVeyraTeam::A, EVeyraChatChannel::Team, TEXT("b"), 5.0) };
			const TArray<FVeyraOutsideChat> Outside = { { EVeyraOutsideChatKind::Party, TEXT("Vale"), TEXT("duo bot"), FString(), 3.0 },
				{ EVeyraOutsideChatKind::DirectFrom, TEXT("Vale"), TEXT("gl"), FString(), 7.0 },
				{ EVeyraOutsideChatKind::DirectTo, TEXT("Vale"), TEXT("ty"), TEXT("sending"), 8.0 } };
			const TArray<FVeyraChatLine> Lines = VeyraChatLog::Describe(Chat, Outside, 8.0, false, Preferences, EVeyraTeam::A, &NoVanguard);
			ASSERT_THAT(AreEqual(3, Lines.Num()));
			ASSERT_THAT(IsTrue(Lines[0].Side == EVeyraChatLineSide::Ally && Lines[0].Text == TEXT("b"), TEXT("the newest three by arrival")));
			ASSERT_THAT(IsTrue(Lines[1].Side == EVeyraChatLineSide::Direct && Lines[1].Prefix == TEXT("[From] ") && Lines[1].Sender == TEXT("Vale: ")));
			ASSERT_THAT(IsTrue(Lines[2].Prefix == TEXT("[To] ") && Lines[2].Text == TEXT("ty (sending)")));
			Preferences.Lines = 4;
			const TArray<FVeyraChatLine> Wider = VeyraChatLog::Describe(Chat, Outside, 8.0, false, Preferences, EVeyraTeam::A, &NoVanguard);
			ASSERT_THAT(IsTrue(Wider[0].Side == EVeyraChatLineSide::Party && Wider[0].Prefix == TEXT("[Party] ") && Wider[0].Text == TEXT("duo bot")));
		}

		TEST_METHOD(TheFlowsPartyAndDirectLinesBecomeTheLogsOutsideLines)
		{
			FVeyraClientSnapshot Snapshot;
			Snapshot.AccountId = TEXT("me");
			Snapshot.DisplayName = TEXT("DevOne");
			Snapshot.Social.Friends.Friends.Add({ TEXT("friend"), TEXT("DevTwo") });
			FVeyraChatEntry Party;
			Party.Kind = VeyraBackendProtocol::EChatKind::Party;
			Party.SenderId = TEXT("me");
			Party.SenderName = TEXT("DevOne");
			Party.Text = TEXT("ready");
			Party.ArrivedAt = 4.0;
			Snapshot.Chat.Party.Lines.Add(Party);
			FVeyraChatEntry Mine;
			Mine.Kind = VeyraBackendProtocol::EChatKind::Direct;
			Mine.SenderId = TEXT("me");
			Mine.With = TEXT("friend");
			Mine.Text = TEXT("hi");
			Mine.Failure = TEXT("rate_limited");
			Mine.ArrivedAt = 6.0;
			Snapshot.Chat.Direct.FindOrAdd(TEXT("friend")).Lines.Add(Mine);
			const TArray<FVeyraOutsideChat> Outside = VeyraChatLog::OutsideOf(Snapshot);
			ASSERT_THAT(AreEqual(2, Outside.Num()));
			ASSERT_THAT(IsTrue(Outside[0].Kind == EVeyraOutsideChatKind::Party && Outside[0].Name == TEXT("DevOne") && Outside[0].ReceivedAt == 4.0));
			ASSERT_THAT(IsTrue(Outside[1].Kind == EVeyraOutsideChatKind::DirectTo && Outside[1].Name == TEXT("DevTwo") && Outside[1].Status == TEXT("not sent")));

			// A conversation kept from before the friendship ended, or a block, is never shown in the match.
			FVeyraChatEntry Former;
			Former.Kind = VeyraBackendProtocol::EChatKind::Direct;
			Former.SenderId = TEXT("former");
			Former.SenderName = TEXT("DevThree");
			Former.With = TEXT("former");
			Former.Text = TEXT("still here?");
			Snapshot.Chat.Direct.FindOrAdd(TEXT("former")).Lines.Add(Former);
			ASSERT_THAT(AreEqual(2, VeyraChatLog::OutsideOf(Snapshot).Num(), TEXT("only friends' conversations")));
		}

		TEST_METHOD(TheLogSitsJustAboveTheComposerAtTheBottomLeft)
		{
			const UVeyraGreyboxSettings& Hud = *GetDefault<UVeyraGreyboxSettings>();
			const FVector2D Screen(1920.0, 1080.0);
			const FVeyraChatFrame Frame = VeyraChatLog::FrameFor(Screen, Hud, 1.0f);
			ASSERT_THAT(IsTrue(Frame.InputTopLeft.X > 0.0 && Frame.InputTopLeft.X < Screen.X / 4.0, TEXT("at the left")));
			ASSERT_THAT(IsTrue(Frame.InputTopLeft.Y + Frame.InputSize.Y <= Screen.Y && Frame.InputTopLeft.Y > Screen.Y / 2.0, TEXT("in the bottom half")));
			ASSERT_THAT(IsTrue(Frame.LogBottomLeft.Y <= Frame.InputTopLeft.Y && Frame.LogBottomLeft.X == Frame.InputTopLeft.X));
			const FVeyraChatFrame Larger = VeyraChatLog::FrameFor(Screen, Hud, 1.5f);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Larger.Width, Frame.Width * 1.5), TEXT("with the HUD's scale")));
		}
	};

	// Veyra.UI.ChatComposer.*: the line chat is typed into (ADR-029 §5).
	TEST_CLASS(ChatComposer, "Veyra.UI")
	{
		FActorTestSpawner Spawner;
		UVeyraChatComposer* Composer = nullptr;
		TArray<TPair<EVeyraChatChannel, FString>> Sent;
		int32 Closes = 0;

		BEFORE_EACH()
		{
			Composer = CreateWidget<UVeyraChatComposer>(&Spawner.GetWorld());
			Composer->Show(EVeyraChatChannel::Team, 250, [this](EVeyraChatChannel Channel, const FString& Typed) { Sent.Add({ Channel, Typed }); }, [this] { ++Closes; });
		}

		TEST_METHOD(EnterSendsWhatWasTypedThenCloses)
		{
			Composer->SetTyped(TEXT("wait for me"));
			Composer->Send();
			ASSERT_THAT(IsTrue(Sent.Num() == 1 && Sent[0].Key == EVeyraChatChannel::Team && Sent[0].Value == TEXT("wait for me") && Closes == 1));
		}

		TEST_METHOD(TabSwitchesBetweenTeamAndAll)
		{
			Composer->SwitchChannel();
			ASSERT_THAT(IsTrue(Composer->GetChannel() == EVeyraChatChannel::All));
			Composer->SetTyped(TEXT("gg"));
			Composer->Send();
			ASSERT_THAT(IsTrue(Sent.Num() == 1 && Sent[0].Key == EVeyraChatChannel::All));
			Composer->SwitchChannel();
			ASSERT_THAT(IsTrue(Composer->GetChannel() == EVeyraChatChannel::Team));
			ASSERT_THAT(IsFalse(UVeyraChatComposer::ChannelLabel(EVeyraChatChannel::All).EqualTo(UVeyraChatComposer::ChannelLabel(EVeyraChatChannel::Team))));
		}

		TEST_METHOD(EscapeAndAnEmptyLineCloseWithoutSending)
		{
			Composer->SetTyped(TEXT("half a thought"));
			Composer->Cancel();
			Composer->SetTyped(TEXT("   "));
			Composer->Send();
			ASSERT_THAT(IsTrue(Sent.IsEmpty() && Closes == 2));
		}
	};

	// Veyra.UI.ChatSettings.*: the chat key and the Communication settings (ADR-029 §4, §5; Chat Bible §2; SET-66, SET-67).
	TEST_CLASS(ChatSettings, "Veyra.UI")
	{
		/** Loaded before each test: CQTest builds its classes while registering them, which in a packaged client is before the engine starts. */
		FVeyraSettingsRegistry Registry;

		BEFORE_EACH()
		{
			UVeyraSettingsSubsystem::LoadRegistry(Registry);
		}

		static const UVeyraGreyboxSettings& Hud() { return *GetDefault<UVeyraGreyboxSettings>(); }

		TEST_METHOD(AllChatIsOnUntilThePlayerTurnsItOff)
		{
			const FVeyraContentId AllChat = FVeyraContentId::FromText(TEXT("communication_all_chat")).GetValue();
			ASSERT_THAT(IsTrue(Registry.Toggles.Contains(AllChat), TEXT("the id the player controller reports")));
			ASSERT_THAT(IsTrue(FVeyraSettingsStore(Registry).IsOn(AllChat), TEXT("Chat Bible §2: on by default")));
		}

		TEST_METHOD(TheChatKeyIsEnterAndItsOwn)
		{
			ASSERT_THAT(IsTrue(GetDefault<UVeyraUIInputSettings>()->ChatKey == EKeys::Enter, TEXT("Enter by default (ADR-029 §5)")));
			UVeyraUIInputSettings* Keys = NewObject<UVeyraUIInputSettings>();
			Keys->MatchMenuKey = EKeys::Escape;
			Keys->ShopKey = EKeys::P;
			Keys->ScoreboardKey = EKeys::Tab;
			Keys->ChatKey = EKeys::Enter;
			ASSERT_THAT(IsTrue(Keys->Validate().IsEmpty()));
			Keys->ChatKey = EKeys::Tab;
			ASSERT_THAT(IsTrue(FString::Join(Keys->Validate(), TEXT(" ")).Contains(TEXT("ChatKey"))));
			Keys->ChatKey = FKey();
			ASSERT_THAT(IsTrue(FString::Join(Keys->Validate(), TEXT(" ")).Contains(TEXT("ChatKey"))));
		}

		TEST_METHOD(TheDefaultsAreTheDevelopersChat)
		{
			const FVeyraSettingsStore Defaults(Registry);
			for (const FVeyraInterfacePreferences& Preferences : { VeyraInterfacePreferences::Resolve(Hud(), nullptr), VeyraInterfacePreferences::Resolve(Hud(), &Defaults) })
			{
				ASSERT_THAT(IsTrue(Preferences.ChatFontSize == Hud().ChatFontSize && Preferences.ChatBackdrop == Hud().ChatBackdropColor));
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Preferences.ChatFadeSeconds, Hud().ChatFadeSeconds) && !Preferences.bChatTimestamps));
			}
		}

		TEST_METHOD(ThePlayersChatSettingsTakeTheirPlace)
		{
			using namespace VeyraInterfacePreferences;
			FVeyraSettingsStore Store(Registry);
			Store.Set(ChatTextSize(), TEXT("ExtraLarge"));
			Store.Set(ChatBackdrop(), TEXT("HighContrast"));
			Store.Set(ChatFadeSeconds(), TEXT("20"));
			Store.Set(ChatTimestamps(), VeyraSettings::On());
			const FVeyraInterfacePreferences Preferences = Resolve(Hud(), &Store);
			ASSERT_THAT(IsTrue(Preferences.ChatFontSize == Hud().ChatExtraLargeFontSize && Preferences.ChatBackdrop == Hud().ChatHighContrastBackdropColor));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Preferences.ChatFadeSeconds, 20.0f) && Preferences.bChatTimestamps));
			Store.Set(ChatTextSize(), TEXT("Large"));
			Store.Set(ChatBackdrop(), TEXT("Transparent"));
			const FVeyraInterfacePreferences Clear = Resolve(Hud(), &Store);
			ASSERT_THAT(IsTrue(Clear.ChatFontSize == Hud().ChatLargeFontSize && Clear.ChatBackdrop.A == 0.0f, TEXT("Transparent draws no backdrop")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
