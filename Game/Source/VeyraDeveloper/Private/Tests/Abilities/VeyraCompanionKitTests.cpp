// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraVitalsSet.h"
#include "Companions/VeyraCompanion.h"
#include "Companions/VeyraCompanionSubsystem.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "CQTest.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Targeting/VeyraVisibility.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "TimerManager.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

#if WITH_AUTOMATION_WORKER

#include <limits>

namespace VeyraAbilitiesTests
{
	/** A world's vision that hides one unit from everyone, as fog or stealth would. */
	class FKitHidingVisibility final : public IVeyraVisibility
	{
	public:
		explicit FKitHidingVisibility(const AActor& InHidden)
			: Hidden(&InHidden)
		{
		}

		virtual bool CanSee(const UObject& /*Observer*/, const AActor& Target) const override { return &Target != Hidden; }
		virtual bool IsVisibleToTeam(EVeyraTeam /*Team*/, const AActor& Target) const override { return &Target != Hidden; }
		virtual void RevealArea(EVeyraTeam /*Team*/, const FVector& /*Centre*/, double /*Radius*/, double /*DurationSeconds*/) override {}
		virtual void RevealShape(EVeyraTeam /*Team*/, const FVeyraPlacedShape& /*Placed*/, double /*DurationSeconds*/) override {}
		virtual void AddDenseFog(const FVeyraFogShape& /*Shape*/, double /*DurationSeconds*/) override {}
		virtual int32 FogVolumeAt(const FVector& /*Point*/) const override { return INDEX_NONE; }

	private:
		const AActor* Hidden;
	};

	/** Fixture values for a companion's kit: a hold, a swap, a burst and a chain. */
	namespace CompanionKitFixture
	{
		constexpr double Range = 800.0;
		constexpr double Leap = 1500.0;
		constexpr double Hold = 3.0;
		constexpr double Zone = 200.0;
		constexpr double Hit = 30.0;
		constexpr double Point = 600.0;
		constexpr double Aside = 150.0;
		constexpr double Lasting = 5.0;
		constexpr double Growth = 0.5;
		constexpr double ChainWidth = 100.0;
		constexpr double Pulse = 0.25;
		constexpr double PerEnemy = 1.0;
		constexpr double Cooldown = 10.0;
		constexpr double Halved = 0.5;
		constexpr double Recall = 5.0;
		constexpr double Lethal = 100000.0;
		constexpr double Tolerance = 1e-3;
		constexpr float Step = 0.1f;
	}

	// Veyra.Abilities.CompanionKits.*: what a Vanguard's kit does with its companion (ADR-034 §5–§8): commands,
	// a swap with it, reactions that burst, a buff that empowers it and chains it, and casts that need it.
	TEST_CLASS(CompanionKits, "Veyra.Abilities")
	{
		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Owner = nullptr;
		TArray<FVeyraDamageDealtEvent> Dealt;
		FDelegateHandle DealtHandle;

		BEFORE_EACH()
		{
			using namespace CompanionKitFixture;
			Tuning.Companions.Add(ArchetypeTestId(TEXT("test_pet")), ExampleCompanion());
			const FVeyraAreaZoneTuning Struck = ZoneOf(Zone, Hit);

			FVeyraCommandAbilityTuning Hunt;
			Hunt.Cast = InstantCast(Range, Cooldown, 0.0);
			Hunt.Cast.NeedsCompanion = EVeyraCompanionNeed::Living;
			Hunt.Cast.CooldownWhile.Add(FVeyraCooldownWhileTuning{ ArchetypeTestId(TEXT("test_leashed")), Halved });
			FVeyraRecastTuning& Recast = Hunt.Cast.RecastWindow.AddDefaulted_GetRef();
			Recast.Ability = ArchetypeTestId(TEXT("test_hunt_recall"));
			Recast.WindowSeconds = Recall;
			Hunt.Order = EVeyraCompanionOrder::Hold;
			Hunt.LeapSpeed = Leap;
			Hunt.HoldSeconds = Hold;
			Hunt.LandingZones = { Struck };
			Tuning.Command.Add(ArchetypeTestId(TEXT("test_hunt")), Hunt);
			FVeyraCommandAbilityTuning Back;
			Back.Cast = InstantCast(0.0, 0.0, 0.0);
			Back.Cast.NeedsCompanion = EVeyraCompanionNeed::Living;
			Back.Order = EVeyraCompanionOrder::Recall;
			Tuning.Command.Add(ArchetypeTestId(TEXT("test_hunt_recall")), Back);

			FVeyraBlinkAbilityTuning Cross;
			Cross.Cast = InstantCast(0.0, Cooldown, 0.0);
			Cross.Cast.NeedsCompanion = EVeyraCompanionNeed::Living;
			Cross.To = EVeyraBlinkTo::OwnCompanion;
			Cross.Swap = EVeyraBlinkSwap::Swap;
			Cross.DepartureZones = { Struck };
			Cross.CompanionDepartureZones = { Struck };
			Tuning.Blink.Add(ArchetypeTestId(TEXT("test_cross")), Cross);

			FVeyraSelfBuffAbilityTuning Leash;
			Leash.Cast = InstantCast(0.0, Cooldown, 0.0);
			Leash.Cast.NeedsCompanion = EVeyraCompanionNeed::Living;
			Leash.Statuses = { ArchetypeTestId(TEXT("test_leashed")) };
			Leash.CompanionStatuses = { ArchetypeTestId(TEXT("test_true_form")) };
			Leash.CompanionDeath = EVeyraCompanionDeath::Ends;
			FVeyraChainTuning& Chain = Leash.Chain.AddDefaulted_GetRef();
			Chain.Width = ChainWidth;
			Chain.PulseSeconds = Pulse;
			Chain.PerEnemySeconds = PerEnemy;
			Chain.Effects.Damage.Add(DamageOf(Hit));
			Tuning.SelfBuff.Add(ArchetypeTestId(TEXT("test_leash")), Leash);

			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_leashed")), StatusOf(EVeyraStatusKind::Counter, 0.0, Lasting));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_true_form")), StatusOf(EVeyraStatusKind::MaxHealth, Growth, Lasting));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_bitten")), StatusOf(EVeyraStatusKind::Counter, 0.0, Lasting));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_rooted")), StatusOf(EVeyraStatusKind::Root, 0.0, Lasting));
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_spell_shield")), StatusOf(EVeyraStatusKind::SpellShield, 0.0, Lasting));
			// A bolt whose hit on a marked target bursts around it.
			FVeyraAreaAbilityTuning Bolt;
			Bolt.Cast = InstantCast(Range, 0.0, 0.0);
			Bolt.Origin = EVeyraAreaOrigin::TargetPoint;
			FVeyraAreaZoneTuning BoltZone = ZoneOf(Zone / 4.0, Hit);
			FVeyraReactionTuning& Reaction = BoltZone.Effects.Reactions.AddDefaulted_GetRef();
			Reaction.Status = ArchetypeTestId(TEXT("test_bitten"));
			Reaction.Consume = EVeyraReactionConsume::Consume;
			FVeyraReactionBurstTuning& Burst = Reaction.Burst.AddDefaulted_GetRef();
			Burst.Shape = CircleOf(Zone);
			Burst.Damage.Add(DamageOf(Hit));
			Bolt.Zones = { BoltZone };
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_bolt")), Bolt);

			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(IsTrue(VeyraAbilityRules::Validate(Tuning, Ranks).IsEmpty(), FString::Join(VeyraAbilityRules::Validate(Tuning, Ranks), TEXT(" | "))));

			FArchetypeTestWorld World{ Spawner };
			Owner = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			ASSERT_THAT(IsTrue(Keeper().Summon(*Owner->GetAbilitySystemComponent(), ArchetypeTestId(TEXT("test_pet")))));
			UVeyraCombatEventSubsystem* Events = Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>();
			DealtHandle = Events->OnDamageDealt.AddLambda([this](const FVeyraDamageDealtEvent& Event) { Dealt.Add(Event); });
		}

		AFTER_EACH()
		{
			Spawner.GetWorld().GetSubsystem<UVeyraCombatEventSubsystem>()->OnDamageDealt.Remove(DealtHandle);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		static FVeyraDamageTuning DamageOf(double Amount)
		{
			FVeyraDamageTuning Damage;
			Damage.Type = EVeyraDamageType::TrueDamage;
			Damage.AmountByRank = { Amount };
			return Damage;
		}

		static FVeyraAreaZoneTuning ZoneOf(double Radius, double Amount)
		{
			FVeyraAreaZoneTuning Zone;
			Zone.Shape = CircleOf(Radius);
			Zone.Effects.Damage.Add(DamageOf(Amount));
			return Zone;
		}

		UVeyraCompanionSubsystem& Keeper()
		{
			return *Spawner.GetWorld().GetSubsystem<UVeyraCompanionSubsystem>();
		}

		AVeyraCompanion& Pet()
		{
			return *Keeper().Find(*Owner->GetAbilitySystemComponent());
		}

		/** Time passes; the companion's forced moves go on with it, as its physics would move them. */
		void Wait(double Seconds)
		{
			UWorld& World = Spawner.GetWorld();
			const double Until = World.GetTimeSeconds() + Seconds;
			while (World.GetTimeSeconds() < Until)
			{
				World.Tick(LEVELTICK_TimeOnly, CompanionKitFixture::Step);
				Pet().GetVeyraMovement()->AdvanceForcedMove(CompanionKitFixture::Step);
				++GFrameCounter;
				World.GetTimerManager().Tick(CompanionKitFixture::Step);
			}
		}

		/** The damage Unit took from Source's hits of Delivery. */
		double TakenFrom(const AActor& Unit, const UAbilitySystemComponent* Source, EVeyraDamageDelivery Delivery) const
		{
			double Sum = 0.0;
			for (const FVeyraDamageDealtEvent& Event : Dealt)
			{
				const UAbilitySystemComponent* Target = Event.Target.Get();
				if (Target && Target->GetAvatarActor() == &Unit && Event.Source.Get() == Source && Event.Delivery == Delivery)
				{
					Sum += Event.Total();
				}
			}
			return Sum;
		}

		UAbilitySystemComponent* PetAbilities()
		{
			return Pet().GetAbilitySystemComponent();
		}

		TEST_METHOD(AHoldSendsItLeapingAndItsLandingIsItsOwnHit)
		{
			using namespace CompanionKitFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_hunt")))));
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Prey = World.SpawnFluxborn(EVeyraTeam::B, FVector(Point, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::W, Prey.GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Pet().GetMode() == EVeyraCompanionMode::Hold));
			Wait(Point / Leap + 3.0 * Step);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TakenFrom(Prey, PetAbilities(), EVeyraDamageDelivery::Ability), Hit, Tolerance),
				TEXT("its landing hits, as the companion's own hit")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(TakenFrom(Prey, Owner->GetAbilitySystemComponent(), EVeyraDamageDelivery::Ability)), TEXT("not its owner's")));
		}

		TEST_METHOD(ItsRecastRecallsItAndTheHoldsEndClosesTheRecall)
		{
			using namespace CompanionKitFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_hunt")))));
			const UVeyraAbilityLoadoutComponent* Loadout = Owner->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::W, FVector(Point, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsNotNull(Loadout->FindOverride(EVeyraAbilitySlot::W), TEXT("its slot offers the recall")));
			Wait(Point / Leap + 3.0 * Step);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::W, Owner->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Pet().GetMode() == EVeyraCompanionMode::Follow, TEXT("recalled")));

			// Sent again once ready, its hold runs out, and the recall it offered closes with it.
			Wait(Cooldown);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::W, FVector(Point, 0.0, 0.0)) == EVeyraCastRejection::None));
			Wait(Point / Leap + Hold + 3.0 * Step);
			ASSERT_THAT(IsTrue(Pet().GetMode() == EVeyraCompanionMode::Follow, TEXT("its hold ran out")));
			ASSERT_THAT(IsNull(Loadout->FindOverride(EVeyraAbilitySlot::W), TEXT("and its recall closed")));
		}

		TEST_METHOD(ACastThatNeedsItsCompanionIsRefusedWhileItIsGone)
		{
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_cross")))));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CompanionKitFixture::Point * 3.0, 0.0, 0.0));
			FVeyraRawDamageEvent Blow;
			Blow.Components.Add({ EVeyraDamageType::TrueDamage, CompanionKitFixture::Lethal });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Enemy.GetAbilitySystemComponent(), *PetAbilities(), Blow)));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::E, Owner->GetActorLocation()) == EVeyraCastRejection::NoCompanion));
		}

		TEST_METHOD(ASwapExchangesPlacesAndEachDepartureEruptsAsItsOwnersHit)
		{
			using namespace CompanionKitFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_cross")))));
			// The companion holds a point ahead, an enemy beside each of them.
			const FVector Ahead(Point, 0.0, 0.0);
			Pet().SetActorLocation(Ahead + FVector(0.0, 0.0, Pet().GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			const FVector OwnerFrom = Owner->GetActorLocation();
			const FVector PetFrom = Pet().GetActorLocation();
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& NearOwner = World.SpawnFluxborn(EVeyraTeam::B, OwnerFrom + FVector(0.0, Aside, 0.0));
			AVeyraTestFluxborn& NearPet = World.SpawnFluxborn(EVeyraTeam::B, PetFrom + FVector(0.0, Aside, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::E, Owner->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Owner->GetActorLocation(), PetFrom) < Zone && FVector::Dist2D(Pet().GetActorLocation(), OwnerFrom) < Zone,
				TEXT("they exchange places")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TakenFrom(NearOwner, Owner->GetAbilitySystemComponent(), EVeyraDamageDelivery::Ability), Hit, Tolerance),
				TEXT("where the owner left erupts as its hit")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TakenFrom(NearPet, PetAbilities(), EVeyraDamageDelivery::Ability), Hit, Tolerance),
				TEXT("and where the companion left, as the companion's")));
		}

		TEST_METHOD(AReactionBurstsAroundItsMarkedTargetAndSparesIt)
		{
			using namespace CompanionKitFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_bolt")))));
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Marked = World.SpawnFluxborn(EVeyraTeam::B, FVector(Point, 0.0, 0.0));
			AVeyraTestFluxborn& Beside = World.SpawnFluxborn(EVeyraTeam::B, FVector(Point, Aside, 0.0));
			const TOptional<FVeyraStatusSpec> Bitten = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_bitten")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*PetAbilities(), *Marked.GetAbilitySystemComponent(), Bitten.GetValue())));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::Q, Marked.GetActorLocation()) == EVeyraCastRejection::None));
			UAbilitySystemComponent* Own = Owner->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TakenFrom(Marked, Own, EVeyraDamageDelivery::Ability), Hit, Tolerance), TEXT("the target takes the bolt alone")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TakenFrom(Beside, Own, EVeyraDamageDelivery::Ability), Hit, Tolerance), TEXT("the one beside it, the burst")));
			// Spent, the mark sets off no second burst.
			Dealt.Reset();
			Wait(Step * 2.0);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::Q, Marked.GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(TakenFrom(Beside, Own, EVeyraDamageDelivery::Ability))));
		}

		TEST_METHOD(AHoldRefusesAPointThatIsNoPlace)
		{
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_hunt")))));
			for (const double Nowhere : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() })
			{
				FVeyraCastTarget Target;
				Target.bHasLocation = true;
				Target.Location = FVector(Nowhere, 0.0, 0.0);
				ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Owner->GetAbilitySystemComponent(), EVeyraAbilitySlot::W, Target) == EVeyraCastRejection::InvalidLocation));
			}
			ASSERT_THAT(IsTrue(Pet().GetMode() == EVeyraCompanionMode::Follow, TEXT("and it stays at its owner's side")));
		}

		/** An enemy roots the companion: it cannot move by walking or by its own abilities. */
		bool RootThePet()
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CompanionKitFixture::Point * 3.0, 0.0, 0.0));
			const TOptional<FVeyraStatusSpec> Rooted = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_rooted")));
			return VeyraCombat::ApplyStatus(*Enemy.GetAbilitySystemComponent(), *PetAbilities(), Rooted.GetValue())
				&& EnumHasAnyFlags(VeyraCombat::GetActionBlocks(*PetAbilities()), EVeyraActionBlocks::Dash);
		}

		TEST_METHOD(ItsLeapWaitsWhileItCannotMove)
		{
			using namespace CompanionKitFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_hunt")))));
			ASSERT_THAT(IsTrue(RootThePet()));
			const FVector PetAt = Pet().GetActorLocation();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::W, FVector(Point, 0.0, 0.0)) == EVeyraCastRejection::CrowdControlled,
				TEXT("a rooted companion cannot leap")));
			Wait(Point / Leap + 3.0 * Step);
			ASSERT_THAT(IsTrue(Pet().GetMode() == EVeyraCompanionMode::Follow && Pet().GetActorLocation().Equals(PetAt), TEXT("so it holds nothing and stays")));
		}

		TEST_METHOD(ItsSwapWaitsWhileItCannotMove)
		{
			using namespace CompanionKitFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_cross")))));
			Pet().SetActorLocation(FVector(Point, 0.0, Pet().GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			ASSERT_THAT(IsTrue(RootThePet()));
			const FVector OwnerAt = Owner->GetActorLocation();
			const FVector PetAt = Pet().GetActorLocation();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::E, OwnerAt) == EVeyraCastRejection::CrowdControlled,
				TEXT("a rooted companion cannot swap places")));
			ASSERT_THAT(IsTrue(Owner->GetActorLocation().Equals(OwnerAt) && Pet().GetActorLocation().Equals(PetAt), TEXT("so nobody moves")));
		}

		TEST_METHOD(ABurstStrikesAnEnemyTheCasterCannotSee)
		{
			using namespace CompanionKitFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_bolt")))));
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Marked = World.SpawnFluxborn(EVeyraTeam::B, FVector(Point, 0.0, 0.0));
			AVeyraTestFluxborn& Unseen = World.SpawnFluxborn(EVeyraTeam::B, FVector(Point, Aside, 0.0));
			UVeyraVisibilityRegistry* Registry = Spawner.GetWorld().GetSubsystem<UVeyraVisibilityRegistry>();
			FKitHidingVisibility Vision(Unseen);
			Registry->Register(Vision);
			const TOptional<FVeyraStatusSpec> Bitten = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_bitten")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*PetAbilities(), *Marked.GetAbilitySystemComponent(), Bitten.GetValue())));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::Q, Marked.GetActorLocation()) == EVeyraCastRejection::None));
			const double Burst = TakenFrom(Unseen, Owner->GetAbilitySystemComponent(), EVeyraDamageDelivery::Ability);
			Registry->Unregister(Vision);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Burst, Hit, Tolerance), TEXT("a burst is no target: it strikes what it reaches, seen or not")));
		}

		TEST_METHOD(AChainStrikesAnEnemyTheCasterCannotSee)
		{
			using namespace CompanionKitFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_leash")))));
			Pet().SetActorLocation(FVector(Point, 0.0, Pet().GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Crossing = World.SpawnFluxborn(EVeyraTeam::B, FVector(Point / 2.0, 0.0, 0.0));
			UVeyraVisibilityRegistry* Registry = Spawner.GetWorld().GetSubsystem<UVeyraVisibilityRegistry>();
			FKitHidingVisibility Vision(Crossing);
			Registry->Register(Vision);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::E, Owner->GetActorLocation()) == EVeyraCastRejection::None));
			const double Chained = TakenFrom(Crossing, Owner->GetAbilitySystemComponent(), EVeyraDamageDelivery::Ability);
			Registry->Unregister(Vision);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Chained, Hit, Tolerance), TEXT("the chain strikes what crosses it, seen or not")));
		}

		TEST_METHOD(ASpellShieldBlocksABurstOnce)
		{
			using namespace CompanionKitFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_bolt")))));
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Marked = World.SpawnFluxborn(EVeyraTeam::B, FVector(Point, 0.0, 0.0));
			AVeyraTestFluxborn& Shielded = World.SpawnFluxborn(EVeyraTeam::B, FVector(Point, Aside, 0.0));
			UAbilitySystemComponent& Guard = *Shielded.GetAbilitySystemComponent();
			const TOptional<FVeyraStatusSpec> Shield = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_spell_shield")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Guard, Guard, Shield.GetValue())));
			const TOptional<FVeyraStatusSpec> Bitten = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_bitten")));
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(*PetAbilities(), *Marked.GetAbilitySystemComponent(), Bitten.GetValue())));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::Q, Marked.GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyZero(TakenFrom(Shielded, Owner->GetAbilitySystemComponent(), EVeyraDamageDelivery::Ability)), TEXT("the shield takes the burst")));
			ASSERT_THAT(IsFalse(FArchetypeTestWorld::Has(Shielded, TEXT("test_spell_shield")), TEXT("and is spent")));
		}

		TEST_METHOD(ABuffEmpowersItChainsItAndEndsWithIt)
		{
			using namespace CompanionKitFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_leash")))));
			const FVector Ahead(Point, 0.0, 0.0);
			Pet().SetActorLocation(Ahead + FVector(0.0, 0.0, Pet().GetActorLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
			const double Before = PetAbilities()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Crossing = World.SpawnFluxborn(EVeyraTeam::B, FVector(Point / 2.0, 0.0, 0.0));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::E, Owner->GetActorLocation()) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(PetAbilities()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()), Before * (1.0 + Growth), Tolerance),
				TEXT("its true form")));
			ASSERT_THAT(IsTrue(Pet().IsChained()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TakenFrom(Crossing, Owner->GetAbilitySystemComponent(), EVeyraDamageDelivery::Ability), Hit, Tolerance),
				TEXT("an enemy on the chain takes it")));
			Wait(PerEnemy / 2.0);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(TakenFrom(Crossing, Owner->GetAbilitySystemComponent(), EVeyraDamageDelivery::Ability), Hit, Tolerance),
				TEXT("no more often than its rate")));

			// Killed, the companion takes the buff with it.
			AVeyraVanguardCharacter& Hunter = World.Spawn(EVeyraTeam::B, FVector(Point * 3.0, 0.0, 0.0));
			FVeyraRawDamageEvent Blow;
			Blow.Components.Add({ EVeyraDamageType::TrueDamage, Lethal });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Hunter.GetAbilitySystemComponent(), *PetAbilities(), Blow)));
			ASSERT_THAT(IsFalse(VeyraCombat::HasStatusFrom(Owner, ArchetypeTestId(TEXT("test_leashed")), *Owner->GetAbilitySystemComponent()), TEXT("the buff ends")));
			ASSERT_THAT(IsFalse(Pet().IsChained()));
		}

		TEST_METHOD(ACooldownStartedWhileAStatusHoldsIsScaled)
		{
			using namespace CompanionKitFixture;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Owner, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_hunt")))));
			const UVeyraCooldownComponent* Cooldowns = Owner->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			const TOptional<FVeyraStatusSpec> Leashed = UVeyraAbilitiesTuningSubsystem::FindStatus(ArchetypeTestId(TEXT("test_leashed")));
			UAbilitySystemComponent& Own = *Owner->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Own, Own, Leashed.GetValue())));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Owner, EVeyraAbilitySlot::W, FVector(Point, 0.0, 0.0)) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Cooldowns->GetDurationSeconds(ArchetypeTestId(TEXT("test_hunt"))), Cooldown * Halved, Tolerance)));
		}

		TEST_METHOD(ValidationKeepsCommandsAndCompanionPartsInShape)
		{
			FVeyraAbilitiesTuning Bad = Tuning;
			Bad.Command.FindChecked(ArchetypeTestId(TEXT("test_hunt"))).LeapSpeed = 0.0;
			Bad.Command.FindChecked(ArchetypeTestId(TEXT("test_hunt_recall"))).HoldSeconds = 1.0;
			Bad.SelfBuff.FindChecked(ArchetypeTestId(TEXT("test_leash"))).Cast.NeedsCompanion = EVeyraCompanionNeed::None;
			Bad.Blink.FindChecked(ArchetypeTestId(TEXT("test_cross"))).Swap = EVeyraBlinkSwap::None;
			const int32 Ranks[] = { 5, 3 };
			ASSERT_THAT(AreEqual(4, VeyraAbilityRules::Validate(Bad, Ranks).Num()));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
