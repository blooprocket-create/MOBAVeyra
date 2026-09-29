// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Tests/Net/VeyraMatchNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"

namespace VeyraNetTests
{
	// Veyra.Net.FountainRecovery.*: a living Vanguard at its own fountain comes back to full Health
	// (Battleground Bible §12; ADR-011 §11), on the server, while its client watches.
	NETWORK_TEST_CLASS(FountainRecovery, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		TUniquePtr<FScopedMatchTuning> Tuning;
		FVeyraGreyboxLayout Layout;

		// Fixture values: a quick recovery, so the test waits a moment.
		static constexpr double ShortPreparationSeconds = 0.1;
		static constexpr double RecoveryFractionPerSecond = 1.0;
		static constexpr double WoundFraction = 0.5;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Layout).IsEmpty()));
			Tuning = MakeUnique<FScopedMatchTuning>();
			Tuning->Tuning.Phases.PreparationSeconds = ShortPreparationSeconds;
			Tuning->Tuning.Fountain.HealthFractionPerSecond = RecoveryFractionPerSecond;
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			Tuning.Reset();
			ExpectedPlayers.Reset();
		}

		static UAbilitySystemComponent& ServerAbilities(FState& State, int32 ClientIndex)
		{
			return *ServerControllerOf(State, ClientIndex)->GetPlayerState<AVeyraPlayerState>()->GetAbilitySystemComponent();
		}

		static double HealthOf(const UAbilitySystemComponent& Abilities)
		{
			return Abilities.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		TEST_METHOD(AWoundedVanguardAtItsFountainRecovers)
		{
			StartMatch(Network, Layout, EVeyraMatchPhase::Live)
				.ThenServer(TEXT("Wound the Vanguard where it started"), [this](FState& State) {
					UAbilitySystemComponent& Victim = ServerAbilities(State, 0);
					FVeyraRawDamageEvent Wound;
					Wound.Components.Add({ EVeyraDamageType::TrueDamage, TestVanguard().BaseStats.MaxHealth * WoundFraction });
					ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(ServerAbilities(State, 1), Victim, Wound)));
					ASSERT_THAT(IsTrue(HealthOf(Victim) < TestVanguard().BaseStats.MaxHealth));
				})
				.UntilServer(TEXT("It recovers to full"), [](FState& State) {
					return HealthOf(ServerAbilities(State, 0)) == TestVanguard().BaseStats.MaxHealth;
				})
				.UntilClient(TEXT("And its client sees it"), 0, [](FState& State) {
					const AVeyraPlayerState* Own = LocalControllerOf(State.World)->GetPlayerState<AVeyraPlayerState>();
					return HealthOf(*Own->GetAbilitySystemComponent()) == TestVanguard().BaseStats.MaxHealth;
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
