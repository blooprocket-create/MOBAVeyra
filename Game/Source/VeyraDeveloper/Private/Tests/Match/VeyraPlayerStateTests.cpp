// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CQTest.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraPlayerState.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraMatchTests
{
	// Veyra.Match.PlayerStateCombatant.*: the PlayerState owns each Vanguard's Ability System
	// Component, in Mixed replication, with the combat Attribute Sets (ADR-006 §4).
	TEST_CLASS(PlayerStateCombatant, "Veyra.Match")
	{
		FActorTestSpawner Spawner;

		TEST_METHOD(OwnsAMixedModeAbilitySystem)
		{
			AVeyraPlayerState& PlayerState = Spawner.SpawnActor<AVeyraPlayerState>();
			UAbilitySystemComponent* AbilitySystem = PlayerState.GetAbilitySystemComponent();
			ASSERT_THAT(IsNotNull(AbilitySystem));
			ASSERT_THAT(IsTrue(AbilitySystem->GetOwner() == &PlayerState));
			ASSERT_THAT(IsTrue(AbilitySystem->GetIsReplicated()));
			ASSERT_THAT(IsTrue(AbilitySystem->ReplicationMode == EGameplayEffectReplicationMode::Mixed));

			const IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(&PlayerState);
			ASSERT_THAT(IsTrue(Interface && Interface->GetAbilitySystemComponent() == AbilitySystem));
		}

		TEST_METHOD(RegistersTheCombatAttributeSets)
		{
			const UAbilitySystemComponent* AbilitySystem = Spawner.SpawnActor<AVeyraPlayerState>().GetAbilitySystemComponent();
			ASSERT_THAT(IsNotNull(AbilitySystem->GetSet<UVeyraVitalsSet>()));
			ASSERT_THAT(IsNotNull(AbilitySystem->GetSet<UVeyraOffenceSet>()));
			ASSERT_THAT(IsNotNull(AbilitySystem->GetSet<UVeyraDefenceSet>()));
		}

		TEST_METHOD(HasADamageAbsorptionComponent)
		{
			AVeyraPlayerState& PlayerState = Spawner.SpawnActor<AVeyraPlayerState>();
			ASSERT_THAT(IsNotNull(PlayerState.FindComponentByClass<UVeyraDamageAbsorptionComponent>()));
		}

		TEST_METHOD(EnforcesTheModifierPolicy)
		{
			UAbilitySystemComponent* AbilitySystem = Spawner.SpawnActor<AVeyraPlayerState>().GetAbilitySystemComponent();
			TestRunner->AddExpectedMessagePlain(TEXT("cannot use"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 1);
			const FActiveGameplayEffectHandle Handle = VeyraCombatTests::ApplyToSelf(*AbilitySystem, VeyraCombatTests::NewTestEffect(
				EGameplayEffectDurationType::Infinite, { { UVeyraDefenceSet::GetArmorAttribute(), EGameplayModOp::Override, 10.0f } }));
			ASSERT_THAT(IsFalse(Handle.WasSuccessfullyApplied()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
