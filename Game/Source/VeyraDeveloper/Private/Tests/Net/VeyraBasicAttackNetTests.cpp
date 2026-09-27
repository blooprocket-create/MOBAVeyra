// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "GameFramework/PlayerState.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	// Veyra.Net.AttackOrders.*: attack and attack-move orders chase into range and attack on the
	// server's clock; every client sees the attack and its damage (Combat Bible §4, §48; ADR-009 §5).
	// Named for the orders, since test class names must be unique and Veyra.Abilities.BasicAttack exists.
	NETWORK_TEST_CLASS(AttackOrders, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation, a melee attack, and how far apart the Vanguards start
		// the test, so the chase is short.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double Range = 150.0;
		static constexpr double WindupFraction = 0.25;
		static constexpr double AcquisitionRadius = 800.0;
		static constexpr double Apart = 700.0;

		int32 AttackerId = INDEX_NONE;
		int32 TargetId = INDEX_NONE;

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static AVeyraPlayerState* ParticipantOf(FState& State, int32 ClientIndex)
		{
			const AVeyraPlayerController* Controller = ServerControllerOf(State, ClientIndex);
			return Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
		}

		static double BaseDamage()
		{
			return VeyraCombatTests::ExampleStats().PhysicalPower;
		}

		/**
		 * On the server: gives both Vanguards the example stats and a melee attack, and brings the
		 * second within a short walk of the first, toward the lane's centre.
		 */
		void PrepareDuel(FState& State)
		{
			AVeyraPlayerState* Attacker = ParticipantOf(State, 0);
			AVeyraPlayerState* Target = ParticipantOf(State, 1);
			ASSERT_THAT(IsTrue(Attacker && Target && Attacker->GetPawn() && Target->GetPawn()));
			FVeyraBasicAttackProfile Melee;
			Melee.Range = Range;
			// True damage, so every machine can compare exact amounts.
			Melee.DamageType = EVeyraDamageType::TrueDamage;
			Melee.PhysicalPowerRatio = 1.0;
			Melee.WindupFraction = WindupFraction;
			Melee.AcquisitionRadius = AcquisitionRadius;
			for (AVeyraPlayerState* Participant : { Attacker, Target })
			{
				ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Participant->GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats())));
				ASSERT_THAT(IsTrue(Participant->FindComponentByClass<UVeyraBasicAttackComponent>()->SetProfile(Melee)));
			}
			const FVector From = Attacker->GetPawn()->GetActorLocation();
			const FVector TowardCentre = FVector(-From.X, 0.0, 0.0).GetSafeNormal();
			Target->GetPawn()->SetActorLocation(From + TowardCentre * Apart);
			AttackerId = Attacker->GetPlayerId();
			TargetId = Target->GetPlayerId();
		}

		/** The participant with PlayerId as this machine sees it. */
		static const APlayerState* SeenParticipant(FState& State, int32 PlayerId)
		{
			for (const APlayerState* Participant : GameStateOf(State.World)->PlayerArray)
			{
				if (Participant && Participant->GetPlayerId() == PlayerId)
				{
					return Participant;
				}
			}
			return nullptr;
		}

		static double HealthLost(const APlayerState* Participant)
		{
			const UAbilitySystemComponent& Unit = *CastChecked<AVeyraPlayerState>(Participant)->GetAbilitySystemComponent();
			return Unit.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - Unit.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		TEST_METHOD(AnAttackOrderChasesIntoRangeAndHitsForEveryone)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State); })
				.UntilClient(TEXT("The attacker's client sees the target close by"), 0, [this](FState& State) {
					const AVeyraVanguardCharacter* Attacker = FindVanguard(State.World, AttackerId);
					const AVeyraVanguardCharacter* Target = FindVanguard(State.World, TargetId);
					return Attacker && Target && FVector::Dist2D(Attacker->GetActorLocation(), Target->GetActorLocation()) <= Apart + 1.0;
				})
				.ThenClient(TEXT("Order the attack"), 0, [this](FState& State) { LocalControllerOf(State.World)->IssueAttackOrder(FindVanguard(State.World, TargetId)); })
				.UntilServer(TEXT("The attack lands"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; })
				.ThenServer(TEXT("In range, with one attack's damage"), [this](FState& State) {
					const APawn* Attacker = ParticipantOf(State, 0)->GetPawn();
					const APawn* Target = ParticipantOf(State, 1)->GetPawn();
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthLost(ParticipantOf(State, 1)), BaseDamage(), 1e-3)));
					ASSERT_THAT(IsTrue(VeyraTargeting::EdgeToEdgeDistance(*Attacker, *Target) <= Range + UVeyraCombatTuningSubsystem::Get().Targeting.ServerRangeTolerance));
					ASSERT_THAT(IsTrue(ParticipantOf(State, 0)->GetVanguardController()->GetAttackTarget() == Target, TEXT("the order keeps attacking")));
				})
				.UntilClients(TEXT("Every client sees the damage and the attack"), [this](FState& State) {
					const APlayerState* Attacker = SeenParticipant(State, AttackerId);
					const APlayerState* Target = SeenParticipant(State, TargetId);
					return Attacker && Target && HealthLost(Target) >= BaseDamage()
						&& Attacker->FindComponentByClass<UVeyraBasicAttackComponent>()->GetState().Target != nullptr;
				})
				.UntilServer(TEXT("It attacks again after its interval"), [](FState& State) {
					return HealthLost(ParticipantOf(State, 1)) >= 2.0 * BaseDamage();
				})
				.ThenClient(TEXT("A move order ends the attack order"), 0, [](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(FVector::ZeroVector); })
				.UntilServer(TEXT("The server drops the attack order"), [](FState& State) {
					return ParticipantOf(State, 0)->GetVanguardController()->GetAttackTarget() == nullptr;
				});
		}

		TEST_METHOD(AttackMoveTakesOnAnEnemyItMeets)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State); })
				.ThenClient(TEXT("Attack-move to the lane's centre"), 0, [](FState& State) {
					LocalControllerOf(State.World)->IssueAttackMoveOrder(FVector::ZeroVector);
				})
				.UntilServer(TEXT("It takes on the enemy and hits it"), [](FState& State) {
					const AVeyraVanguardController* Controller = ParticipantOf(State, 0)->GetVanguardController();
					return Controller->GetAttackTarget() == ParticipantOf(State, 1)->GetPawn() && HealthLost(ParticipantOf(State, 1)) > 0.0;
				})
				.ThenServer(TEXT("Still an attack-move"), [this](FState& State) {
					ASSERT_THAT(IsTrue(ParticipantOf(State, 0)->GetVanguardController()->GetAttackMoveDestination().IsSet()));
				})
				.UntilClients(TEXT("Every client sees the damage"), [this](FState& State) {
					const APlayerState* Target = SeenParticipant(State, TargetId);
					return Target && HealthLost(Target) >= BaseDamage();
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
