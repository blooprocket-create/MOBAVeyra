// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Targeting/VeyraTargeting.h"
#include "CQTest.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Passives/VeyraDisciplinesPassive.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.Disciplines.*: Angeru's No Master, his stances and his ranks (Roster Bible §15;
	// ADR-031 §2, §3, §10), from the committed tuning.
	TEST_CLASS(Disciplines, "Veyra.Vanguards")
	{
		// Fixture values: where enemies stand, how long a windup takes to land, and XP for many levels.
		static constexpr double Near = 200.0;
		static constexpr double Aside = 50.0;
		static constexpr double Landed = 0.5;
		static constexpr double Spent = 100.0;
		static constexpr double ManyLevels = 50000.0;
		static constexpr double Sliver = 1.0;
		static constexpr float Step = 0.05f;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Angeru = nullptr;
		UVeyraProgressionComponent* Progression = nullptr;
		UVeyraAbilityLoadoutComponent* Loadout = nullptr;
		UVeyraCooldownComponent* Cooldowns = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Angeru = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Angeru->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("angeru")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared));
			Participant->SetPassive(Prepared.Passive);
			ASSERT_THAT(IsNotNull(Cast<UVeyraDisciplinesPassive>(Prepared.Passive)));
			Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
			Loadout = Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			Cooldowns = Participant->FindComponentByClass<UVeyraCooldownComponent>();
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		/** Levels him up and ranks Q, W and E once each. */
		void RankTheBasics()
		{
			Progression->AddExperience(ManyLevels);
			for (const EVeyraAbilitySlot Slot : { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W, EVeyraAbilitySlot::E })
			{
				ASSERT_THAT(IsTrue(Progression->AllocateRank(Slot) == EVeyraRankRefusal::None));
			}
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(Step);
			}
		}

		void Mark(AActor& Unit, const TCHAR* Status) const
		{
			const TOptional<FVeyraStatusSpec> Spec = UVeyraAbilitiesTuningSubsystem::FindStatus(Id(Status));
			VeyraCombat::ApplyStatus(*Angeru->GetAbilitySystemComponent(), *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit), Spec.GetValue());
		}

		bool Holds(const AActor& Unit, const TCHAR* Status) const
		{
			return VeyraCombat::HasStatusFrom(&Unit, Id(Status), *Angeru->GetAbilitySystemComponent());
		}

		EVeyraCastRejection CastOn(EVeyraAbilitySlot Slot, AActor& Unit) const
		{
			FVeyraCastTarget Target;
			Target.Actor = &Unit;
			Target.bHasLocation = true;
			Target.Location = Unit.GetActorLocation();
			return VeyraAbilities::TryCast(*Angeru->GetAbilitySystemComponent(), Slot, Target);
		}

		FVeyraContentId In(EVeyraAbilitySlot Slot) const
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Slot);
			return Entry ? Entry->Ability : FVeyraContentId();
		}

		TEST_METHOD(HisStanceIsLearntFromTheStartAndHisBasicsRankToSix)
		{
			ASSERT_THAT(AreEqual(1, Progression->GetRank(EVeyraAbilitySlot::R)));
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::R) == EVeyraRankRefusal::MaxRank));
			Progression->AddExperience(ManyLevels);
			for (int32 Rank = 1; Rank <= 6; ++Rank)
			{
				ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::Q) == EVeyraRankRefusal::None, FString::Printf(TEXT("rank %d"), Rank)));
			}
			ASSERT_THAT(IsTrue(Progression->AllocateRank(EVeyraAbilitySlot::Q) == EVeyraRankRefusal::MaxRank));
		}

		TEST_METHOD(BothStancesCastAndTheSwapWaitsForVeilStance)
		{
			RankTheBasics();
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Angeru, EVeyraAbilitySlot::W, FVector(0.0, Near, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::W) == Id(TEXT("angeru_false_body_swap")), TEXT("False Body offers its swap")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Angeru, EVeyraAbilitySlot::R, Angeru->GetActorLocation()) == EVeyraCastRejection::None));
			Wait(Landed);
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::Q) == Id(TEXT("angeru_flowing_cut")) && In(EVeyraAbilitySlot::W) == Id(TEXT("angeru_severing_arc"))
				&& In(EVeyraAbilitySlot::E) == Id(TEXT("angeru_passing_step")), TEXT("Blade Stance")));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, Enemy) == EVeyraCastRejection::None, TEXT("Passing Step, ranked with Black Step")));
			Wait(Cooldowns->GetRemainingSecondsNow(Id(TEXT("angeru_forsake_the_schools"))) + Landed);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Angeru, EVeyraAbilitySlot::R, Angeru->GetActorLocation()) == EVeyraCastRejection::None));
			Wait(Landed);
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::W) == Id(TEXT("angeru_false_body_swap")), TEXT("back in Veil Stance, the swap returns while the shadow stands")));
		}

		TEST_METHOD(HeChangesStanceRightAfterABlackStep)
		{
			RankTheBasics();
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, Enemy) == EVeyraCastRejection::None));
			const EVeyraCastRejection Switch = CastOn(EVeyraAbilitySlot::R, Enemy);
			ASSERT_THAT(IsTrue(Switch == EVeyraCastRejection::None, FString::Printf(TEXT("refused: %s"), LexToString(Switch))));
			Wait(Landed);
			ASSERT_THAT(IsTrue(Cooldowns->GetRemainingSecondsNow(Id(TEXT("angeru_forsake_the_schools"))) > 0.0, TEXT("it committed")));
			ASSERT_THAT(IsTrue(In(EVeyraAbilitySlot::Q) == Id(TEXT("angeru_flowing_cut")), TEXT("and he is in Blade Stance")));
		}

		TEST_METHOD(ABladeHitOnAVeiledEnemySpendsItForExecution)
		{
			RankTheBasics();
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Veiled = World.Spawn(EVeyraTeam::B, FVector(Near, Aside, 0.0));
			AVeyraVanguardCharacter& Plain = World.Spawn(EVeyraTeam::B, FVector(Near, -Aside, 0.0));
			Mark(Veiled, TEXT("angeru_veiled"));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Angeru, EVeyraAbilitySlot::R, Angeru->GetActorLocation()) == EVeyraCastRejection::None));
			Wait(Landed);
			const FVeyraContentId Forsake = Id(TEXT("angeru_forsake_the_schools"));
			const double Before = Cooldowns->GetRemainingSecondsNow(Forsake);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Angeru, EVeyraAbilitySlot::W, FVector(Near, 0.0, 0.0)) == EVeyraCastRejection::None));
			Wait(Landed);
			ASSERT_THAT(IsFalse(Holds(Veiled, TEXT("angeru_veiled")), TEXT("spent")));
			ASSERT_THAT(IsTrue(Holds(Veiled, TEXT("angeru_drawn")) && Holds(Plain, TEXT("angeru_drawn")), TEXT("Severing Arc marks both Drawn")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(Veiled) > FArchetypeTestWorld::HealthLost(Plain), TEXT("Execution adds to the strike")));
			ASSERT_THAT(IsTrue(Cooldowns->GetRemainingSecondsNow(Forsake) < Before - Landed - 0.5, TEXT("and shortens the stance's cooldown")));
		}

		TEST_METHOD(ALethalBladeHitStillSpendsVeiled)
		{
			RankTheBasics();
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Veiled = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			Mark(Veiled, TEXT("angeru_veiled"));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Angeru, EVeyraAbilitySlot::R, Angeru->GetActorLocation()) == EVeyraCastRejection::None));
			Wait(Landed);
			const FVeyraContentId Forsake = Id(TEXT("angeru_forsake_the_schools"));
			const double Before = Cooldowns->GetRemainingSecondsNow(Forsake);
			// A sliver of Health left, so Severing Arc's strike finishes it.
			UAbilitySystemComponent& Target = *Veiled.GetAbilitySystemComponent();
			FVeyraRawDamageEvent Wound;
			Wound.Components.Add({ EVeyraDamageType::TrueDamage, Target.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) - Sliver });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Angeru->GetAbilitySystemComponent(), Target, Wound)));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Angeru, EVeyraAbilitySlot::W, FVector(Near, 0.0, 0.0)) == EVeyraCastRejection::None));
			Wait(Landed);
			ASSERT_THAT(IsFalse(VeyraTargeting::IsAlive(&Veiled), TEXT("the strike was lethal")));
			ASSERT_THAT(IsTrue(Cooldowns->GetRemainingSecondsNow(Forsake) < Before - Landed - 0.5, TEXT("and still spent Veiled, shortening the stance's cooldown")));
		}

		TEST_METHOD(PassingStepNamesVanguardsFluxbornAndWildlifeOnly)
		{
			RankTheBasics();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Angeru, EVeyraAbilitySlot::R, Angeru->GetActorLocation()) == EVeyraCastRejection::None));
			Wait(Landed);
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestObjective& Well = Spawner.SpawnActorAt<AVeyraTestObjective>(FVector(Near, Aside, 0.0), FRotator::ZeroRotator);
			VeyraCombat::InitializeStats(*Well.GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats());
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, Well) == EVeyraCastRejection::InvalidTarget, TEXT("not an objective")));
			AVeyraTestFluxborn& Fluxborn = World.SpawnFluxborn(EVeyraTeam::B, FVector(Near, -Aside, 0.0));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, Fluxborn) == EVeyraCastRejection::None, TEXT("a Fluxborn, as canon says")));
		}

		TEST_METHOD(AVeilHitOnADrawnEnemySpendsItForVanish)
		{
			RankTheBasics();
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Drawn = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			Mark(Drawn, TEXT("angeru_drawn"));
			UAbilitySystemComponent& Abilities = *Angeru->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::SpendResource(Abilities, Spent)));
			const double Focus = Abilities.GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
			const double Cost = VeyraAbilities::ResourceCostOf(Abilities, Id(TEXT("angeru_black_step")));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, Drawn) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(Holds(Drawn, TEXT("angeru_drawn")), TEXT("spent")));
			ASSERT_THAT(IsTrue(Holds(Drawn, TEXT("angeru_veiled")), TEXT("Black Step marks it Veiled")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Angeru, TEXT("angeru_vanish")), TEXT("Vanish speeds him up")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Abilities.GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute()), Focus - Cost / 2.0, 0.01),
				TEXT("half the Focus comes back")));
			const FVeyraContentId BlackStep = Id(TEXT("angeru_black_step"));
			const double FullCooldown = VeyraAbilityRules::CooldownSeconds(UVeyraAbilitiesTuningSubsystem::Get(), BlackStep, 1);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cooldowns->GetRemainingSecondsNow(BlackStep), FullCooldown / 2.0, 0.01), TEXT("and half Black Step's cooldown")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
