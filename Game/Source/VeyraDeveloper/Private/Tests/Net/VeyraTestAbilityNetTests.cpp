// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Life/VeyraLifeComponent.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	/** The Q ability every developer Vanguard is given, and fixture numbers for it. */
	struct FTestBolt
	{
		static FVeyraContentId Id() { return UVeyraMatchTuningSubsystem::Get().DeveloperLoadout.AbilityQ; }
		static constexpr double LongRange = 10000.0;
		static constexpr double CooldownSeconds = 30.0;
		static constexpr double ResourceCost = 10.0;
		static constexpr double DamageAmount = 50.0;

		static void Configure(FVeyraAbilitiesTuning& Tuning, double CastRange)
		{
			FVeyraTargetedDamageAbilityTuning Bolt;
			Bolt.CastRange = CastRange;
			Bolt.CooldownSeconds = CooldownSeconds;
			Bolt.ResourceCost = ResourceCost;
			Bolt.DamageType = EVeyraDamageType::TrueDamage;
			Bolt.DamageAmount = DamageAmount;
			Tuning.TargetedDamage.Add(Id(), Bolt);
		}
	};

	inline const AVeyraPlayerState* ParticipantOf(const UWorld* World, int32 PlayerId)
	{
		const AVeyraGameState* GameState = GameStateOf(World);
		for (const APlayerState* Participant : GameState ? GameState->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			if (Participant && Participant->GetPlayerId() == PlayerId)
			{
				return Cast<AVeyraPlayerState>(Participant);
			}
		}
		return nullptr;
	}

	inline double AttributeOf(const UWorld* World, int32 PlayerId, const FGameplayAttribute& Attribute)
	{
		const AVeyraPlayerState* Participant = ParticipantOf(World, PlayerId);
		return Participant ? Participant->GetAbilitySystemComponent()->GetNumericAttribute(Attribute) : -1.0;
	}

	// Veyra.Net.TestAbility.*: the done criterion's automated half (ADR-006 M3). A client casts the
	// developer test ability at the other client's Vanguard; the server validates and resolves it,
	// and every machine sees the result.
	NETWORK_TEST_CLASS(TestAbility, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		TUniquePtr<FScopedAbilitiesTuning> Abilities;
		FVeyraGreyboxLayout Layout;

		static constexpr double ShortPreparationSeconds = 0.1;

		int32 CasterId = INDEX_NONE;
		int32 TargetId = INDEX_NONE;
		double MaxHealth = 0.0;
		double MaxResource = 0.0;

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
			MaxHealth = Tuning->Tuning.DeveloperLoadout.MaxHealth;
			MaxResource = Tuning->Tuning.DeveloperLoadout.MaxResource;
			Abilities = MakeUnique<FScopedAbilitiesTuning>();
			FTestBolt::Configure(Abilities->Tuning, FTestBolt::LongRange);
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Abilities.Reset();
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		/** Records which participant casts (client 0) and which is targeted (client 1). */
		FPIENetworkComponent<FState>& Identify(FPIENetworkComponent<FState>& Chain)
		{
			return Chain.ThenServer(TEXT("Identify the players"), [this](FState& State) {
				CasterId = ServerControllerOf(State, 0)->PlayerState->GetPlayerId();
				TargetId = ServerControllerOf(State, 1)->PlayerState->GetPlayerId();
			});
		}

		void CastFromClient0(FState& State, int32 PlayerId)
		{
			LocalControllerOf(State.World)->IssueCastOrder(EVeyraAbilitySlot::Q, FindVanguard(State.World, PlayerId));
		}

		TEST_METHOD(AValidCastResolvesForEveryone)
		{
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenClient(0, [this](FState& State) { CastFromClient0(State, TargetId); })
				.UntilClients(TEXT("Every machine sees the damage"), [this](FState& State) {
					return AttributeOf(State.World, TargetId, UVeyraVitalsSet::GetHealthAttribute()) == MaxHealth - FTestBolt::DamageAmount;
				})
				.UntilClient(TEXT("The caster sees its cost and cooldown"), 0, [this](FState& State) {
					const AVeyraPlayerState* Caster = ParticipantOf(State.World, CasterId);
					const UVeyraCooldownComponent* Cooldowns = Caster ? Caster->FindComponentByClass<UVeyraCooldownComponent>() : nullptr;
					return AttributeOf(State.World, CasterId, UVeyraResourceSet::GetResourceAttribute()) == MaxResource - FTestBolt::ResourceCost
						&& Cooldowns && Cooldowns->GetRemainingSeconds(FTestBolt::Id(), GameStateOf(State.World)->GetGameplayServerTime()) > 0.0;
				})
				.ThenClient(0, [this](FState& State) { ASSERT_THAT(AreEqual(LocalControllerOf(State.World)->GetCastRejectionCount(), 0)); });
		}

		TEST_METHOD(TheServerRefusesInvalidCasts)
		{
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenClient(0, [this](FState& State) { CastFromClient0(State, CasterId); })
				.UntilClient(0, [](FState& State) { return LocalControllerOf(State.World)->GetLastCastRejection() == EVeyraCastRejection::InvalidTarget; })
				.ThenClient(0, [this](FState& State) {
					CastFromClient0(State, TargetId);
					CastFromClient0(State, TargetId);
				})
				.UntilClient(0, [](FState& State) { return LocalControllerOf(State.World)->GetLastCastRejection() == EVeyraCastRejection::OnCooldown; })
				.UntilClients(TEXT("Only one cast landed"), [this](FState& State) {
					return AttributeOf(State.World, TargetId, UVeyraVitalsSet::GetHealthAttribute()) == MaxHealth - FTestBolt::DamageAmount;
				});
		}

		TEST_METHOD(TheServerRefusesCastsOutOfRange)
		{
			constexpr double ShortRange = 100.0;
			Abilities->Tuning.TargetedDamage.Reset();
			FTestBolt::Configure(Abilities->Tuning, ShortRange);
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenClient(0, [this](FState& State) { CastFromClient0(State, TargetId); })
				.UntilClient(0, [](FState& State) { return LocalControllerOf(State.World)->GetLastCastRejection() == EVeyraCastRejection::OutOfRange; });
		}

		TEST_METHOD(TheServerRefusesCastsItCannotPayFor)
		{
			Tuning->Tuning.DeveloperLoadout.MaxResource = FTestBolt::ResourceCost / 2.0;
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenClient(0, [this](FState& State) { CastFromClient0(State, TargetId); })
				.UntilClient(0, [](FState& State) {
					return LocalControllerOf(State.World)->GetLastCastRejection() == EVeyraCastRejection::InsufficientResource;
				});
		}

		// A dead Vanguard's body leaves the map, so a client has nothing left to aim at; the rule that a
		// dead target is refused is covered by Veyra.Abilities.TargetedCasting.
		TEST_METHOD(ADeadVanguardCannotBeTargeted)
		{
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Live))
				.ThenServer(TEXT("Kill the target"), [this](FState& State) {
					FVeyraRawDamageEvent Lethal;
					Lethal.Components.Add({ EVeyraDamageType::TrueDamage, MaxHealth });
					UAbilitySystemComponent& Caster = *ServerControllerOf(State, 0)->GetPlayerState<AVeyraPlayerState>()->GetAbilitySystemComponent();
					UAbilitySystemComponent& Target = *ServerControllerOf(State, 1)->GetPlayerState<AVeyraPlayerState>()->GetAbilitySystemComponent();
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Caster, Target, Lethal)));
				})
				.UntilClient(TEXT("The target's death reaches the caster"), 0, [this](FState& State) {
					const AVeyraPlayerState* Target = ParticipantOf(State.World, TargetId);
					return Target && !Target->FindComponentByClass<UVeyraLifeComponent>()->IsAlive();
				})
				.UntilClient(TEXT("Its body leaves the map"), 0, [this](FState& State) { return FindVanguard(State.World, TargetId) == nullptr; })
				.ThenClient(0, [this](FState& State) { CastFromClient0(State, TargetId); })
				.UntilClient(0, [](FState& State) { return LocalControllerOf(State.World)->GetLastCastRejection() == EVeyraCastRejection::InvalidTarget; });
		}

		TEST_METHOD(TheMatchRefusesCastsBeforeItIsLiveAndWhilePaused)
		{
			constexpr double LongPreparationSeconds = 600.0;
			Tuning->Tuning.Phases.PreparationSeconds = LongPreparationSeconds;
			Identify(StartMatch(Network, Layout, EVeyraMatchPhase::Preparation))
				.ThenClient(0, [this](FState& State) { CastFromClient0(State, TargetId); })
				.UntilClient(0, [](FState& State) { return LocalControllerOf(State.World)->GetLastCastRejection() == EVeyraCastRejection::WrongPhase; })
				.ThenServer(TEXT("Pause"), [this](FState& State) { ASSERT_THAT(IsTrue(GameModeOf(State.World)->PauseMatch(*ServerControllerOf(State, 0)))); })
				.UntilClient(0, [](FState& State) { return GameStateOf(State.World)->IsMatchPaused(); })
				.ThenClient(0, [this](FState& State) { CastFromClient0(State, TargetId); })
				.UntilClient(0, [](FState& State) { return LocalControllerOf(State.World)->GetLastCastRejection() == EVeyraCastRejection::Paused; })
				.ThenServer([this](FState& State) { ASSERT_THAT(IsTrue(GameModeOf(State.World)->ResumeMatch())); });
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
