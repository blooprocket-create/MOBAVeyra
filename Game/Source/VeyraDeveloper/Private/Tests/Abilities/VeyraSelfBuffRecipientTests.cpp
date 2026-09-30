// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.SelfBuffRecipient.*: a self-buff cast at an allied Vanguard (ADR-027 §4, §9): the
	// ally named takes it, anything else named gives it to the caster, and its aura follows the ally.
	TEST_CLASS(SelfBuffRecipient, "Veyra.Abilities")
	{
		// Fixture values, independent of any Vanguard's data.
		static constexpr double CastRange = 600.0;
		static constexpr double ShieldAmount = 80.0;
		static constexpr double AuraRadius = 250.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr double Lethal = 1.0e6;
		static constexpr double GustDamage = 30.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_gust")), StatusOf(EVeyraStatusKind::MoveSpeed, 0.2, LongSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_breeze")), StatusOf(EVeyraStatusKind::MoveSpeed, 0.1, LongSeconds));

			FVeyraSelfBuffAbilityTuning Windward;
			Windward.Cast = InstantCast(CastRange, LongSeconds, 0.0);
			Windward.Recipient = EVeyraBuffRecipient::CasterOrAlly;
			Windward.Statuses = { ArchetypeTestId(TEXT("test_gust")) };
			FVeyraShieldTuning& Shield = Windward.Shields.AddDefaulted_GetRef();
			Shield.Id = ArchetypeTestId(TEXT("test_windward_shield"));
			Shield.Category = EVeyraShieldCategory::Universal;
			Shield.AmountByRank = { ShieldAmount };
			Shield.DurationSeconds = LongSeconds;
			Windward.Aura.Add(FVeyraAuraTuning{ AuraRadius, LongSeconds, LongSeconds / 2.0, { ArchetypeTestId(TEXT("test_breeze")) } });
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_windward")), Windward);

			// A buff whose zone lands on its recipient and hurts the enemies there, as ROOM TO BREATHE's push.
			FVeyraSelfBuffAbilityTuning Breather;
			Breather.Cast = InstantCast(CastRange, LongSeconds, 0.0);
			Breather.Recipient = EVeyraBuffRecipient::CasterOrAlly;
			FVeyraAreaZoneTuning& Gust = Breather.RecipientZones.AddDefaulted_GetRef();
			Gust.Shape = CircleOf(AuraRadius);
			Gust.Effects.Damage.Add(FVeyraDamageTuning{ EVeyraDamageType::TrueDamage, { GustDamage }, 0.0, 0.0 });
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_breather")), Breather);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_windward")))));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		EVeyraCastRejection CastAt(AActor* Named) const
		{
			FVeyraCastTarget Target;
			Target.Actor = Named;
			return VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::E, Target);
		}

		static double ShieldOf(const AActor& Unit)
		{
			const APawn* Pawn = Cast<APawn>(&Unit);
			const UVeyraDamageAbsorptionComponent* Absorption = Pawn && Pawn->GetPlayerState() ? Pawn->GetPlayerState()->FindComponentByClass<UVeyraDamageAbsorptionComponent>() : nullptr;
			double Total = 0.0;
			for (const FVeyraShieldEntry& Entry : Absorption ? Absorption->GetLedger().Shields : TArray<FVeyraShieldEntry>())
			{
				Total += Entry.Remaining;
			}
			return Total;
		}

		TEST_METHOD(AnAllyNamedInRangeTakesTheBuffAndTheCastEventNamesIt)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(CastRange / 2.0, 0.0, 0.0));
			TWeakObjectPtr<AActor> Named;
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnCastCommitted.AddLambda([&Named](const FVeyraCastEvent& Event) { Named = Event.Target; });
			ASSERT_THAT(IsTrue(CastAt(&Ally) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.Has(Ally, TEXT("test_gust")) && FMath::IsNearlyEqual(ShieldOf(Ally), ShieldAmount, 1e-3), TEXT("the ally takes the buff")));
			ASSERT_THAT(IsFalse(World.Has(*Caster, TEXT("test_gust")) || ShieldOf(*Caster) > 0.0, TEXT("and the caster does not")));
			ASSERT_THAT(IsTrue(Named.Get() == &Ally, TEXT("the cast event names the ally, for passives such as Slipstream")));
		}

		TEST_METHOD(NamingAnEnemyBuffsTheCaster)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CastRange / 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(CastAt(&Enemy) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.Has(*Caster, TEXT("test_gust")) && !World.Has(Enemy, TEXT("test_gust")), TEXT("the caster, as Smart Self-Cast")));
		}

		TEST_METHOD(NamingNothingBuffsTheCaster)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(CastAt(nullptr) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.Has(*Caster, TEXT("test_gust")) && FMath::IsNearlyEqual(ShieldOf(*Caster), ShieldAmount, 1e-3)));
		}

		TEST_METHOD(AnAllyOutOfRangeOrDeadRefusesTheCast)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Far = World.Spawn(EVeyraTeam::A, FVector(CastRange * 3.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(CastAt(&Far) == EVeyraCastRejection::OutOfRange));
			AVeyraVanguardCharacter& Fallen = World.Spawn(EVeyraTeam::A, FVector(0.0, CastRange / 2.0, 0.0));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(0.0, -CastRange * 3.0, 0.0));
			FVeyraRawDamageEvent Blow;
			Blow.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), *Fallen.GetAbilitySystemComponent(), Blow)));
			ASSERT_THAT(IsTrue(CastAt(&Fallen) == EVeyraCastRejection::TargetDead));
			ASSERT_THAT(IsFalse(World.Has(*Caster, TEXT("test_gust")), TEXT("a refused cast buffs no one")));
		}

		TEST_METHOD(ItsAuraFollowsTheAllyItBuffs)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(CastRange / 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& BesideAlly = World.Spawn(EVeyraTeam::A, FVector(CastRange / 2.0 + AuraRadius / 2.0, 0.0, 0.0));
			AVeyraVanguardCharacter& BesideCaster = World.Spawn(EVeyraTeam::A, FVector(-AuraRadius / 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(CastAt(&Ally) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(World.Has(BesideAlly, TEXT("test_breeze")), TEXT("around the ally")));
			ASSERT_THAT(IsFalse(World.Has(BesideCaster, TEXT("test_breeze")), TEXT("not around the caster")));
		}

		TEST_METHOD(ItsZonesLandOnTheAllyItBuffs)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Breather = World.Spawn(EVeyraTeam::A, FVector(0.0, CastRange * 3.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(Breather, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_breather")))));
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(CastRange / 2.0, CastRange * 3.0, 0.0));
			AVeyraVanguardCharacter& NearAlly = World.Spawn(EVeyraTeam::B, FVector(CastRange / 2.0 + AuraRadius / 2.0, CastRange * 3.0, 0.0));
			AVeyraVanguardCharacter& NearCaster = World.Spawn(EVeyraTeam::B, FVector(-AuraRadius / 2.0, CastRange * 3.0, 0.0));
			FVeyraCastTarget Target;
			Target.Actor = &Ally;
			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Breather.GetAbilitySystemComponent(), EVeyraAbilitySlot::W, Target) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(World.HealthLost(NearAlly), GustDamage, 1e-3), TEXT("the enemy beside the ally")));
			ASSERT_THAT(IsTrue(World.HealthLost(NearCaster) == 0.0, TEXT("not the one beside the caster")));
		}

		TEST_METHOD(ValidationKeepsAnAllysBuffFromTheCastersOwnParts)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraSelfBuffAbilityTuning& Windward = Broken.SelfBuff.FindChecked(ArchetypeTestId(TEXT("test_windward")));
			Windward.Cast.CastRange = 0.0;
			Windward.Recast = EVeyraRecast::EndsEarly;
			const TArray<FString> Problems = VeyraAbilityRules::Validate(Broken, RankCounts);
			ASSERT_THAT(IsTrue(Problems.ContainsByPredicate([](const FString& Problem) { return Problem.StartsWith(TEXT("/selfBuff/test_windward/recipient:")); }),
				FString::Join(Problems, TEXT(" | "))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
