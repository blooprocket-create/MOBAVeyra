// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Chat/VeyraChatSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.MatchChat.*: Team Chat reaches the sender's side, All Chat both, and mutes and All Chat
	// off are kept at delivery (Chat & Communication Bible §2; ADR-029). Two players on one side and one
	// on the other.
	NETWORK_TEST_CLASS(MatchChat, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedTestTickets> Tickets;
		TUniquePtr<FScopedMatchAssignment> Assignment;
		FVeyraGreyboxLayout Layout;
		double SentAt = 0.0;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		// Time enough for a message to have reached every client it may.
		static constexpr double DeliveryGraceSeconds = 1.0;
		// The first two clients are teammates; the third plays against them.
		static constexpr int32 ClientCount = 3;
		static constexpr int32 SenderIndex = 0;
		static constexpr int32 TeammateIndex = 1;
		static constexpr int32 OpponentIndex = 2;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tickets = MakeUnique<FScopedTestTickets>();
			Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::A, EVeyraTeam::B });
			ASSERT_THAT(IsTrue(Assignment->Problems.IsEmpty(), FString::Join(Assignment->Problems, TEXT(" | "))));
			BuildMatchNetwork(Network, ClientCount);
		}

		AFTER_EACH()
		{
			Assignment.Reset();
			Tickets.Reset();
			Tuning.Reset();
		}

		static bool OnSide(FState& State, EVeyraTeam Side)
		{
			const AVeyraPlayerController* Local = LocalControllerOf(State.World);
			const AVeyraPlayerState* Own = Local ? Local->GetPlayerState<AVeyraPlayerState>() : nullptr;
			return Own && Own->GetVeyraTeam() == Side;
		}

		static int32 PlayerIdOf(FState& State, int32 ClientIndex)
		{
			const AVeyraPlayerController* Controller = ServerControllerOf(State, ClientIndex);
			return Controller && Controller->PlayerState ? Controller->PlayerState->GetPlayerId() : INDEX_NONE;
		}

		/** Waits DeliveryGraceSeconds on the server, from now. */
		void Settle(FState& /*State*/)
		{
			SentAt = FPlatformTime::Seconds();
		}

		TEST_METHOD(TeamChatReachesItsSideAndAllChatBoth)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("A player says hello to its side"), SenderIndex, [](FState& State) {
					LocalControllerOf(State.World)->RequestChat(EVeyraChatChannel::Team, TEXT("  hello\n "));
				})
				.UntilClients(TEXT("Its side receives it, cleaned, sender and all"), [](FState& State) {
					if (!OnSide(State, EVeyraTeam::A))
					{
						return true;
					}
					const TArray<FVeyraReceivedChat>& Chat = LocalControllerOf(State.World)->GetChat();
					return Chat.Num() == 1 && Chat[0].Message.Channel == EVeyraChatChannel::Team && Chat[0].Message.Text == TEXT("hello");
				})
				.ThenServer(TEXT("Note the time"), [this](FState& State) { Settle(State); })
				.UntilServer(TEXT("Give it time to reach everyone it may"), [this](FState& /*State*/) { return FPlatformTime::Seconds() - SentAt >= DeliveryGraceSeconds; })
				.ThenClient(TEXT("Nothing reached the other side"), OpponentIndex, [this](FState& State) {
					ASSERT_THAT(IsTrue(LocalControllerOf(State.World)->GetChat().IsEmpty()));
				})
				.ThenClient(TEXT("The opponent says gg to all"), OpponentIndex, [](FState& State) {
					LocalControllerOf(State.World)->RequestChat(EVeyraChatChannel::All, TEXT("gg"));
				})
				.UntilClients(TEXT("Everyone receives it"), [](FState& State) {
					const TArray<FVeyraReceivedChat>& Chat = LocalControllerOf(State.World)->GetChat();
					return !Chat.IsEmpty() && Chat.Last().Message.Channel == EVeyraChatChannel::All && Chat.Last().Message.Text == TEXT("gg")
						&& Chat.Last().Message.SenderTeam == EVeyraTeam::B;
				});
		}

		TEST_METHOD(MutesAndAllChatOffAreKeptAtDelivery)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenClient(TEXT("The opponent turns All Chat off"), OpponentIndex, [](FState& State) {
					LocalControllerOf(State.World)->RequestAllChat(false);
				})
				.ThenServer(TEXT("The teammate mutes the sender"), [this](FState& State) {
					// Through the teammate's controller on the server, as its client's request arrives.
					ServerControllerOf(State, TeammateIndex)->RequestMute(PlayerIdOf(State, SenderIndex), true);
				})
				.UntilServer(TEXT("The server keeps both"), [this](FState& State) {
					const UVeyraChatSubsystem* ChatOwner = State.World->GetSubsystem<UVeyraChatSubsystem>();
					return ChatOwner && ChatOwner->IsMuted(PlayerIdOf(State, TeammateIndex), PlayerIdOf(State, SenderIndex))
						&& !ChatOwner->IsAllChatOn(PlayerIdOf(State, OpponentIndex));
				})
				.ThenClient(TEXT("The sender says hi to all"), SenderIndex, [](FState& State) {
					LocalControllerOf(State.World)->RequestChat(EVeyraChatChannel::All, TEXT("hi all"));
				})
				.UntilClient(TEXT("The sender sees its own message"), SenderIndex, [](FState& State) {
					return LocalControllerOf(State.World)->GetChat().Num() == 1;
				})
				.ThenServer(TEXT("Note the time"), [this](FState& State) { Settle(State); })
				.UntilServer(TEXT("Give it time to reach everyone it may"), [this](FState& /*State*/) { return FPlatformTime::Seconds() - SentAt >= DeliveryGraceSeconds; })
				.ThenClient(TEXT("The teammate who muted it read nothing"), TeammateIndex, [this](FState& State) {
					ASSERT_THAT(IsTrue(LocalControllerOf(State.World)->GetChat().IsEmpty()));
				})
				.ThenClient(TEXT("Nor did the opponent with All Chat off; and it cannot send to All"), OpponentIndex, [this](FState& State) {
					ASSERT_THAT(IsTrue(LocalControllerOf(State.World)->GetChat().IsEmpty()));
					LocalControllerOf(State.World)->RequestChat(EVeyraChatChannel::All, TEXT("hello?"));
				})
				.UntilClient(TEXT("Refused, and told so in its chat"), OpponentIndex, [](FState& State) {
					const AVeyraPlayerController* Local = LocalControllerOf(State.World);
					return Local->GetLastChatRefusal() == EVeyraChatRefusal::AllChatOff && !Local->GetChat().IsEmpty()
						&& Local->GetChat().Last().Notice == EVeyraChatNotice::Refused && Local->GetChat().Last().Refusal == EVeyraChatRefusal::AllChatOff;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
