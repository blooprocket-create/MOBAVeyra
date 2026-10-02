// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attunements/VeyraAttunementSubsystem.h"
#include "Components/ActorTestSpawner.h"
#include "CQTest.h"
#include "Engine/World.h"
#include "Gold/VeyraGoldComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Shop/VeyraShopSubsystem.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/Combat/VeyraCombatTestHelpers.h"
#include "Tests/Items/VeyraItemsTestCatalog.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraItemsTests
{
	namespace BurstFixture
	{
		// Fixture values.
		constexpr double Purse = 10000.0;
		constexpr double Blow = 60.0;
		constexpr double Bonus = 40.0;
		constexpr double Lethal = 1.0e6;
		constexpr double Tolerance = 1e-3;
		constexpr float WorldStep = 0.1f;
	}

	// Veyra.Items.BurstAttunements.*: No Allegiance, Clean Break, Through the Guard, No One Coming and Reenactment
	// (Item Bible §8–§9; ADR-051 §3). Each test gives the fixture catalog's Masterwork the Attunement under test.
	TEST_CLASS(BurstAttunements, "Veyra.Items")
	{
		FActorTestSpawner Spawner;
		FVeyraItemsTuning Tuning = TestCatalog();
		UVeyraShopSubsystem* Shop = nullptr;
		AVeyraPlayerState* Participant = nullptr;
		AVeyraVanguardCharacter* HolderBody = nullptr;
		UAbilitySystemComponent* Enemy = nullptr;
		TArray<FVeyraDamageDealtEvent> Dealt;

		BEFORE_EACH()
		{
			using namespace BurstFixture;
			UVeyraItemsTuningSubsystem::SetTestOverride(&Tuning);
			Shop = Spawner.GetWorld().GetSubsystem<UVeyraShopSubsystem>();
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			HolderBody = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Participant = HolderBody->GetPlayerState<AVeyraPlayerState>();
			Enemy = World.Spawn(EVeyraTeam::B, FVector(200.0, 0.0, 0.0)).GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(Shop && Participant && Enemy));
			// No resistances, so every bonus lands whole.
			Enemy->SetNumericAttributeBase(UVeyraDefenceSet::GetArmorAttribute(), 0.0f);
			Enemy->SetNumericAttributeBase(UVeyraDefenceSet::GetMagicResistAttribute(), 0.0f);
			UVeyraShopSubsystem::InitializeInventory(*Participant);
			ASSERT_THAT(IsTrue(Participant->FindComponentByClass<UVeyraGoldComponent>()->Grant(Purse, EVeyraGoldReason::Developer)));
			Events().OnDamageDealt.AddLambda([this](const FVeyraDamageDealtEvent& Event) { Dealt.Add(Event); });
		}

		AFTER_EACH()
		{
			UVeyraItemsTuningSubsystem::SetTestOverride(nullptr);
		}

		UVeyraCombatEventSubsystem& Events()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>();
		}

		UVeyraAttunementSubsystem& Attunements()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraAttunementSubsystem>();
		}

		UAbilitySystemComponent& Holder() const
		{
			return *Participant->GetAbilitySystemComponent();
		}

		/** Gives the Masterwork Attunement and buys it at the fountain. */
		void Hold(const TCHAR* Attunement)
		{
			Tuning.Items[ItemId(TEXT("test_temper"))].Attunement = { ItemId(Attunement) };
			Tuning.WeightOfWar.Reset();
			Shop->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Shop->Buy(*Participant, ItemId(TEXT("test_temper"))) == EVeyraShopRefusal::None));
		}

		void Hit(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, double Amount, EVeyraDamageDelivery Delivery)
		{
			FVeyraRawDamageEvent Damage;
			Damage.Components.Add({ EVeyraDamageType::TrueDamage, Amount });
			Damage.Delivery = Delivery;
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(Source, Target, Damage)));
		}

		void Hit(EVeyraDamageDelivery Delivery, double Amount = BurstFixture::Blow)
		{
			Hit(Holder(), *Enemy, Amount, Delivery);
		}

		/** The holder commits a cast of Ability, as the cast path announces it. */
		void Commit(const TCHAR* Ability)
		{
			FVeyraCastEvent Cast;
			Cast.Caster = &Holder();
			Cast.Ability = ItemId(Ability);
			Events().OnCastCommitted.Broadcast(Cast);
		}

		TArray<FVeyraDamageDealtEvent> Procs() const
		{
			return Dealt.FilterByPredicate([](const FVeyraDamageDealtEvent& Event) { return Event.Delivery == EVeyraDamageDelivery::Proc; });
		}

		void RunFor(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, BurstFixture::WorldStep);
			}
		}

		UAbilitySystemComponent& SpawnVanguard(EVeyraTeam Team, const FVector& At)
		{
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			return *World.Spawn(Team, At).GetAbilitySystemComponent();
		}

		static double ShieldOn(const UAbilitySystemComponent& Unit)
		{
			double Sum = 0.0;
			for (const FVeyraShieldEntry& Shield : Unit.GetOwner()->FindComponentByClass<UVeyraDamageAbsorptionComponent>()->GetLedger().Shields)
			{
				Sum += Shield.Remaining;
			}
			return Sum;
		}

		static const FVeyraStatusEntry* StatusOn(const UAbilitySystemComponent& Unit, EVeyraStatusKind Kind)
		{
			return Unit.GetOwner()->FindComponentByClass<UVeyraStatusComponent>()->GetLedger().Entries.FindByPredicate(
				[Kind](const FVeyraStatusEntry& Entry) { return Entry.Kind == Kind; });
		}

		TEST_METHOD(AnItemsFlatPhysicalPenetrationReachesItsHoldersOffence)
		{
			// Fixture value: Veil Needle's identity on the fixture grip (ADR-051 §1).
			constexpr double Penetration = 8.0;
			Tuning.Items[ItemId(TEXT("test_grip"))].Stats.PhysicalPenetrationFlat = Penetration;
			const double Before = Holder().GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPenetrationFlatAttribute());
			Shop->SetAtFountain(*Participant, true);
			ASSERT_THAT(IsTrue(Shop->Buy(*Participant, ItemId(TEXT("test_grip"))) == EVeyraShopRefusal::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Holder().GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPenetrationFlatAttribute()), Before + Penetration)));
		}

		TEST_METHOD(NoAllegiancePaysForADifferentActionAfterAQuietOpening)
		{
			using namespace BurstFixture;
			constexpr double Quiet = 4.0;
			constexpr double Opening = 3.0;
			constexpr double Penetration = 10.0;
			FVeyraNoAllegianceTuning& NoAllegiance = Tuning.NoAllegiance.Add(ItemId(TEXT("test_allegiance")));
			NoAllegiance.QuietSeconds = Quiet;
			NoAllegiance.OpeningSeconds = Opening;
			NoAllegiance.Bonus.Base = Bonus;
			NoAllegiance.Penetration = Penetration;
			Hold(TEXT("test_allegiance"));

			Commit(TEXT("test_strike"));
			Hit(EVeyraDamageDelivery::Ability);
			Hit(EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsTrue(Procs().IsEmpty(), TEXT("the same ability again pays nothing")));
			Hit(EVeyraDamageDelivery::Proc);
			ASSERT_THAT(IsTrue(Procs().Num() == 1, TEXT("a proc is no action, so it consumes nothing")));
			Commit(TEXT("test_follow"));
			Hit(EVeyraDamageDelivery::Ability);
			const TArray<FVeyraDamageDealtEvent> Paid = Procs();
			ASSERT_THAT(IsTrue(Paid.Num() == 2 && FMath::IsNearlyEqual(Paid.Last().Of(EVeyraDamageType::Physical), Bonus, Tolerance),
				TEXT("a different ability consumes the Opening for the bonus")));

			Hit(EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(Procs().Num() == 2, TEXT("no Opening without a quiet period first")));
			RunFor(Quiet + WorldStep);
			Hit(EVeyraDamageDelivery::BasicAttack);
			RunFor(Opening + WorldStep);
			Hit(EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsTrue(Procs().Num() == 2, TEXT("an Opening lapses after its time")));
		}

		TEST_METHOD(CleanBreakSpeedsAndShieldsTheHolderOnATakedown)
		{
			using namespace BurstFixture;
			constexpr double Window = 3.0;
			constexpr int32 Stacks = 4;
			constexpr double Share = 0.25;
			constexpr double Cap = 250.0;
			FVeyraCleanBreakTuning& CleanBreak = Tuning.CleanBreak.Add(ItemId(TEXT("test_break")));
			CleanBreak.WindowSeconds = Window;
			CleanBreak.SpeedPerStack = 0.1;
			CleanBreak.SpeedStacks = Stacks;
			CleanBreak.SpeedStackSeconds = 0.5;
			CleanBreak.ShieldShare = Share;
			CleanBreak.ShieldCap = Cap;
			CleanBreak.ShieldSeconds = Window;
			Hold(TEXT("test_break"));
			UAbilitySystemComponent& Friend = SpawnVanguard(EVeyraTeam::A, FVector(0.0, 200.0, 0.0));

			Hit(EVeyraDamageDelivery::BasicAttack, Blow);
			Hit(EVeyraDamageDelivery::Ability, Blow);
			Hit(Friend, *Enemy, Lethal, EVeyraDamageDelivery::Ability);
			const FVeyraStatusEntry* Speed = StatusOn(Holder(), EVeyraStatusKind::MoveSpeed);
			ASSERT_THAT(IsTrue(Speed && Speed->Stacks == Stacks, TEXT("a burst of Movement Speed, every stack at once")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(ShieldOn(Holder()), Share * Blow * 2.0, Tolerance), TEXT("a shield sized by what it dealt")));

			// Too long after the holder's last hit, a takedown grants nothing.
			UAbilitySystemComponent& Late = SpawnVanguard(EVeyraTeam::B, FVector(300.0, 0.0, 0.0));
			Hit(Holder(), Late, Blow, EVeyraDamageDelivery::BasicAttack);
			RunFor(Window + WorldStep);
			const double Before = ShieldOn(Holder());
			Hit(Friend, Late, Lethal, EVeyraDamageDelivery::Ability);
			ASSERT_THAT(IsTrue(ShieldOn(Holder()) <= Before + Tolerance, TEXT("no shield for a takedown past the window")));
		}

		TEST_METHOD(ThroughTheGuardDetonatesTheBreachOfAnAllysShieldTheHolderBreaks)
		{
			using namespace BurstFixture;
			constexpr double Shield = 100.0;
			constexpr double Share = 0.5;
			FVeyraThroughTheGuardTuning& Guard = Tuning.ThroughTheGuard.Add(ItemId(TEXT("test_guard")));
			Guard.BrandSeconds = 3.0;
			Guard.BreachShare = Share;
			Guard.BreachConversion = 1.0;
			Hold(TEXT("test_guard"));
			UAbilitySystemComponent& Warden = SpawnVanguard(EVeyraTeam::B, FVector(200.0, 200.0, 0.0));

			// A shield the enemy gave itself never qualifies.
			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(*Enemy, *Enemy, EVeyraShieldCategory::Universal, Shield, 10.0).IsValid()));
			Hit(EVeyraDamageDelivery::BasicAttack, Shield);
			ASSERT_THAT(IsTrue(Procs().IsEmpty(), TEXT("a self-granted shield breaks for nothing")));

			ASSERT_THAT(IsTrue(VeyraCombat::GrantShield(Warden, *Enemy, EVeyraShieldCategory::Universal, Shield, 10.0).IsValid()));
			Hit(EVeyraDamageDelivery::BasicAttack, Blow);
			ASSERT_THAT(IsTrue(Procs().IsEmpty(), TEXT("branded, not yet broken")));
			Hit(EVeyraDamageDelivery::BasicAttack, Blow);
			const TArray<FVeyraDamageDealtEvent> Detonated = Procs();
			ASSERT_THAT(IsTrue(Detonated.Num() == 1 && FMath::IsNearlyEqual(Detonated[0].Of(EVeyraDamageType::Physical), Share * Shield, Tolerance),
				TEXT("broken by the holder, the Breach detonates: half of everything it absorbed")));
		}

		TEST_METHOD(NoOneComingMarksALoneVanguardLocksInAndAnAllyBreaksIt)
		{
			using namespace BurstFixture;
			constexpr double Radius = 1000.0;
			constexpr double LockDamage = 100.0;
			FVeyraNoOneComingTuning& NoOneComing = Tuning.NoOneComing.Add(ItemId(TEXT("test_alone")));
			NoOneComing.ProtectionRadius = Radius;
			NoOneComing.MarkSeconds = 4.0;
			NoOneComing.Speed = 0.15;
			NoOneComing.LockDamage = LockDamage;
			NoOneComing.LockSeconds = 2.0;
			NoOneComing.Bonus.Base = Bonus;
			NoOneComing.CheckSeconds = 0.25;
			Hold(TEXT("test_alone"));

			bool bLocked = false;
			Hit(EVeyraDamageDelivery::BasicAttack, Blow);
			ASSERT_THAT(IsTrue(Attunements().IsAbandoned(Holder(), *Enemy, &bLocked) && !bLocked, TEXT("alone, it is marked Abandoned")));
			ASSERT_THAT(IsNotNull(StatusOn(Holder(), EVeyraStatusKind::MoveSpeedTowardEnemyVanguards), TEXT("and the holder closes in faster")));
			Hit(EVeyraDamageDelivery::BasicAttack, Blow);
			ASSERT_THAT(IsTrue(Attunements().IsAbandoned(Holder(), *Enemy, &bLocked) && bLocked, TEXT("enough damage locks it in")));
			ASSERT_THAT(IsTrue(Procs().IsEmpty()));
			Hit(EVeyraDamageDelivery::BasicAttack, Blow);
			ASSERT_THAT(IsTrue(Procs().Num() == 1 && FMath::IsNearlyEqual(Procs()[0].Of(EVeyraDamageType::Physical), Bonus, Tolerance),
				TEXT("the next hit finishes it")));

			// A second Vanguard, marked, is saved by an ally arriving before the lock.
			UAbilitySystemComponent& Other = SpawnVanguard(EVeyraTeam::B, FVector(-3000.0, 0.0, 0.0));
			Hit(Holder(), Other, Blow / 2.0, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsTrue(Attunements().IsAbandoned(Holder(), Other)));
			SpawnVanguard(EVeyraTeam::B, FVector(-3000.0, Radius / 2.0, 0.0));
			Attunements().UpdateAbandoned();
			ASSERT_THAT(IsFalse(Attunements().IsAbandoned(Holder(), Other), TEXT("an ally's arrival breaks an unlocked mark")));
			Hit(Holder(), Other, Blow / 2.0, EVeyraDamageDelivery::BasicAttack);
			ASSERT_THAT(IsFalse(Attunements().IsAbandoned(Holder(), Other), TEXT("and no new mark while it stands near")));
		}

		TEST_METHOD(ReenactmentReplaysARememberedWoundAfterRepositioning)
		{
			using namespace BurstFixture;
			constexpr double Displacement = 300.0;
			constexpr double Replay = 0.4;
			FVeyraReenactmentTuning& Reenactment = Tuning.Reenactment.Add(ItemId(TEXT("test_memory")));
			Reenactment.QuietSeconds = 5.0;
			Reenactment.MemorySeconds = 4.0;
			Reenactment.Displacement = Displacement;
			Reenactment.ReplayShare = Replay;
			Hold(TEXT("test_memory"));

			Hit(EVeyraDamageDelivery::Ability, Blow);
			Hit(EVeyraDamageDelivery::BasicAttack, Blow);
			ASSERT_THAT(IsTrue(Procs().IsEmpty(), TEXT("from where it stood, nothing replays")));
			HolderBody->SetActorLocation(HolderBody->GetActorLocation() - FVector(Displacement * 1.5, 0.0, 0.0));
			Hit(EVeyraDamageDelivery::BasicAttack, Blow);
			const TArray<FVeyraDamageDealtEvent> Replayed = Procs();
			ASSERT_THAT(IsTrue(Replayed.Num() == 1 && FMath::IsNearlyEqual(Replayed[0].Of(EVeyraDamageType::Magic), Replay * Blow, Tolerance),
				TEXT("repositioned, the remembered wound replays as Magic Damage")));
			Hit(EVeyraDamageDelivery::Ability, Blow);
			ASSERT_THAT(IsTrue(Procs().Num() == 1, TEXT("spent, and no new memory without a quiet period")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
