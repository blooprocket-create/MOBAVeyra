// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "AbilitySystemComponent.h"
#include "Feedback/VeyraCombatTextRules.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraAbilitiesTests
{
	// Veyra.Match.CombatTextRouting.*: which combat text numbers each of Combat's outcomes gives a player (ADR-052 §1).
	TEST_CLASS(CombatTextRouting, "Veyra.Match")
	{
		FActorTestSpawner Spawner;

		static UAbilitySystemComponent& UnitOf(AVeyraVanguardCharacter& Vanguard)
		{
			return *Vanguard.GetAbilitySystemComponent();
		}

		TEST_METHOD(ADamageInstanceGivesItsDealerAndItsReceiverANumberPerType)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Dealer = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Target = World.Spawn(EVeyraTeam::B, FVector(300.0, 0.0, 0.0));
			AVeyraVanguardCharacter& Bystander = World.Spawn(EVeyraTeam::A, FVector(0.0, 300.0, 0.0));
			FVeyraDamageDealtEvent Event;
			Event.Source = &UnitOf(Dealer);
			Event.Target = &UnitOf(Target);
			Event.bCritical = true;
			Event.Dealt.Add({ EVeyraDamageType::Physical, 40.0 });
			Event.Dealt.Add({ EVeyraDamageType::Magic, 15.0 });

			const TArray<FVeyraCombatTextLine> Dealt = VeyraCombatTextRouting::ForDamage(Event, UnitOf(Dealer));
			ASSERT_THAT(IsTrue(Dealt.Num() == 2, TEXT("one number per type")));
			for (const FVeyraCombatTextLine& Line : Dealt)
			{
				ASSERT_THAT(IsTrue(Line.Kind == EVeyraCombatTextKind::DamageDealt && Line.Unit.Get() == &Target && Line.Other.Get() == &Dealer && Line.bCritical));
			}
			ASSERT_THAT(IsTrue(Dealt[0].DamageType == EVeyraDamageType::Physical && Dealt[0].Amount == 40.0f));
			ASSERT_THAT(IsTrue(Dealt[1].DamageType == EVeyraDamageType::Magic && Dealt[1].Amount == 15.0f));

			const TArray<FVeyraCombatTextLine> Taken = VeyraCombatTextRouting::ForDamage(Event, UnitOf(Target));
			ASSERT_THAT(IsTrue(Taken.Num() == 2 && Taken[0].Kind == EVeyraCombatTextKind::DamageReceived && Taken[0].Unit.Get() == &Target, TEXT("at the receiver's own Vanguard")));
			ASSERT_THAT(IsTrue(VeyraCombatTextRouting::ForDamage(Event, UnitOf(Bystander)).IsEmpty(), TEXT("an ally who took no part")));
		}

		TEST_METHOD(AUnitsHealingAndShieldsShowToBothEndsAndNoOneElse)
		{
			constexpr double Restored = 25.0;
			constexpr double Added = 30.0;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Healer = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Healed = World.Spawn(EVeyraTeam::A, FVector(0.0, 300.0, 0.0));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(300.0, 0.0, 0.0));

			FVeyraHealthRestored Heal;
			Heal.Provider = &UnitOf(Healer);
			Heal.Target = &UnitOf(Healed);
			Heal.Restored = Restored;
			for (AVeyraVanguardCharacter* End : { &Healer, &Healed })
			{
				const TOptional<FVeyraCombatTextLine> Line = VeyraCombatTextRouting::ForHealing(Heal, UnitOf(*End));
				ASSERT_THAT(IsTrue(Line.IsSet() && Line->Kind == EVeyraCombatTextKind::Healing && Line->Unit.Get() == &Healed && Line->Other.Get() == &Healer
					&& Line->Amount == static_cast<float>(Restored)));
			}
			ASSERT_THAT(IsFalse(VeyraCombatTextRouting::ForHealing(Heal, UnitOf(Enemy)).IsSet()));
			Heal.Provider = nullptr;
			ASSERT_THAT(IsFalse(VeyraCombatTextRouting::ForHealing(Heal, UnitOf(Healed)).IsSet(), TEXT("regeneration heals no one in particular")));

			FVeyraShieldGranted Shield;
			Shield.Provider = &UnitOf(Healer);
			Shield.Target = &UnitOf(Healed);
			Shield.Added = Added;
			for (AVeyraVanguardCharacter* End : { &Healer, &Healed })
			{
				const TOptional<FVeyraCombatTextLine> Line = VeyraCombatTextRouting::ForShield(Shield, UnitOf(*End));
				ASSERT_THAT(IsTrue(Line.IsSet() && Line->Kind == EVeyraCombatTextKind::Shielding && Line->Unit.Get() == &Healed && Line->Amount == static_cast<float>(Added)));
			}
			ASSERT_THAT(IsFalse(VeyraCombatTextRouting::ForShield(Shield, UnitOf(Enemy)).IsSet()));
			Shield.Added = 0.0;
			ASSERT_THAT(IsFalse(VeyraCombatTextRouting::ForShield(Shield, UnitOf(Healed)).IsSet(), TEXT("a grant that added nothing")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
