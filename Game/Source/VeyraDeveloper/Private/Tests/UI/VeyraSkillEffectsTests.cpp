// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Components/ActorTestSpawner.h"
#include "Kit/VeyraKitPresentationSettings.h"
#include "Kit/VeyraSkillEffectsSubsystem.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraVanguardCharacter.h"

namespace VeyraSkillEffectsTests
{
	// Veyra.UI.SkillEffects.*: what an ability's own effects read from fixture tuning, and which recipes the kit
	// presentation settings accept (ADR-072 §4).
	TEST_CLASS(SkillEffects, "Veyra.UI")
	{
		static constexpr double BeamLength = 1300.0;
		static constexpr double SlamRadius = 225.0;
		static constexpr double InnerRadius = 100.0;
		static constexpr double LanceRange = 1000.0;
		static constexpr double FallDelay = 0.9;
		static constexpr double Slack = 1e-6;

		static FVeyraContentId Id(const TCHAR* Name)
		{
			return FVeyraContentId::FromText(Name).GetValue();
		}

		/** A beam (a rectangle), a slam (a circle within a circle), a delayed fall and a lance (a projectile). */
		static FVeyraAbilitiesTuning Tuning()
		{
			FVeyraAbilitiesTuning Tuning;
			FVeyraAreaAbilityTuning& Beam = Tuning.Area.Add(Id(TEXT("test_beam")));
			FVeyraShape& Line = Beam.Zones.AddDefaulted_GetRef().Shape;
			Line.Kind = EVeyraShapeKind::Rectangle;
			Line.Length = BeamLength;
			FVeyraAreaAbilityTuning& Slam = Tuning.Area.Add(Id(TEXT("test_slam")));
			Slam.Zones.AddDefaulted_GetRef().Shape.Radius = InnerRadius;
			Slam.Zones.AddDefaulted_GetRef().Shape.Radius = SlamRadius;
			FVeyraAreaAbilityTuning& Fall = Tuning.Area.Add(Id(TEXT("test_fall")));
			Fall.DelaySeconds = FallDelay;
			Fall.Zones.AddDefaulted_GetRef().Shape.Radius = SlamRadius;
			Tuning.Skillshot.Add(Id(TEXT("test_lance"))).Projectile.Range = LanceRange;
			return Tuning;
		}

		/** A recipe with one commit effect, which the settings accept. */
		static FVeyraAbilityEffects Recipe(const TCHAR* Ability)
		{
			FVeyraAbilityEffects Effects;
			Effects.Ability = Ability;
			Effects.Commit.Effect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Test/NS_Test.NS_Test")));
			Effects.Commit.Scale = 1.0f;
			return Effects;
		}

		/** The settings' problems with Recipes as their only ability effects. */
		static TArray<FString> ProblemsWith(const TArray<FVeyraAbilityEffects>& Recipes)
		{
			UVeyraKitPresentationSettings* Settings = DuplicateObject(GetDefault<UVeyraKitPresentationSettings>(), GetTransientPackage());
			Settings->AbilityEffects = Recipes;
			return Settings->Validate();
		}

		TEST_METHOD(AChannelRunsTheAbilitysOwnReach)
		{
			const FVeyraAbilitiesTuning Fixture = Tuning();
			ASSERT_THAT(IsNear(VeyraSkillEffects::ReachOf(Fixture, Id(TEXT("test_beam"))), BeamLength, Slack, TEXT("a rectangle's length")));
			ASSERT_THAT(IsNear(VeyraSkillEffects::ReachOf(Fixture, Id(TEXT("test_slam"))), SlamRadius, Slack, TEXT("the outermost zone's radius")));
			ASSERT_THAT(IsNear(VeyraSkillEffects::ReachOf(Fixture, Id(TEXT("test_lance"))), LanceRange, Slack, TEXT("a projectile's range")));
			ASSERT_THAT(IsNear(VeyraSkillEffects::ReachOf(Fixture, Id(TEXT("test_unknown"))), 0.0, Slack));
		}

		TEST_METHOD(ADelayedAreaLandsAfterItsOwnDelay)
		{
			const FVeyraAbilitiesTuning Fixture = Tuning();
			ASSERT_THAT(IsNear(VeyraSkillEffects::LandingDelayOf(Fixture, Id(TEXT("test_fall"))), FallDelay, Slack));
			ASSERT_THAT(IsNear(VeyraSkillEffects::LandingDelayOf(Fixture, Id(TEXT("test_slam"))), 0.0, Slack, TEXT("one that lands at once shows its commit")));
			ASSERT_THAT(IsNear(VeyraSkillEffects::LandingDelayOf(Fixture, Id(TEXT("test_lance"))), 0.0, Slack));
		}

		TEST_METHOD(ARecipeNeedsAStageAScaleAndBonesForItsWindup)
		{
			ASSERT_THAT(IsTrue(ProblemsWith({ Recipe(TEXT("test_ok")) }).IsEmpty()));
			FVeyraAbilityEffects Empty;
			Empty.Ability = TEXT("test_empty");
			ASSERT_THAT(IsFalse(ProblemsWith({ Empty }).IsEmpty(), TEXT("a recipe shows at least one stage")));
			FVeyraAbilityEffects Unscaled = Recipe(TEXT("test_unscaled"));
			Unscaled.Commit.Scale = 0.0f;
			ASSERT_THAT(IsFalse(ProblemsWith({ Unscaled }).IsEmpty()));
			FVeyraAbilityEffects Boneless = Recipe(TEXT("test_boneless"));
			Boneless.Windup = Boneless.Commit;
			ASSERT_THAT(IsFalse(ProblemsWith({ Boneless }).IsEmpty(), TEXT("a windup pours from the bones it names")));
			Boneless.WindupBones = { TEXT("hand_r") };
			ASSERT_THAT(IsTrue(ProblemsWith({ Boneless }).IsEmpty()));
			FVeyraAbilityEffects Aimed = Recipe(TEXT("test_aimed"));
			Aimed.Commit = FVeyraSkillEffectStage();
			Aimed.Impact = Recipe(TEXT("test_aimed")).Commit;
			Aimed.bCommitAtTarget = true;
			ASSERT_THAT(IsFalse(ProblemsWith({ Aimed }).IsEmpty(), TEXT("only a commit effect is placed at the target")));
			ASSERT_THAT(IsFalse(ProblemsWith({ Recipe(TEXT("test_twice")), Recipe(TEXT("TEST_TWICE")) }).IsEmpty(), TEXT("each ability once, ignoring case")));
		}

		TEST_METHOD(ATargetedCommitPlaysOnTheAimedGroundWhateverTheCastersHeight)
		{
			const FVector Caster(0.0, 0.0, 300.0);
			const FVector Aimed(600.0, 100.0, -50.0);
			ASSERT_THAT(IsTrue(VeyraSkillEffects::CommitPlacement(Caster, Aimed, true).Equals(Aimed), TEXT("its height is the ground's where it was aimed")));
			ASSERT_THAT(IsTrue(VeyraSkillEffects::CommitPlacement(Caster, Aimed, false).Equals(Caster)));
		}

		TEST_METHOD(AnImpactShowsOnlyWhereAProjectileThisMachineDrewEnded)
		{
			constexpr double Window = 0.5;
			constexpr double Speed = 1600.0;
			constexpr double SeenAt = 10.0;
			FVeyraSeenProjectile Seen;
			Seen.SeenAt = SeenAt;
			Seen.At = FVector(500.0, 0.0, 0.0);
			Seen.Speed = Speed;
			// The server's end may lie behind where it was last drawn (drawn ahead of a hit) or ahead of it (removed first).
			ASSERT_THAT(IsTrue(VeyraSkillEffects::SawItEnd(Seen, FVector(420.0, 0.0, 0.0), SeenAt + 0.05, Window)));
			ASSERT_THAT(IsTrue(VeyraSkillEffects::SawItEnd(Seen, FVector(500.0 + Speed * Window * 0.5, 0.0, 0.0), SeenAt + Window * 0.9, Window)));
			// Not once its window has passed (it left sight long since), nor farther than it flies in its window.
			ASSERT_THAT(IsFalse(VeyraSkillEffects::SawItEnd(Seen, Seen.At, SeenAt + Window * 2.0, Window)));
			ASSERT_THAT(IsFalse(VeyraSkillEffects::SawItEnd(Seen, FVector(500.0 + Speed * Window * 2.0, 0.0, 0.0), SeenAt + 0.05, Window)));
		}

		TEST_METHOD(EveryShippedRecipeNamesAnAbility)
		{
			const UVeyraKitPresentationSettings& Settings = *GetDefault<UVeyraKitPresentationSettings>();
			const TArray<FString> Problems = Settings.Validate();
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), *FString::Join(Problems, TEXT("; "))));
			const FVeyraAbilitiesTuning& Committed = UVeyraAbilitiesTuningSubsystem::Get();
			for (const FVeyraAbilityEffects& Effects : Settings.AbilityEffects)
			{
				const TOptional<FVeyraContentId> Ability = FVeyraContentId::FromText(Effects.Ability.ToString().ToLower());
				ASSERT_THAT(IsTrue(Ability.IsSet() && VeyraAbilityRules::Defines(Committed, *Ability),
					*FString::Printf(TEXT("%s is in Abilities.json"), *Effects.Ability.ToString())));
				ASSERT_THAT(IsTrue(!Effects.Channel.IsSet() || VeyraSkillEffects::ReachOf(Committed, *Ability) > 0.0,
					*FString::Printf(TEXT("%s's channel runs a reach Abilities.json gives"), *Effects.Ability.ToString())));
			}
		}
	};

	// Veyra.UI.SkillEffectsWorld.*: an ability's own stages drawn from what a client sees in a world (ADR-072 §4).
	TEST_CLASS(SkillEffectsWorld, "Veyra.UI")
	{
		static constexpr double Range = 1000.0;
		static constexpr double LongSeconds = 60.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			// A bolt whose windup lasts the whole test.
			FVeyraSkillshotAbilityTuning Charged;
			Charged.Cast = VeyraAbilitiesTests::InstantCast(Range, LongSeconds, 0.0);
			Charged.Cast.WindupSeconds = LongSeconds;
			Charged.Projectile = FVeyraProjectileTuning{ Range, Range / 10.0, Range };
			Charged.Collision = EVeyraSkillshotCollision::Pierce;
			Tuning.Skillshot.Add(VeyraAbilitiesTests::ArchetypeTestId(TEXT("test_charged")), Charged);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);
			// Its own windup stage, shown on its caster's hand.
			FVeyraAbilityEffects Effects;
			Effects.Ability = TEXT("test_charged");
			Effects.Windup.Effect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Veyra/UI/Presentation/Effects/NS_VeyraChargeSwirl.NS_VeyraChargeSwirl")));
			Effects.Windup.Scale = 1.0f;
			Effects.WindupBones = { TEXT("hand_r") };
			GetMutableDefault<UVeyraKitPresentationSettings>()->AbilityEffects.Add(Effects);
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		AFTER_EACH()
		{
			GetMutableDefault<UVeyraKitPresentationSettings>()->AbilityEffects.RemoveAll(
				[](const FVeyraAbilityEffects& Effects) { return Effects.Ability == TEXT("test_charged"); });
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		TEST_METHOD(ACasterFirstSeenMidWindupShowsItsWindupStage)
		{
			// No cue began this windup, as for a caster first seen out of the fog, or by a client joining mid-cast.
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, VeyraAbilitiesTests::ArchetypeTestId(TEXT("test_charged")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::Q, FVector(Range, 0.0, 0.0)) == EVeyraCastRejection::None));
			UVeyraSkillEffectsSubsystem* Skills = Spawner.GetWorld().GetSubsystem<UVeyraSkillEffectsSubsystem>();
			ASSERT_THAT(IsNotNull(Skills));
			ASSERT_THAT(IsFalse(Skills->IsShowing(*Caster)));
			Skills->Refresh();
			ASSERT_THAT(IsTrue(Skills->IsShowing(*Caster), TEXT("it takes up the windup it is seen holding")));
		}
	};
}

#endif
