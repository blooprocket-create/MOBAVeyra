// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Passives/VeyraKitStatusesPassive.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraVanguards.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraVanguardsTests
{
	using VeyraAbilitiesTests::FArchetypeTestWorld;

	// Veyra.Vanguards.Embedded.*: Korruk's Splinters and Fractured (Roster Bible §8; ADR-026 §1–§2),
	// from the committed tuning.
	TEST_CLASS(Embedded, "Veyra.Vanguards")
	{
		// Fixture values: where the enemies stand.
		static constexpr double Near = 300.0;

		FActorTestSpawner Spawner;
		AVeyraVanguardCharacter* Korruk = nullptr;

		BEFORE_EACH()
		{
			FArchetypeTestWorld World{ Spawner };
			Korruk = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Korruk->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("korruk")));
			ASSERT_THAT(IsTrue(Prepared.bPrepared && Cast<UVeyraKitStatusesPassive>(Prepared.Passive) != nullptr));
			Participant->SetPassive(Prepared.Passive);
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		UAbilitySystemComponent& Self() const
		{
			return *Korruk->GetAbilitySystemComponent();
		}

		static const FVeyraStatusTuning& Splinter()
		{
			return UVeyraAbilitiesTuningSubsystem::Get().Statuses.FindChecked(Id(TEXT("korruk_splinter")));
		}

		static const FVeyraEffectBundleTuning& Effects(const TCHAR* Ability, int32 Zone = 0)
		{
			return UVeyraAbilitiesTuningSubsystem::Get().Area.FindChecked(Id(Ability)).Zones[Zone].Effects;
		}

		static int32 Splinters(const AActor& Unit)
		{
			const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			const UVeyraStatusComponent* Statuses = AbilitySystem->GetOwner()->FindComponentByClass<UVeyraStatusComponent>();
			const FVeyraStatusEntry* Entry = Statuses->GetLedger().Entries.FindByPredicate([](const FVeyraStatusEntry& Each) { return Each.Id == Id(TEXT("korruk_splinter")); });
			return Entry ? Entry->Stacks : 0;
		}

		void Embed(AActor& Unit, int32 Times)
		{
			const TOptional<FVeyraStatusSpec> Spec = UVeyraAbilitiesTuningSubsystem::FindStatus(Id(TEXT("korruk_splinter")));
			for (int32 Index = 0; Index < Times; ++Index)
			{
				VeyraCombat::ApplyStatus(Self(), *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit), Spec.GetValue());
			}
		}

		void Hit(AActor& Unit, const FVeyraEffectBundleTuning& Bundle)
		{
			VeyraEffectDelivery::Apply(Self(), Unit, VeyraEffectDelivery::Prepare(Self(), Bundle, 1), FVeyraEffectFrame(), FVeyraAbilityHitSource());
		}

		TEST_METHOD(SplintersEmbedOnlyInVanguardsAndAtTheMostBecomeFractured)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraTestFluxborn& Fluxborn = World.SpawnFluxborn(EVeyraTeam::B, FVector(-Near, 0.0, 0.0));
			Embed(Fluxborn, 1);
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(Fluxborn, TEXT("korruk_splinter")), TEXT("only enemy Vanguards (Roster Bible §8)")));
			Embed(Enemy, Splinter().MaxStacks - 1);
			ASSERT_THAT(AreEqual(Splinter().MaxStacks - 1, Splinters(Enemy)));
			Embed(Enemy, 1);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Enemy, TEXT("korruk_fractured")) && Splinters(Enemy) == 0, TEXT("at the most Splinters, Fractured")));
		}

		TEST_METHOD(RuptureRipsOutSplintersForMoreAndFracturedForMost)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Clean = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Splintered = World.Spawn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			AVeyraVanguardCharacter& Fractured = World.Spawn(EVeyraTeam::B, FVector(-Near, 0.0, 0.0));
			Embed(Splintered, 2);
			Embed(Fractured, Splinter().MaxStacks);
			const FVeyraEffectBundleTuning& Rupture = Effects(TEXT("korruk_rupture"));
			Hit(Clean, Rupture);
			Hit(Splintered, Rupture);
			Hit(Fractured, Rupture);
			const double Low = FArchetypeTestWorld::HealthLost(Clean);
			const double Middle = FArchetypeTestWorld::HealthLost(Splintered);
			const double High = FArchetypeTestWorld::HealthLost(Fractured);
			ASSERT_THAT(IsTrue(Low > 0.0 && Middle > Low && High > Middle, *FString::Printf(TEXT("%g, %g, %g"), Low, Middle, High)));
			ASSERT_THAT(IsTrue(Splinters(Splintered) == 0 && !FArchetypeTestWorld::Has(Fractured, TEXT("korruk_fractured")), TEXT("it spends what it detonates")));
		}

		TEST_METHOD(PressureMineKnocksUpAndDetonatesAFracturedTarget)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Plain = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Fractured = World.Spawn(EVeyraTeam::B, FVector(-Near, 0.0, 0.0));
			Embed(Fractured, Splinter().MaxStacks);
			const FVeyraEffectBundleTuning& Mine = Effects(TEXT("korruk_pressure_mine"));
			Hit(Plain, Mine);
			Hit(Fractured, Mine);
			ASSERT_THAT(IsTrue(Splinters(Plain) == 1 && FArchetypeTestWorld::Has(Plain, TEXT("korruk_mine_slow")) && !FArchetypeTestWorld::Has(Plain, TEXT("korruk_mine_knockup"))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Fractured, TEXT("korruk_mine_knockup")) && !FArchetypeTestWorld::Has(Fractured, TEXT("korruk_fractured"))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(Fractured) > FArchetypeTestWorld::HealthLost(Plain)));
		}

		TEST_METHOD(SpineburstsCentreEmbedsMore)
		{
			const FVeyraAreaAbilityTuning& Spineburst = UVeyraAbilitiesTuningSubsystem::Get().Area.FindChecked(Id(TEXT("korruk_spineburst")));
			const auto Count = [](const FVeyraAreaZoneTuning& Zone) { return Zone.Effects.Statuses.FilterByPredicate([](const FVeyraContentId& Each) { return Each == Id(TEXT("korruk_splinter")); }).Num(); };
			ASSERT_THAT(IsTrue(Spineburst.Zones.Num() == 2 && Count(Spineburst.Zones[0]) > Count(Spineburst.Zones[1]) && Count(Spineburst.Zones[1]) > 0));
		}

		TEST_METHOD(ShatterfieldsWavesEmbedAndItsLastDetonates)
		{
			const FVeyraLingerTuning& Field = UVeyraAbilitiesTuningSubsystem::Get().Area.FindChecked(Id(TEXT("korruk_shatterfield"))).Linger[0];
			ASSERT_THAT(IsTrue(Field.PulseEffects.Num() == 1 && Field.PulseEffects[0].Statuses.Contains(Id(TEXT("korruk_splinter")))));
			ASSERT_THAT(IsTrue(Field.EndEffects.Num() == 1 && Field.EndEffects[0].Reactions.ContainsByPredicate([](const FVeyraReactionTuning& Each) {
				return Each.Status == Id(TEXT("korruk_fractured")) && Each.Consume == EVeyraReactionConsume::Consume;
			}) && Field.EndWarningSeconds > 0.0, TEXT("the end is readable (Roster Bible §8)")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
