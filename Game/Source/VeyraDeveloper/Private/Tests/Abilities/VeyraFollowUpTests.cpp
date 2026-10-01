// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Delivery/VeyraProjectile.h"
#include "EngineUtils.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraAbilitiesTuning.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraAbilitiesTests
{
	// Veyra.Abilities.FollowUps.*: dashes through a target, follow-ups a mark or a takedown opens, and a shot
	// that comes back (ADR-030 §6-§8). Fixture values throughout.
	TEST_CLASS(FollowUps, "Veyra.Abilities")
	{
		static constexpr double Reach = 600.0;
		static constexpr double Beyond = 200.0;
		static constexpr double DashSpeed = 1500.0;
		static constexpr double Cooldown = 10.0;
		static constexpr double Window = 3.0;
		static constexpr double Blow = 40.0;
		static constexpr double ShotSpeed = 1200.0;
		static constexpr double Refund = 0.5;
		static constexpr double LongSeconds = 60.0;
		static constexpr double Tolerance = 1.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;
		AVeyraVanguardCharacter* Enemy = nullptr;

		static FVeyraContentId Id(const TCHAR* Text) { return ArchetypeTestId(Text); }

		static FVeyraEffectBundleTuning Hurts()
		{
			FVeyraEffectBundleTuning Effects;
			FVeyraDamageTuning& Damage = Effects.Damage.AddDefaulted_GetRef();
			Damage.Type = EVeyraDamageType::Magic;
			Damage.AmountByRank = { Blow };
			return Effects;
		}

		BEFORE_EACH()
		{
			// A dash through its target, and a second one that opens only against a marked target and takes only it.
			FVeyraDashAbilityTuning Tag;
			Tag.Cast = InstantCast(Reach, Cooldown, 0.0);
			FVeyraRecastTuning& Again = Tag.Cast.RecastWindow.AddDefaulted_GetRef();
			Again.Ability = Id(TEXT("test_tag_again"));
			Again.WindowSeconds = Window;
			Again.OpensWhen = EVeyraRecastCondition::TargetHeld;
			Again.HeldStatus = { Id(TEXT("test_mark")) };
			Tag.Direction = EVeyraDashDirection::ThroughTarget;
			Tag.Distance = Beyond;
			Tag.Speed = DashSpeed;
			Tag.ContactEffects = Hurts();
			Tuning.Dash.Add(Id(TEXT("test_tag")), Tag);
			FVeyraDashAbilityTuning TagAgain = Tag;
			TagAgain.Cast.RecastWindow.Reset();
			TagAgain.Cast.TargetMustHold = { Id(TEXT("test_mark")) };
			Tuning.Dash.Add(Id(TEXT("test_tag_again")), TagAgain);

			// A ball that comes back from a marked target.
			FVeyraSkillshotAbilityTuning Ball;
			Ball.Cast = InstantCast(Reach, Cooldown, 0.0);
			Ball.Projectile.Speed = ShotSpeed;
			Ball.Projectile.Radius = 30.0;
			Ball.Projectile.Range = Reach;
			Ball.Effects = Hurts();
			FVeyraReturnShotTuning& Return = Ball.ReturnIfHeld.AddDefaulted_GetRef();
			Return.Status = Id(TEXT("test_mark"));
			Return.Speed = ShotSpeed;
			Return.CooldownRefund = Refund;
			Tuning.Skillshot.Add(Id(TEXT("test_ball")), Ball);
			Tuning.Statuses.Add(Id(TEXT("test_mark")), StatusOf(EVeyraStatusKind::Counter, 0.0, LongSeconds));
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
			Enemy = &World.Spawn(EVeyraTeam::B, FVector(Reach / 2.0, 0.0, 0.0));
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		EVeyraCastRejection CastOn(EVeyraAbilitySlot Slot, AActor& Target) const
		{
			FVeyraCastTarget On;
			On.Actor = &Target;
			On.bHasLocation = true;
			On.Location = Target.GetActorLocation();
			return VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), Slot, On);
		}

		void Mark(AActor& Unit) const
		{
			FVeyraStatusSpec Spec;
			Spec.Id = Id(TEXT("test_mark"));
			Spec.Kind = EVeyraStatusKind::Counter;
			Spec.DurationSeconds = LongSeconds;
			VeyraCombat::ApplyStatus(*Caster->GetAbilitySystemComponent(), *UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit), Spec);
		}

		FVeyraContentId Holds(EVeyraAbilitySlot Slot) const
		{
			const FVeyraLoadoutEntry* Entry = Caster->GetPlayerState()->FindComponentByClass<UVeyraAbilityLoadoutComponent>()->FindSlot(Slot);
			return Entry ? Entry->Ability : FVeyraContentId();
		}

		TEST_METHOD(ADashThroughATargetPassesItAndItTakesTheContactEffects)
		{
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::E, Id(TEXT("test_tag")))));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, *Enemy) == EVeyraCastRejection::None));
			const UVeyraMovementComponent& Movement = *Caster->GetVeyraMovement();
			ASSERT_THAT(IsTrue(Movement.IsDashing()));
			const TOptional<FVector> End = Movement.GetForcedMoveDestination();
			ASSERT_THAT(IsTrue(End.IsSet() && FMath::IsNearlyEqual(End->X, Enemy->GetActorLocation().X + Beyond, Tolerance),
				FString::Printf(TEXT("to %s, past the target"), End.IsSet() ? *End->ToString() : TEXT("nowhere"))));
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::HealthLost(*Enemy) > 0.0, TEXT("the unit it passes takes its effects")));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::E) == Id(TEXT("test_tag")), TEXT("no follow-up: the target held no mark")));
		}

		TEST_METHOD(AMarkedTargetOpensTheFollowUpWhichTakesOnlyAMarkedTarget)
		{
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::E, Id(TEXT("test_tag")))));
			Mark(*Enemy);
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, *Enemy) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::E) == Id(TEXT("test_tag_again")), TEXT("the target held the mark as the cast committed")));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Other = World.Spawn(EVeyraTeam::B, FVector(0.0, Reach / 2.0, 0.0));
			// Landed where it is: a dash under way holds back another (ADR-031 §7).
			ASSERT_THAT(IsTrue(VeyraCombat::Blink(*Caster->GetAbilitySystemComponent(), Caster->GetActorLocation())));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, Other) == EVeyraCastRejection::InvalidTarget, TEXT("the follow-up takes only a marked target")));
		}

		TEST_METHOD(ATakedownOpensAFollowUpAndSomeoneElsesDoesNot)
		{
			FVeyraDashAbilityTuning& Tag = Tuning.Dash.FindChecked(Id(TEXT("test_tag")));
			Tag.Cast.RecastWindow[0].OpensWhen = EVeyraRecastCondition::TargetFalls;
			Tag.Cast.RecastWindow[0].HeldStatus.Reset();
			Tag.Cast.RecastWindow[0].FallsWithinSeconds = Window;
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::E, Id(TEXT("test_tag")))));
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Ally = World.Spawn(EVeyraTeam::A, FVector(0.0, -Reach / 2.0, 0.0));
			AVeyraVanguardCharacter& Other = World.Spawn(EVeyraTeam::B, FVector(0.0, Reach / 2.0, 0.0));
			FVeyraRawDamageEvent Lethal;
			Lethal.Components.Add({ EVeyraDamageType::TrueDamage, 100000.0 });

			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, Other) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Ally.GetAbilitySystemComponent(), *Other.GetAbilitySystemComponent(), Lethal)));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::E) == Id(TEXT("test_tag")), TEXT("an ally's kill opens nothing")));

			Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>()->ClearCooldown(Id(TEXT("test_tag")));
			// Landed where it is: a dash under way holds back another (ADR-031 §7).
			ASSERT_THAT(IsTrue(VeyraCombat::Blink(*Caster->GetAbilitySystemComponent(), Caster->GetActorLocation())));
			ASSERT_THAT(IsTrue(CastOn(EVeyraAbilitySlot::E, *Enemy) == EVeyraCastRejection::None));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::E) == Id(TEXT("test_tag")), TEXT("not while the target stands")));
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Caster->GetAbilitySystemComponent(), *Enemy->GetAbilitySystemComponent(), Lethal)));
			ASSERT_THAT(IsTrue(Holds(EVeyraAbilitySlot::E) == Id(TEXT("test_tag_again")), TEXT("the caster's takedown opens it")));
		}

		TEST_METHOD(AShotComesBackFromAMarkedTargetAndCatchingItRefundsItsCooldown)
		{
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::Learn(*Caster, EVeyraAbilitySlot::Q, Id(TEXT("test_ball")))));
			UVeyraCooldownComponent& Cooldowns = *Caster->GetPlayerState()->FindComponentByClass<UVeyraCooldownComponent>();
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, Enemy->GetActorLocation()) == EVeyraCastRejection::None));
			TActorIterator<AVeyraProjectile> First(&Spawner.GetWorld());
			ASSERT_THAT(IsTrue(static_cast<bool>(First)));
			First->AdvanceBy(Reach / ShotSpeed);
			int32 Flying = 0;
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				Flying += It->IsActorBeingDestroyed() ? 0 : 1;
			}
			ASSERT_THAT(AreEqual(0, Flying, TEXT("an unmarked target sends nothing back")));

			Cooldowns.ClearCooldown(Id(TEXT("test_ball")));
			Mark(*Enemy);
			ASSERT_THAT(IsTrue(FArchetypeTestWorld::CastAt(*Caster, EVeyraAbilitySlot::Q, Enemy->GetActorLocation()) == EVeyraCastRejection::None));
			const double Before = Cooldowns.GetRemainingSecondsNow(Id(TEXT("test_ball")));
			TArray<AVeyraProjectile*> Shots;
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				if (!It->IsActorBeingDestroyed())
				{
					Shots.Add(*It);
				}
			}
			ASSERT_THAT(AreEqual(1, Shots.Num()));
			Shots[0]->AdvanceBy(Reach / ShotSpeed);
			AVeyraProjectile* Back = nullptr;
			for (TActorIterator<AVeyraProjectile> It(&Spawner.GetWorld()); It; ++It)
			{
				if (!It->IsActorBeingDestroyed() && It->GetHomingTarget() == Caster)
				{
					Back = *It;
				}
			}
			ASSERT_THAT(IsNotNull(Back, TEXT("the ball flies back to its caster")));
			Back->AdvanceBy(Reach / ShotSpeed);
			const double After = Cooldowns.GetRemainingSecondsNow(Id(TEXT("test_ball")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(After, Before * (1.0 - Refund), 0.01), FString::Printf(TEXT("caught: %g of %g left"), After, Before)));
		}

		TEST_METHOD(ValidationKeepsEachFollowUpsFieldsToItsCondition)
		{
			constexpr int32 RankCounts[] = { 5, 3 };
			FVeyraAbilitiesTuning Broken = Tuning;
			FVeyraDashAbilityTuning& Tag = Broken.Dash.FindChecked(Id(TEXT("test_tag")));
			Tag.Contact = EVeyraDashContact::StopAtFirstEnemy;
			Tag.Cast.RecastWindow[0].HeldStatus.Reset();
			Tag.Cast.RecastWindow[0].FallsWithinSeconds = Window;
			Broken.Skillshot.FindChecked(Id(TEXT("test_ball"))).ReturnIfHeld[0].CooldownRefund = 2.0;
			const FString All = FString::Join(VeyraAbilityRules::Validate(Broken, RankCounts), TEXT(" | "));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/dash/test_tag/direction")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/dash/test_tag/cast/recastWindow/0/heldStatus")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/dash/test_tag/cast/recastWindow/0/fallsWithinSeconds")), All));
			ASSERT_THAT(IsTrue(All.Contains(TEXT("/skillshot/test_ball/returnIfHeld/0")), All));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
