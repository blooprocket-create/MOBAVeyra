// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.SelfBuffOptions.*: a self-buff's Temporary Health, its end payload scaled by the
	// hits its caster takes, and an aura's statuses on enemies (ADR-018 §6), as Patch's kit uses them.
	TEST_CLASS(SelfBuffOptions, "Veyra.Abilities")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double LongSeconds = 60.0;
		static constexpr double PayloadAfter = 1.5;
		static constexpr double PayloadRadius = 350.0;
		static constexpr double BaseSeconds = 0.5;
		static constexpr double PerHit = 0.1;
		static constexpr double MostSeconds = 0.8;
		static constexpr double Temporary = 100.0;
		static constexpr double MaxHealthShare = 0.1;
		static constexpr double Near = 200.0;
		static constexpr double Far = 600.0;
		static constexpr double Hit = 5.0;
		static constexpr float WorldStep = 0.05f;
		static constexpr double Tolerance = 1e-3;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_dormant")), StatusOf(EVeyraStatusKind::Dormant, 0.0, PayloadAfter));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_fear")), StatusOf(EVeyraStatusKind::Fear, 0.3, BaseSeconds));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_chill")), StatusOf(EVeyraStatusKind::Slow, 0.2, 1.0));

			FVeyraSelfBuffAbilityTuning Dead;
			Dead.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Dead.Statuses.Add(ArchetypeTestId(TEXT("test_dormant")));
			FVeyraEndPayloadTuning& Payload = Dead.EndPayload.AddDefaulted_GetRef();
			Payload.AfterSeconds = PayloadAfter;
			Payload.Radius = PayloadRadius;
			Payload.Status = ArchetypeTestId(TEXT("test_fear"));
			Payload.BaseSeconds = BaseSeconds;
			Payload.SecondsPerHit = PerHit;
			Payload.MaxSeconds = MostSeconds;
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_play_dead")), Dead);

			FVeyraSelfBuffAbilityTuning Giant;
			Giant.Cast = InstantCast(0.0, LongSeconds, 0.0);
			FVeyraTemporaryHealthTuning& Extra = Giant.TemporaryHealth.AddDefaulted_GetRef();
			Extra.AmountByRank = { Temporary };
			Extra.MaxHealthRatio = MaxHealthShare;
			Extra.DurationSeconds = LongSeconds;
			FVeyraAuraTuning& Aura = Giant.Aura.AddDefaulted_GetRef();
			Aura.Radius = PayloadRadius;
			Aura.DurationSeconds = LongSeconds;
			Aura.RefreshSeconds = WorldStep * 5.0;
			Aura.EnemyStatuses.Add(ArchetypeTestId(TEXT("test_chill")));
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_giant")), Giant);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		void AdvanceWorld(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, WorldStep);
				++GFrameCounter;
				World.GetTimerManager().Tick(WorldStep);
			}
		}

		static const FVeyraStatusEntry* Find(const AActor& Unit, const TCHAR* Status)
		{
			const FVeyraContentId Id = ArchetypeTestId(Status);
			const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			const UVeyraStatusComponent* Statuses = AbilitySystem ? AbilitySystem->GetOwner()->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
			return Statuses ? Statuses->GetLedger().Entries.FindByPredicate([&Id](const FVeyraStatusEntry& Entry) { return Entry.Id == Id; }) : nullptr;
		}

		static void Strike(AActor& From, AActor& Whom)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Hit });
			VeyraCombat::DealDamage(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&From), *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Whom), Damage);
		}

		TEST_METHOD(AnEndPayloadLastsLongerForEachHitTakenUpToItsMost)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Close = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Distant = World.Spawn(EVeyraTeam::B, FVector(Far, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_play_dead")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, FVector::ZeroVector) == EVeyraCastRejection::None));
			// Two hits: the Fear lasts its base and two steps.
			Strike(Close, *Caster);
			Strike(Distant, *Caster);
			AdvanceWorld(PayloadAfter + WorldStep);
			const FVeyraStatusEntry* Fear = Find(Close, TEXT("test_fear"));
			ASSERT_THAT(IsTrue(Fear != nullptr && Find(Distant, TEXT("test_fear")) == nullptr, TEXT("only the enemy within its radius")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Fear->EndsAt - Fear->StartedAt, BaseSeconds + PerHit * 2.0, Tolerance),
				FString::Printf(TEXT("feared for %g"), Fear->EndsAt - Fear->StartedAt)));
		}

		TEST_METHOD(AnEndPayloadNeverOutlastsItsMost)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Close = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_play_dead")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, FVector::ZeroVector) == EVeyraCastRejection::None));
			for (int32 Strikes = 0; Strikes < 10; ++Strikes)
			{
				Strike(Close, *Caster);
			}
			AdvanceWorld(PayloadAfter + WorldStep);
			const FVeyraStatusEntry* Fear = Find(Close, TEXT("test_fear"));
			ASSERT_THAT(IsTrue(Fear != nullptr && FMath::IsNearlyEqual(Fear->EndsAt - Fear->StartedAt, MostSeconds, Tolerance)));
		}

		TEST_METHOD(ASpellShieldBlocksAnEndPayload)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Shielded = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Open = World.Spawn(EVeyraTeam::B, FVector(0.0, Near, 0.0));
			FVeyraStatusSpec Ward;
			Ward.Id = ArchetypeTestId(TEXT("test_ward"));
			Ward.Kind = EVeyraStatusKind::SpellShield;
			Ward.DurationSeconds = LongSeconds;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*Shielded.GetAbilitySystemComponent(), *Shielded.GetAbilitySystemComponent(), Ward)));
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_play_dead")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, FVector::ZeroVector) == EVeyraCastRejection::None));
			AdvanceWorld(PayloadAfter + WorldStep);
			ASSERT_THAT(IsTrue(Find(Shielded, TEXT("test_fear")) == nullptr && Find(Shielded, TEXT("test_ward")) == nullptr,
				TEXT("the payload is a hit a Spell Shield blocks, and spends it (ADR-025 §4)")));
			ASSERT_THAT(IsTrue(Find(Open, TEXT("test_fear")) != nullptr));
		}

		TEST_METHOD(TemporaryHealthIsItsAmountAndAShareOfMaxHealth)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_giant")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::Q, FVector::ZeroVector) == EVeyraCastRejection::None));
			const UVeyraDamageAbsorptionComponent* Absorption = Caster->GetPlayerState()->FindComponentByClass<UVeyraDamageAbsorptionComponent>();
			ASSERT_THAT(IsTrue(Absorption && Absorption->GetLedger().TemporaryHealth.Num() == 1));
			const double Expected = Temporary + VeyraCombatTests::ExampleStats().MaxHealth * MaxHealthShare;
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Absorption->GetLedger().TemporaryHealth[0].Remaining, Expected, Tolerance)));
		}

		TEST_METHOD(AnAurasEnemyStatusesLandOnEnemiesInRangeOnly)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Close = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Distant = World.Spawn(EVeyraTeam::B, FVector(Far, 0.0, 0.0));
			AVeyraVanguardCharacter& Friend = World.Spawn(EVeyraTeam::A, FVector(0.0, Near, 0.0));
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(0.0, -Near, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_giant")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::Q, FVector::ZeroVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Close, TEXT("test_chill")) && FArchetypeTestWorld::Has(Minion, TEXT("test_chill"))));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(Distant, TEXT("test_chill")) || FArchetypeTestWorld::Has(Friend, TEXT("test_chill"))));
		}

		TEST_METHOD(AnAuraReachesWhoComesNearAfterItsCast)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Latecomer = World.Spawn(EVeyraTeam::B, FVector(Far, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_giant")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::Q, FVector::ZeroVector) == EVeyraCastRejection::None));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(Latecomer, TEXT("test_chill"))));
			// The cast has ended; the aura has not.
			Latecomer.SetActorLocation(FVector(Near, 0.0, Latecomer.GetActorLocation().Z));
			AdvanceWorld(WorldStep * 10.0);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Latecomer, TEXT("test_chill")), TEXT("it keeps refreshing after its cast")));
		}

		TEST_METHOD(APayloadThatNeedsHitsComesOnlyAfterThem)
		{
			Tuning.SelfBuff.FindChecked(ArchetypeTestId(TEXT("test_play_dead"))).EndPayload[0].MinHits = 1;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Close = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_play_dead")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, FVector::ZeroVector) == EVeyraCastRejection::None));
			AdvanceWorld(PayloadAfter + WorldStep);
			ASSERT_THAT(IsTrue(Find(Close, TEXT("test_fear")) == nullptr, TEXT("no hit, no counter")));
		}

		TEST_METHOD(ValidationKeepsTheOptionsInShape)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, RankCounts).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, RankCounts), TEXT(" | "))));
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.SelfBuff.FindChecked(ArchetypeTestId(TEXT("test_play_dead"))).EndPayload[0].MaxSeconds = BaseSeconds / 2.0;
			Broken.SelfBuff.FindChecked(ArchetypeTestId(TEXT("test_giant"))).TemporaryHealth[0].DurationSeconds = 0.0;
			Broken.SelfBuff.FindChecked(ArchetypeTestId(TEXT("test_giant"))).Aura[0].EnemyStatuses.Add(ArchetypeTestId(TEXT("no_such_status")));
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/selfBuff/test_play_dead/endPayload/0:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/selfBuff/test_giant/temporaryHealth/0:")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("no_such_status")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
