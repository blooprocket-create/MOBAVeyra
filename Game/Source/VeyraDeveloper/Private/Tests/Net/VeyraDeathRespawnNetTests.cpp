// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Life/VeyraLifeComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	// Veyra.Net.DeathRespawn.*: a Vanguard that dies leaves the map and returns at its fountain after
	// the tuned delay, with a new body; its PlayerState, and so its cooldowns, carry on (ADR-006 §4,
	// Combat Bible §18, §44).
	NETWORK_TEST_CLASS(DeathRespawn, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedAbilitiesTuning> Abilities;
		FVeyraGreyboxLayout Layout;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double RespawnSeconds = 0.5;
		static constexpr double LongRange = 10000.0;
		static constexpr double LongCooldownSeconds = 60.0;
		static constexpr double BoltDamage = 50.0;

		int32 VictimId = INDEX_NONE;
		int32 EnemyId = INDEX_NONE;
		FVector VictimStart = FVector::ZeroVector;
		TWeakObjectPtr<APawn> FirstBody;
		TWeakObjectPtr<AVeyraVanguardController> FirstController;

		BEFORE_ALL()
		{
			LoadOnDemandEngineModules();
		}

		BEFORE_EACH()
		{
			IgnoreLoginViewTargetRpc(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.Respawn.DelaySeconds = RespawnSeconds;
			Abilities = MakeUnique<FScopedAbilitiesTuning>();
			FVeyraTargetedDamageAbilityTuning Bolt;
			Bolt.CastRange = LongRange;
			Bolt.CooldownSeconds = LongCooldownSeconds;
			Bolt.DamageType = EVeyraDamageType::TrueDamage;
			Bolt.DamageAmount = BoltDamage;
			Abilities->Tuning.TargetedDamage.Add(Tuning->Tuning.DeveloperLoadout.AbilityQ, Bolt);
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Abilities.Reset();
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static AVeyraPlayerState& ServerParticipant(FState& State, int32 ClientIndex)
		{
			return *ServerControllerOf(State, ClientIndex)->GetPlayerState<AVeyraPlayerState>();
		}

		double RemainingCooldown(const AVeyraPlayerState& Participant, double Now) const
		{
			return Participant.FindComponentByClass<UVeyraCooldownComponent>()->GetRemainingSeconds(Tuning->Tuning.DeveloperLoadout.AbilityQ, Now);
		}

		TEST_METHOD(ADeadVanguardRespawnsAtItsFountainWithItsCooldowns)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Note the victim and its enemy"), [this](FState& State) {
					AVeyraPlayerState& Victim = ServerParticipant(State, 0);
					VictimId = Victim.GetPlayerId();
					EnemyId = ServerParticipant(State, 1).GetPlayerId();
					FirstBody = Victim.GetPawn();
					FirstController = Victim.GetVanguardController();
					VictimStart = FirstBody->GetActorLocation();
				})
				.ThenClient(TEXT("The victim casts, starting its cooldown"), 0, [this](FState& State) {
					LocalControllerOf(State.World)->IssueCastOrder(EVeyraAbilitySlot::Q, FindVanguard(State.World, EnemyId));
				})
				.UntilServer(TEXT("The cooldown runs"), [this](FState& State) {
					return RemainingCooldown(ServerParticipant(State, 0), State.World->GetTimeSeconds()) > 0.0;
				})
				.ThenServer(TEXT("Kill the victim"), [this](FState& State) {
					AVeyraPlayerState& Victim = ServerParticipant(State, 0);
					FVeyraRawDamageEvent Lethal;
					Lethal.Components.Add({ EVeyraDamageType::TrueDamage, Tuning->Tuning.DeveloperLoadout.MaxHealth });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*ServerParticipant(State, 1).GetAbilitySystemComponent(), *Victim.GetAbilitySystemComponent(), Lethal)));
					ASSERT_THAT(IsFalse(Victim.FindComponentByClass<UVeyraLifeComponent>()->IsAlive()));
				})
				.UntilClients(TEXT("Every machine sees the body leave"), [this](FState& State) { return FindVanguard(State.World, VictimId) == nullptr; })
				.UntilServer(TEXT("The victim respawns"), [this](FState& State) {
					AVeyraPlayerState& Victim = ServerParticipant(State, 0);
					return Victim.GetPawn() != nullptr && Victim.FindComponentByClass<UVeyraLifeComponent>()->IsAlive();
				})
				.ThenServer(TEXT("With a new body at its fountain, full Health and its cooldown"), [this](FState& State) {
					AVeyraPlayerState& Victim = ServerParticipant(State, 0);
					UAbilitySystemComponent& Abilities = *Victim.GetAbilitySystemComponent();
					ASSERT_THAT(IsTrue(Victim.GetPawn() != FirstBody.Get()));
					// The same controller outlived the old body and moves the new one.
					ASSERT_THAT(IsTrue(FirstController.IsValid() && Victim.GetVanguardController() == FirstController.Get()));
					ASSERT_THAT(IsTrue(Victim.GetPawn()->GetController() == FirstController.Get()));
					// The sides start at opposite ends, DistanceFromCenterX from the centre, so this is its own start.
					ASSERT_THAT(IsTrue(FVector::Dist2D(Victim.GetPawn()->GetActorLocation(), VictimStart) < Layout.TeamStarts.DistanceFromCenterX));
					ASSERT_THAT(IsTrue(Abilities.GetAvatarActor() == Victim.GetPawn()));
					ASSERT_THAT(IsTrue(Abilities.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) == Tuning->Tuning.DeveloperLoadout.MaxHealth));
					ASSERT_THAT(IsTrue(RemainingCooldown(Victim, State.World->GetTimeSeconds()) > 0.0));
				})
				.UntilClient(TEXT("The victim's client views and drives the new body"), 0, [this](FState& State) {
					const AVeyraPlayerController* Controller = LocalControllerOf(State.World);
					const APawn* Body = Controller ? Controller->GetVanguard() : nullptr;
					return Body && Controller->GetViewTarget() == Body
						&& Controller->GetPlayerState<AVeyraPlayerState>()->GetAbilitySystemComponent()->GetAvatarActor() == Body;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
