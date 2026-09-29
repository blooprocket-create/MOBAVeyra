// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Movement/VeyraMovementComponent.h"
#include "Structures/VeyraStructureAttackComponent.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tests/Net/VeyraBattlegroundNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tests/Net/VeyraVanguardNetTestHelpers.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

namespace VeyraNetTests
{
	// Veyra.Net.TowerAggro.*: a tower shoots on its own cadence, Fluxborn first, until an enemy Vanguard
	// hurts a defender in its range; then it turns on that Vanguard, and every client sees the shots
	// land (Combat Bible §33, §55; ADR-011 §8).
	NETWORK_TEST_CLASS(TowerAggro, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Greybox;
		TWeakObjectPtr<AVeyraTestFluxborn> Minion;
		int32 IntruderId = INDEX_NONE;

		// Fixture values: a short preparation, and a hit on a defender to draw aggression.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double Hit = 10.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Greybox).IsEmpty()));
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

		static AVeyraStructure* OuterSpire(const FState& State)
		{
			return State.World->GetSubsystem<UVeyraBattlegroundSubsystem>()->FindStructure(EVeyraTeam::B, EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 0);
		}

		/** A point on the ground Along the outer Spire's range, toward Team A's side. */
		static FVector InRange(const FState& State, double Along, double Across)
		{
			const AVeyraStructure* Spire = OuterSpire(State);
			const double Reach = Spire->GetSimpleCollisionRadius() + UVeyraWorldTuningSubsystem::Get().TowerAttack.Range * Along;
			const FVector Centre = Spire->GetActorLocation();
			return FVector(Centre.X - Reach, Centre.Y + Across, 0.0);
		}

		static AVeyraPlayerState* OfTeam(FState& State, EVeyraTeam Team)
		{
			for (int32 Client = 0; Client < MatchClientCount; ++Client)
			{
				AVeyraPlayerState* Participant = ParticipantOf(State, Client);
				if (Participant && Participant->GetVeyraTeam() == Team)
				{
					return Participant;
				}
			}
			return nullptr;
		}

		static double HealthLost(const APlayerState* Participant)
		{
			const UAbilitySystemComponent* AbilitySystem = Participant ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Participant) : nullptr;
			return AbilitySystem ? VeyraCombat::GetMissingHealth(*AbilitySystem) : 0.0;
		}

		TEST_METHOD(ATowerShootsFluxbornUntilAVanguardHurtsADefender)
		{
			StartBattleground(Network, Greybox, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Team A's Vanguard and a Fluxborn walk into team B's outer Spire's range"), [this](FState& State) {
					AVeyraPlayerState* Intruder = OfTeam(State, EVeyraTeam::A);
					ASSERT_THAT(IsTrue(Intruder && Intruder->GetPawn()));
					Intruder->GetPawn()->SetActorLocation(InRange(State, 0.7, 200.0));
					IntruderId = Intruder->GetPlayerId();
					FActorSpawnParameters Params;
					Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
					AVeyraTestFluxborn* Unit = State.World->SpawnActor<AVeyraTestFluxborn>(AVeyraTestFluxborn::StaticClass(), FTransform(InRange(State, 0.2, 0.0)), Params);
					ASSERT_THAT(IsNotNull(Unit));
					Unit->SetVeyraTeam(EVeyraTeam::A);
					ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(*Unit->GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats())));
					Unit->GetVeyraMovement()->SetMovementMode(MOVE_Walking);
					Minion = Unit;
				})
				.UntilServer(TEXT("The Spire shoots the Fluxborn on its own, and spares the Vanguard"), [this](FState& State) {
					const AVeyraTestFluxborn* Unit = Minion.Get();
					const UAbilitySystemComponent* AbilitySystem = Unit ? Unit->GetAbilitySystemComponent() : nullptr;
					return AbilitySystem && VeyraCombat::GetMissingHealth(*AbilitySystem) > 0.0 && HealthLost(OfTeam(State, EVeyraTeam::A)) == 0.0
						&& OuterSpire(State)->GetAttack()->GetTarget() == Unit;
				})
				.ThenServer(TEXT("Team A's Vanguard hurts team B's, both in range"), [this](FState& State) {
					AVeyraPlayerState* Attacker = OfTeam(State, EVeyraTeam::A);
					AVeyraPlayerState* Defender = OfTeam(State, EVeyraTeam::B);
					ASSERT_THAT(IsTrue(Attacker && Defender && Defender->GetPawn()));
					Defender->GetPawn()->SetActorLocation(InRange(State, 0.5, -200.0));
					FVeyraRawDamageEvent Damage;
					Damage.Components.Add({ EVeyraDamageType::TrueDamage, Hit });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Attacker->GetAbilitySystemComponent(), *Defender->GetAbilitySystemComponent(), Damage)));
				})
				.UntilServer(TEXT("The Spire turns on the attacker with priority"), [](FState& State) {
					const UVeyraStructureAttackComponent* Attack = OuterSpire(State)->GetAttack();
					return Attack->GetTarget() == OfTeam(State, EVeyraTeam::A)->GetPawn() && Attack->HasPriority();
				})
				.UntilClients(TEXT("Every client sees its shots land on the attacker"), [this](FState& State) {
					for (const APlayerState* Participant : GameStateOf(State.World)->PlayerArray)
					{
						if (Participant && Participant->GetPlayerId() == IntruderId)
						{
							return HealthLost(Participant) > 0.0;
						}
					}
					return false;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
