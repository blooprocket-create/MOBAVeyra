// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attribution/VeyraAttributionComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "CombatState/VeyraCombatStateComponent.h"
#include "CQTest.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Life/VeyraKillCredit.h"
#include "Statuses/VeyraStatusTypes.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraOwnedUnitTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	UAbilitySystemComponent& AbilitiesOf(AActor& Unit)
	{
		return *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
	}

	bool IsFighting(const UAbilitySystemComponent& Unit)
	{
		const UVeyraCombatStateComponent* State = Unit.GetOwner()->FindComponentByClass<UVeyraCombatStateComponent>();
		return State && State->IsInCombat();
	}

	// Veyra.Combat.OwnedUnits.*: what an owned unit causes is its owner's (Combat Bible §32; ADR-034 §1),
	// and the Max Health status (ADR-034 §2).
	TEST_CLASS(OwnedUnits, "Veyra.Combat")
	{
		// Fixture values, not tuning.
		static constexpr double Lethal = 100000.0;
		static constexpr double Graze = 10.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Owner = nullptr;
		AVeyraTestOwnedUnit* Pet = nullptr;
		TArray<FVeyraDeathEvent> Deaths;
		TArray<FVeyraHostileDamageEvent> Hits;
		FDelegateHandle DeathHandle;
		FDelegateHandle HitHandle;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Owner = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Pet = &Spawner.SpawnActorAt<AVeyraTestOwnedUnit>(FVector(Graze, 0.0, 0.0), FRotator::ZeroRotator);
			Pet->SetVeyraTeam(EVeyraTeam::A);
			Pet->SetOwnerAbilities(Owner->GetAbilitySystemComponent());
			ASSERT_THAT(IsTrue(VeyraCombat::InitializeStats(AbilitiesOf(*Pet), VeyraCombatTests::ExampleStats())));
			UVeyraCombatEventSubsystem* Events = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>();
			DeathHandle = Events->OnDeath.AddLambda([this](const FVeyraDeathEvent& Event) { Deaths.Add(Event); });
			HitHandle = Events->OnHostileDamage.AddLambda([this](const FVeyraHostileDamageEvent& Event) { Hits.Add(Event); });
		}

		AFTER_EACH()
		{
			UVeyraCombatEventSubsystem* Events = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>();
			Events->OnDeath.Remove(DeathHandle);
			Events->OnHostileDamage.Remove(HitHandle);
		}

		bool Bite(AActor& Target, double Amount) const
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			Damage.Delivery = EVeyraDamageDelivery::BasicAttack;
			return VeyraCombat::DealDamage(AbilitiesOf(*Pet), AbilitiesOf(Target), Damage);
		}

		TEST_METHOD(AnOwnedUnitAnswersToItsOwnerAndAnyOtherToItself)
		{
			UAbilitySystemComponent& PetAbilities = AbilitiesOf(*Pet);
			UAbilitySystemComponent& OwnerAbilities = *Owner->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ResponsibleFor(&PetAbilities) == &OwnerAbilities));
			ASSERT_THAT(IsTrue(VeyraCombat::ResponsibleFor(&OwnerAbilities) == &OwnerAbilities));
			ASSERT_THAT(IsNull(VeyraCombat::ResponsibleFor(nullptr)));
			// A summon's summon makes no new root (§32).
			AVeyraTestOwnedUnit& Spawn = Spawner.SpawnActorAt<AVeyraTestOwnedUnit>(FVector::ZeroVector, FRotator::ZeroRotator);
			Spawn.SetOwnerAbilities(&PetAbilities);
			ASSERT_THAT(IsTrue(VeyraCombat::ResponsibleFor(&AbilitiesOf(Spawn)) == &OwnerAbilities, TEXT("followed to the root")));
			// An owner that is gone leaves the unit answering for itself.
			Pet->SetOwnerAbilities(nullptr);
			ASSERT_THAT(IsTrue(VeyraCombat::ResponsibleFor(&PetAbilities) == &PetAbilities));
		}

		TEST_METHOD(ItsHitIsItsOwnersFightAndContribution)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(Bite(Enemy, Graze)));
			ASSERT_THAT(IsTrue(IsFighting(*Owner->GetAbilitySystemComponent()) && IsFighting(*Enemy.GetAbilitySystemComponent()),
				TEXT("the owner and its enemy are in Combat State")));
			const UVeyraAttributionComponent* Attribution = Enemy.GetPlayerState()->FindComponentByClass<UVeyraAttributionComponent>();
			ASSERT_THAT(IsTrue(Attribution && Attribution->GetContributions().ContainsByPredicate([this](const FVeyraContribution& Each) {
				return Each.Contributor.Get() == Owner->GetAbilitySystemComponent(); }), TEXT("the owner contributed")));
			ASSERT_THAT(AreEqual(1, Hits.Num()));
			ASSERT_THAT(IsTrue(Hits[0].Source.Get() == &AbilitiesOf(*Pet), TEXT("the hit was the owned unit's")));
			ASSERT_THAT(IsTrue(Hits[0].Responsible.Get() == Owner->GetAbilitySystemComponent(), TEXT("and its owner answers for it")));
		}

		TEST_METHOD(ItsKillIsItsOwnersAndKillingItIsNoTakedown)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(Bite(Enemy, Lethal)));
			ASSERT_THAT(AreEqual(1, Deaths.Num()));
			ASSERT_THAT(IsTrue(Deaths[0].Killer.Get() == Owner->GetAbilitySystemComponent(), TEXT("the owner killed it")));
			ASSERT_THAT(IsTrue(Deaths[0].LethalUnit.Get() == &AbilitiesOf(*Pet), TEXT("with the owned unit's hit")));
			ASSERT_THAT(IsTrue(Deaths[0].CreditedKiller.Get() == Owner->GetAbilitySystemComponent(), TEXT("and is credited")));

			// An enemy that kills the owned unit kills no Vanguard.
			AVeyraVanguardCharacter& Hunter = World.Spawn(EVeyraTeam::B, FVector::ZeroVector);
			FVeyraRawDamageEvent Blow;
			Blow.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Hunter.GetAbilitySystemComponent(), AbilitiesOf(*Pet), Blow)));
			ASSERT_THAT(AreEqual(2, Deaths.Num()));
			ASSERT_THAT(IsTrue(Deaths[1].Victim.Get() == &AbilitiesOf(*Pet) && Deaths[1].Assisters.IsEmpty()));
			ASSERT_THAT(IsTrue(VeyraKillCredit::TakedownParticipants(Deaths[1]).IsEmpty(), TEXT("no takedown")));
		}

		TEST_METHOD(AMaxHealthStatusRaisesTheMaximumAndHealthKeepsItsShare)
		{
			UAbilitySystemComponent& Unit = AbilitiesOf(*Pet);
			const double Before = Unit.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
			FVeyraRawDamageEvent Half;
			Half.Components.Add({ EVeyraDamageType::TrueDamage, Before / 2.0 });
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), Unit, Half)));

			constexpr double Growth = 0.5;
			constexpr double Seconds = 10.0;
			FVeyraStatusSpec TrueForm;
			TrueForm.Id = FVeyraContentId::FromText(TEXT("test_true_form")).GetValue();
			TrueForm.Kind = EVeyraStatusKind::MaxHealth;
			TrueForm.Magnitude = Growth;
			TrueForm.DurationSeconds = Seconds;
			ASSERT_THAT(IsTrue(VeyraStatuses::Validate(TrueForm).IsEmpty()));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Owner->GetAbilitySystemComponent(), Unit, TrueForm)));
			const double Raised = Unit.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Raised, Before * (1.0 + Growth), Tolerance), TEXT("the maximum grows")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Unit.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()), Raised / 2.0, Tolerance),
				TEXT("Health keeps its half")));

			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(Unit, TrueForm.Id)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Unit.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()), Before, Tolerance)));

			FVeyraStatusSpec Gone = TrueForm;
			Gone.Magnitude = -1.0;
			ASSERT_THAT(IsFalse(VeyraStatuses::Validate(Gone).IsEmpty(), TEXT("no status takes the whole maximum")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
