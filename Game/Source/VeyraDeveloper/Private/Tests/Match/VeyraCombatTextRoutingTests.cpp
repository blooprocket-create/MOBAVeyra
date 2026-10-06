// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "AbilitySystemComponent.h"
#include "Feedback/VeyraCombatTextRules.h"
#include "Feedback/VeyraKillFeedTypes.h"
#include "VeyraPlayerState.h"
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

		TEST_METHOD(AVanguardsFallIsATakedownWithItsKillerOrElseAnExecution)
		{
			// The kill feed's lines (ADR-065 §10).
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Killer = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Helper = World.Spawn(EVeyraTeam::A, FVector(0.0, 300.0, 0.0));
			AVeyraVanguardCharacter& Victim = World.Spawn(EVeyraTeam::B, FVector(300.0, 0.0, 0.0));
			FVeyraDeathEvent Death;
			Death.Victim = &UnitOf(Victim);
			Death.CreditedKiller = &UnitOf(Killer);
			Death.Assisters.Add(&UnitOf(Helper));
			const TOptional<FVeyraKillFeedLine> Takedown = VeyraKillFeedRules::LineFor(Death);
			const AVeyraPlayerState* KillerState = Killer.GetPlayerState<AVeyraPlayerState>();
			const AVeyraPlayerState* VictimState = Victim.GetPlayerState<AVeyraPlayerState>();
			ASSERT_THAT(IsTrue(Takedown.IsSet() && Takedown->Kind == EVeyraKillFeedKind::Takedown && Takedown->Assists == 1 && !Takedown->bFirstBlood));
			ASSERT_THAT(IsTrue(Takedown->KillerPlayerId == KillerState->GetPlayerId() && Takedown->KillerSide == EVeyraTeam::A));
			ASSERT_THAT(IsTrue(Takedown->VictimPlayerId == VictimState->GetPlayerId() && Takedown->VictimSide == EVeyraTeam::B));

			Death.CreditedKiller.Reset();
			const TOptional<FVeyraKillFeedLine> Execution = VeyraKillFeedRules::LineFor(Death);
			ASSERT_THAT(IsTrue(Execution.IsSet() && Execution->Kind == EVeyraKillFeedKind::Execution && Execution->KillerPlayerId == INDEX_NONE));
		}

		TEST_METHOD(GoldShowsWhereAFallEarnedItOrOverThePlayersVanguardButNeverForIncome)
		{
			AActor& Own = Spawner.SpawnActor<AActor>();
			AActor& Fallen = Spawner.SpawnActor<AActor>();
			const FVector Fell(400.0, -200.0, 0.0);
			const TOptional<FVeyraCombatTextLine> LastHit = VeyraCombatTextRouting::ForGold(21.0, EVeyraGoldReason::LastHit, FVeyraGoldSource{ &Fallen, Fell }, &Own);
			ASSERT_THAT(IsTrue(LastHit.IsSet() && LastHit->Kind == EVeyraCombatTextKind::Gold && LastHit->bFixed && LastHit->Unit.Get() == &Fallen
				&& FVector(LastHit->Where).Equals(Fell) && LastHit->Amount == 21.0f, TEXT("where the unit fell")));
			const TOptional<FVeyraCombatTextLine> Kill = VeyraCombatTextRouting::ForGold(300.0, EVeyraGoldReason::Kill, {}, &Own);
			ASSERT_THAT(IsTrue(Kill.IsSet() && !Kill->bFixed && Kill->Unit.Get() == &Own, TEXT("over the player's own Vanguard")));
			for (const EVeyraGoldReason Income : { EVeyraGoldReason::Starting, EVeyraGoldReason::Passive, EVeyraGoldReason::Sale, EVeyraGoldReason::Undo, EVeyraGoldReason::Developer })
			{
				ASSERT_THAT(IsFalse(VeyraCombatTextRouting::ForGold(300.0, Income, {}, &Own).IsSet(), LexToString(Income)));
			}
			ASSERT_THAT(IsFalse(VeyraCombatTextRouting::ForGold(300.0, EVeyraGoldReason::Kill, {}, nullptr).IsSet(), TEXT("nowhere to show")));
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

		TEST_METHOD(SelfDamageIsReceivedAndDamageToAnAllyIsNotDealt)
		{
			// Fixture value.
			constexpr double Cost = 25.0;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Self = World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(0.0, 300.0, 0.0));
			FVeyraDamageDealtEvent Event;
			Event.Source = &UnitOf(Self);
			Event.Target = &UnitOf(Self);
			Event.Dealt.Add({ EVeyraDamageType::TrueDamage, Cost });
			// Self-Damage is damage its Vanguard took (Combat Bible §47).
			const TArray<FVeyraCombatTextLine> Lines = VeyraCombatTextRouting::ForDamage(Event, UnitOf(Self));
			ASSERT_THAT(IsTrue(Lines.Num() == 1 && Lines[0].Kind == EVeyraCombatTextKind::DamageReceived && Lines[0].Unit.Get() == &Self
				&& Lines[0].Amount == static_cast<float>(Cost)));

			Event.Target = &UnitOf(Ally);
			ASSERT_THAT(IsTrue(VeyraCombatTextRouting::ForDamage(Event, UnitOf(Self)).IsEmpty(), TEXT("dealt is dealt to an enemy")));
			const TArray<FVeyraCombatTextLine> Taken = VeyraCombatTextRouting::ForDamage(Event, UnitOf(Ally));
			ASSERT_THAT(IsTrue(Taken.Num() == 1 && Taken[0].Kind == EVeyraCombatTextKind::DamageReceived, TEXT("received is whatever its Vanguard took")));
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
