// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	// Veyra.Net.ReplicatedStatuses.*: statuses run on the server and every client sees them; crowd control
	// holds a Vanguard's orders until it ends (Combat Bible §8, §9; ADR-009 §1, §2).
	NETWORK_TEST_CLASS(ReplicatedStatuses, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation, a Slow and a Stun, and a comparison allowance for
		// float positions.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double SlowMagnitude = 0.4;
		static constexpr double SlowSeconds = 1.5;
		static constexpr double StunSeconds = 2.0;
		static constexpr double PositionSlack = 1.0;

		int32 TargetId = INDEX_NONE;
		FVector HeldAt = FVector::ZeroVector;
		double StunnedAt = 0.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			// Loading waits for both clients rather than the loading timeout.
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static AVeyraPlayerState* TargetState(FState& State)
		{
			const AVeyraPlayerController* Target = ServerControllerOf(State, 0);
			return Target ? Target->GetPlayerState<AVeyraPlayerState>() : nullptr;
		}

		/** The target's statuses as this machine sees them. */
		const FVeyraStatusLedger* LedgerSeenBy(FState& State) const
		{
			const AVeyraGameState* GameState = GameStateOf(State.World);
			const TObjectPtr<APlayerState>* Participant = GameState
				? GameState->PlayerArray.FindByPredicate([this](const APlayerState* Candidate) { return Candidate && Candidate->GetPlayerId() == TargetId; })
				: nullptr;
			const UVeyraStatusComponent* Statuses = Participant ? (*Participant)->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
			return Statuses ? &Statuses->GetLedger() : nullptr;
		}

		bool SeesStatus(FState& State, EVeyraStatusKind Kind) const
		{
			const FVeyraStatusLedger* Ledger = LedgerSeenBy(State);
			return Ledger && Ledger->Entries.ContainsByPredicate([Kind](const FVeyraStatusEntry& Entry) { return Entry.Kind == Kind; });
		}

		bool SeesNoStatus(FState& State) const
		{
			const FVeyraStatusLedger* Ledger = LedgerSeenBy(State);
			return Ledger && Ledger->Entries.IsEmpty();
		}

		bool IsNear2D(const AActor* Actor, const FVector& Point, double Allowance) const
		{
			return Actor && FVector::Dist2D(Actor->GetActorLocation(), Point) <= Allowance;
		}

		static FVeyraStatusSpec Status(const TCHAR* Id, EVeyraStatusKind Kind, double Magnitude, double DurationSeconds)
		{
			FVeyraStatusSpec Spec;
			Spec.Id = FVeyraContentId::FromText(Id).GetValue();
			Spec.Kind = Kind;
			Spec.Magnitude = Magnitude;
			Spec.DurationSeconds = DurationSeconds;
			return Spec;
		}

		/** On the server: stuns the target and notes where and when. */
		void StunTarget(FState& State)
		{
			AVeyraPlayerState* Target = TargetState(State);
			ASSERT_THAT(IsNotNull(Target));
			UAbilitySystemComponent& AbilitySystem = *Target->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(AbilitySystem, AbilitySystem, Status(TEXT("test_stun"), EVeyraStatusKind::Stun, 0.0, StunSeconds))));
			HeldAt = Target->GetPawn()->GetActorLocation();
			StunnedAt = State.World->GetTimeSeconds();
		}

		TEST_METHOD(EveryClientSeesASlowUntilItEnds)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Slow the first client's Vanguard"), [this](FState& State) {
					AVeyraPlayerState* Target = TargetState(State);
					ASSERT_THAT(IsNotNull(Target));
					TargetId = Target->GetPlayerId();
					UAbilitySystemComponent& AbilitySystem = *Target->GetAbilitySystemComponent();
					ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(AbilitySystem, AbilitySystem, Status(TEXT("test_slow"), EVeyraStatusKind::Slow, SlowMagnitude, SlowSeconds))));
				})
				.UntilClients(TEXT("Every client sees the Slow"), [this](FState& State) { return SeesStatus(State, EVeyraStatusKind::Slow); })
				.ThenClients(TEXT("With its strength"), [this](FState& State) {
					ASSERT_THAT(IsTrue(LedgerSeenBy(State)->Entries[0].Magnitude == SlowMagnitude));
				})
				.UntilServer(TEXT("The Slow ends"), [this](FState& State) { return SeesNoStatus(State); })
				.UntilClients(TEXT("Every client sees it end"), [this](FState& State) { return SeesNoStatus(State); });
		}

		TEST_METHOD(AStunHoldsAMoveOrderUntilItEnds)
		{
			const FVector Destination = FVector::ZeroVector;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Stun the first client's Vanguard"), [this](FState& State) {
					TargetId = TargetState(State)->GetPlayerId();
					StunTarget(State);
				})
				.UntilClient(TEXT("Its client sees the Stun"), 0, [this](FState& State) { return SeesStatus(State, EVeyraStatusKind::Stun); })
				.ThenClient(TEXT("Order a move and a cast"), 0, [Destination](FState& State) {
					LocalControllerOf(State.World)->IssueMoveOrder(Destination);
					LocalControllerOf(State.World)->IssueCastOrder(EVeyraAbilitySlot::Q, nullptr);
				})
				.UntilClient(TEXT("The cast is refused"), 0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastCastRejection() == EVeyraCastRejection::CrowdControlled;
				})
				.UntilServer(TEXT("The server holds the move"), [](FState& State) {
					return TargetState(State)->GetVanguardController()->GetMoveOrder().IsSet();
				})
				.ThenServer(TEXT("The Vanguard has not moved while stunned"), [this](FState& State) {
					ASSERT_THAT(IsTrue(State.World->GetTimeSeconds() < StunnedAt + StunSeconds, TEXT("the stun ended before the order was checked")));
					ASSERT_THAT(IsTrue(IsNear2D(TargetState(State)->GetPawn(), HeldAt, PositionSlack)));
				})
				.UntilServer(TEXT("After the Stun it carries out the order"), [this, Destination](FState& State) {
					return IsNear2D(TargetState(State)->GetPawn(), Destination, Tuning->Tuning.Orders.ArrivalTolerance + PositionSlack);
				})
				.ThenServer(TEXT("Not before the Stun ended"), [this](FState& State) {
					ASSERT_THAT(IsTrue(State.World->GetTimeSeconds() >= StunnedAt + StunSeconds));
				})
				.ThenClient(0, [this](FState& State) { ASSERT_THAT(AreEqual(LocalControllerOf(State.World)->GetOrderRejectionCount(), 0)); });
		}

		TEST_METHOD(AStunStopsAMovingVanguardWhichThenCarriesOn)
		{
			const FVector Destination = FVector::ZeroVector;
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Send the first client's Vanguard to the centre"), [this, Destination](FState& State) {
					AVeyraPlayerState* Target = TargetState(State);
					ASSERT_THAT(IsNotNull(Target));
					TargetId = Target->GetPlayerId();
					ASSERT_THAT(IsTrue(Target->GetVanguardController()->MoveToDestination(Destination) == EVeyraOrderRejection::None));
				})
				.UntilServer(TEXT("It is moving"), [](FState& State) { return !TargetState(State)->GetPawn()->GetVelocity().IsNearlyZero(); })
				.ThenServer(TEXT("Stun it"), [this](FState& State) {
					StunTarget(State);
					ASSERT_THAT(IsTrue(TargetState(State)->GetPawn()->GetVelocity().IsNearlyZero(), TEXT("a stunned Vanguard stops at once")));
					ASSERT_THAT(IsTrue(TargetState(State)->GetVanguardController()->GetMoveOrder().IsSet(), TEXT("the stun dropped the order")));
				})
				.UntilServer(TEXT("Half the Stun passes"), [this](FState& State) { return State.World->GetTimeSeconds() >= StunnedAt + StunSeconds / 2.0; })
				.ThenServer(TEXT("It stayed where it was stunned"), [this](FState& State) {
					ASSERT_THAT(IsTrue(IsNear2D(TargetState(State)->GetPawn(), HeldAt, PositionSlack)));
				})
				.UntilServer(TEXT("After the Stun it arrives"), [this, Destination](FState& State) {
					return IsNear2D(TargetState(State)->GetPawn(), Destination, Tuning->Tuning.Orders.ArrivalTolerance + PositionSlack);
				})
				.UntilClients(TEXT("Every client sees it arrive"), [this, Destination](FState& State) {
					return IsNear2D(FindVanguard(State.World, TargetId), Destination, Tuning->Tuning.Orders.ArrivalTolerance + PositionSlack);
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
