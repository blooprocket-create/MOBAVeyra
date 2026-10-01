// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraVitalsSet.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "CQTest.h"
#include "Passives/VeyraAllHandsPassive.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::ArchetypeTestId;
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	/** Fixture values for All Hands: its companion stands at the origin, the fight about it. */
	namespace AllHandsFixture
	{
		constexpr double Radius = 600.0;
		constexpr double Work = 1.0;
		constexpr double Cooldown = 2.0;
		constexpr double Threshold = 3.0;
		constexpr double Repair = 40.0;
		constexpr double Lifetime = 60.0;
		constexpr double Graze = 10.0;
		constexpr double Wound = 100.0;
		constexpr double Lethal = 100000.0;
		constexpr double Near = 400.0;
		constexpr double Tolerance = 1e-3;
		constexpr float Step = 0.1f;
	}

	// Veyra.Vanguards.AllHands.*: Work its allies earn fighting beside its owner's companion, spent on repairs (ADR-037 §6).
	TEST_CLASS(AllHands, "Veyra.Vanguards")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Abilities;
		FVeyraVanguardsTuning Vanguards;
		AVeyraVanguardCharacter* Owner = nullptr;
		AVeyraVanguardCharacter* Ally = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;
		UVeyraAllHandsPassive* Passive = nullptr;

		BEFORE_EACH()
		{
			using namespace AllHandsFixture;
			Abilities.Companions.Add(ArchetypeTestId(TEXT("test_picket")), VeyraAbilitiesTests::ExampleCompanion());
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Abilities);
			Vanguards = UVeyraVanguardsTuningSubsystem::Get();
			FVeyraAllHandsTuning Hands;
			Hands.Radius = Radius;
			Hands.Work = Work;
			Hands.PerContributorSeconds = Cooldown;
			Hands.Threshold = Threshold;
			Hands.Repair = Repair;
			Vanguards.AllHands.Add(PassiveId(), Hands);
			UVeyraVanguardsTuningSubsystem::SetTestOverride(&Vanguards);

			FArchetypeTestWorld World{ Spawner };
			Owner = &World.Spawn(EVeyraTeam::A, FVector(-Near, 0.0, 0.0));
			Ally = &World.Spawn(EVeyraTeam::A, FVector(-Near, Near, 0.0));
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraPlayerState* Participant = Owner->GetPlayerState<AVeyraPlayerState>();
			Passive = NewObject<UVeyraAllHandsPassive>(Participant);
			Passive->Start(*Participant->GetAbilitySystemComponent(), PassiveId());
			Participant->SetPassive(Passive);
		}

		AFTER_EACH()
		{
			UVeyraVanguardsTuningSubsystem::SetTestOverride(nullptr);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		static FVeyraContentId PassiveId()
		{
			return ArchetypeTestId(TEXT("test_all_hands"));
		}

		UVeyraCompanionSubsystem& Keeper()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraCompanionSubsystem>();
		}

		AVeyraCompanion* Picket()
		{
			return Keeper().FindLiving(*Owner->GetAbilitySystemComponent());
		}

		bool Deploy()
		{
			return Keeper().Deploy(*Owner->GetAbilitySystemComponent(), ArchetypeTestId(TEXT("test_picket")), FVector::ZeroVector, FVector::ForwardVector,
				AllHandsFixture::Lifetime);
		}

		static bool Hit(AActor& Source, AActor& Target, double Amount)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			return VeyraCombat::DealDamage(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Source),
				*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target), Damage);
		}

		static double HealthOf(const AActor& Unit)
		{
			return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit)->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, AllHandsFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(AllHandsFixture::Step);
			}
		}

		TEST_METHOD(EachAllysHitsEarnWorkOncePerItsCooldownAndTheThresholdRepairs)
		{
			using namespace AllHandsFixture;
			ASSERT_THAT(IsTrue(Deploy()));
			ASSERT_THAT(IsTrue(Hit(*Enemy, *Picket(), Wound)));
			const double Wounded = HealthOf(*Picket());
			ASSERT_THAT(IsTrue(Hit(*Owner, *Enemy, Graze)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Passive->GetWork(), Work)));
			ASSERT_THAT(IsTrue(Hit(*Owner, *Enemy, Graze)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Passive->GetWork(), Work), TEXT("once per its cooldown")));
			ASSERT_THAT(IsTrue(Hit(*Ally, *Enemy, Graze)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Passive->GetWork(), Work * 2.0), TEXT("each contributor its own")));
			Wait(Cooldown);
			ASSERT_THAT(IsTrue(Hit(*Owner, *Enemy, Graze)));
			ASSERT_THAT(AreEqual(1, Passive->GetRepairCount()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Passive->GetWork(), 0.0), TEXT("spent at the threshold")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOf(*Picket()), Wounded + Repair, Tolerance), TEXT("its companion repaired")));
		}

		TEST_METHOD(NothingElseEarnsWork)
		{
			using namespace AllHandsFixture;
			ASSERT_THAT(IsTrue(Deploy()));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Distant = World.Spawn(EVeyraTeam::B, FVector(Radius * 2.0, 0.0, 0.0));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			ASSERT_THAT(IsTrue(Hit(*Owner, Distant, Graze)));
			ASSERT_THAT(IsTrue(Hit(*Ally, Minion, Graze)));
			ASSERT_THAT(IsTrue(Hit(*Picket(), *Enemy, Graze)));
			ASSERT_THAT(IsTrue(Passive->GetWork() == 0.0, TEXT("not far from its companion, not on a Fluxborn, not from the companion itself")));
		}

		TEST_METHOD(NoCompanionNoWork)
		{
			using namespace AllHandsFixture;
			ASSERT_THAT(IsTrue(Hit(*Owner, *Enemy, Graze)));
			ASSERT_THAT(IsTrue(Passive->GetWork() == 0.0, TEXT("none before it stands")));
			ASSERT_THAT(IsTrue(Deploy()));
			ASSERT_THAT(IsTrue(Hit(*Ally, *Enemy, Graze)));
			ASSERT_THAT(IsTrue(Passive->GetWork() > 0.0));
			ASSERT_THAT(IsTrue(Hit(*Enemy, *Picket(), Lethal)));
			ASSERT_THAT(IsNull(Picket()));
			ASSERT_THAT(IsTrue(Hit(*Owner, *Enemy, Graze)));
			ASSERT_THAT(IsTrue(Passive->GetWork() == 0.0, TEXT("what it held went with its companion")));
		}

		TEST_METHOD(AHitThatDealsNothingEarnsNoWork)
		{
			ASSERT_THAT(IsTrue(Deploy()));
			Hit(*Owner, *Enemy, 0.0);
			ASSERT_THAT(IsTrue(Passive->GetWork() == 0.0, TEXT("only damage dealt earns Work")));
		}

		TEST_METHOD(ARedeployedCompanionStartsWithoutTheOldWork)
		{
			using namespace AllHandsFixture;
			ASSERT_THAT(IsTrue(Deploy()));
			ASSERT_THAT(IsTrue(Hit(*Owner, *Enemy, Graze)));
			ASSERT_THAT(IsTrue(Hit(*Ally, *Enemy, Graze)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Passive->GetWork(), Work * 2.0)));
			ASSERT_THAT(IsTrue(Hit(*Enemy, *Picket(), Lethal)));
			ASSERT_THAT(IsTrue(Deploy()));
			Wait(Cooldown);
			ASSERT_THAT(IsTrue(Hit(*Owner, *Enemy, Graze)));
			ASSERT_THAT(AreEqual(0, Passive->GetRepairCount(), TEXT("the new companion's Work starts afresh")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Passive->GetWork(), Work)));
		}

		TEST_METHOD(ItRepairsNoFurtherThanFull)
		{
			using namespace AllHandsFixture;
			ASSERT_THAT(IsTrue(Deploy()));
			const double Full = HealthOf(*Picket());
			ASSERT_THAT(IsTrue(Hit(*Owner, *Enemy, Graze)));
			ASSERT_THAT(IsTrue(Hit(*Ally, *Enemy, Graze)));
			Wait(Cooldown);
			ASSERT_THAT(IsTrue(Hit(*Owner, *Enemy, Graze)));
			ASSERT_THAT(AreEqual(1, Passive->GetRepairCount()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(HealthOf(*Picket()), Full, Tolerance)));
		}

		TEST_METHOD(ValidationKeepsItsWorkWithinItsThreshold)
		{
			FVeyraVanguardsTuning Broken = Vanguards;
			Broken.AllHands[PassiveId()].Threshold = AllHandsFixture::Work / 2.0;
			const TArray<FString> Problems = VeyraVanguardRules::Validate(Broken, Abilities, UVeyraProgressionTuningSubsystem::Get());
			ASSERT_THAT(IsTrue(Problems.ContainsByPredicate([](const FString& Problem) { return Problem.Contains(TEXT("/allHands/")); })));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
