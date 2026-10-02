// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Absence/VeyraAbsenceSubsystem.h"
#include "Echoes/VeyraEcho.h"
#include "Echoes/VeyraEchoSubsystem.h"
#include "Gold/VeyraGoldComponent.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraAbilityTypes.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraNetTests
{
	// Veyra.Net.SecondSelf.*: a player who projects The Second Self's Echo commands it until it ends: its Vanguard waits in
	// Stasis, its orders move the Echo, its camera follows the Echo, and all of it comes back (Item Bible §11; ADR-050 §6).
	NETWORK_TEST_CLASS(SecondSelf, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
			FVector VanguardAt = FVector::ZeroVector;
			FVector EchoAt = FVector::ZeroVector;
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedTestTickets> Tickets;
		TUniquePtr<FScopedMatchAssignment> Assignment;
		FVeyraGreyboxLayout Layout;
		TWeakObjectPtr<AVeyraPlayerState> Holder;
		TWeakObjectPtr<AVeyraEcho> Projected;

		// Fixture values: a short preparation, a purse for the Mythical, and how far the Echo forms and is sent.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double Purse = 10000.0;
		static constexpr double Out = 300.0;
		static constexpr double Walk = 250.0;
		static constexpr double Moved = 50.0;
		// The client in this PIE instance projects the Echo: the first client.
		static constexpr int32 HolderInstance = 1;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			// Rostered, so a player who leaves keeps its PlayerState for its return (ADR-019 §1).
			Tickets = MakeUnique<FScopedTestTickets>();
			Assignment = MakeUnique<FScopedMatchAssignment>(TArray<EVeyraTeam>{ EVeyraTeam::A, EVeyraTeam::B });
			ASSERT_THAT(IsTrue(Assignment->Problems.IsEmpty(), FString::Join(Assignment->Problems, TEXT(" | "))));
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Assignment.Reset();
			Tickets.Reset();
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		/** The committed catalog's Mythical (Item Bible §11). */
		static FVeyraContentId SecondSelfItem()
		{
			return FVeyraContentId::FromText(TEXT("the_second_self")).GetValue();
		}

		static AVeyraPlayerState& ServerParticipant(FState& State)
		{
			return *ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>();
		}

		static AVeyraEcho* ServerEcho(FState& State)
		{
			return State.World->GetSubsystem<UVeyraEchoSubsystem>()->FindStanding(*ServerParticipant(State).GetAbilitySystemComponent());
		}

		static bool InStasis(const AVeyraPlayerState& Participant)
		{
			const UVeyraStatusComponent* Statuses = Participant.FindComponentByClass<UVeyraStatusComponent>();
			return Statuses && Statuses->Has(EVeyraStatusKind::Stasis);
		}

		/** Buys The Second Self for the first client's participant, and projects its Echo until it takes control. */
		FPIENetworkComponent<FState>& ProjectAndCommand()
		{
			return StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Give the player The Second Self"), [this](FState& State) {
					AVeyraPlayerState& Participant = ServerParticipant(State);
					ASSERT_THAT(IsTrue(Participant.FindComponentByClass<UVeyraGoldComponent>()->Grant(Purse, EVeyraGoldReason::Developer)));
					UVeyraShopSubsystem* Shop = State.World->GetSubsystem<UVeyraShopSubsystem>();
					Shop->SetAtFountain(Participant, true);
					ASSERT_THAT(IsTrue(Shop->Buy(Participant, SecondSelfItem()) == EVeyraShopRefusal::None));
					State.VanguardAt = Participant.GetPawn()->GetActorLocation();
					Holder = &Participant;
				})
				.ThenClient(TEXT("Project the Echo beside its Vanguard"), 0, [](FState& State) {
					AVeyraPlayerController* Player = LocalControllerOf(State.World);
					FVeyraCastTarget Target;
					Target.bHasLocation = true;
					Target.Location = Player->GetVanguard()->GetActorLocation() + FVector(Out, 0.0, 0.0);
					Player->IssueCastOrder(VeyraAbilitySlots::Items[0], Target);
				})
				.UntilServer(TEXT("Its Vanguard waits in Stasis and its Echo takes control"), [this](FState& State) {
					AVeyraPlayerState& Participant = ServerParticipant(State);
					AVeyraEcho* Echo = ServerEcho(State);
					Projected = Echo;
					return InStasis(Participant) && Echo && ServerControllerOf(State, 0)->GetCommandedBody() == Echo
						&& Echo->GetController() != nullptr;
				});
		}

		TEST_METHOD(ItsOrdersMoveTheEchoAndItsCameraFollowsUntilItEnds)
		{
			ProjectAndCommand()
				.UntilClient(TEXT("Its camera follows the Echo"), 0, [](FState& State) {
					const APawn* Body = LocalControllerOf(State.World)->GetCommandedBody();
					return Body && Body->IsA<AVeyraEcho>();
				})
				.ThenServer(TEXT("Note where the Echo stands"), [](FState& State) { State.EchoAt = ServerEcho(State)->GetActorLocation(); })
				.ThenClient(TEXT("Order a move"), 0, [](FState& State) {
					AVeyraPlayerController* Player = LocalControllerOf(State.World);
					Player->IssueMoveOrder(Player->GetCommandedBody()->GetActorLocation() + FVector(0.0, Walk, 0.0));
				})
				.UntilServer(TEXT("The Echo walks, and the Vanguard stays where it stood"), [](FState& State) {
					const AVeyraEcho* Echo = ServerEcho(State);
					const APawn* Vanguard = ServerParticipant(State).GetPawn();
					return Echo && FVector::Dist2D(Echo->GetActorLocation(), State.EchoAt) > Moved
						&& FVector::Dist2D(Vanguard->GetActorLocation(), State.VanguardAt) < Moved;
				})
				.ThenServer(TEXT("End the Echo"), [](FState& State) {
					State.World->GetSubsystem<UVeyraEchoSubsystem>()->End(*ServerParticipant(State).GetAbilitySystemComponent(), EVeyraEchoEnd::Faded);
				})
				.UntilServer(TEXT("Its Vanguard wakes, and its orders move it again"), [](FState& State) {
					return !InStasis(ServerParticipant(State)) && !ServerControllerOf(State, 0)->IsCommandingEcho();
				})
				.UntilClient(TEXT("Its camera follows its Vanguard again"), 0, [](FState& State) {
					AVeyraPlayerController* Player = LocalControllerOf(State.World);
					return Player->GetCommandedBody() && Player->GetCommandedBody() == Player->GetVanguard();
				});
		}

		TEST_METHOD(ItsPlayerLeavingEndsItSoTheAutopilotTakesTheVanguard)
		{
			ProjectAndCommand()
				.ThenServer(TEXT("Its player leaves"), [](FState& /*State*/) {
					const FWorldContext* Context = GEngine->GetWorldContextFromPIEInstance(HolderInstance);
					GEngine->Exec(Context ? Context->World() : nullptr, TEXT("disconnect"));
				})
				.UntilServer(TEXT("The server counts it disconnected"), [this](FState& State) {
					const FVeyraAbsenceRecord* Record = Holder.IsValid() ? State.World->GetSubsystem<UVeyraAbsenceSubsystem>()->Find(*Holder) : nullptr;
					return Record && Record->Absence == EVeyraAbsence::Disconnected;
				})
				// At once, not when its Integrity fades: the Echo would otherwise go on with its last order while the Vanguard
				// waits in Stasis, out of the autopilot's reach (ADR-050 §6; Match Flow Bible §4).
				.ThenServer(TEXT("Its Echo has ended and its Vanguard is awake"), [this](FState& /*State*/) {
					ASSERT_THAT(IsTrue(!Projected.IsValid() || Projected->IsWithdrawn(), TEXT("the Echo ended with its player")));
					ASSERT_THAT(IsTrue(Holder.IsValid() && !InStasis(*Holder), TEXT("its Vanguard woke for the autopilot")));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
