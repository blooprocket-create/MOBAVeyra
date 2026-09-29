// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "CQTest.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.CamouflageEnds.*: attacking or an offensive cast ends Camouflage; taking damage
	// and acting on oneself do not (Combat Bible §11; ADR-018 §4).
	TEST_CLASS(CamouflageEnds, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double Reach = 300.0;
		static constexpr double Detection = 400.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double SmiteDamage = 50.0;
		static constexpr double Hit = 10.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;
		UVeyraAbilityLoadoutComponent* Loadout = nullptr;

		static FVeyraSelfBuffAbilityTuning SelfBuffApplying(const TCHAR* Status)
		{
			FVeyraSelfBuffAbilityTuning Buff;
			Buff.Cast = InstantCast(0.0, 0.0, 0.0);
			Buff.Statuses.Add(ArchetypeTestId(Status));
			return Buff;
		}

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_veil")), StatusOf(EVeyraStatusKind::Camouflage, Detection, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_haste")), StatusOf(EVeyraStatusKind::MoveSpeed, 0.2, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_slow")), StatusOf(EVeyraStatusKind::Slow, 0.3, LongSeconds));
			// Now You See Me!: a self-buff that hides its caster.
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_vanish")), SelfBuffApplying(TEXT("test_veil")));
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_self_haste")), SelfBuffApplying(TEXT("test_haste")));

			FVeyraAreaAbilityTuning Blast;
			Blast.Cast = InstantCast(0.0, 0.0, 0.0);
			Blast.Origin = EVeyraAreaOrigin::Caster;
			FVeyraAreaZoneTuning& Zone = Blast.Zones.AddDefaulted_GetRef();
			Zone.Shape = CircleOf(Reach);
			Zone.Effects.Statuses.Add(ArchetypeTestId(TEXT("test_slow")));
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_blast")), Blast);

			FVeyraTargetedDamageAbilityTuning Smite;
			Smite.CastRange = Reach;
			Smite.DamageType = EVeyraDamageType::TrueDamage;
			Smite.DamageAmount = SmiteDamage;
			Tuning.TargetedDamage.Add(ArchetypeTestId(TEXT("test_smite")), Smite);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Reach / 2.0, 0.0, 0.0));
			Loadout = Caster->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_vanish")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::Q, FVector::ZeroVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Hidden(), TEXT("its own cast hides it")));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		bool Hidden() const
		{
			return FArchetypeTestWorld::Has(*Caster, TEXT("test_veil"));
		}

		/** Q holds Ability for a while, at Q's rank. */
		bool Hold(const TCHAR* Ability) const
		{
			FVeyraOverrideSpec Spec;
			Spec.Ability = ArchetypeTestId(Ability);
			Spec.Use = EVeyraOverrideUse::WhileActive;
			return Loadout->Override(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::Q, Spec);
		}

		TEST_METHOD(AnOffensiveCastEndsItAndASelfBuffDoesNot)
		{
			ASSERT_THAT(IsTrue(Hold(TEXT("test_self_haste"))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, FVector::ZeroVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Hidden(), TEXT("a self-buff acts on its caster alone")));
			ASSERT_THAT(IsTrue(Hold(TEXT("test_blast"))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, FVector::ZeroVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(Hidden()));
		}

		TEST_METHOD(ATargetedCastEndsItAndIsAnnounced)
		{
			TArray<FVeyraCastEvent> Committed;
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnCastCommitted.AddLambda([&Committed](const FVeyraCastEvent& Event) { Committed.Add(Event); });
			ASSERT_THAT(IsTrue(Hold(TEXT("test_smite"))));
			FVeyraCastTarget Target;
			Target.Actor = Enemy;
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::Q, Target) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(Hidden()));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) == SmiteDamage));
			ASSERT_THAT(IsTrue(Committed.Num() == 1 && Committed[0].Ability == ArchetypeTestId(TEXT("test_smite")) && Committed[0].bOffensive));
		}

		TEST_METHOD(AnAttackEndsIt)
		{
			FVeyraBasicAttackProfile Profile;
			Profile.Range = Reach;
			Profile.DamageType = EVeyraDamageType::TrueDamage;
			Profile.PhysicalPowerRatio = 1.0;
			Profile.WindupFraction = 0.25;
			Profile.AcquisitionRadius = Reach;
			UVeyraBasicAttackComponent* Attacks = Caster->GetPlayerState()->FindComponentByClass<UVeyraBasicAttackComponent>();
			ASSERT_THAT(IsTrue(Attacks && Attacks->SetProfile(Profile)));
			ASSERT_THAT(IsTrue(Attacks->StartAttack(*Enemy) == EVeyraAttackRejection::None));
			ASSERT_THAT(IsFalse(Hidden(), TEXT("as the attack begins")));
		}

		TEST_METHOD(TakingDamageDoesNotEndIt)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Hit });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy->GetAbilitySystemComponent(), *Caster->GetAbilitySystemComponent(), Damage)));
			ASSERT_THAT(IsTrue(Hidden()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
