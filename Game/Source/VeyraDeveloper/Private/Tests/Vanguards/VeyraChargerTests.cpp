// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraResourceSet.h"
#include "CQTest.h"
#include "Passives/VeyraChargerPassive.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.Charger.*: a kept resource, as Charge, and the passive that fills it from nearby
	// Fluxborn deaths (Roster Bible §4; ADR-033 §1, §2), from the committed tuning.
	TEST_CLASS(Charger, "Veyra.Vanguards")
	{
		// Fixture values: a lethal blow, a little resource to restore, and how much farther than the radius is far.
		static constexpr double Lethal = 100000.0;
		static constexpr double Some = 30.0;
		static constexpr double Beyond = 500.0;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Relay = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Relay = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Relay->GetPlayerState<AVeyraPlayerState>();
			ASSERT_THAT(IsNotNull(UVeyraVanguardsTuningSubsystem::FindCharger(PassiveId())));
			ASSERT_THAT(IsTrue(VeyraCombat::KeepResource(*Participant->GetAbilitySystemComponent())));
			UVeyraChargerPassive* Passive = NewObject<UVeyraChargerPassive>(Participant);
			Passive->Start(*Participant->GetAbilitySystemComponent(), PassiveId());
			Participant->SetPassive(Passive);
		}

		static FVeyraContentId PassiveId()
		{
			return FVeyraContentId::FromText(TEXT("relay_charger")).GetValue();
		}

		static const FVeyraChargerTuning& Tuning()
		{
			return *UVeyraVanguardsTuningSubsystem::FindCharger(PassiveId());
		}

		double Charge() const
		{
			return Relay->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute());
		}

		void Kill(AActor& Unit) const
		{
			FVeyraRawDamageEvent Blow;
			Blow.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
			VeyraCombat::DealDamage(*Relay->GetAbilitySystemComponent(), *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit), Blow);
		}

		TEST_METHOD(AKeptResourceStartsEmptyAndRevivalLeavesItBe)
		{
			UAbilitySystemComponent& Abilities = *Relay->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::IsResourceKept(Abilities)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(Charge()), TEXT("it starts empty")));
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreResource(Abilities, Some)));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Beyond, 0.0, 0.0));
			FVeyraRawDamageEvent Blow;
			Blow.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), Abilities, Blow)));
			ASSERT_THAT(IsTrue(VeyraCombat::Revive(Abilities)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Charge(), Some, Tolerance), TEXT("revival keeps what it held")));
			ASSERT_THAT(IsFalse(VeyraCombat::IsResourceKept(*Enemy.GetAbilitySystemComponent()), TEXT("an ordinary resource is not kept")));
		}

		TEST_METHOD(GrowingLeavesAKeptResourceAsItIs)
		{
			UAbilitySystemComponent& Abilities = *Relay->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::RestoreResource(Abilities, Some)));
			FVeyraStatBlock Growth;
			Growth.MaxResource = Some;
			ASSERT_THAT(IsTrue(VeyraCombat::GrowBaseStats(Abilities, Growth)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Charge(), Some, Tolerance), TEXT("its most grows, and what it holds does not")));
		}

		TEST_METHOD(ANearbyFluxbornDeathOfEitherSideGivesCharge)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Enemy = World.SpawnFluxborn(EVeyraTeam::B, FVector(Tuning().Radius / 2.0, 0.0, 0.0));
			AVeyraTestFluxborn& Ally = World.SpawnFluxborn(EVeyraTeam::A, FVector(0.0, Tuning().Radius / 2.0, 0.0));
			Kill(Enemy);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Charge(), Tuning().ChargePerDeath, Tolerance), TEXT("an enemy Fluxborn")));
			// An enemy Vanguard far off finishes the allied one.
			AVeyraVanguardCharacter& Killer = World.Spawn(EVeyraTeam::B, FVector(-(Tuning().Radius + Beyond), 0.0, 0.0));
			FVeyraRawDamageEvent Blow;
			Blow.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Killer.GetAbilitySystemComponent(), *Ally.GetAbilitySystemComponent(), Blow)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Charge(), 2.0 * Tuning().ChargePerDeath, Tolerance), TEXT("and an allied one")));
		}

		TEST_METHOD(AFarDeathOrWildlifeGivesNone)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Far = World.SpawnFluxborn(EVeyraTeam::B, FVector(Tuning().Radius + Beyond, 0.0, 0.0));
			AVeyraTestWildlife& Beast = Spawner.SpawnActorAt<AVeyraTestWildlife>(FVector(0.0, Tuning().Radius / 2.0, 0.0), FRotator::ZeroRotator);
			VeyraCombat::InitializeStats(*Beast.GetAbilitySystemComponent(), VeyraCombatTests::ExampleStats());
			Kill(Far);
			Kill(Beast);
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(Charge())));
		}

		TEST_METHOD(TheBoostMultipliesWhatEachDeathGives)
		{
			const TOptional<FVeyraStatusSpec> Boost = UVeyraAbilitiesTuningSubsystem::FindStatus(Tuning().BoostStatus);
			ASSERT_THAT(IsTrue(Boost.IsSet()));
			VeyraCombat::ApplyStatus(*Relay->GetAbilitySystemComponent(), *Relay->GetAbilitySystemComponent(), Boost.GetValue());
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Enemy = World.SpawnFluxborn(EVeyraTeam::B, FVector(Tuning().Radius / 2.0, 0.0, 0.0));
			Kill(Enemy);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Charge(), Tuning().ChargePerDeath * Tuning().BoostMultiplier, Tolerance)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
