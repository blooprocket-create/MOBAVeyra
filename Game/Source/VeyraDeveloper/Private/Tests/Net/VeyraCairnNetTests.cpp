// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "GameFramework/PlayerState.h"
#include "Movement/VeyraMovementComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.Vanguards.Cairn.*: Cairn's kit in a match (Character Bible §18): every ability cast
	// through the player's orders, Deep Foundation's lockout, and his ultimate paying its own shield.
	// Both players are Cairn at level 6 with a rank in each ability; amounts come from the tuning.
	NETWORK_TEST_CLASS(Cairn, "Veyra.Net.Vanguards")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a short preparation, the level that opens the first ultimate rank, how far
		// apart the Vanguards stand for each ability, and allowances for float positions.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr int32 UltimateLevel = 6;
		static constexpr double HookDistance = 700.0;
		static constexpr double SlamDistance = 170.0;
		static constexpr double BurdenDistance = 200.0;
		static constexpr double PositionSlack = 5.0;
		static constexpr double Tolerance = 1e-2;

		int32 CasterId = INDEX_NONE;
		int32 TargetId = INDEX_NONE;
		FVector TargetPoint = FVector::ZeroVector;

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.DeveloperMatch.Vanguards = { Id(TEXT("cairn")) };
			Tuning->Tuning.DeveloperMatch.StartingRank = EVeyraDeveloperStartingRank::None;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		static AVeyraPlayerState* ParticipantOf(FState& State, int32 ClientIndex)
		{
			const AVeyraPlayerController* Controller = ServerControllerOf(State, ClientIndex);
			return Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
		}

		/** On the server: both Cairns reach the ultimate's level and rank every ability; the second stands Distance away toward the centre. */
		void PrepareDuel(FState& State, double Distance)
		{
			AVeyraPlayerState* Caster = ParticipantOf(State, 0);
			AVeyraPlayerState* Target = ParticipantOf(State, 1);
			ASSERT_THAT(IsTrue(Caster && Target && Caster->GetPawn() && Target->GetPawn()));
			ASSERT_THAT(IsTrue(Caster->GetVanguardId() == Id(TEXT("cairn")) && Target->GetVanguardId() == Id(TEXT("cairn"))));
			const TArray<int32>& ToNextLevel = UVeyraProgressionTuningSubsystem::Get().Experience.ToNextLevel;
			int32 Experience = 0;
			for (int32 Level = 1; Level < UltimateLevel; ++Level)
			{
				Experience += ToNextLevel[Level - 1];
			}
			for (AVeyraPlayerState* Participant : { Caster, Target })
			{
				UVeyraProgressionComponent* Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
				Progression->AddExperience(Experience);
				for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
				{
					ASSERT_THAT(IsTrue(Progression->AllocateRank(Slot) == EVeyraRankRefusal::None));
				}
			}
			const FVector From = Caster->GetPawn()->GetActorLocation();
			TargetPoint = From + FVector(-From.X, 0.0, 0.0).GetSafeNormal() * Distance;
			Target->GetPawn()->SetActorLocation(TargetPoint);
			CasterId = Caster->GetPlayerId();
			TargetId = Target->GetPlayerId();
		}

		void CastAtTheTarget(FState& State, EVeyraAbilitySlot Slot) const
		{
			FVeyraCastTarget Target;
			Target.Actor = FindVanguard(State.World, TargetId);
			Target.bHasLocation = true;
			Target.Location = TargetPoint;
			LocalControllerOf(State.World)->IssueCastOrder(Slot, Target);
		}

		static double HealthLost(const AVeyraPlayerState* Participant)
		{
			const UAbilitySystemComponent& Unit = *Participant->GetAbilitySystemComponent();
			return Unit.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - Unit.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		static const TArray<FVeyraShieldEntry>& ShieldsOf(const AVeyraPlayerState* Participant)
		{
			return Participant->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger().Shields;
		}

		/** The shield Deep Foundation grants the caster now. */
		static double FoundationGrant(FState& State)
		{
			const FVeyraDeepFoundationTuning& Foundation = *UVeyraVanguardsTuningSubsystem::FindDeepFoundation(Id(TEXT("cairn_deep_foundation")));
			return VeyraEffectDelivery::ShieldGrant(*ParticipantOf(State, 0)->GetAbilitySystemComponent(), Foundation.Shield, 1).Amount;
		}

		/** Whether this machine sees the participant with PlayerId carrying a status of Kind. */
		static bool SeesStatus(FState& State, int32 PlayerId, EVeyraStatusKind Kind)
		{
			for (const APlayerState* Participant : GameStateOf(State.World)->PlayerArray)
			{
				if (Participant && Participant->GetPlayerId() == PlayerId)
				{
					return Participant->FindComponentByClass<UVeyraStatusComponent>()->GetLedger().Entries.ContainsByPredicate(
						[Kind](const FVeyraStatusEntry& Entry) { return Entry.Kind == Kind; });
				}
			}
			return false;
		}

		TEST_METHOD(IronGraspPullsTheEnemyAndGrantsDeepFoundation)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, HookDistance); })
				.ThenClient(TEXT("Hook the enemy"), 0, [this](FState& State) { CastAtTheTarget(State, EVeyraAbilitySlot::Q); })
				.UntilServer(TEXT("The hook lands"), [](FState& State) { return HealthLost(ParticipantOf(State, 1)) > 0.0; })
				.ThenServer(TEXT("Cairn gains his Deep Foundation shield"), [this](FState& State) {
					const TArray<FVeyraShieldEntry>& Shields = ShieldsOf(ParticipantOf(State, 0));
					ASSERT_THAT(AreEqual(1, Shields.Num()));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Shields[0].Remaining, FoundationGrant(State), Tolerance)));
				})
				.UntilServer(TEXT("The pull ends"), [](FState& State) {
					return !ParticipantOf(State, 1)->GetPawn()->FindComponentByClass<UVeyraMovementComponent>()->IsDisplaced();
				})
				.UntilClients(TEXT("Every client sees the enemy pulled to Cairn"), [this](FState& State) {
					const AVeyraVanguardCharacter* Caster = FindVanguard(State.World, CasterId);
					const AVeyraVanguardCharacter* Target = FindVanguard(State.World, TargetId);
					return Caster && Target && VeyraTargeting::EdgeToEdgeDistance(*Caster, *Target) <= PositionSlack;
				});
		}

		TEST_METHOD(CrushingHoldStunsTheEnemyInFront)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, SlamDistance); })
				.ThenClient(TEXT("Slam toward the enemy"), 0, [this](FState& State) { CastAtTheTarget(State, EVeyraAbilitySlot::W); })
				.UntilClients(TEXT("Every client sees the enemy stunned"), [this](FState& State) { return SeesStatus(State, TargetId, EVeyraStatusKind::Stun); })
				.ThenServer(TEXT("With the slam's damage and Cairn's shield"), [this](FState& State) {
					ASSERT_THAT(IsTrue(HealthLost(ParticipantOf(State, 1)) > 0.0));
					ASSERT_THAT(AreEqual(1, ShieldsOf(ParticipantOf(State, 0)).Num()));
				});
		}

		TEST_METHOD(ImmovableHardensCairnUntilHeEndsItEarly)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, HookDistance); })
				.ThenClient(TEXT("Brace"), 0, [this](FState& State) { CastAtTheTarget(State, EVeyraAbilitySlot::E); })
				.UntilClients(TEXT("Every client sees Cairn braced"), [this](FState& State) {
					return SeesStatus(State, CasterId, EVeyraStatusKind::DamageReduction) && SeesStatus(State, CasterId, EVeyraStatusKind::DisplacementResistance)
						&& SeesStatus(State, CasterId, EVeyraStatusKind::MoveSpeed);
				})
				.ThenClient(TEXT("End it early"), 0, [this](FState& State) { CastAtTheTarget(State, EVeyraAbilitySlot::E); })
				.UntilClients(TEXT("Every client sees it end"), [this](FState& State) {
					return !SeesStatus(State, CasterId, EVeyraStatusKind::DamageReduction) && !SeesStatus(State, CasterId, EVeyraStatusKind::MoveSpeed);
				});
		}

		TEST_METHOD(BurdenOfTheDepthsPaysItsOwnShieldForTheVanguardItCatches)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, BurdenDistance); })
				.ThenClient(TEXT("Anchor and erupt"), 0, [this](FState& State) { CastAtTheTarget(State, EVeyraAbilitySlot::R); })
				.UntilClients(TEXT("Every client sees the enemy stunned"), [this](FState& State) { return SeesStatus(State, TargetId, EVeyraStatusKind::Stun); })
				.ThenServer(TEXT("One shield, the ultimate's own, and no Deep Foundation shield for that target"), [this](FState& State) {
					const AVeyraPlayerState* Caster = ParticipantOf(State, 0);
					const FVeyraAreaAbilityTuning& Burden = *UVeyraAbilitiesTuningSubsystem::FindArea(Id(TEXT("cairn_burden_of_the_depths")));
					const double Expected = VeyraEffectDelivery::ShieldGrant(*Caster->GetAbilitySystemComponent(), Burden.Zones[0].CasterShieldPerVanguard[0], 1).Amount;
					const TArray<FVeyraShieldEntry>& Shields = ShieldsOf(Caster);
					ASSERT_THAT(AreEqual(1, Shields.Num()));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Shields[0].Remaining, Expected, Tolerance), FString::Printf(TEXT("shield %g, expected %g"), Shields[0].Remaining, Expected)));
					ASSERT_THAT(IsTrue(Caster->FindComponentByClass<UVeyraCooldownComponent>()->GetRemainingSecondsNow(Id(TEXT("cairn_burden_of_the_depths"))) > 0.0));
				});
		}

		TEST_METHOD(TheSameEnemyGrantsDeepFoundationOncePerLockout)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Prepare the duel"), [this](FState& State) { PrepareDuel(State, HookDistance); })
				.ThenClient(TEXT("Hook the enemy"), 0, [this](FState& State) { CastAtTheTarget(State, EVeyraAbilitySlot::Q); })
				.UntilServer(TEXT("The pull ends"), [](FState& State) {
					return HealthLost(ParticipantOf(State, 1)) > 0.0 && !ParticipantOf(State, 1)->GetPawn()->FindComponentByClass<UVeyraMovementComponent>()->IsDisplaced();
				})
				.ThenServer(TEXT("Aim the slam where the enemy now stands"), [this](FState& State) { TargetPoint = ParticipantOf(State, 1)->GetPawn()->GetActorLocation(); })
				.ThenClient(TEXT("Slam it"), 0, [this](FState& State) { CastAtTheTarget(State, EVeyraAbilitySlot::W); })
				.UntilServer(TEXT("The stun lands"), [this](FState& State) { return SeesStatus(State, TargetId, EVeyraStatusKind::Stun); })
				.ThenServer(TEXT("Still one Deep Foundation grant"), [this](FState& State) {
					const TArray<FVeyraShieldEntry>& Shields = ShieldsOf(ParticipantOf(State, 0));
					ASSERT_THAT(AreEqual(1, Shields.Num()));
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Shields[0].Remaining, FoundationGrant(State), Tolerance),
						FString::Printf(TEXT("shield %g, one grant %g"), Shields[0].Remaining, FoundationGrant(State))));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
