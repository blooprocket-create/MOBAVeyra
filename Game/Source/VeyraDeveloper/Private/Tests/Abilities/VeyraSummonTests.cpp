// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionController.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "CQTest.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	/** Fixture values for summoned companions, beside CompanionFixture's. */
	namespace SummonFixture
	{
		constexpr double Lifetime = 6.0;
		constexpr double Pulse = 1.0;
		constexpr double Heal = 25.0;
		constexpr double CastRange = 800.0;
		constexpr double Injury = 100.0;
		constexpr double Lasting = 4.0;
	}

	// Veyra.Abilities.Summons.*: a companion summoned for a while, bound to an ally it escorts and helps or to an
	// enemy it hunts and slows, redirected by a recast, and gone for good when its time runs out or it is killed
	// (ADR-035 §5).
	TEST_CLASS(Summons, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		static FVeyraCommandAbilityTuning SummonAt(EVeyraCompanionBind Bind)
		{
			FVeyraCommandAbilityTuning Command;
			Command.Cast = InstantCast(SummonFixture::CastRange, 0.0, 0.0);
			Command.Order = EVeyraCompanionOrder::Summon;
			Command.Companion = { ArchetypeTestId(TEXT("test_current")) };
			Command.LifetimeSeconds = SummonFixture::Lifetime;
			Command.BindTo = Bind;
			return Command;
		}

		BEFORE_EACH()
		{
			using namespace SummonFixture;
			FVeyraCompanionTuning Current = ExampleCompanion();
			FVeyraEscortTuning& Escort = Current.Escort.AddDefaulted_GetRef();
			Escort.PulseSeconds = Pulse;
			Escort.HealAmount = Heal;
			Escort.Statuses = { ArchetypeTestId(TEXT("test_swell")) };
			Current.AttackStatuses = { ArchetypeTestId(TEXT("test_drag")) };
			Tuning.Companions.Add(ArchetypeTestId(TEXT("test_current")), Current);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_swell")), StatusOf(EVeyraStatusKind::MoveSpeed, 0.1, Lasting));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_drag")), StatusOf(EVeyraStatusKind::Slow, 0.2, Lasting));
			Tuning.Command.Add(ArchetypeTestId(TEXT("test_escort")), SummonAt(EVeyraCompanionBind::Ally));
			Tuning.Command.Add(ArchetypeTestId(TEXT("test_hunt")), SummonAt(EVeyraCompanionBind::Enemy));
			FVeyraCommandAbilityTuning Redirect;
			Redirect.Cast = InstantCast(CastRange, 0.0, 0.0);
			Redirect.Order = EVeyraCompanionOrder::Redirect;
			Redirect.BindTo = EVeyraCompanionBind::Ally;
			Tuning.Command.Add(ArchetypeTestId(TEXT("test_redirect")), Redirect);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			UVeyraProgressionComponent* Progression = Caster->GetPlayerState()->FindComponentByClass<UVeyraProgressionComponent>();
			Progression->Initialize(FVeyraStatGrowth(), 0.0);
			Progression->AddExperience(CompanionFixture::ManyLevels);
			UVeyraAbilityLoadoutComponent* Loadout = Caster->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			const TPair<EVeyraAbilitySlot, const TCHAR*> Kit[] = { { EVeyraAbilitySlot::Q, TEXT("test_escort") }, { EVeyraAbilitySlot::W, TEXT("test_hunt") },
				{ EVeyraAbilitySlot::E, TEXT("test_redirect") } };
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

		UVeyraCompanionSubsystem& Keeper()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraCompanionSubsystem>();
		}

		AVeyraCompanion* Current()
		{
			return Keeper().Find(*Caster->GetAbilitySystemComponent());
		}

		EVeyraCastRejection CastOn(EVeyraAbilitySlot Slot, AActor& Unit) const
		{
			FVeyraCastTarget Target;
			Target.Actor = &Unit;
			Target.bHasLocation = true;
			Target.Location = Unit.GetActorLocation();
			return VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), Slot, Target);
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, CompanionFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(CompanionFixture::Step);
			}
		}

		static double Lost(const AActor& Unit)
		{
			const UAbilitySystemComponent& Abilities = *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			return Abilities.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - Abilities.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute());
		}

		static bool Hit(AActor& Source, AActor& Target, double Amount)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			return VeyraCombat::DealDamage(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Source),
				*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Target), Damage);
		}

		TEST_METHOD(ItEscortsTheAllyItNamesAndHelpsIt)
		{
			using namespace SummonFixture;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(CompanionFixture::Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CompanionFixture::Far, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Hit(Enemy, Ally, Injury)));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::Q, Ally) == EVeyraCastRejection::None));
			AVeyraCompanion* Companion = Current();
			ASSERT_THAT(IsTrue(Companion && Companion->GetMode() == EVeyraCompanionMode::Escort && Companion->GetBoundTo() == &Ally));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(Ally), Injury - Heal, CompanionFixture::Tolerance)
				&& FArchetypeTestWorld::Has(Ally, TEXT("test_swell")), TEXT("it helps its ally as it binds")));
			Wait(Pulse + CompanionFixture::Step);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Lost(Ally), Injury - 2.0 * Heal, CompanionFixture::Tolerance), TEXT("and again at its pulse")));
			Brain(*Companion).Think();
			ASSERT_THAT(IsNull(Brain(*Companion).GetTarget(), TEXT("an escort fights nothing")));
		}

		TEST_METHOD(ItMayEscortItsCaster)
		{
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::Q, *Caster) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Current() && Current()->GetBoundTo() == Caster));
		}

		TEST_METHOD(ItHuntsTheEnemyItNamesAndItsAttackSlows)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CompanionFixture::Near * 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::W, Enemy) == EVeyraCastRejection::None));
			AVeyraCompanion& Companion = *Current();
			ASSERT_THAT(IsTrue(Companion.GetMode() == EVeyraCompanionMode::Hunt && Companion.GetBoundTo() == &Enemy));
			Brain(Companion).Think();
			ASSERT_THAT(IsTrue(Brain(Companion).GetTarget() == &Enemy, TEXT("it hunts its prey")));
			// Beside its prey, its bite lands and slows.
			Companion.SetActorLocation(Enemy.GetActorLocation() - FVector(CompanionFixture::Reach / 2.0, 0.0, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
			UVeyraBasicAttackComponent& Attacks = *Companion.GetBasicAttack();
			ASSERT_THAT(IsTrue(Attacks.StartAttack(Enemy) == EVeyraAttackRejection::None));
			Attacks.Commit();
			ASSERT_THAT(IsTrue(Lost(Enemy) > 0.0 && FArchetypeTestWorld::Has(Enemy, TEXT("test_drag"))));
		}

		TEST_METHOD(ARecastRedirectsItKeepingItsTime)
		{
			using namespace SummonFixture;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& First = World.Spawn(EVeyraTeam::A, FVector(CompanionFixture::Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Second = World.Spawn(EVeyraTeam::A, FVector(0.0, CompanionFixture::Near, 0.0));
			AVeyraVanguardCharacter& Third = World.Spawn(EVeyraTeam::A, FVector(0.0, -CompanionFixture::Near, 0.0));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::Q, First) == EVeyraCastRejection::None));
			AVeyraCompanion* Formed = Current();
			Wait(Lifetime / 2.0);
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::Q, Second) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Current() == Formed && Formed->GetBoundTo() == &Second, TEXT("summoned again while it lives, it is redirected")));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, Third) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Formed->GetBoundTo() == &Third, TEXT("and so a redirect does")));
			Wait(Lifetime / 2.0 + CompanionFixture::Think + CompanionFixture::Step);
			ASSERT_THAT(IsNull(Current(), TEXT("its time ran from its first summon")));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, Third) == EVeyraCastRejection::NoCompanion, TEXT("a redirect needs it living")));
		}

		TEST_METHOD(KilledItIsGoneForGood)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CompanionFixture::Near * 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::W, Enemy) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Hit(Enemy, *Current(), CompanionFixture::Lethal)));
			ASSERT_THAT(IsNull(Current(), TEXT("killed, it goes")));
			Wait(CompanionFixture::Reform + CompanionFixture::Step);
			ASSERT_THAT(IsNull(Current(), TEXT("and does not reform")));
		}

		TEST_METHOD(ValidationWantsASummonToNameItsCompanionAndTime)
		{
			const int32 Ranks[] = { 5, 3 };
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.Command[ArchetypeTestId(TEXT("test_escort"))].LifetimeSeconds = 0.0;
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty(), TEXT("a summon that lasts no time")));
			Broken = Tuning;
			Broken.Command[ArchetypeTestId(TEXT("test_hunt"))].Companion = { ArchetypeTestId(TEXT("test_missing")) };
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty(), TEXT("a summon of nothing")));
			Broken = Tuning;
			Broken.Command[ArchetypeTestId(TEXT("test_redirect"))].BindTo = EVeyraCompanionBind::None;
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty(), TEXT("a redirect that binds nothing")));
		}

		static AVeyraCompanionController& Brain(AVeyraCompanion& Companion)
		{
			return *CastChecked<AVeyraCompanionController>(Companion.GetController());
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
