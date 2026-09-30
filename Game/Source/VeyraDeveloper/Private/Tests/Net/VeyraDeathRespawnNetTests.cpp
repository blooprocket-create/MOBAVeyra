// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Buyback/VeyraBuybackComponent.h"
#include "Camera/VeyraCameraRig.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Gold/VeyraGoldComponent.h"
#include "Life/VeyraLifeComponent.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
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
		/** Economy.json as a test sets it, while it does. */
		TOptional<FVeyraEconomyTuning> Economy;
		FVeyraGreyboxLayout Layout;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double RespawnSeconds = 0.5;
		static constexpr double LongRange = 10000.0;
		static constexpr double LongCooldownSeconds = 60.0;
		static constexpr double BoltDamage = 50.0;
		static constexpr double CentredWithin = 1.0;

		int32 VictimId = INDEX_NONE;
		int32 EnemyId = INDEX_NONE;
		FVector VictimStart = FVector::ZeroVector;
		TWeakObjectPtr<APawn> FirstBody;
		TWeakObjectPtr<AVeyraVanguardController> FirstController;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			// One timer at every level, and no lengthening with the match clock.
			Tuning->Tuning.Respawn.SecondsByLevel = { RespawnSeconds };
			Tuning->Tuning.Respawn.Elapsed = FVeyraRespawnElapsedTuning();
			Abilities = MakeUnique<FScopedAbilitiesTuning>();
			FVeyraTargetedDamageAbilityTuning Bolt;
			Bolt.CastRange = LongRange;
			Bolt.CooldownSeconds = LongCooldownSeconds;
			Bolt.DamageType = EVeyraDamageType::TrueDamage;
			Bolt.DamageAmount = BoltDamage;
			Abilities->Tuning.TargetedDamage.Add(TestVanguardAbilityQ(), Bolt);
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			UVeyraEconomyTuningSubsystem::SetTestOverride(nullptr);
			Economy.Reset();
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
			return Participant.FindComponentByClass<UVeyraCooldownComponent>()->GetRemainingSeconds(TestVanguardAbilityQ(), Now);
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
					Lethal.Components.Add({ EVeyraDamageType::TrueDamage, TestVanguard().BaseStats.MaxHealth });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*ServerParticipant(State, 1).GetAbilitySystemComponent(), *Victim.GetAbilitySystemComponent(), Lethal)));
					ASSERT_THAT(IsFalse(Victim.FindComponentByClass<UVeyraLifeComponent>()->IsAlive()));
				})
				.UntilClients(TEXT("Every machine sees the body leave"), [this](FState& State) { return FindVanguard(State.World, VictimId) == nullptr; })
				.UntilClient(TEXT("The victim's client learns when it returns"), 0, [](FState& State) {
					return LocalControllerOf(State.World)->GetPlayerState<AVeyraPlayerState>()->GetRespawnAt() > 0.0;
				})
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
					ASSERT_THAT(IsTrue(Abilities.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) == TestVanguard().BaseStats.MaxHealth));
					ASSERT_THAT(IsTrue(RemainingCooldown(Victim, State.World->GetTimeSeconds()) > 0.0));
				})
				.UntilClient(TEXT("The victim's client drives the new body, and its camera goes to it"), 0, [this](FState& State) {
					const AVeyraPlayerController* Controller = LocalControllerOf(State.World);
					const APawn* Body = Controller ? Controller->GetVanguard() : nullptr;
					// The client views its camera rig, which a respawn puts on the new body (ADR-020 §1).
					const AActor* Camera = Controller && Controller->GetCameraRig() ? Controller->GetCameraRig() : static_cast<const AActor*>(Body);
					return Body && Controller->GetViewTarget() == Camera && FVector::Dist2D(Camera->GetActorLocation(), Body->GetActorLocation()) <= CentredWithin
						&& Controller->GetPlayerState<AVeyraPlayerState>()->GetAbilitySystemComponent()->GetAvatarActor() == Body;
				});
		}

		// Buyback's fixture values: the first respawn waits long enough for the buyback to beat it, the
		// second far longer than the test; the buyback opens at once and cools down for longer than the test.
		static constexpr double FirstRespawnSeconds = 3.0;
		static constexpr double SecondRespawnSeconds = 60.0;
		static constexpr double BuybackCooldownSeconds = 60.0;
		static constexpr double Plenty = 10000.0;

		double FirstDeathAt = 0.0;
		double GoldBeforeBuyback = 0.0;

		void KillVictim(FState& State)
		{
			AVeyraPlayerState& Victim = ServerParticipant(State, 0);
			FVeyraRawDamageEvent Lethal;
			Lethal.Components.Add({ EVeyraDamageType::TrueDamage, TestVanguard().BaseStats.MaxHealth });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*ServerParticipant(State, 1).GetAbilitySystemComponent(), *Victim.GetAbilitySystemComponent(), Lethal)));
			ASSERT_THAT(IsFalse(Victim.FindComponentByClass<UVeyraLifeComponent>()->IsAlive()));
		}

		TEST_METHOD(ABuybackRespawnsAtOnceSpendsTheWaitingRespawnAndCoolsDown)
		{
			Economy.Emplace(UVeyraEconomyTuningSubsystem::Get());
			Economy->Buyback.AvailableFromSeconds = 0.0;
			Economy->Buyback.CooldownSeconds = BuybackCooldownSeconds;
			UVeyraEconomyTuningSubsystem::SetTestOverride(&Economy.GetValue());
			Tuning->Tuning.Respawn.SecondsByLevel = { FirstRespawnSeconds };
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Give the victim the Gold, and kill it"), [this](FState& State) {
					AVeyraPlayerState& Victim = ServerParticipant(State, 0);
					VictimId = Victim.GetPlayerId();
					FirstBody = Victim.GetPawn();
					UVeyraGoldComponent& Gold = *Victim.FindComponentByClass<UVeyraGoldComponent>();
					ASSERT_THAT(IsTrue(Gold.Grant(Plenty, EVeyraGoldReason::Developer)));
					GoldBeforeBuyback = Gold.GetGold();
					FirstDeathAt = State.World->GetTimeSeconds();
					KillVictim(State);
				})
				.UntilClients(TEXT("Every machine sees the body leave"), [this](FState& State) { return FindVanguard(State.World, VictimId) == nullptr; })
				.ThenClient(TEXT("Its player buys back from the shop"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestBuyback(); })
				.UntilServer(TEXT("It respawns"), [this](FState& State) {
					AVeyraPlayerState& Victim = ServerParticipant(State, 0);
					return Victim.GetPawn() != nullptr && Victim.FindComponentByClass<UVeyraLifeComponent>()->IsAlive();
				})
				.ThenServer(TEXT("At once, for its price, and its cooldown began; then it dies again"), [this](FState& State) {
					AVeyraPlayerState& Victim = ServerParticipant(State, 0);
					ASSERT_THAT(IsTrue(State.World->GetTimeSeconds() < FirstDeathAt + FirstRespawnSeconds, TEXT("before its respawn was due")));
					const UVeyraBuybackComponent& Buyback = *Victim.FindComponentByClass<UVeyraBuybackComponent>();
					ASSERT_THAT(AreEqual(1, Buyback.GetPurchases()));
					// The match is under a minute old: the price is the base, with no earlier buyback.
					const double Paid = GoldBeforeBuyback - Victim.FindComponentByClass<UVeyraGoldComponent>()->GetGold();
					ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Paid, Economy->Buyback.BaseCost), FString::Printf(TEXT("paid %.2f"), Paid)));
					ASSERT_THAT(IsTrue(Buyback.GetReadyAt() > State.World->GetTimeSeconds()));
					Tuning->Tuning.Respawn.SecondsByLevel = { SecondRespawnSeconds };
					KillVictim(State);
				})
				.ThenClient(TEXT("Its player tries to buy back again"), 0, [](FState& State) { LocalControllerOf(State.World)->RequestBuyback(); })
				.UntilClient(TEXT("And learns its buyback is cooling down"), 0, [](FState& State) {
					const AVeyraPlayerController* Controller = LocalControllerOf(State.World);
					return Controller->GetBuybackRefusalCount() > 0 && Controller->GetLastBuybackRefusal() == EVeyraBuybackRefusal::CoolingDown;
				})
				.UntilServer(TEXT("The first death's respawn would have come due"), [this](FState& State) {
					return State.World->GetTimeSeconds() > FirstDeathAt + FirstRespawnSeconds;
				})
				.ThenServer(TEXT("It is still dead: the buyback spent that respawn"), [this](FState& State) {
					ASSERT_THAT(IsFalse(ServerParticipant(State, 0).FindComponentByClass<UVeyraLifeComponent>()->IsAlive()));
					ASSERT_THAT(AreEqual(1, ServerParticipant(State, 0).FindComponentByClass<UVeyraBuybackComponent>()->GetPurchases()));
				});
		}

		TEST_METHOD(AZeroDelayRespawnsAtOnce)
		{
			// The schema allows 0; the engine's timers would ignore it.
			Tuning->Tuning.Respawn.SecondsByLevel = { 0.0 };
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Kill the victim"), [this](FState& State) {
					AVeyraPlayerState& Victim = ServerParticipant(State, 0);
					FirstBody = Victim.GetPawn();
					FVeyraRawDamageEvent Lethal;
					Lethal.Components.Add({ EVeyraDamageType::TrueDamage, TestVanguard().BaseStats.MaxHealth });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*ServerParticipant(State, 1).GetAbilitySystemComponent(), *Victim.GetAbilitySystemComponent(), Lethal)));
				})
				.UntilServer(TEXT("The victim respawns in a new body"), [this](FState& State) {
					AVeyraPlayerState& Victim = ServerParticipant(State, 0);
					return Victim.GetPawn() != nullptr && Victim.GetPawn() != FirstBody.Get() && Victim.FindComponentByClass<UVeyraLifeComponent>()->IsAlive();
				})
				.ThenServer(TEXT("And the old body is gone"), [this](FState& /*State*/) {
					const APawn* OldBody = FirstBody.Get();
					ASSERT_THAT(IsTrue(OldBody == nullptr || OldBody->IsActorBeingDestroyed()));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
