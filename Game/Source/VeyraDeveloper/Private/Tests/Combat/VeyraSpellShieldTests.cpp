// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "AbilitySystemComponent.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Tests/Abilities/VeyraTestFluxborn.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	// Veyra.Combat.SpellShield.*: a Spell Shield blocks the next hostile ability hit whole, and only
	// that (Combat Bible §19; ADR-025 §4). The ability sites' own tests show each asks.
	TEST_CLASS(SpellShield, "Veyra.Combat")
	{
		// Fixture values.
		static constexpr double LongSeconds = 60.0;
		static constexpr double Poke = 10.0;

		FActorTestSpawner Spawner;
		TArray<FVeyraSpellShieldBlocked> Blocked;

		BEFORE_EACH()
		{
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnSpellShieldBlocked.AddLambda(
				[this](const FVeyraSpellShieldBlocked& Event) { Blocked.Add(Event); });
		}

		UAbilitySystemComponent& Unit(EVeyraTeam Team, double X)
		{
			AVeyraTestFluxborn& Body = Spawner.SpawnActorAt<AVeyraTestFluxborn>(FVector(X, 0.0, 0.0), FRotator::ZeroRotator);
			Body.SetVeyraTeam(Team);
			UAbilitySystemComponent& AbilitySystem = *Body.GetAbilitySystemComponent();
			VeyraCombat::InitializeStats(AbilitySystem, ExampleStats());
			return AbilitySystem;
		}

		static FVeyraStatusSpec Ward()
		{
			FVeyraStatusSpec Spec;
			Spec.Id = FVeyraContentId::FromText(TEXT("test_ward")).GetValue();
			Spec.Kind = EVeyraStatusKind::SpellShield;
			Spec.DurationSeconds = LongSeconds;
			return Spec;
		}

		static bool Holds(const UAbilitySystemComponent& Unit)
		{
			return Unit.GetOwner()->FindComponentByClass<UVeyraStatusComponent>()->Has(EVeyraStatusKind::SpellShield);
		}

		TEST_METHOD(OnlyAnEnemysAbilityHitConsumesItAndCombatSaysSo)
		{
			UAbilitySystemComponent& Shielded = Unit(EVeyraTeam::A, 0.0);
			UAbilitySystemComponent& Ally = Unit(EVeyraTeam::A, 200.0);
			UAbilitySystemComponent& Enemy = Unit(EVeyraTeam::B, 400.0);
			ASSERT_THAT(IsFalse(VeyraCombat::BlockAbilityHit(Shielded, Enemy), TEXT("nothing to block with")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Shielded, Shielded, Ward())));
			ASSERT_THAT(IsFalse(VeyraCombat::BlockAbilityHit(Shielded, Ally), TEXT("an ally's hit is not hostile")));
			ASSERT_THAT(IsTrue(Holds(Shielded)));
			ASSERT_THAT(IsTrue(VeyraCombat::BlockAbilityHit(Shielded, Enemy)));
			ASSERT_THAT(IsFalse(Holds(Shielded), TEXT("consumed")));
			ASSERT_THAT(IsTrue(Blocked.Num() == 1 && Blocked[0].Target.Get() == &Shielded && Blocked[0].Source.Get() == &Enemy && Blocked[0].Shield == Ward().Id));
			ASSERT_THAT(IsFalse(VeyraCombat::BlockAbilityHit(Shielded, Enemy), TEXT("one hit per shield")));
		}

		TEST_METHOD(BasicAttacksProcsAndTicksPassIt)
		{
			UAbilitySystemComponent& Shielded = Unit(EVeyraTeam::A, 0.0);
			UAbilitySystemComponent& Enemy = Unit(EVeyraTeam::B, 400.0);
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Shielded, Shielded, Ward())));
			for (const EVeyraDamageDelivery Delivery : { EVeyraDamageDelivery::BasicAttack, EVeyraDamageDelivery::Proc, EVeyraDamageDelivery::Periodic })
			{
				FVeyraRawDamageEvent Damage;
				Damage.Components.Add({ EVeyraDamageType::TrueDamage, Poke });
				Damage.Delivery = Delivery;
				ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Enemy, Shielded, Damage)));
			}
			ASSERT_THAT(IsTrue(Holds(Shielded) && Blocked.IsEmpty(), TEXT("none of them is an ability hit (Combat Bible §19)")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraCombat::GetMissingHealth(Shielded), Poke * 3.0), TEXT("and each lands")));
		}

		TEST_METHOD(AShieldHasNoMagnitude)
		{
			ASSERT_THAT(IsTrue(VeyraStatuses::Validate(Ward()).IsEmpty()));
			FVeyraStatusSpec Strong = Ward();
			Strong.Magnitude = 0.5;
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Strong).IsEmpty()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
