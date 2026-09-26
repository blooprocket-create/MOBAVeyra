// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestActor.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "TimerManager.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardController.h"

namespace VeyraNetTests
{
	// Veyra.Net.MatchPause.*: the ADR-006 §8 spike. An approved pause stops every gameplay clock
	// while networking keeps running (Match Flow Bible §10.2). Each timer category the bible lists
	// runs on one of the mechanisms checked here: effect durations (buffs, shields), the cooldown
	// ledger, world timers (respawn, buyback, spawn and penalty clocks), world time (the match clock)
	// and actor ticks (movement, regeneration, combat).
	//
	// Clients learn of the pause from the GameState. These in-process tests cannot check that a
	// client's own world pauses: the engine replicates that through the map's WorldSettings, and
	// in-process play sessions replicate no map-placed actor, under Iris or the legacy system. The
	// multi-process container test covers it.
	NETWORK_TEST_CLASS(MatchPause, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
			AVeyraNetTestActor* Actor = nullptr;
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double ShieldAmount = 50.0;
		static constexpr double ShieldSeconds = 2.0;
		static constexpr double CooldownSeconds = 2.0;
		static constexpr float TimerSeconds = 2.0f;
		static constexpr double PauseHoldRealSeconds = 0.5;
		static constexpr int32 ValueSentWhilePaused = 3;

		// What the server saw when the pause began.
		FActiveGameplayEffectHandle Shield;
		FTimerHandle Timer;
		bool bTimerFired = false;
		double PausedWorldTime = 0.0;
		double PausedRealTime = 0.0;
		double PausedShieldRemaining = 0.0;
		double PausedTimerRemaining = 0.0;
		double PausedCooldownRemaining = 0.0;
		double PausedOwnerCooldownRemaining = 0.0;
		double PausedMatchClock = 0.0;
		FVector PausedLocation = FVector::ZeroVector;
		TMap<int32, double> PausedClientClocks;

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
			// Loading waits for both clients rather than the loading timeout.
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static AVeyraPlayerState* MoverState(FState& State)
		{
			const AVeyraPlayerController* Mover = ServerControllerOf(State, 0);
			return Mover ? Mover->GetPlayerState<AVeyraPlayerState>() : nullptr;
		}

		FVeyraContentId Ability() const
		{
			return Tuning->Tuning.DeveloperLoadout.AbilityQ;
		}

		static UVeyraCooldownComponent* CooldownsOf(const APlayerState* PlayerState)
		{
			return PlayerState ? PlayerState->FindComponentByClass<UVeyraCooldownComponent>() : nullptr;
		}

		/** On the mover's own client: its Q cooldown as its HUD would show it. */
		double OwnerCooldownRemaining(FState& State) const
		{
			const AVeyraPlayerController* Controller = LocalControllerOf(State.World);
			const UVeyraCooldownComponent* Cooldowns = Controller ? CooldownsOf(Controller->PlayerState) : nullptr;
			return Cooldowns ? Cooldowns->GetRemainingSeconds(Ability(), GameStateOf(State.World)->GetGameplayServerTime()) : 0.0;
		}

		static const FActiveGameplayEffect* FindShield(FState& State, const FActiveGameplayEffectHandle& Handle)
		{
			const AVeyraPlayerState* PlayerState = MoverState(State);
			return PlayerState ? PlayerState->GetAbilitySystemComponent()->GetActiveGameplayEffect(Handle) : nullptr;
		}

		TEST_METHOD(PauseFreezesGameplayClocksButNotTheNetwork)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Start a shield, a cooldown, a timer and a move"), [this](FState& State) {
					AVeyraPlayerState* PlayerState = MoverState(State);
					ASSERT_THAT(IsNotNull(PlayerState));
					UAbilitySystemComponent& AbilitySystem = *PlayerState->GetAbilitySystemComponent();
					Shield = VeyraCombat::GrantShield(AbilitySystem, AbilitySystem, EVeyraShieldCategory::Universal, ShieldAmount, ShieldSeconds);
					ASSERT_THAT(IsTrue(Shield.WasSuccessfullyApplied()));
					CooldownsOf(PlayerState)->StartCooldown(Ability(), CooldownSeconds);
					State.World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([this] { bTimerFired = true; }), TimerSeconds, false);
					const FVector FarEnd(Layout.TeamStarts.DistanceFromCenterX, 0.0, 0.0);
					ASSERT_THAT(IsTrue(PlayerState->GetVanguardController()->MoveToDestination(FarEnd) == EVeyraOrderRejection::None));
				})
				.UntilServer(TEXT("The Vanguard is moving"), [](FState& State) {
					const AVeyraPlayerState* PlayerState = MoverState(State);
					return PlayerState && PlayerState->GetPawn() && !PlayerState->GetPawn()->GetVelocity().IsNearlyZero();
				})
				.UntilClient(TEXT("The mover's client has its cooldown"), 0, [this](FState& State) { return OwnerCooldownRemaining(State) > 0.0; })
				.ThenServer(TEXT("Pause"), [this](FState& State) {
					PausedWorldTime = State.World->GetTimeSeconds();
					PausedRealTime = State.World->GetUnpausedTimeSeconds();
					PausedShieldRemaining = FindShield(State, Shield)->GetTimeRemaining(State.World->GetTimeSeconds());
					PausedTimerRemaining = State.World->GetTimerManager().GetTimerRemaining(Timer);
					PausedCooldownRemaining = CooldownsOf(MoverState(State))->GetRemainingSecondsNow(Ability());
					PausedMatchClock = GameStateOf(State.World)->GetMatchClockSeconds();
					PausedLocation = MoverState(State)->GetPawn()->GetActorLocation();
					ASSERT_THAT(IsTrue(GameModeOf(State.World)->PauseMatch(*ServerControllerOf(State, 0))));
				})
				.UntilClients(TEXT("Clients see the pause"), [](FState& State) { return GameStateOf(State.World)->IsMatchPaused(); })
				.ThenClients(TEXT("Note each client's match clock"), [this](FState& State) {
					PausedClientClocks.Add(State.ClientIndex, GameStateOf(State.World)->GetMatchClockSeconds());
				})
				.ThenClient(TEXT("Note the mover's cooldown on its client"), 0, [this](FState& State) { PausedOwnerCooldownRemaining = OwnerCooldownRemaining(State); })
				.UntilServer(TEXT("Hold the pause"), [this](FState& State) {
					return State.World->GetUnpausedTimeSeconds() - PausedRealTime >= PauseHoldRealSeconds;
				})
				.ThenServer(TEXT("Nothing moved during the pause"), [this](FState& State) {
					ASSERT_THAT(IsTrue(State.World->GetTimeSeconds() == PausedWorldTime));
					ASSERT_THAT(IsTrue(FindShield(State, Shield) != nullptr));
					ASSERT_THAT(IsTrue(FindShield(State, Shield)->GetTimeRemaining(State.World->GetTimeSeconds()) == PausedShieldRemaining));
					ASSERT_THAT(IsTrue(State.World->GetTimerManager().GetTimerRemaining(Timer) == PausedTimerRemaining));
					ASSERT_THAT(IsFalse(bTimerFired));
					ASSERT_THAT(IsTrue(CooldownsOf(MoverState(State))->GetRemainingSecondsNow(Ability()) == PausedCooldownRemaining));
					ASSERT_THAT(IsTrue(GameStateOf(State.World)->GetMatchClockSeconds() == PausedMatchClock));
					ASSERT_THAT(IsTrue(MoverState(State)->GetPawn()->GetActorLocation() == PausedLocation));
				})
				.ThenClients(TEXT("Clients' match clocks stopped too"), [this](FState& State) {
					ASSERT_THAT(IsTrue(GameStateOf(State.World)->GetMatchClockSeconds() == PausedClientClocks.FindChecked(State.ClientIndex)));
				})
				.ThenClient(TEXT("So did the cooldown its client shows"), 0, [this](FState& State) {
					ASSERT_THAT(IsTrue(OwnerCooldownRemaining(State) == PausedOwnerCooldownRemaining));
				})
				.ThenClient(0, [](FState& State) { LocalControllerOf(State.World)->IssueMoveOrder(FVector::ZeroVector); })
				.UntilClient(0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastOrderRejection() == EVeyraOrderRejection::Paused;
				})
				.SpawnAndReplicate<AVeyraNetTestActor, &FState::Actor>()
				.ThenServer(TEXT("Replicate a change while paused"), [](FState& State) { State.Actor->SetValue(ValueSentWhilePaused); })
				.UntilClients(TEXT("Clients receive it while paused"), [](FState& State) {
					return GameStateOf(State.World)->IsMatchPaused() && State.Actor->GetValue() == ValueSentWhilePaused;
				})
				.ThenServer(TEXT("Resume"), [this](FState& State) { ASSERT_THAT(IsTrue(GameModeOf(State.World)->ResumeMatch())); })
				.UntilServer(TEXT("The shield, the cooldown and the timer end on their frozen schedule"), [this](FState& State) {
					return FindShield(State, Shield) == nullptr && bTimerFired && CooldownsOf(MoverState(State))->GetRemainingSecondsNow(Ability()) == 0.0;
				})
				.ThenServer(TEXT("They resumed from their saved values"), [this](FState& State) {
					const double LongestRemaining = FMath::Max3(PausedShieldRemaining, PausedTimerRemaining, PausedCooldownRemaining);
					ASSERT_THAT(IsTrue(State.World->GetTimeSeconds() >= PausedWorldTime + LongestRemaining));
					ASSERT_THAT(IsTrue(MoverState(State)->GetPawn()->GetActorLocation() != PausedLocation));
				})
				.UntilClients(TEXT("Clients see the match resume"), [](FState& State) { return !GameStateOf(State.World)->IsMatchPaused(); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
