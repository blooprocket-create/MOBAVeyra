// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Kit/VeyraKitPresentationSettings.h"
#include "Kit/VeyraSkillEffectsSubsystem.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

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
}

#endif
