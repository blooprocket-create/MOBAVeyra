// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "AbilitySystemGlobals.h"
#include "Components/ActorTestSpawner.h"
#include "Cues/VeyraCombatCues.h"
#include "Delivery/VeyraDelayedArea.h"
#include "Delivery/VeyraProjectile.h"
#include "NiagaraSystem.h"
#include "VeyraPlayerState.h"
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
		static constexpr double SectorArc = 90.0;
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

		TEST_METHOD(AVanguardsAttackLookIsNamedOnceWithAScale)
		{
			const auto ProblemsOf = [](const TArray<FVeyraVanguardAttack>& Attacks) {
				UVeyraKitPresentationSettings* Settings = DuplicateObject(GetDefault<UVeyraKitPresentationSettings>(), GetTransientPackage());
				Settings->VanguardAttacks = Attacks;
				return Settings->Validate();
			};
			FVeyraVanguardAttack Own;
			Own.Vanguard = TEXT("test_attacker");
			Own.Travel.Effect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Test/NS_Test.NS_Test")));
			Own.Travel.Scale = 1.0f;
			ASSERT_THAT(IsTrue(ProblemsOf({ Own }).IsEmpty()));
			ASSERT_THAT(IsFalse(ProblemsOf({ Own, Own }).IsEmpty(), TEXT("a Vanguard has one look of its own")));
			FVeyraVanguardAttack Unscaled = Own;
			Unscaled.Travel.Scale = 0.0f;
			ASSERT_THAT(IsFalse(ProblemsOf({ Unscaled }).IsEmpty(), TEXT("a look needs a scale above 0")));
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

		TEST_METHOD(AThrownThingArcsFromTheHandToWhereItLands)
		{
			const FVector From(0.0, 0.0, 200.0);
			const FVector To(800.0, 0.0, 0.0);
			constexpr double Height = 300.0;
			ASSERT_THAT(IsTrue(VeyraSkillEffects::ArcPoint(From, To, 0.0, Height).Equals(From)));
			ASSERT_THAT(IsTrue(VeyraSkillEffects::ArcPoint(From, To, 1.0, Height).Equals(To)));
			ASSERT_THAT(IsTrue(VeyraSkillEffects::ArcPoint(From, To, 0.5, Height).Equals(FVector(400.0, 0.0, 100.0 + Height)), TEXT("highest halfway, by its height")));
		}

		TEST_METHOD(ATetherComesHomeAtItsSpeedAndStopsThere)
		{
			const FVector From(1000.0, 0.0, 0.0);
			const FVector Hand(0.0, 0.0, 0.0);
			constexpr double Speed = 2000.0;
			bool bHome = true;
			ASSERT_THAT(IsTrue(VeyraSkillEffects::RetractPoint(From, Hand, 0.25, Speed, bHome).Equals(FVector(500.0, 0.0, 0.0))));
			ASSERT_THAT(IsFalse(bHome));
			ASSERT_THAT(IsTrue(VeyraSkillEffects::RetractPoint(From, Hand, 1.0, Speed, bHome).Equals(Hand)));
			ASSERT_THAT(IsTrue(bHome, TEXT("it never overshoots the hand")));
		}

		TEST_METHOD(ATetherReturnsAtItsPullsSpeedOrElseItsFlights)
		{
			FVeyraAbilitiesTuning Fixture;
			FVeyraSkillshotAbilityTuning Hook;
			Hook.Projectile.Speed = 1600.0;
			Hook.Effects.Displacement.Add(FVeyraDisplacementTuning{ EVeyraDisplacementDirection::TowardOrigin, 1000.0, 1200.0 });
			Fixture.Skillshot.Add(Id(TEXT("test_hook")), Hook);
			FVeyraSkillshotAbilityTuning Bolt;
			Bolt.Projectile.Speed = 1800.0;
			Fixture.Skillshot.Add(Id(TEXT("test_bolt")), Bolt);
			ASSERT_THAT(IsNear(VeyraSkillEffects::RetractSpeedOf(Fixture, Id(TEXT("test_hook"))), 1200.0, Slack, TEXT("hauled in at its pull's speed")));
			ASSERT_THAT(IsNear(VeyraSkillEffects::RetractSpeedOf(Fixture, Id(TEXT("test_bolt"))), 1800.0, Slack, TEXT("one that pulls nothing flies back")));
			ASSERT_THAT(IsNear(VeyraSkillEffects::RetractSpeedOf(Fixture, Id(TEXT("test_unknown"))), 0.0, Slack));
		}

		TEST_METHOD(ATetherAnArcAndAChannelBoneAreWholeOrAbsent)
		{
			FVeyraAbilityEffects Half = Recipe(TEXT("test_half_tether"));
			Half.TetherBone = TEXT("prop_r");
			ASSERT_THAT(IsFalse(ProblemsWith({ Half }).IsEmpty(), TEXT("a tether names where it flies from too")));
			Half.TetherFromBone = TEXT("hand_r");
			ASSERT_THAT(IsTrue(ProblemsWith({ Half }).IsEmpty()));
			FVeyraAbilityEffects Arc = Recipe(TEXT("test_arc"));
			Arc.Arc = Arc.Commit;
			ASSERT_THAT(IsFalse(ProblemsWith({ Arc }).IsEmpty(), TEXT("an arc needs its bone and a height")));
			Arc.ArcBone = TEXT("hand_r");
			Arc.ArcHeight = 200.0f;
			ASSERT_THAT(IsTrue(ProblemsWith({ Arc }).IsEmpty()));
			FVeyraAbilityEffects Anchored = Recipe(TEXT("test_anchor"));
			Anchored.ChannelBone = TEXT("hand_r");
			ASSERT_THAT(IsFalse(ProblemsWith({ Anchored }).IsEmpty(), TEXT("only a channel pours from a channel bone")));
		}

		TEST_METHOD(AnAreaIsDrawnAtItsTrueShape)
		{
			// One source for every size: the shape Abilities.json gives (ADR-075 §3).
			FVeyraShape Sector;
			Sector.Kind = EVeyraShapeKind::Sector;
			Sector.Radius = SlamRadius;
			Sector.ArcDegrees = SectorArc;
			const FVeyraAreaParameters Fan = VeyraSkillEffects::AreaParametersOf(Sector, 0.0);
			ASSERT_THAT(IsTrue(Fan.Shape == 1 && Fan.Radius == SlamRadius && FMath::IsNearlyEqual(Fan.HalfArc, SectorArc / 2.0, Slack)));
			FVeyraShape Lane;
			Lane.Kind = EVeyraShapeKind::Rectangle;
			Lane.Length = LanceRange;
			Lane.Width = InnerRadius;
			const FVeyraAreaParameters Strip = VeyraSkillEffects::AreaParametersOf(Lane, 1.0);
			ASSERT_THAT(IsTrue(Strip.Shape == 2 && Strip.Length == LanceRange && Strip.Width == InnerRadius && Strip.Landing == 1.0));
			FVeyraShape Circle;
			Circle.Radius = InnerRadius;
			const FVeyraAreaParameters Ring = VeyraSkillEffects::AreaParametersOf(Circle, 0.0);
			ASSERT_THAT(IsTrue(Ring.Shape == 0 && Ring.HalfArc == 180.0 && Ring.Length == InnerRadius * 2.0 && Ring.Width == InnerRadius * 2.0));
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
		static constexpr double ShortSeconds = 2.0;
		static constexpr double Nearer = 50.0;
		static constexpr int32 HookCast = 3;
		static constexpr int32 FallCast = 7;
		static constexpr double Slack = 1e-6;
		static constexpr const TCHAR* OwnAttackEffect = TEXT("/Game/Veyra/UI/Presentation/Effects/NS_VeyraTrail.NS_VeyraTrail");

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
			// A hook flung on its chain from the caster's hand, drawn in place of its projectile.
			Tuning.Skillshot.Add(VeyraAbilitiesTests::ArchetypeTestId(TEXT("test_hook"))).Projectile = FVeyraProjectileTuning{ Range, Range / 10.0, Range };
			FVeyraAbilityEffects Hook;
			Hook.Ability = TEXT("test_hook");
			Hook.bHideProjectileBody = true;
			Hook.TetherBone = TEXT("prop_r");
			Hook.TetherFromBone = TEXT("hand_r");
			Hook.Tether.Effect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Veyra/UI/Presentation/Effects/NS_VeyraChainLine.NS_VeyraChainLine")));
			Hook.Tether.Scale = 1.0f;
			GetMutableDefault<UVeyraKitPresentationSettings>()->AbilityEffects.Add(Hook);
			// A fall that lands long after it is aimed, by its tuning, with an impact where it lands.
			FVeyraAreaAbilityTuning& Fall = Tuning.Area.Add(VeyraAbilitiesTests::ArchetypeTestId(TEXT("test_fall")));
			Fall.DelaySeconds = LongSeconds;
			Fall.Zones.AddDefaulted_GetRef().Shape.Radius = Range / 4.0;
			FVeyraAbilityEffects Landing;
			Landing.Ability = TEXT("test_fall");
			Landing.Impact.Effect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Veyra/UI/Presentation/Effects/NS_VeyraImpact.NS_VeyraImpact")));
			Landing.Impact.Scale = 1.0f;
			// Drawn over where it will land until then (ADR-075 §3).
			Landing.Area.Effect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Veyra/UI/Presentation/Effects/NS_VeyraLightMark.NS_VeyraLightMark")));
			Landing.Area.Scale = 1.0f;
			GetMutableDefault<UVeyraKitPresentationSettings>()->AbilityEffects.Add(Landing);
			// A Vanguard whose basic attacks fly a look of their own, in place of the shared one (ADR-072 §9).
			FVeyraVanguardAttack Own;
			Own.Vanguard = TEXT("test_attacker");
			Own.Travel.Effect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(OwnAttackEffect));
			Own.Travel.Scale = 1.0f;
			GetMutableDefault<UVeyraKitPresentationSettings>()->VanguardAttacks.Add(Own);
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		AFTER_EACH()
		{
			GetMutableDefault<UVeyraKitPresentationSettings>()->AbilityEffects.RemoveAll(
				[](const FVeyraAbilityEffects& Effects)
				{ return Effects.Ability == TEXT("test_charged") || Effects.Ability == TEXT("test_hook") || Effects.Ability == TEXT("test_fall"); });
			GetMutableDefault<UVeyraKitPresentationSettings>()->VanguardAttacks.RemoveAll(
				[](const FVeyraVanguardAttack& Attack) { return Attack.Vanguard == TEXT("test_attacker"); });
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

		/** A hook Launcher flings from From, as the server launches one. */
		AVeyraProjectile& Hook(const AActor& Launcher, const FVector& From)
		{
			AVeyraProjectile* Flung = Spawner.GetWorld().SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), FTransform(From));
			check(Flung);
			Flung->LaunchLine(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Launcher), FVector::ForwardVector,
				FVeyraProjectileTuning{ Range, Range / 10.0, Range }, EVeyraSkillshotCollision::FirstEnemy, FVeyraPreparedEffects(), FVeyraPreparedEffects(),
				VeyraAbilitiesTests::ArchetypeTestId(TEXT("test_hook")), HookCast);
			return *Flung;
		}

		TEST_METHOD(AHookIsDrawnFromTheOneWhoLaunchedIt)
		{
			// Its side's other Vanguard stands nearer where it set off, and no cast state names its caster.
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			const FVector From(Range, 0.0, 0.0);
			AVeyraVanguardCharacter& Bystander = World.Spawn(EVeyraTeam::A, From + FVector(Nearer, 0.0, 0.0));
			UVeyraSkillEffectsSubsystem* Skills = Spawner.GetWorld().GetSubsystem<UVeyraSkillEffectsSubsystem>();
			ASSERT_THAT(IsNotNull(Skills));
			Skills->NoteProjectileDrawn(Hook(*Caster, From), From, FLinearColor::White);
			ASSERT_THAT(IsTrue(Skills->FindTether(*Caster).IsSet(), TEXT("its chain runs from the one who launched it")));
			ASSERT_THAT(IsFalse(Skills->FindTether(Bystander).IsSet()));
		}

		TEST_METHOD(AHookWhoseCasterIsNotSeenShowsItsOwnBody)
		{
			UVeyraSkillEffectsSubsystem* Skills = Spawner.GetWorld().GetSubsystem<UVeyraSkillEffectsSubsystem>();
			ASSERT_THAT(IsNotNull(Skills));
			AVeyraProjectile& Flung = Hook(*Caster, FVector(Range, 0.0, 0.0));
			ASSERT_THAT(IsTrue(Skills->HidesProjectileBody(Flung), TEXT("drawn on its chain from a caster seen here")));
			// As on a client that does not see its caster: the replicated launcher resolves to nothing.
			Flung.SetInstigator(nullptr);
			ASSERT_THAT(IsFalse(Skills->HidesProjectileBody(Flung), TEXT("with no caster to chain it to, it shows itself")));
		}

		/** A basic attack Launcher fires from From: a projectile of no ability, as the server launches one. */
		AVeyraProjectile& BasicAttack(const AActor& Launcher, const FVector& From)
		{
			AVeyraProjectile* Shot = Spawner.GetWorld().SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), FTransform(From));
			check(Shot);
			Shot->LaunchLine(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Launcher), FVector::ForwardVector,
				FVeyraProjectileTuning{ Range, Range / 10.0, Range }, EVeyraSkillshotCollision::FirstEnemy, FVeyraPreparedEffects(), FVeyraPreparedEffects(),
				FVeyraContentId(), 0);
			Shot->MarkBasicAttack();
			return *Shot;
		}

		TEST_METHOD(AProjectileOfNoAbilityThatIsNoAttackKeepsItsBody)
		{
			// As an ability's return flies: launched with no ability, and no basic attack.
			UVeyraSkillEffectsSubsystem* Skills = Spawner.GetWorld().GetSubsystem<UVeyraSkillEffectsSubsystem>();
			ASSERT_THAT(IsNotNull(Skills));
			AVeyraProjectile* Return = Spawner.GetWorld().SpawnActor<AVeyraProjectile>(AVeyraProjectile::StaticClass(), FTransform(FVector(Range, 0.0, 0.0)));
			ASSERT_THAT(IsNotNull(Return));
			Return->LaunchHoming(*Caster->GetAbilitySystemComponent(), *Caster, Range / 10.0, Range / 100.0, FVeyraPreparedEffects(), FVeyraContentId(), 0);
			float Scale = 0.0f;
			ASSERT_THAT(IsNull(Skills->ProjectileTravelOf(*Return, Scale), TEXT("no attack's look")));
			ASSERT_THAT(IsFalse(Skills->HidesProjectileBody(*Return), TEXT("its own body still drawn")));
		}

		TEST_METHOD(ABasicAttackFliesTheSharedLookInPlaceOfItsBody)
		{
			// A unit that is no Vanguard, as a minion, has no look of its own.
			VeyraAbilitiesTests::FArchetypeTestWorld World{ Spawner };
			const AActor& Minion = World.SpawnFluxborn(EVeyraTeam::A, FVector::ZeroVector);
			UVeyraSkillEffectsSubsystem* Skills = Spawner.GetWorld().GetSubsystem<UVeyraSkillEffectsSubsystem>();
			ASSERT_THAT(IsNotNull(Skills));
			const AVeyraProjectile& Shot = BasicAttack(Minion, FVector(Range, 0.0, 0.0));
			float Scale = 0.0f;
			const UNiagaraSystem* Flies = Skills->ProjectileTravelOf(Shot, Scale);
			ASSERT_THAT(IsNotNull(Flies, TEXT("the shared attack look flies")));
			ASSERT_THAT(AreEqual(FSoftObjectPath(Flies), GetDefault<UVeyraKitPresentationSettings>()->AttackTravel.Effect.ToSoftObjectPath()));
			ASSERT_THAT(IsTrue(Skills->HidesProjectileBody(Shot), TEXT("with no grey-box ball under it")));
		}

		TEST_METHOD(AVanguardsOwnAttackLookReplacesTheShared)
		{
			Caster->GetPlayerState<AVeyraPlayerState>()->SetVanguardId(FVeyraContentId::FromText(TEXT("test_attacker")).GetValue());
			UVeyraSkillEffectsSubsystem* Skills = Spawner.GetWorld().GetSubsystem<UVeyraSkillEffectsSubsystem>();
			ASSERT_THAT(IsNotNull(Skills));
			float Scale = 0.0f;
			const UNiagaraSystem* Flies = Skills->ProjectileTravelOf(BasicAttack(*Caster, FVector(Range, 0.0, 0.0)), Scale);
			ASSERT_THAT(IsNotNull(Flies));
			ASSERT_THAT(AreEqual(FSoftObjectPath(Flies), FSoftObjectPath(OwnAttackEffect), TEXT("its Vanguard's own look, found through who launched it")));
		}

		TEST_METHOD(ADelayedAreaLandsWhenTheServerResolvesIt)
		{
			// Its commit is seen here; its delay counted from then runs past when the server's area resolves.
			UVeyraSkillEffectsSubsystem* Skills = Spawner.GetWorld().GetSubsystem<UVeyraSkillEffectsSubsystem>();
			ASSERT_THAT(IsNotNull(Skills));
			const FVeyraContentId Fall = VeyraAbilitiesTests::ArchetypeTestId(TEXT("test_fall"));
			const FVector Where(Range, 0.0, 0.0);
			FVeyraCombatCue Commit;
			Commit.Kind = EVeyraCombatCueKind::CastCommit;
			Commit.Unit = Caster;
			Commit.Ability = Fall;
			Commit.Location = Where;
			Commit.CastId = FallCast;
			Skills->NoteCue(Commit);
			ASSERT_THAT(AreEqual(Skills->GetPendingLandings(), 1));
			AVeyraDelayedArea* Area = Spawner.GetWorld().SpawnActor<AVeyraDelayedArea>(AVeyraDelayedArea::StaticClass(), FTransform(Where));
			ASSERT_THAT(IsNotNull(Area));
			FVeyraEffectFrame Placement;
			Placement.Origin = Where;
			Area->Arm(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Caster), Placement, TArray<FVeyraPreparedZone>(), ShortSeconds, Fall, FallCast);
			Skills->Refresh();
			const TArray<double> Times = Skills->GetLandingTimes();
			ASSERT_THAT(AreEqual(Times.Num(), 1));
			ASSERT_THAT(IsNear(Times[0], Area->GetResolvesAt(), Slack, TEXT("it lands when the server's area resolves")));
		}

		TEST_METHOD(ADelayedAreaIsDrawnOverWhereItWillLand)
		{
			UVeyraSkillEffectsSubsystem* Skills = Spawner.GetWorld().GetSubsystem<UVeyraSkillEffectsSubsystem>();
			ASSERT_THAT(IsNotNull(Skills));
			const FVeyraContentId Fall = VeyraAbilitiesTests::ArchetypeTestId(TEXT("test_fall"));
			const FVector Where(Range, 0.0, 0.0);
			AVeyraDelayedArea* Area = Spawner.GetWorld().SpawnActor<AVeyraDelayedArea>(AVeyraDelayedArea::StaticClass(), FTransform(Where));
			ASSERT_THAT(IsNotNull(Area));
			FVeyraEffectFrame Placement;
			Placement.Origin = Where;
			FVeyraPreparedZone Zone;
			Zone.Shape = Tuning.Area[Fall].Zones[0].Shape;
			Area->Arm(*UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Caster), Placement, { Zone }, LongSeconds, Fall, FallCast);
			Skills->Refresh();
			const TArray<FVeyraAreaParameters> Drawn = Skills->FindAreas(*Area);
			ASSERT_THAT(AreEqual(Drawn.Num(), 1, TEXT("one effect over its one zone")));
			ASSERT_THAT(IsTrue(Drawn[0].Shape == 0 && FMath::IsNearlyEqual(Drawn[0].Radius, Range / 4.0, Slack), TEXT("its circle, as Abilities.json shapes it")));
			ASSERT_THAT(IsTrue(Drawn[0].Landing == 0.0, TEXT("long before it lands")));
			ASSERT_THAT(IsTrue(Drawn[0].Normal.Equals(FVector::UpVector, 1e-3), TEXT("level over level ground")));

			// The same place, a recipe of another scale: it is drawn for that one, not the first one's.
			FVeyraAbilityEffects* Recipe = GetMutableDefault<UVeyraKitPresentationSettings>()->AbilityEffects.FindByPredicate(
				[](const FVeyraAbilityEffects& Effects) { return Effects.Ability == TEXT("test_fall"); });
			ASSERT_THAT(IsNotNull(Recipe));
			Recipe->Area.Scale *= 2.0f;
			Skills->Refresh();
			const TArray<FVeyraAreaParameters> Redrawn = Skills->FindAreas(*Area);
			ASSERT_THAT(IsTrue(Redrawn.Num() == 1 && Redrawn[0].Scale == Recipe->Area.Scale && Redrawn[0].Recipe == FName(TEXT("test_fall"))));
		}
	};
}

#endif
