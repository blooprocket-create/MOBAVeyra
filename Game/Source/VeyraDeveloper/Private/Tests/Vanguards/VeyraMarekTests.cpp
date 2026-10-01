// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraVitalsSet.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "CQTest.h"
#include "Delivery/VeyraEffectDelivery.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Passives/VeyraAccordPassive.h"
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

	/** Fixture values for Marek's tests: XP for many levels, a blow, where units stand, and a step of time. */
	namespace MarekFixture
	{
		constexpr double ManyLevels = 50000.0;
		constexpr double Graze = 10.0;
		constexpr double Lethal = 100000.0;
		constexpr double Near = 300.0;
		constexpr double Aside = 150.0;
		constexpr double Tolerance = 1e-3;
		constexpr float Step = 0.1f;
	}

	/** Marek, prepared from the committed tuning at many levels with every slot ranked, and Nix beside him. */
	struct FMarekTestRig
	{
		FActorTestSpawner& Spawner;
		AVeyraVanguardCharacter* Marek = nullptr;
		UVeyraAccordPassive* Accord = nullptr;

		bool Prepare()
		{
			FArchetypeTestWorld World{ Spawner };
			Marek = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			AVeyraPlayerState* Participant = Marek->GetPlayerState<AVeyraPlayerState>();
			const FVeyraPreparedVanguard Prepared = VeyraVanguards::PrepareCombatant(*Participant->GetAbilitySystemComponent(), Id(TEXT("marek")));
			if (!Prepared.bPrepared)
			{
				return false;
			}
			Participant->SetPassive(Prepared.Passive);
			Accord = Cast<UVeyraAccordPassive>(Prepared.Passive);
			UVeyraProgressionComponent* Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->AddExperience(MarekFixture::ManyLevels);
			for (const EVeyraAbilitySlot Slot : { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::W, EVeyraAbilitySlot::E, EVeyraAbilitySlot::R })
			{
				if (Progression->AllocateRank(Slot) != EVeyraRankRefusal::None)
				{
					return false;
				}
			}
			// He has grown; his companion grows with him at its next keeping.
			Keeper().Keep(Abilities());
			return Accord != nullptr && Nix() != nullptr;
		}

		static FVeyraContentId Id(const TCHAR* Text)
		{
			return FVeyraContentId::FromText(Text).GetValue();
		}

		UAbilitySystemComponent& Abilities() const
		{
			return *Marek->GetAbilitySystemComponent();
		}

		UVeyraCompanionSubsystem& Keeper() const
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraCompanionSubsystem>();
		}

		AVeyraCompanion* Nix() const
		{
			return Keeper().Find(Abilities());
		}

		const FVeyraAccordTuning& Tuning() const
		{
			return *UVeyraVanguardsTuningSubsystem::FindAccord(Id(TEXT("marek_bound_together")));
		}

		void Wait(double Seconds) const
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, MarekFixture::Step);
				if (AVeyraCompanion* Companion = Nix())
				{
					Companion->GetVeyraMovement()->AdvanceForcedMove(MarekFixture::Step);
				}
				++GFrameCounter;
				World.GetTimerManager().Tick(MarekFixture::Step);
			}
		}

		static bool Hit(UAbilitySystemComponent& Source, AActor& Target, double Amount)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			return VeyraCombat::DealDamage(Source, *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target), Damage);
		}
	};

	// Veyra.Vanguards.Accord.*: Bound Together (Roster Bible §10; ADR-034 §9), from the committed tuning.
	TEST_CLASS(Accord, "Veyra.Vanguards")
	{
		FActorTestSpawner Spawner;
		TUniquePtr<FMarekTestRig> Rig;
		TArray<FVeyraDamageDealtEvent> Procs;
		FDelegateHandle DealtHandle;

		BEFORE_EACH()
		{
			Rig = MakeUnique<FMarekTestRig>(FMarekTestRig{ Spawner });
			ASSERT_THAT(IsTrue(Rig->Prepare()));
			DealtHandle = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDamageDealt.AddLambda([this](const FVeyraDamageDealtEvent& Event) {
				if (Event.Delivery == EVeyraDamageDelivery::Proc)
				{
					Procs.Add(Event);
				}
			});
		}

		AFTER_EACH()
		{
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDamageDealt.Remove(DealtHandle);
			Rig.Reset();
		}

		TEST_METHOD(NixFormsBesideMarekAsHisCompanion)
		{
			const AVeyraCompanion* Nix = Rig->Nix();
			ASSERT_THAT(IsTrue(Nix->GetVeyraTeam() == EVeyraTeam::A && Nix->GetOwnerAbilities() == &Rig->Abilities()));
			ASSERT_THAT(IsTrue(Nix->GetDefinitionId() == FMarekTestRig::Id(TEXT("marek_nix"))));
		}

		TEST_METHOD(BothHittingOneEnemyInTheWindowSetsOffAccordOncePerItsCooldown)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(MarekFixture::Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(*Rig->Nix()->GetAbilitySystemComponent(), Enemy, MarekFixture::Graze)));
			ASSERT_THAT(IsTrue(VeyraCombat::HasStatusFrom(&Enemy, Rig->Tuning().Mark, *Rig->Nix()->GetAbilitySystemComponent()), TEXT("Nix's bite marks it")));
			ASSERT_THAT(AreEqual(0, Rig->Accord->GetAccordCount()));
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(Rig->Abilities(), Enemy, MarekFixture::Graze)));
			ASSERT_THAT(AreEqual(1, Rig->Accord->GetAccordCount(), TEXT("Accord")));
			ASSERT_THAT(IsTrue(Procs.Num() == 1 && Procs[0].Source.Get() == &Rig->Abilities() && Procs[0].Of(EVeyraDamageType::Magic) > 0.0,
				TEXT("magic damage from Marek, as a proc")));
			// Within its cooldown on that enemy, another pair of hits sets off nothing.
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(*Rig->Nix()->GetAbilitySystemComponent(), Enemy, MarekFixture::Graze)));
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(Rig->Abilities(), Enemy, MarekFixture::Graze)));
			ASSERT_THAT(AreEqual(1, Rig->Accord->GetAccordCount()));
			// Once it has passed, it may.
			Rig->Wait(Rig->Tuning().PerTargetSeconds);
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(*Rig->Nix()->GetAbilitySystemComponent(), Enemy, MarekFixture::Graze)));
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(Rig->Abilities(), Enemy, MarekFixture::Graze)));
			ASSERT_THAT(AreEqual(2, Rig->Accord->GetAccordCount()));
		}

		TEST_METHOD(HitsFartherApartThanTheWindowSetOffNothing)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Minion = World.SpawnFluxborn(EVeyraTeam::B, FVector(MarekFixture::Near, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(*Rig->Nix()->GetAbilitySystemComponent(), Minion, MarekFixture::Graze)));
			Rig->Wait(Rig->Tuning().WindowSeconds + MarekFixture::Step * 2.0);
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(Rig->Abilities(), Minion, MarekFixture::Graze)));
			ASSERT_THAT(AreEqual(0, Rig->Accord->GetAccordCount()));
		}

		TEST_METHOD(AccordCutsCrossTheChainAndStrikesHarderOnTheLeash)
		{
			// Two alike enemies, so their resistances take the same share of each Accord.
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& First = World.Spawn(EVeyraTeam::B, FVector(MarekFixture::Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Second = World.Spawn(EVeyraTeam::B, FVector(MarekFixture::Near, MarekFixture::Aside, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Rig->Marek, EVeyraAbilitySlot::E, Rig->Marek->GetActorLocation()) == EVeyraCastRejection::None));
			const UVeyraCooldownComponent* Cooldowns = Rig->Marek->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			const FVeyraContentId Cross = FMarekTestRig::Id(TEXT("marek_cross_the_chain"));
			Procs.Reset();
			const double Before = Cooldowns->GetRemainingSecondsNow(Cross);
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(*Rig->Nix()->GetAbilitySystemComponent(), First, MarekFixture::Graze)));
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(Rig->Abilities(), First, MarekFixture::Graze)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cooldowns->GetRemainingSecondsNow(Cross), Before - Rig->Tuning().RefundSeconds, MarekFixture::Tolerance),
				TEXT("Cross the Chain comes back sooner")));
			ASSERT_THAT(AreEqual(1, Procs.Num()));
			const double Plain = Procs[0].Of(EVeyraDamageType::Magic);

			const TOptional<FVeyraStatusSpec> Leashed = UVeyraAbilitiesTuningSubsystem::FindStatus(Rig->Tuning().BoostStatus);
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Rig->Abilities(), Rig->Abilities(), Leashed.GetValue())));
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(*Rig->Nix()->GetAbilitySystemComponent(), Second, MarekFixture::Graze)));
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(Rig->Abilities(), Second, MarekFixture::Graze)));
			ASSERT_THAT(AreEqual(2, Procs.Num()));
			ASSERT_THAT(IsTrue(Plain > 0.0 && FMath::IsNearlyEqual(Procs[1].Of(EVeyraDamageType::Magic) / Plain, Rig->Tuning().BoostMultiplier, MarekFixture::Tolerance),
				TEXT("on the leash it strikes harder")));
		}
	};

	// Veyra.Vanguards.BlackAccord.*: Marek's kit (Roster Bible §10; ADR-034), from the committed tuning.
	TEST_CLASS(BlackAccord, "Veyra.Vanguards")
	{
		FActorTestSpawner Spawner;
		TUniquePtr<FMarekTestRig> Rig;

		BEFORE_EACH()
		{
			Rig = MakeUnique<FMarekTestRig>(FMarekTestRig{ Spawner });
			ASSERT_THAT(IsTrue(Rig->Prepare()));
		}

		AFTER_EACH()
		{
			Rig.Reset();
		}

		static double HealthOf(const AActor& Unit)
		{
			return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit)->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		TEST_METHOD(WitchfireOnAnEnemyNixBitExplodesAroundIt)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Bitten = World.SpawnFluxborn(EVeyraTeam::B, FVector(MarekFixture::Near, 0.0, 0.0));
			AVeyraTestFluxborn& Beside = World.SpawnFluxborn(EVeyraTeam::B, FVector(MarekFixture::Near, MarekFixture::Aside, 0.0));
			const double BesideBefore = HealthOf(Beside);
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(*Rig->Nix()->GetAbilitySystemComponent(), Bitten, MarekFixture::Graze)));
			// Witchfire's hit, as its bolt lands on the bitten enemy.
			const FVeyraSkillshotAbilityTuning* Witchfire = UVeyraAbilitiesTuningSubsystem::FindSkillshot(FMarekTestRig::Id(TEXT("marek_witchfire")));
			ASSERT_THAT(IsNotNull(Witchfire));
			FVeyraEffectFrame Frame;
			Frame.Origin = Rig->Marek->GetActorLocation();
			VeyraEffectDelivery::Apply(Rig->Abilities(), Bitten, VeyraEffectDelivery::Prepare(Rig->Abilities(), Witchfire->Effects, 1), Frame,
				FVeyraAbilityHitSource{ FMarekTestRig::Id(TEXT("marek_witchfire")), 1 });
			ASSERT_THAT(IsTrue(HealthOf(Beside) < BesideBefore, TEXT("the explosion reaches the one beside it")));
			ASSERT_THAT(IsFalse(VeyraCombat::HasStatusFrom(&Bitten, Rig->Tuning().Mark, *Rig->Nix()->GetAbilitySystemComponent()), TEXT("the bite is spent")));
		}

		TEST_METHOD(HuntSendsNixToHoldThePointAndItsRecastCallsItBack)
		{
			const FVector Point(MarekFixture::Near * 2.0, 0.0, 0.0);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Rig->Marek, EVeyraAbilitySlot::W, Point) == EVeyraCastRejection::None));
			// It commits after its windup; a timer set this frame counts from the next tick.
			const FVeyraCommandAbilityTuning* Hunt = UVeyraAbilitiesTuningSubsystem::FindCommand(FMarekTestRig::Id(TEXT("marek_hunt")));
			Rig->Wait(Hunt->Cast.WindupSeconds + 3.0 * MarekFixture::Step);
			ASSERT_THAT(IsTrue(Rig->Nix()->GetMode() == EVeyraCompanionMode::Hold, TEXT("Nix holds")));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Rig->Nix()->GetHoldPoint(), Point) < MarekFixture::Aside, TEXT("the point Hunt named")));
			Rig->Wait(1.0);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Rig->Marek, EVeyraAbilitySlot::W, Rig->Marek->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Rig->Nix()->GetMode() == EVeyraCompanionMode::Follow));
		}

		TEST_METHOD(CrossTheChainSwapsMarekAndNix)
		{
			AVeyraCompanion& Nix = *Rig->Nix();
			Nix.SetActorLocation(FVector(MarekFixture::Near * 2.0, 0.0, Nix.GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			const FVector MarekFrom = Rig->Marek->GetActorLocation();
			const FVector NixFrom = Nix.GetActorLocation();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Rig->Marek, EVeyraAbilitySlot::E, MarekFrom) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Rig->Marek->GetActorLocation(), NixFrom) < MarekFixture::Aside && FVector::Dist2D(Nix.GetActorLocation(), MarekFrom) < MarekFixture::Aside));
		}

		TEST_METHOD(HellOnALeashTurnsNixToItsTrueFormChainedAndEndsWithIt)
		{
			AVeyraCompanion& Nix = *Rig->Nix();
			const double Before = Nix.GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Rig->Marek, EVeyraAbilitySlot::R, Rig->Marek->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Nix.GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) > Before && Nix.IsChained(),
				TEXT("its true form, chained to Marek")));
			// Hunt cools down faster on the leash.
			const UVeyraCooldownComponent* Cooldowns = Rig->Marek->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Rig->Marek, EVeyraAbilitySlot::W, FVector(MarekFixture::Near, 0.0, 0.0)) == EVeyraCastRejection::None));
			const FVeyraContentId Hunt = FMarekTestRig::Id(TEXT("marek_hunt"));
			const FVeyraCommandAbilityTuning* HuntTuning = UVeyraAbilitiesTuningSubsystem::FindCommand(Hunt);
			Rig->Wait(HuntTuning->Cast.WindupSeconds + 3.0 * MarekFixture::Step);
			const double Full = VeyraAbilityRules::ValueAtRank(HuntTuning->Cast.CooldownSecondsByRank, 1);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cooldowns->GetDurationSeconds(Hunt), Full * HuntTuning->Cast.CooldownWhile[0].Multiplier, MarekFixture::Tolerance),
				TEXT("a shorter Hunt")));
			// Nix killed, the ultimate ends.
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Hunter = World.Spawn(EVeyraTeam::B, FVector(MarekFixture::Near * 10.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FMarekTestRig::Hit(*Hunter.GetAbilitySystemComponent(), Nix, MarekFixture::Lethal)));
			ASSERT_THAT(IsFalse(VeyraCombat::HasStatusFrom(Rig->Marek, Rig->Tuning().BoostStatus, Rig->Abilities()), TEXT("Hell on a Leash ends")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
