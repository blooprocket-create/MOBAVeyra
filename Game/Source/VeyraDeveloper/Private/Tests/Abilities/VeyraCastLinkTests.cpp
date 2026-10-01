// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Cooldowns/VeyraCooldownComponent.h"
#include "CQTest.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for linked casts: two cooldowns, a long status and many levels. */
	namespace CastLinkFixture
	{
		constexpr double CalmCooldown = 8.0;
		constexpr double StormCooldown = 6.0;
		constexpr double Lasting = 10.0;
		constexpr double ManyLevels = 5000.0;
		constexpr double Tolerance = 0.05;
	}

	// Veyra.Abilities.CastLinks.*: a cast that shares another ability's cooldown, and one a status holds back
	// (ADR-035 §1, §2).
	TEST_CLASS(CastLinks, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			using namespace CastLinkFixture;
			FVeyraSelfBuffAbilityTuning Calm;
			Calm.Cast = InstantCast(0.0, CalmCooldown, 0.0);
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_calm")), Calm);
			FVeyraSelfBuffAbilityTuning Storm;
			Storm.Cast = InstantCast(0.0, StormCooldown, 0.0);
			Storm.Cast.CooldownOf = { ArchetypeTestId(TEXT("test_calm")) };
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_storm")), Storm);
			FVeyraSelfBuffAbilityTuning Weather;
			Weather.Cast = InstantCast(0.0, 0.0, 0.0);
			Weather.Cast.RefusedWhile = { ArchetypeTestId(TEXT("test_lock")) };
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_weather")), Weather);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_lock")), StatusOf(EVeyraStatusKind::Counter, 0.0, Lasting));
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			APlayerState* PlayerState = Caster->GetPlayerState();
			UVeyraProgressionComponent* Progression = PlayerState->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->Initialize(FVeyraStatGrowth(), 0.0);
			Progression->AddExperience(ManyLevels);
			UVeyraAbilityLoadoutComponent* Loadout = PlayerState->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			const TPair<EVeyraAbilitySlot, const TCHAR*> Kit[] = { { EVeyraAbilitySlot::Q, TEXT("test_calm") }, { EVeyraAbilitySlot::W, TEXT("test_storm") },
				{ EVeyraAbilitySlot::E, TEXT("test_weather") } };
			for (const TPair<EVeyraAbilitySlot, const TCHAR*>& Entry : Kit)
			{
				ASSERT_THAT(IsTrue(Loadout->Grant(*Caster->GetAbilitySystemComponent(), Entry.Key, ArchetypeTestId(Entry.Value))));
				ASSERT_THAT(IsTrue(Progression->AllocateRank(Entry.Key) == EVeyraRankRefusal::None));
			}
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		EVeyraCastRejection CastIn(EVeyraAbilitySlot Slot) const
		{
			return FArchetypeTestWorld::CastAt(*Caster, Slot, Caster->GetActorLocation());
		}

		TEST_METHOD(ATwinCastSharesItsCooldown)
		{
			const UVeyraCooldownComponent& Cooldowns = *Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			const UVeyraAbilityLoadoutComponent& Loadout = *Caster->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			ASSERT_THAT(IsTrue(Loadout.CooldownIdOf(ArchetypeTestId(TEXT("test_storm"))) == ArchetypeTestId(TEXT("test_calm")), TEXT("held under the other's ID")));
			ASSERT_THAT(IsTrue(CastIn(EVeyraAbilitySlot::W) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cooldowns.GetRemainingSecondsNow(ArchetypeTestId(TEXT("test_calm"))), CastLinkFixture::StormCooldown,
				CastLinkFixture::Tolerance), TEXT("at the length of the ability cast")));
			ASSERT_THAT(IsTrue(CastIn(EVeyraAbilitySlot::Q) == EVeyraCastRejection::OnCooldown, TEXT("so its twin waits too")));
		}

		TEST_METHOD(AStatusItsCasterHoldsHoldsItBack)
		{
			UAbilitySystemComponent& Abilities = *Caster->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Abilities, Abilities, UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_lock"))).GetValue())));
			ASSERT_THAT(IsTrue(CastIn(EVeyraAbilitySlot::E) == EVeyraCastRejection::HeldBack, TEXT("held back while it holds the lock")));
			ASSERT_THAT(IsTrue(VeyraCombat::RemoveStatus(Abilities, ArchetypeTestId(TEXT("test_lock")))));
			ASSERT_THAT(IsTrue(CastIn(EVeyraAbilitySlot::E) == EVeyraCastRejection::None, TEXT("and free once it ends")));
		}

		TEST_METHOD(ValidationKeepsLinksToOneStepAndRealStatuses)
		{
			const int32 Ranks[] = { 5, 3 };
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.SelfBuff[ArchetypeTestId(TEXT("test_calm"))].Cast.CooldownOf = { ArchetypeTestId(TEXT("test_weather")) };
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty(), TEXT("a link to an ability that shares another's is a chain")));
			Broken = Tuning;
			Broken.SelfBuff[ArchetypeTestId(TEXT("test_storm"))].Cast.CooldownOf = { ArchetypeTestId(TEXT("test_missing")) };
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty(), TEXT("a link to nothing")));
			Broken = Tuning;
			Broken.SelfBuff[ArchetypeTestId(TEXT("test_weather"))].Cast.RefusedWhile = { ArchetypeTestId(TEXT("test_missing")) };
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty(), TEXT("a status /statuses does not define")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
