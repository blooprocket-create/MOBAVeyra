// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Passives/VeyraKitStatusesPassive.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.HazardExposure.*: Mavra's Contaminated, Exposure and Unstable (Roster Bible §17;
	// ADR-026 §1–§2, §7), from the committed tuning.
	TEST_CLASS(HazardExposure, "Veyra.Vanguards")
	{
		// Fixture values: where the enemies stand.
		static constexpr double Near = 300.0;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Mavra = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Mavra = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Mavra->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("mavra")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Cast<UVeyraKitStatusesPassive>(Prepared.Passive) != nullptr));
			Participant->SetPassive(Prepared.Passive);
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		static const FVeyraAreaAbilityTuning& Area(const TCHAR* Ability)
		{
			return UVeyraAbilitiesTuningSubsystem::Get().Area.FindChecked(Id(Ability));
		}

		static int32 Exposure(const AActor& Unit)
		{
			const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			const UVeyraStatusComponent* Statuses = AbilitySystem->GetOwner()->FindComponentByClass<UVeyraStatusComponent>();
			const FVeyraStatusEntry* Entry = Statuses->GetLedger().Entries.FindByPredicate([](const FVeyraStatusEntry& Each) { return Each.Id == Id(TEXT("mavra_exposure")); });
			return Entry ? Entry->Stacks : 0;
		}

		void Hit(AActor& Unit, const FVeyraEffectBundleTuning& Bundle)
		{
			UAbilitySystemComponent& Self = *Mavra->GetAbilitySystemComponent();
			VeyraEffectDelivery::Apply(Self, Unit, VeyraEffectDelivery::Prepare(Self, Bundle, 1), FVeyraEffectFrame(), FVeyraAbilityHitSource());
		}

		int32 MostExposure() const
		{
			return UVeyraAbilitiesTuningSubsystem::Get().Statuses.FindChecked(Id(TEXT("mavra_exposure"))).MaxStacks;
		}

		TEST_METHOD(ContaminatingBuildsExposureOnVanguardsUntilTheyAreUnstable)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraTestFluxborn& Fluxborn = World.SpawnFluxborn(EVeyraTeam::B, FVector(-Near, 0.0, 0.0));
			const FVeyraEffectBundleTuning& Line = Area(TEXT("mavra_caustic_line")).Zones[0].Effects;
			Hit(Fluxborn, Line);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Fluxborn, TEXT("mavra_contaminated")) && !FArchetypeTestWorld::Has(Fluxborn, TEXT("mavra_exposure")),
				TEXT("Contaminated lands on any unit, Exposure on Vanguards only (ADR-026 §7)")));
			for (int32 Application = 1; Application < MostExposure(); ++Application)
			{
				Hit(Enemy, Line);
			}
			ASSERT_THAT(IsTrue(Exposure(Enemy) == MostExposure() - 1 && !FArchetypeTestWorld::Has(Enemy, TEXT("mavra_unstable"))));
			Hit(Enemy, Line);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Enemy, TEXT("mavra_unstable")) && Exposure(Enemy) == 0 && FArchetypeTestWorld::Has(Enemy, TEXT("mavra_contaminated"))));
		}

		TEST_METHOD(ContaminatedHitsHarderAsMavraLevels)
		{
			// Fixture value: a Level well above the first.
			constexpr int32 Higher = 5;
			UVeyraProgressionComponent* Progression = Mavra->GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>();
			const TArray<int32>& ToNext = UVeyraProgressionTuningSubsystem::Get().Experience.ToNextLevel;
			int32 Experience = 0;
			for (int32 Level = 1; Level < Higher; ++Level)
			{
				Experience += ToNext[Level - 1];
			}
			Progression->AddExperience(Experience);
			ASSERT_THAT(AreEqual(Higher, Progression->GetLevel()));
			UAbilitySystemComponent& Self = *Mavra->GetAbilitySystemComponent();
			const FVeyraPreparedEffects Line = VeyraEffectDelivery::Prepare(Self, Area(TEXT("mavra_caustic_line")).Zones[0].Effects, 1);
			const FVeyraStatusSpec* Contaminated = Line.Statuses.FindByPredicate([](const FVeyraStatusSpec& Each) { return Each.Id == Id(TEXT("mavra_contaminated")); });
			const TOptional<FVeyraStatusSpec> AtHigher = UVeyraAbilitiesTuningSubsystem::FindStatus(Id(TEXT("mavra_contaminated")), Higher);
			const TOptional<FVeyraStatusSpec> AtFirst = UVeyraAbilitiesTuningSubsystem::FindStatus(Id(TEXT("mavra_contaminated")), 1);
			ASSERT_THAT(IsTrue(Contaminated && AtHigher.IsSet() && AtFirst.IsSet() && AtHigher->Magnitude > AtFirst->Magnitude));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Contaminated->Magnitude, AtHigher->Magnitude, 1e-6),
				*FString::Printf(TEXT("ticks for %g at Level %d, as its data scales it"), Contaminated->Magnitude, Higher)));
		}

		TEST_METHOD(FlashCureStunsAnUnstableTargetAndRootsTheRest)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Plain = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Unstable = World.Spawn(EVeyraTeam::B, FVector(-Near, 0.0, 0.0));
			for (int32 Application = 0; Application < MostExposure(); ++Application)
			{
				Hit(Unstable, Area(TEXT("mavra_caustic_line")).Zones[0].Effects);
			}
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Unstable, TEXT("mavra_unstable"))));
			const FVeyraEffectBundleTuning& Cure = Area(TEXT("mavra_flash_cure")).Zones[0].Effects;
			Hit(Plain, Cure);
			Hit(Unstable, Cure);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Plain, TEXT("mavra_flash_cure_root")) && !FArchetypeTestWorld::Has(Plain, TEXT("mavra_flash_cure_stun"))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Unstable, TEXT("mavra_flash_cure_stun")) && !FArchetypeTestWorld::Has(Unstable, TEXT("mavra_flash_cure_root")),
				TEXT("the root becomes a stun (Roster Bible §17)")));
			ASSERT_THAT(IsTrue(!FArchetypeTestWorld::Has(Unstable, TEXT("mavra_unstable")) && FArchetypeTestWorld::Has(Unstable, TEXT("mavra_contaminated")) && Exposure(Unstable) == 1,
				TEXT("Unstable is spent, Contaminated goes on, and Exposure starts again (ADR-026 §7)")));
		}

		TEST_METHOD(FlashCureHardensSoonerInsideCodeBlack)
		{
			const FVeyraAreaAbilityTuning& Cure = Area(TEXT("mavra_flash_cure"));
			ASSERT_THAT(IsTrue(Cure.DelayWithin.Num() == 1 && Cure.DelayWithin[0].Ability == Id(TEXT("mavra_code_black")) && Cure.DelayWithin[0].DelaySeconds < Cure.DelaySeconds));
		}

		TEST_METHOD(PressureLeakAndCodeBlackEndInWarnedEruptions)
		{
			const FVeyraLingerTuning& Leak = Area(TEXT("mavra_pressure_leak")).Linger[0];
			ASSERT_THAT(IsTrue(Leak.EndEffects.Num() == 1 && Leak.EndEffects[0].Statuses.Contains(Id(TEXT("mavra_leak_knockup"))) && Leak.EndWarningSeconds > 0.0,
				TEXT("the enemy knows it is coming (Roster Bible §17)")));
			const FVeyraLingerTuning& CodeBlack = Area(TEXT("mavra_code_black")).Linger[0];
			ASSERT_THAT(IsTrue(CodeBlack.PulseEffects.Num() == 1 && CodeBlack.PulseEffects[0].Reactions.ContainsByPredicate([](const FVeyraReactionTuning& Each) { return Each.Status == Id(TEXT("mavra_unstable")); })));
			ASSERT_THAT(IsTrue(CodeBlack.EndEffects.Num() == 1 && CodeBlack.EndEffects[0].Displacement.Num() == 1
				&& CodeBlack.EndEffects[0].Displacement[0].Direction == EVeyraDisplacementDirection::TowardOrigin && CodeBlack.EndWarningSeconds > 0.0,
				TEXT("it collapses inward at its end")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
