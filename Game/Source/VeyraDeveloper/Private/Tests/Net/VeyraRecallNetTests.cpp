// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Recall/VeyraRecallComponent.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraGameMode.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraNetTests
{
	// Veyra.Net.Recall.*: a Vanguard channels home to its fountain; a new order, hostile damage and a
	// Stun interrupt it (Economy & Progression Bible §10; ADR-012 §8).
	NETWORK_TEST_CLASS(Recall, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
			FVector Home = FVector::ZeroVector;
			double EndsAt = 0.0;
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedVanguardsTuning> TallBodies;
		/** Where the server placed the Vanguard at the start, for the client steps. */
		FVector Home = FVector::ZeroVector;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short channel, a spot well away from the fountain, a scratch and a Stun.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double ShortChannelSeconds = 0.5;
		static constexpr double AwayDistance = 1500.0;
		static constexpr double HomeTolerance = 100.0;
		static constexpr double ScratchDamage = 1.0;
		static constexpr double StunSeconds = 5.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.Recall.ChannelSeconds = ShortChannelSeconds;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			TallBodies.Reset();
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static AVeyraPlayerState& ServerParticipant(FState& State, int32 ClientIndex = 0)
		{
			return *ServerControllerOf(State, ClientIndex)->GetPlayerState<AVeyraPlayerState>();
		}

		static APawn& ServerBody(FState& State)
		{
			return *ServerParticipant(State).GetPawn();
		}

		static UVeyraRecallComponent& ServerRecall(FState& State)
		{
			return *ServerParticipant(State).FindComponentByClass<UVeyraRecallComponent>();
		}

		/** On the server: the first client's Recall order, as its key sends it. */
		static EVeyraOrderRejection OrderRecall(FState& State)
		{
			return GameModeOf(State.World)->HandleRecallOrder(*ServerControllerOf(State, 0));
		}

		/** Moves the Vanguard well away from its fountain, noting where home is. */
		static void SendAway(FState& State)
		{
			APawn& Body = ServerBody(State);
			State.Home = Body.GetActorLocation();
			Body.TeleportTo(State.Home + FVector(State.Home.X > 0.0 ? -AwayDistance : AwayDistance, 0.0, 0.0), Body.GetActorRotation());
		}

		TEST_METHOD(AFinishedChannelBringsTheVanguardHome)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Send the Vanguard away"), [this](FState& State) {
					SendAway(State);
					Home = State.Home;
				})
				.ThenClient(TEXT("Recall"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestRecall(); })
				.UntilClient(TEXT("The client sees the channel"), 0, [](FState& State) {
					return LocalControllerOf(State.World)->GetPlayerState<AVeyraPlayerState>()->FindComponentByClass<UVeyraRecallComponent>()->IsRecalling();
				})
				.UntilServer(TEXT("It arrives home"), [](FState& State) {
					return FVector::Dist2D(ServerBody(State).GetActorLocation(), State.Home) < HomeTolerance && !ServerRecall(State).IsRecalling();
				})
				.UntilClient(TEXT("Its client sees it home, the channel over"), 0, [this](FState& State) {
					const AVeyraPlayerController* Own = LocalControllerOf(State.World);
					const APawn* Body = Own->GetVanguard();
					return Body && FVector::Dist2D(Body->GetActorLocation(), Home) < HomeTolerance
						&& !Own->GetPlayerState<AVeyraPlayerState>()->FindComponentByClass<UVeyraRecallComponent>()->IsRecalling();
				});
		}

		TEST_METHOD(ACastInProgressStopsARecallBeginning)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("A cast holds the Vanguard, then ends"), [this](FState& State) {
					UVeyraCastStateComponent& CastState = *ServerParticipant(State).FindComponentByClass<UVeyraCastStateComponent>();
					FVeyraCastState Winding;
					Winding.Phase = EVeyraCastPhase::Windup;
					CastState.SetState(Winding);
					ASSERT_THAT(IsTrue(OrderRecall(State) == EVeyraOrderRejection::Casting));
					ASSERT_THAT(IsFalse(ServerRecall(State).IsRecalling()));
					CastState.Clear();
					ASSERT_THAT(IsTrue(OrderRecall(State) == EVeyraOrderRejection::None && ServerRecall(State).IsRecalling()));
				});
		}

		TEST_METHOD(AVanguardTallerThanItsStartStillArrivesHome)
		{
			// A body taller than the start's capsule stands into the floor there, as Cairn's does; it
			// spawned there anyway, and must arrive there too.
			constexpr double ExtraHalfHeight = 10.0;
			TallBodies = MakeUnique<FScopedVanguardsTuning>();
			const double StartHalfHeight = GetDefault<APlayerStart>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			TallBodies->Definition(TestVanguardId()).Body.CapsuleHalfHeight = StartHalfHeight + ExtraHalfHeight;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Send the Vanguard away, and recall"), [this](FState& State) {
					SendAway(State);
					ASSERT_THAT(IsTrue(OrderRecall(State) == EVeyraOrderRejection::None));
				})
				.UntilServer(TEXT("It arrives home"), [](FState& State) {
					return FVector::Dist2D(ServerBody(State).GetActorLocation(), State.Home) < HomeTolerance && !ServerRecall(State).IsRecalling();
				});
		}

		TEST_METHOD(AMoveOrderInterruptsTheChannel)
		{
			// Long enough that the move order arrives well before the channel would end.
			constexpr double LongerChannelSeconds = 2.0;
			constexpr double Margin = 0.3;
			Tuning->Tuning.Recall.ChannelSeconds = LongerChannelSeconds;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Send the Vanguard away"), [](FState& State) { SendAway(State); })
				.ThenClient(TEXT("Recall"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestRecall(); })
				.UntilServer(TEXT("The channel runs"), [](FState& State) {
					State.EndsAt = ServerRecall(State).GetChannel().EndsAt;
					return ServerRecall(State).IsRecalling();
				})
				.ThenClient(TEXT("Order a move"), 0, [](FState& State) {
					const APawn* Body = LocalControllerOf(State.World)->GetVanguard();
					LocalControllerOf(State.World)->IssueMoveOrder(Body->GetActorLocation() + FVector(100.0, 0.0, 0.0));
				})
				.UntilServer(TEXT("The channel's time passes"), [](FState& State) { return State.World->GetTimeSeconds() > State.EndsAt + Margin; })
				.ThenServer(TEXT("It was interrupted, and the Vanguard stayed away"), [this](FState& State) {
					ASSERT_THAT(IsFalse(ServerRecall(State).IsRecalling()));
					ASSERT_THAT(IsTrue(FVector::Dist2D(ServerBody(State).GetActorLocation(), State.Home) > AwayDistance / 2.0));
				});
		}

		TEST_METHOD(HostileDamageInterruptsTheChannel)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Recall, and take a scratch from the enemy"), [this](FState& State) {
					ASSERT_THAT(IsTrue(OrderRecall(State) == EVeyraOrderRejection::None && ServerRecall(State).IsRecalling()));
					FVeyraRawDamageEvent Scratch;
					Scratch.Components.Add({ EVeyraDamageType::TrueDamage, ScratchDamage });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*ServerParticipant(State, 1).GetAbilitySystemComponent(),
						*ServerParticipant(State).GetAbilitySystemComponent(), Scratch)));
					ASSERT_THAT(IsFalse(ServerRecall(State).IsRecalling(), TEXT("the damage should end the channel at once")));
				});
		}

		TEST_METHOD(AStunInterruptsTheChannelAndStopsANewOne)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Recall, then be stunned"), [this](FState& State) {
					ASSERT_THAT(IsTrue(OrderRecall(State) == EVeyraOrderRejection::None && ServerRecall(State).IsRecalling()));
					FVeyraStatusSpec Stun;
					Stun.Id = FVeyraContentId::FromText(TEXT("test_stun")).GetValue();
					Stun.Kind = EVeyraStatusKind::Stun;
					Stun.DurationSeconds = StunSeconds;
					UAbilitySystemComponent& Abilities = *ServerParticipant(State).GetAbilitySystemComponent();
					ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities, Abilities, Stun)));
					ASSERT_THAT(IsFalse(ServerRecall(State).IsRecalling(), TEXT("the Stun should end the channel at once")));
					ASSERT_THAT(IsTrue(OrderRecall(State) == EVeyraOrderRejection::CrowdControlled, TEXT("no Recall begins while stunned")));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
