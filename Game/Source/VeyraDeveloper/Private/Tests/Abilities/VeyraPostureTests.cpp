// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionController.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "CQTest.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	namespace PostureFixture
	{
		// Fixture values: the companion's reach is CompanionFixture::Reach.
		constexpr double Range = 1000.0;
		constexpr double Out = 500.0;
		constexpr double Close = 130.0;
		constexpr double Lifetime = 60.0;
		constexpr double Move = 4.0;
		constexpr double Lasting = 120.0;
		constexpr double Share = 0.5;
		constexpr double Arc = 120.0;
		constexpr double CoverReach = 300.0;
		constexpr double Capacity = 500.0;
		constexpr double Transfer = 0.25;
		constexpr double AuraRadius = 400.0;
		constexpr double AuraPulse = 1.0;
		constexpr double ShieldAmount = 50.0;
		constexpr double ShieldSeconds = 3.0;
		constexpr double ManyLevels = 5000.0;
		constexpr float Step = 0.1f;
	}

	// Veyra.Abilities.Postures.*: a deployed companion's postures, and moving it with an ally (ADR-037 §2, §3).
	TEST_CLASS(Postures, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Owner = nullptr;

		BEFORE_EACH()
		{
			using namespace PostureFixture;
			FVeyraCompanionTuning Picket = ExampleCompanion();
			FVeyraCompanionPostureTuning& Gun = Picket.Postures.AddDefaulted_GetRef();
			Gun.Statuses = { ArchetypeTestId(TEXT("test_gun")) };
			FVeyraCompanionPostureTuning& Bulwark = Picket.Postures.AddDefaulted_GetRef();
			Bulwark.Attacks = EVeyraPostureAttacks::Holds;
			Bulwark.Statuses = { ArchetypeTestId(TEXT("test_bulwark")) };
			Picket.MovingStatuses = { ArchetypeTestId(TEXT("test_moving")) };
			FVeyraMovingAuraTuning& Aura = Picket.MovingAura.AddDefaulted_GetRef();
			Aura.Radius = AuraRadius;
			Aura.PulseSeconds = AuraPulse;
			Aura.UnitKinds = { EVeyraUnitKind::Fluxborn };
			FVeyraShieldTuning& Shield = Aura.Shield.AddDefaulted_GetRef();
			Shield.Id = ArchetypeTestId(TEXT("test_line_shield"));
			Shield.AmountByRank = { ShieldAmount };
			Shield.DurationSeconds = ShieldSeconds;
			Tuning.Companions.Add(ArchetypeTestId(TEXT("test_picket")), Picket);

			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_gun")), StatusOf(EVeyraStatusKind::Counter, 0.0, Lasting));
			FVeyraStatusTuning Cover = StatusOf(EVeyraStatusKind::Cover, Share, Lasting);
			Cover.ArcDegrees = Arc;
			Cover.Cover.Add(FVeyraStatusCoverTuning{ CoverReach, Capacity, Transfer });
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_bulwark")), Cover);
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_moving")), StatusOf(EVeyraStatusKind::Counter, 0.0, Lasting));

			FVeyraCommandAbilityTuning Set;
			Set.Cast = InstantCast(Range, 0.0, 0.0);
			Set.Order = EVeyraCompanionOrder::Deploy;
			Set.Companion = { ArchetypeTestId(TEXT("test_picket")) };
			Set.LifetimeSeconds = Lifetime;
			Tuning.Command.Add(ArchetypeTestId(TEXT("test_set")), Set);
			FVeyraCommandAbilityTuning Brace;
			Brace.Cast = InstantCast(Range, 0.0, 0.0);
			Brace.Order = EVeyraCompanionOrder::ChangePosture;
			Tuning.Command.Add(ArchetypeTestId(TEXT("test_brace")), Brace);
			FVeyraCommandAbilityTuning Line;
			Line.Cast = InstantCast(Range, 0.0, 0.0);
			Line.Order = EVeyraCompanionOrder::Unanchor;
			Line.LifetimeSeconds = Move;
			Line.BindTo = EVeyraCompanionBind::Ally;
			Tuning.Command.Add(ArchetypeTestId(TEXT("test_line")), Line);

			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Owner = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			// Three abilities: a second Learn would start its progression afresh.
			APlayerState* Participant = Owner->GetPlayerState();
			UVeyraProgressionComponent* Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>();
			UVeyraAbilityLoadoutComponent* Loadout = Participant->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			Progression->Initialize(FVeyraStatGrowth(), 0.0);
			Progression->AddExperience(ManyLevels);
			const TPair<EVeyraAbilitySlot, const TCHAR*> Kit[] = { { EVeyraAbilitySlot::Q, TEXT("test_set") }, { EVeyraAbilitySlot::W, TEXT("test_brace") },
				{ EVeyraAbilitySlot::E, TEXT("test_line") } };
			for (const TPair<EVeyraAbilitySlot, const TCHAR*>& Each : Kit)
			{
				ASSERT_THAT(IsTrue(Loadout->Grant(*Owner->GetAbilitySystemComponent(), Each.Key, ArchetypeTestId(Each.Value))));
				ASSERT_THAT(IsTrue(Progression->AllocateRank(Each.Key) == EVeyraRankRefusal::None));
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

		AVeyraCompanion* Picket()
		{
			return Keeper().Find(*Owner->GetAbilitySystemComponent());
		}

		AVeyraCompanionController& Brain()
		{
			return *CastChecked<AVeyraCompanionController>(Picket()->GetController());
		}

		EVeyraCastRejection Order(EVeyraAbilitySlot Slot, const FVector& Point, AActor* Unit = nullptr)
		{
			FVeyraCastTarget Target;
			Target.Actor = Unit;
			Target.bHasLocation = true;
			Target.Location = Point;
			return VeyraAbilities::TryCast(*Owner->GetAbilitySystemComponent(), Slot, Target);
		}

		bool Deployed()
		{
			return Order(EVeyraAbilitySlot::Q, FVector(PostureFixture::Out, 0.0, 0.0)) == EVeyraCastRejection::None && Picket();
		}

		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, PostureFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(PostureFixture::Step);
			}
		}

		static int32 ShieldsOn(const AActor& Unit)
		{
			const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
			const UVeyraDamageAbsorptionComponent* Absorption = Abilities->GetOwner()->FindComponentByClass<UVeyraDamageAbsorptionComponent>();
			return Absorption ? Absorption->GetLedger().Shields.Num() : 0;
		}

		TEST_METHOD(ItStandsInItsFirstPostureHoldingItsStatuses)
		{
			ASSERT_THAT(IsTrue(Deployed()));
			ASSERT_THAT(AreEqual(0, Picket()->GetPosture()));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Picket(), TEXT("test_gun"))));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Picket(), TEXT("test_bulwark"))));
		}

		TEST_METHOD(AChangeSwapsItsStatusesFacesThePointAndHoldsItsFire)
		{
			using namespace PostureFixture;
			ASSERT_THAT(IsTrue(Deployed()));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(Out + Close, 0.0, 0.0));
			// Within its cast range, which clamps the point.
			ASSERT_THAT(IsTrue(Order(EVeyraAbilitySlot::W, FVector(Out, Out, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(AreEqual(1, Picket()->GetPosture()));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Picket(), TEXT("test_bulwark"))));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(*Picket(), TEXT("test_gun"))));
			ASSERT_THAT(IsTrue(Picket()->GetActorForwardVector().Y > 1.0 - KINDA_SMALL_NUMBER, TEXT("facing the point")));
			Brain().Think();
			ASSERT_THAT(IsNull(Brain().GetTarget(), TEXT("it holds its fire")));
			ASSERT_THAT(IsTrue(Order(EVeyraAbilitySlot::W, FVector(Range, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(AreEqual(0, Picket()->GetPosture(), TEXT("the first after the last")));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(*Picket(), TEXT("test_gun"))));
			Brain().Think();
			ASSERT_THAT(IsTrue(Brain().GetTarget() == &Enemy, TEXT("and fights again")));
		}

		TEST_METHOD(MovedItWalksWithItsAllyHoldingItsMovingStatusesThenAnchorsInItsPosture)
		{
			using namespace PostureFixture;
			ASSERT_THAT(IsTrue(Deployed()));
			ASSERT_THAT(IsTrue(Order(EVeyraAbilitySlot::W, FVector(Range, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Order(EVeyraAbilitySlot::E, FVector(0.0, Range, 0.0), Owner) == EVeyraCastRejection::None));
			AVeyraCompanion& Moved = *Picket();
			ASSERT_THAT(IsTrue(Moved.IsMoving() && Moved.GetMode() == EVeyraCompanionMode::Escort && Moved.GetBoundTo() == Owner));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Moved, TEXT("test_moving"))));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(Moved, TEXT("test_bulwark")), TEXT("its moving statuses instead of its posture's")));
			ASSERT_THAT(IsFalse(Moved.HoldsFire(), TEXT("moving, it fires")));
			ASSERT_THAT(IsTrue(Moved.GetActorForwardVector().Y > 1.0 - KINDA_SMALL_NUMBER, TEXT("facing the cast's way")));
			Wait(Move + AuraPulse);
			ASSERT_THAT(IsTrue(!Moved.IsMoving() && Moved.GetMode() == EVeyraCompanionMode::Anchored, TEXT("anchored again")));
			ASSERT_THAT(AreEqual(1, Moved.GetPosture()));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Has(Moved, TEXT("test_bulwark")), TEXT("in its earlier posture")));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(Moved, TEXT("test_moving"))));
		}

		TEST_METHOD(MovingItFiresAtWhatItsAttackReaches)
		{
			using namespace PostureFixture;
			ASSERT_THAT(IsTrue(Deployed()));
			ASSERT_THAT(IsTrue(Order(EVeyraAbilitySlot::W, FVector(Range, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Order(EVeyraAbilitySlot::E, FVector(Range, 0.0, 0.0), Owner) == EVeyraCastRejection::None));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, Picket()->GetActorLocation() + FVector(Close, 0.0, 0.0));
			Brain().Think();
			ASSERT_THAT(IsTrue(Brain().GetTarget() == &Enemy));
		}

		TEST_METHOD(ItsAuraShieldsTheAlliesOfItsKindsAboutItAsItMoves)
		{
			using namespace PostureFixture;
			ASSERT_THAT(IsTrue(Deployed()));
			FArchetypeTestWorld World{ Spawner };
			const FVector Near = Picket()->GetActorLocation() + FVector(0.0, AuraRadius / 2.0, 0.0);
			AVeyraTestFluxborn& Ally = World.SpawnFluxborn(EVeyraTeam::A, Near);
			AVeyraTestFluxborn& Foe = World.SpawnFluxborn(EVeyraTeam::B, Near + FVector(0.0, Close, 0.0));
			AVeyraVanguardCharacter& Friend = World.Spawn(EVeyraTeam::A, Near - FVector(0.0, Close, 0.0));
			Wait(AuraPulse * 2.0);
			ASSERT_THAT(AreEqual(0, ShieldsOn(Ally), TEXT("anchored, no aura")));
			ASSERT_THAT(IsTrue(Order(EVeyraAbilitySlot::E, FVector(Range, 0.0, 0.0), Owner) == EVeyraCastRejection::None));
			ASSERT_THAT(AreEqual(1, ShieldsOn(Ally), TEXT("an allied Fluxborn is shielded as it sets off")));
			ASSERT_THAT(AreEqual(0, ShieldsOn(Foe), TEXT("never an enemy")));
			ASSERT_THAT(AreEqual(0, ShieldsOn(Friend), TEXT("nor an ally of another kind")));
		}

		TEST_METHOD(OrdersNeedTheirCompanion)
		{
			ASSERT_THAT(IsTrue(Order(EVeyraAbilitySlot::W, FVector(PostureFixture::Range, 0.0, 0.0)) == EVeyraCastRejection::NoCompanion));
			ASSERT_THAT(IsTrue(Order(EVeyraAbilitySlot::E, FVector(PostureFixture::Range, 0.0, 0.0), Owner) == EVeyraCastRejection::NoCompanion));
			// A companion kept for good is no deployed one to move.
			ASSERT_THAT(IsTrue(Keeper().Summon(*Owner->GetAbilitySystemComponent(), ArchetypeTestId(TEXT("test_picket")))));
			ASSERT_THAT(IsNotNull(Picket()));
			ASSERT_THAT(IsTrue(Order(EVeyraAbilitySlot::E, FVector(PostureFixture::Range, 0.0, 0.0), Owner) == EVeyraCastRejection::NoCompanion));
		}

		TEST_METHOD(ValidationKeepsPosturesAurasAndMovesWhole)
		{
			const int32 Ranks[] = { 5, 3 };
			FVeyraAbilitiesTuning Broken = Tuning;
			Broken.Companions[ArchetypeTestId(TEXT("test_picket"))].Postures.SetNum(1);
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty(), TEXT("one posture is nothing to change between")));
			Broken = Tuning;
			Broken.Companions[ArchetypeTestId(TEXT("test_picket"))].MovingAura[0].Radius = 0.0;
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty()));
			Broken = Tuning;
			Broken.Command[ArchetypeTestId(TEXT("test_line"))].BindTo = EVeyraCompanionBind::None;
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty(), TEXT("a move names an ally")));
			Broken = Tuning;
			Broken.Command[ArchetypeTestId(TEXT("test_brace"))].LifetimeSeconds = 1.0;
			ASSERT_THAT(IsFalse(VeyraAbilityRules::Validate(Broken, Ranks).IsEmpty()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
