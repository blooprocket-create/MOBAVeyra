// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attribution/VeyraAttributionComponent.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "CQTest.h"
#include "Effects/VeyraCombatEffects.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCombatTests
{
	/** A combatant on Side, with the example stats. */
	inline UAbilitySystemComponent& SpawnSidedCombatant(FActorTestSpawner& Spawner, EVeyraTeam Side)
	{
		UAbilitySystemComponent& Unit = SpawnCombatant(Spawner);
		CastChecked<AVeyraPlayerState>(Unit.GetOwner())->SetVeyraTeam(Side);
		VeyraCombat::InitializeStats(Unit, ExampleStats());
		return Unit;
	}

	inline FVeyraRawDamageEvent TrueDamage(double Amount)
	{
		FVeyraRawDamageEvent Damage;
		Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
		return Damage;
	}

	inline FVeyraStatusSpec TimedStatus(const TCHAR* Id, EVeyraStatusKind Kind, double Magnitude, double DurationSeconds)
	{
		FVeyraStatusSpec Spec;
		Spec.Id = FVeyraContentId::FromText(Id).GetValue();
		Spec.Kind = Kind;
		Spec.Magnitude = Magnitude;
		Spec.DurationSeconds = DurationSeconds;
		return Spec;
	}

	inline bool IsInCombat(const UAbilitySystemComponent& Unit)
	{
		return Unit.GetOwner()->FindComponentByClass<UVeyraCombatStateComponent>()->IsInCombat();
	}

	// Veyra.Combat.CombatState.*: fighting an enemy Vanguard puts both in Combat State; death takes a
	// unit out of it (Combat Bible §28).
	TEST_CLASS(CombatState, "Veyra.Combat")
	{
		static constexpr double LongSeconds = 60.0;

		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Unit = nullptr;
		UAbilitySystemComponent* Ally = nullptr;
		UAbilitySystemComponent* Enemy = nullptr;

		BEFORE_EACH()
		{
			Unit = &SpawnSidedCombatant(Spawner, EVeyraTeam::A);
			Ally = &SpawnSidedCombatant(Spawner, EVeyraTeam::A);
			Enemy = &SpawnSidedCombatant(Spawner, EVeyraTeam::B);
		}

		TEST_METHOD(DamageBetweenEnemyVanguardsPutsBothInCombat)
		{
			ASSERT_THAT(IsFalse(IsInCombat(*Unit)));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy, *Unit, TrueDamage(10.0))));
			ASSERT_THAT(IsTrue(IsInCombat(*Unit) && IsInCombat(*Enemy)));
			ASSERT_THAT(IsFalse(IsInCombat(*Ally)));
		}

		TEST_METHOD(DamageFromAnAllyOrItselfIsNoFight)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Ally, *Unit, TrueDamage(10.0))));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Unit, *Unit, TrueDamage(10.0))));
			ASSERT_THAT(IsFalse(IsInCombat(*Unit) || IsInCombat(*Ally)));
		}

		TEST_METHOD(NoDamageIsNoFight)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy, *Unit, TrueDamage(0.0))));
			ASSERT_THAT(IsFalse(IsInCombat(*Unit) || IsInCombat(*Enemy)));
		}

		TEST_METHOD(AHostileStatusPutsBothInCombat)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Unit, *Unit, TimedStatus(TEXT("haste"), EVeyraStatusKind::MoveSpeed, 0.2, LongSeconds))));
			ASSERT_THAT(IsFalse(IsInCombat(*Unit), TEXT("a unit's own buff is no fight")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Enemy, *Unit, TimedStatus(TEXT("chill"), EVeyraStatusKind::Slow, 0.2, LongSeconds))));
			ASSERT_THAT(IsTrue(IsInCombat(*Unit) && IsInCombat(*Enemy)));
		}

		TEST_METHOD(DeathEndsCombatState)
		{
			constexpr double LethalDamage = 100000.0;
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy, *Unit, TrueDamage(LethalDamage))));
			ASSERT_THAT(IsFalse(IsInCombat(*Unit)));
			ASSERT_THAT(IsTrue(IsInCombat(*Enemy), TEXT("the killer stays in combat")));
		}
	};

	// Veyra.Combat.Attribution.*: assists and takedowns (Combat Bible §18; ADR-009 §1, §3).
	TEST_CLASS(Attribution, "Veyra.Combat")
	{
		static constexpr double LethalDamage = 100000.0;
		static constexpr double LongSeconds = 60.0;

		FActorTestSpawner Spawner;
		UAbilitySystemComponent* Victim = nullptr;
		UAbilitySystemComponent* VictimAlly = nullptr;
		UAbilitySystemComponent* Killer = nullptr;
		UAbilitySystemComponent* Helper = nullptr;
		UAbilitySystemComponent* Controller = nullptr;
		TArray<FVeyraDeathEvent> Deaths;
		FDelegateHandle DeathHandle;

		BEFORE_EACH()
		{
			Victim = &SpawnSidedCombatant(Spawner, EVeyraTeam::A);
			VictimAlly = &SpawnSidedCombatant(Spawner, EVeyraTeam::A);
			Killer = &SpawnSidedCombatant(Spawner, EVeyraTeam::B);
			Helper = &SpawnSidedCombatant(Spawner, EVeyraTeam::B);
			Controller = &SpawnSidedCombatant(Spawner, EVeyraTeam::B);
			DeathHandle = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDeath.AddLambda(
				[this](const FVeyraDeathEvent& Event) { Deaths.Add(Event); });
		}

		AFTER_EACH()
		{
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDeath.Remove(DeathHandle);
		}

		static bool Names(const TArray<TWeakObjectPtr<UAbilitySystemComponent>>& Assisters, const UAbilitySystemComponent* Unit)
		{
			return Assisters.ContainsByPredicate([Unit](const TWeakObjectPtr<UAbilitySystemComponent>& Assister) { return Assister.Get() == Unit; });
		}

		/** Seconds left on Unit's status effects, as the Ability System Component counts them. */
		static TArray<float> StatusTimeRemaining(const UAbilitySystemComponent& Unit)
		{
			FGameplayEffectQuery Query;
			Query.EffectDefinition = UVeyraStatusEffect::StaticClass();
			return Unit.GetActiveEffectsTimeRemaining(Query);
		}

		TEST_METHOD(TheDeathEventNamesTheKillerAndItsAssisters)
		{
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Helper, *Victim, TrueDamage(10.0))));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Controller, *Victim, TimedStatus(TEXT("chill"), EVeyraStatusKind::Slow, 0.2, LongSeconds))));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*VictimAlly, *Victim, TrueDamage(10.0))));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Killer, *Victim, TrueDamage(LethalDamage))));

			ASSERT_THAT(AreEqual(1, Deaths.Num()));
			ASSERT_THAT(IsTrue(Deaths[0].Killer.Get() == Killer));
			ASSERT_THAT(AreEqual(2, Deaths[0].Assisters.Num()));
			ASSERT_THAT(IsTrue(Names(Deaths[0].Assisters, Helper) && Names(Deaths[0].Assisters, Controller),
				TEXT("damage and crowd control both earn assists; an ally's damage and the kill itself do not")));
		}

		TEST_METHOD(OnlyContributionsInsideTheWindowAssist)
		{
			const double Window = UVeyraCombatTuningSubsystem::Get().Attribution.AssistWindowSeconds;
			UVeyraAttributionComponent* Contributions = Victim->GetOwner()->FindComponentByClass<UVeyraAttributionComponent>();
			ASSERT_THAT(IsNotNull(Contributions));
			Contributions->Clear();
			Contributions->NoteContribution(*Helper, 0.0);
			ASSERT_THAT(AreEqual(1, Contributions->GetAssisters(Killer, Window).Num()));
			ASSERT_THAT(IsTrue(Contributions->GetAssisters(Killer, Window + 1.0).IsEmpty()));
			ASSERT_THAT(IsTrue(Contributions->GetAssisters(Helper, Window).IsEmpty(), TEXT("the killer is not its own assister")));
		}

		TEST_METHOD(TakedownsExtendStatusesUpToTheirMaximum)
		{
			constexpr double Duration = 10.0;
			constexpr double Extension = 2.0;
			constexpr double MaxExtension = 3.0;
			FVeyraStatusSpec Frenzy = TimedStatus(TEXT("frenzy"), EVeyraStatusKind::AttackSpeed, 0.5, Duration);
			Frenzy.TakedownExtensionSeconds = Extension;
			Frenzy.TakedownExtensionMaxSeconds = MaxExtension;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Killer, *Killer, Frenzy)));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Helper, *Helper, Frenzy)));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Controller, *Controller, Frenzy)));

			// The first takedown: Killer kills, Helper assists, Controller takes no part.
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Helper, *Victim, TrueDamage(10.0))));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Killer, *Victim, TrueDamage(LethalDamage))));
			const TArray<float> KillerRemaining = StatusTimeRemaining(*Killer);
			ASSERT_THAT(IsTrue(KillerRemaining.Num() == 1 && FMath::IsNearlyEqual(KillerRemaining[0], Duration + Extension, 1e-3)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StatusTimeRemaining(*Helper)[0], Duration + Extension, 1e-3)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StatusTimeRemaining(*Controller)[0], Duration, 1e-3)));

			// The second stops at the maximum.
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Killer, *VictimAlly, TrueDamage(LethalDamage))));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(StatusTimeRemaining(*Killer)[0], Duration + MaxExtension, 1e-3)));
			const FVeyraStatusEntry& Entry = Killer->GetOwner()->FindComponentByClass<UVeyraStatusComponent>()->GetLedger().Entries[0];
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Entry.EndsAt - Entry.StartedAt, Duration + MaxExtension, 1e-3)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
