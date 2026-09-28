// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Casting/VeyraCastStateComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Greybox/VeyraGreyboxOutline.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Gold/VeyraGoldComponent.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Hud/VeyraHudModel.h"
#include "Ledger/VeyraFluxLedger.h"
#include "Life/VeyraLifeComponent.h"
#include "Interfaces/IProjectManager.h"
#include "Materials/MaterialInterface.h"
#include "Wells/VeyraFluxWell.h"
#include "Wildlife/VeyraWildlife.h"
#include "Modules/ModuleManager.h"
#include "ProjectDescriptor.h"
#include "Progression/VeyraProgressionRules.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Recall/VeyraRecallComponent.h"
#include "State/VeyraTeamFluxState.h"
#include "Structures/VeyraStructure.h"
#include "Tests/Abilities/VeyraAbilityTestHelpers.h"
#include "Tests/World/VeyraBattlegroundTestLayout.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraFluxTuningSubsystem.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"

namespace VeyraAbilitiesTests
{
	// Veyra.UI.Greybox.*: the grey-box presentation in a client world (ADR-008 §1). Bodies take their
	// capsules, telegraphs equal the tuning's shapes where the delivery places them, projectiles are
	// drawn from their launch data, and the HUD shows the participant's replicated state.
	TEST_CLASS(Greybox, "Veyra.UI")
	{
		// Fixture values, independent of the committed Abilities.json.
		static constexpr double InnerRadius = 200.0;
		static constexpr double OuterRadius = 450.0;
		static constexpr double ArcDegrees = 90.0;
		static constexpr double CastRange = 700.0;
		static constexpr double ProjectileSpeed = 1000.0;
		static constexpr double ProjectileRadius = 40.0;
		static constexpr double ProjectileRange = 5000.0;
		static constexpr double LongSeconds = 60.0;
		static constexpr float StepSeconds = 0.1f;
		static constexpr int32 FlightSteps = 5;
		static constexpr int32 CircleSegments = 24;
		static constexpr double Tolerance = 1.0;

		FActorTestSpawner Spawner;
		FVeyraAbilitiesTuning Tuning;
		AVeyraVanguardCharacter* Caster = nullptr;

		BEFORE_EACH()
		{
			Tuning.Statuses.Add(ArchetypeTestId(TEXT("test_stun")), StatusOf(EVeyraStatusKind::Stun, 0.0, LongSeconds));

			// A slam on the caster that winds up for the whole test: a sector inside a circle.
			FVeyraAreaAbilityTuning Slam;
			Slam.Cast = InstantCast(CastRange, LongSeconds, 0.0);
			Slam.Cast.WindupSeconds = LongSeconds;
			Slam.Origin = EVeyraAreaOrigin::Caster;
			FVeyraShape Sector;
			Sector.Kind = EVeyraShapeKind::Sector;
			Sector.Radius = InnerRadius;
			Sector.ArcDegrees = ArcDegrees;
			Slam.Zones.AddDefaulted_GetRef().Shape = Sector;
			Slam.Zones.AddDefaulted_GetRef().Shape = CircleOf(OuterRadius);
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_slam")), Slam);

			// A mortar at a ground point that hits after a delay.
			FVeyraAreaAbilityTuning Mortar;
			Mortar.Cast = InstantCast(CastRange, LongSeconds, 0.0);
			Mortar.Origin = EVeyraAreaOrigin::TargetPoint;
			Mortar.DelaySeconds = LongSeconds;
			Mortar.Zones.AddDefaulted_GetRef().Shape = CircleOf(InnerRadius);
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_mortar")), Mortar);

			FVeyraSkillshotAbilityTuning Bolt;
			Bolt.Cast = InstantCast(CastRange, LongSeconds, 0.0);
			Bolt.Projectile = FVeyraProjectileTuning{ ProjectileSpeed, ProjectileRadius, ProjectileRange };
			Bolt.Collision = EVeyraSkillshotCollision::Pierce;
			Tuning.Skillshot.Add(ArchetypeTestId(TEXT("test_bolt_line")), Bolt);

			FVeyraSkillshotAbilityTuning AimedBolt = Bolt;
			AimedBolt.Cast.WindupSeconds = LongSeconds;
			Tuning.Skillshot.Add(ArchetypeTestId(TEXT("test_aimed_bolt")), AimedBolt);

			// An empowerment that waits the whole test for the next basic attack.
			FVeyraEmpoweredAttackAbilityTuning Heavy;
			Heavy.Cast = InstantCast(0.0, LongSeconds, 0.0);
			Heavy.DurationSeconds = LongSeconds;
			Tuning.EmpoweredAttack.Add(ArchetypeTestId(TEXT("test_heavy")), Heavy);
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(&Tuning);

			FArchetypeTestWorld World{ Spawner };
			Caster = &World.Spawn(EVeyraTeam::A, FVector::ZeroVector);
		}

		AFTER_EACH()
		{
			UVeyraAbilitiesTuningSubsystem::SetTestOverride(nullptr);
		}

		UVeyraGreyboxSubsystem& RefreshedGreybox()
		{
			UVeyraGreyboxSubsystem* Subsystem = Spawner.GetWorld().GetSubsystem<UVeyraGreyboxSubsystem>();
			check(Subsystem);
			Subsystem->Refresh();
			return *Subsystem;
		}

		/** Advances the world's clock by Steps short steps; the units stay where they are. */
		void Wait(int32 Steps)
		{
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				Spawner.GetWorld().Tick(LEVELTICK_TimeOnly, StepSeconds);
			}
		}

		static bool SameShape(const FVeyraShape& A, const FVeyraShape& B)
		{
			return A.Kind == B.Kind && A.Radius == B.Radius && A.ArcDegrees == B.ArcDegrees && A.Length == B.Length && A.Width == B.Width;
		}

		static FLinearColor ColorShownBy(const UStaticMeshComponent& Shape)
		{
			FLinearColor Color = FLinearColor::Transparent;
			Shape.GetMaterial(0)->GetVectorParameterValue(FHashedMaterialParameterInfo(GetDefault<UVeyraGreyboxSettings>()->ColorParameter), Color);
			return Color;
		}

		TEST_METHOD(TheSettingsAreCompleteAndTheirAssetsLoad)
		{
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			const TArray<FString> Problems = Settings.Validate();
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" "))));
			ASSERT_THAT(IsNotNull(Settings.BodyMesh.LoadSynchronous()));
			ASSERT_THAT(IsNotNull(Settings.ProjectileMesh.LoadSynchronous()));
			const UMaterialInterface* Material = Settings.ShapeMaterial.LoadSynchronous();
			ASSERT_THAT(IsNotNull(Material));
			FLinearColor Unused;
			ASSERT_THAT(IsTrue(Material->GetVectorParameterValue(FHashedMaterialParameterInfo(Settings.ColorParameter), Unused),
				TEXT("the shape material has the colour parameter")));
		}

		TEST_METHOD(OnlyClientsLoadThePresentation)
		{
			// Servers never build or load a ClientOnly module (ModuleLayers.json, Presentation).
			const FProjectDescriptor* Project = IProjectManager::Get().GetCurrentProject();
			ASSERT_THAT(IsNotNull(Project));
			const FModuleDescriptor* Module = Project->Modules.FindByPredicate([](const FModuleDescriptor& Each) { return Each.Name == TEXT("VeyraUI"); });
			ASSERT_THAT(IsTrue(Module && Module->Type == EHostType::ClientOnly));
			ASSERT_THAT(IsTrue(FModuleManager::Get().IsModuleLoaded(TEXT("VeyraUI")), TEXT("this editor is not a dedicated server")));
		}

		TEST_METHOD(EveryUnitGetsABodyShapedLikeItsCapsuleInItsSidesColour)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraTestFluxborn& Enemy = World.SpawnFluxborn(EVeyraTeam::B, FVector(CastRange, 0.0, 0.0));
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			for (const APawn* Unit : { static_cast<APawn*>(Caster), static_cast<APawn*>(&Enemy) })
			{
				const UStaticMeshComponent* Body = Presentation.FindBody(*Unit);
				ASSERT_THAT(IsNotNull(Body));
				float Radius = 0.0f;
				float HalfHeight = 0.0f;
				Unit->GetSimpleCollisionCylinder(Radius, HalfHeight);
				const FVector Extent = Body->Bounds.BoxExtent;
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Extent.X, Radius, Tolerance) && FMath::IsNearlyEqual(Extent.Z, HalfHeight, Tolerance),
					FString::Printf(TEXT("body %s, capsule %.1f x %.1f"), *Extent.ToString(), Radius, HalfHeight)));
				ASSERT_THAT(IsTrue(FVector::Dist(Body->Bounds.Origin, Unit->GetActorLocation()) < Tolerance, TEXT("centred on the capsule")));
			}
			// With no player to view it, side A stands in for the viewer's allies.
			ASSERT_THAT(IsTrue(ColorShownBy(*Presentation.FindBody(*Caster)).Equals(Settings.AllyColor)));
			ASSERT_THAT(IsTrue(ColorShownBy(*Presentation.FindBody(Enemy)).Equals(Settings.EnemyColor)));
		}

		TEST_METHOD(TheBattlegroundsGroundAndStructuresAreDrawn)
		{
			ASSERT_THAT(IsNull(RefreshedGreybox().GetGround(), TEXT("a world with no battleground draws none")));
			UVeyraBattlegroundSubsystem* Battleground = Spawner.GetWorld().GetSubsystem<UVeyraBattlegroundSubsystem>();
			Battleground->SpawnStructures(VeyraWorldTests::CompactBattleground());
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();

			// The river, each stretch of each lane's road, and both bases' pads, from World.json's layout.
			const FVeyraBattlegroundLayout& Layout = UVeyraWorldTuningSubsystem::Get().Layout;
			int32 Stretches = 0;
			for (const FVeyraLaneLayout& Lane : Layout.Lanes)
			{
				Stretches += Lane.Points.Num() - 1;
			}
			const AActor* Ground = Presentation.GetGround();
			ASSERT_THAT(IsNotNull(Ground));
			TArray<UStaticMeshComponent*> Markings;
			Ground->GetComponents(Markings);
			ASSERT_THAT(AreEqual(1 + Stretches + 2, Markings.Num()));
			for (const UStaticMeshComponent* Marking : Markings)
			{
				ASSERT_THAT(IsTrue(Marking->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Marking->CanEverAffectNavigation(),
					TEXT("the ground decides nothing")));
			}
			for (const AVeyraStructure* Structure : Battleground->GetStructures())
			{
				ASSERT_THAT(IsNotNull(Presentation.FindBody(*Structure), TEXT("every structure has a body")));
			}

			// Each structure's bar names it and says whether the structures before it protect it.
			const double Now = Presentation.GetServerNow();
			const TOptional<FVeyraHudStructure> Outer = VeyraHud::StructureOf(*Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 0), Now);
			const TOptional<FVeyraHudStructure> Middle = VeyraHud::StructureOf(*Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 1), Now);
			ASSERT_THAT(IsTrue(Outer.IsSet() && Outer->Kind == EVeyraStructureKind::LaneSpire && !Outer->bInvulnerable));
			ASSERT_THAT(IsTrue(Middle.IsSet() && Middle->bInvulnerable, TEXT("the outer Spire still stands")));
			ASSERT_THAT(IsFalse(VeyraHud::StructureOf(*Caster, Now).IsSet(), TEXT("a Vanguard is no structure")));
		}

		TEST_METHOD(NeutralUnitsAreDrawnGreyAndNamedOnTheHud)
		{
			// A creature says what it is; a Flux Well where it stands in its cycle (ADR-014 §2, §4).
			constexpr double UntilOpen = 10.0;
			AVeyraWildlife& Creature = Spawner.SpawnActor<AVeyraWildlife>();
			const FVeyraContentId Species = FVeyraContentId::FromText(TEXT("ashfang")).GetValue();
			Creature.Configure(Species, 0, Creature.GetActorLocation(), OuterRadius);
			AVeyraFluxWell& Well = Spawner.SpawnActor<AVeyraFluxWell>();
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const FLinearColor Neutral = GetDefault<UVeyraGreyboxSettings>()->NeutralColor;
			for (const APawn* Unit : { static_cast<APawn*>(&Creature), static_cast<APawn*>(&Well) })
			{
				const UStaticMeshComponent* Body = Presentation.FindBody(*Unit);
				ASSERT_THAT(IsTrue(Body && ColorShownBy(*Body).Equals(Neutral), TEXT("on no side, grey")));
			}
			const double Now = Presentation.GetServerNow();
			const TOptional<FVeyraContentId> Named = VeyraHud::SpeciesOf(Creature);
			ASSERT_THAT(IsTrue(Named.IsSet() && Named.GetValue() == Species));
			Well.SetState(EVeyraFluxWellState::Closed, Now + UntilOpen);
			TOptional<FVeyraHudFluxWell> Shown = VeyraHud::FluxWellOf(Well, Now);
			ASSERT_THAT(IsTrue(Shown.IsSet() && Shown->State == EVeyraFluxWellState::Closed && FMath::IsNearlyEqual(Shown->OpensInSeconds, UntilOpen, Tolerance)));
			Well.SetState(EVeyraFluxWellState::Open, 0.0);
			Shown = VeyraHud::FluxWellOf(Well, Now);
			ASSERT_THAT(IsTrue(Shown->State == EVeyraFluxWellState::Open && Shown->OpensInSeconds == 0.0));
			ASSERT_THAT(IsFalse(VeyraHud::FluxWellOf(*Caster, Now).IsSet() || VeyraHud::SpeciesOf(*Caster).IsSet(), TEXT("a Vanguard is neither")));
		}

		TEST_METHOD(AStunnedUnitIsTinted)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CastRange, 0.0, 0.0));
			UVeyraStatusComponent* Statuses = Enemy.GetPlayerState()->FindComponentByClass<UVeyraStatusComponent>();
			const FVeyraContentId Stun = ArchetypeTestId(TEXT("test_stun"));
			ASSERT_THAT(IsTrue(Statuses->Apply(*Caster->GetAbilitySystemComponent(), VeyraAbilityRules::ToStatusSpec(Stun, Tuning.Statuses[Stun]))));

			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			const FLinearColor Expected = FLinearColor::LerpUsingHSV(Settings.EnemyColor, Settings.StunColor, Settings.StatusTintStrength);
			ASSERT_THAT(IsTrue(Presentation.BodyColorOf(Enemy).Equals(Expected)));
			ASSERT_THAT(IsTrue(ColorShownBy(*Presentation.FindBody(Enemy)).Equals(Expected)));
		}

		TEST_METHOD(AWindupTelegraphsTheZonesWhereTheyWillLand)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, ArchetypeTestId(TEXT("test_slam")))));
			const FVector Aim(0.0, CastRange / 2.0, 0.0);
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::W, Aim) == EVeyraCastRejection::None));

			const TArray<FVeyraTelegraph>& Telegraphs = RefreshedGreybox().GetTelegraphs();
			const TArray<FVeyraAreaZoneTuning>& Zones = Tuning.Area[ArchetypeTestId(TEXT("test_slam"))].Zones;
			ASSERT_THAT(AreEqual(Zones.Num(), Telegraphs.Num()));
			for (int32 Index = 0; Index < Zones.Num(); ++Index)
			{
				const FVeyraTelegraph& Telegraph = Telegraphs[Index];
				ASSERT_THAT(IsTrue(SameShape(Telegraph.Placed.Shape, Zones[Index].Shape), TEXT("innermost first, as the tuning lists them")));
				ASSERT_THAT(IsTrue(FVector::Dist2D(Telegraph.Placed.Origin, Caster->GetActorLocation()) < Tolerance, TEXT("an area on the caster")));
				ASSERT_THAT(IsTrue(Telegraph.Placed.Direction.Equals(Aim.GetSafeNormal()), TEXT("facing the cast")));
				ASSERT_THAT(IsTrue(Telegraph.Source == EVeyraTelegraphSource::Windup && Telegraph.Team == EVeyraTeam::A));
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Telegraph.RemainingSeconds, LongSeconds, Tolerance)));
			}
		}

		TEST_METHOD(ASkillshotTelegraphsItsPath)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_aimed_bolt")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::Q, FVector(CastRange, 0.0, 0.0)) == EVeyraCastRejection::None));
			const TArray<FVeyraTelegraph>& Telegraphs = RefreshedGreybox().GetTelegraphs();
			ASSERT_THAT(AreEqual(1, Telegraphs.Num()));
			const FVeyraShape& Path = Telegraphs[0].Placed.Shape;
			ASSERT_THAT(IsTrue(Path.Kind == EVeyraShapeKind::Rectangle && Path.Length == ProjectileRange && Path.Width == 2.0 * ProjectileRadius,
				TEXT("as long as its range and as wide as its projectile")));
			ASSERT_THAT(IsTrue(Telegraphs[0].Placed.Direction.Equals(FVector::ForwardVector)));
		}

		TEST_METHOD(ADelayedAreaIsTelegraphedWhereItWillHit)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::E, ArchetypeTestId(TEXT("test_mortar")))));
			const FVector Point(CastRange / 2.0, CastRange / 4.0, 0.0);
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::E, Point) == EVeyraCastRejection::None));
			const TArray<FVeyraTelegraph>& Telegraphs = RefreshedGreybox().GetTelegraphs();
			ASSERT_THAT(AreEqual(1, Telegraphs.Num()));
			const FVeyraTelegraph& Telegraph = Telegraphs[0];
			ASSERT_THAT(IsTrue(Telegraph.Source == EVeyraTelegraphSource::DelayedArea));
			ASSERT_THAT(IsTrue(SameShape(Telegraph.Placed.Shape, Tuning.Area[ArchetypeTestId(TEXT("test_mortar"))].Zones[0].Shape)));
			ASSERT_THAT(IsTrue(FVector::Dist2D(Telegraph.Placed.Origin, Point) < Tolerance));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Telegraph.RemainingSeconds, LongSeconds, Tolerance)));
		}

		TEST_METHOD(ALineProjectileIsDrawnFromItsLaunchData)
		{
			FArchetypeTestWorld World{ Spawner };
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, ArchetypeTestId(TEXT("test_bolt_line")))));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::Q, FVector(CastRange, 0.0, 0.0)) == EVeyraCastRejection::None));
			TActorIterator<AVeyraProjectile> Projectile(&Spawner.GetWorld());
			ASSERT_THAT(IsTrue(static_cast<bool>(Projectile)));

			const UStaticMeshComponent* Sphere = RefreshedGreybox().FindProjectileVisual(**Projectile);
			ASSERT_THAT(IsNotNull(Sphere));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Sphere->Bounds.BoxExtent.X, ProjectileRadius, Tolerance), TEXT("scaled to its radius")));
			ASSERT_THAT(IsTrue(Sphere->GetComponentLocation().Equals(Projectile->GetLaunchedFrom(), Tolerance)));

			Wait(FlightSteps);
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const double Flown = FVector::Dist2D(Sphere->GetComponentLocation(), Projectile->GetLaunchedFrom());
			ASSERT_THAT(IsTrue(Sphere->GetComponentLocation().Equals(Projectile->GetLineLocationAt(Presentation.GetServerNow()), Tolerance)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Flown, ProjectileSpeed * StepSeconds * FlightSteps, Tolerance),
				FString::Printf(TEXT("flew %.1f"), Flown)));
		}

		TEST_METHOD(AHomingProjectileIsDrawnFollowingItsTarget)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Target = World.Spawn(EVeyraTeam::B, FVector(ProjectileRange, 0.0, 0.0));
			AVeyraProjectile& Projectile = Spawner.SpawnActorAt<AVeyraProjectile>(Caster->GetActorLocation(), FRotator::ZeroRotator);
			Projectile.LaunchHoming(*Caster->GetAbilitySystemComponent(), Target, ProjectileSpeed, ProjectileRadius, FVeyraPreparedEffects(),
				ArchetypeTestId(TEXT("test_bolt_line")), 1);
			const UStaticMeshComponent* Sphere = RefreshedGreybox().FindProjectileVisual(Projectile);
			ASSERT_THAT(IsNotNull(Sphere));

			Wait(FlightSteps);
			RefreshedGreybox();
			const FVector Expected = Caster->GetActorLocation() + (Target.GetActorLocation() - Caster->GetActorLocation()).GetSafeNormal() * ProjectileSpeed * StepSeconds * FlightSteps;
			ASSERT_THAT(IsTrue(Sphere->GetComponentLocation().Equals(Expected, Tolerance),
				FString::Printf(TEXT("drawn at %s, expected %s"), *Sphere->GetComponentLocation().ToString(), *Expected.ToString())));
		}

		TEST_METHOD(TheHudShowsTheParticipantsProgressionAndCooldowns)
		{
			FArchetypeTestWorld World{ Spawner };
			const FVeyraContentId Mortar = ArchetypeTestId(TEXT("test_mortar"));
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, Mortar)));
			ASSERT_THAT(IsTrue(World.CastAt(*Caster, EVeyraAbilitySlot::Q, FVector(CastRange / 2.0, 0.0, 0.0)) == EVeyraCastRejection::None));

			const AVeyraPlayerState& Participant = *Caster->GetPlayerState<AVeyraPlayerState>();
			const double Now = RefreshedGreybox().GetServerNow();
			const FVeyraProgressionTuning& Progression = UVeyraProgressionTuningSubsystem::Get();
			FVeyraHudPlayer Player = VeyraHud::DescribePlayer(Participant, Now);
			ASSERT_THAT(AreEqual(1, Player.Level));
			ASSERT_THAT(AreEqual(VeyraProgression::ExperienceToNextLevel(1, Progression), Player.ExperienceToNextLevel));
			ASSERT_THAT(AreEqual(0, Player.UnspentSkillPoints));
			ASSERT_THAT(AreEqual(4, Player.Slots.Num()));
			const FVeyraHudSlot& Q = Player.Slots[0];
			ASSERT_THAT(IsTrue(Q.Slot == EVeyraAbilitySlot::Q && Q.Ability == Mortar && Q.Rank == 1));
			ASSERT_THAT(AreEqual(VeyraProgression::MaxRank(EVeyraAbilitySlot::Q, Progression), Q.MaxRank));
			const double Cooldown = Participant.FindComponentByClass<UVeyraCooldownComponent>()->GetRemainingSeconds(Mortar, Now);
			ASSERT_THAT(IsTrue(Cooldown > 0.0 && Q.CooldownSeconds == Cooldown, TEXT("the ledger's cooldown")));
			ASSERT_THAT(IsFalse(Player.Slots[1].Ability.IsValid() || Q.bCanRankUp, TEXT("W is empty, and no skill point is left")));
			const TOptional<FVeyraHudVitals> Vitals = VeyraHud::VitalsOf(*Caster);
			ASSERT_THAT(IsTrue(Vitals.IsSet() && Player.Vitals.Health == Vitals->Health && Player.Vitals.MaxHealth == VeyraCombatTests::ExampleStats().MaxHealth));

			// A level's skill point opens Q, W and E, but not the ultimate before its level.
			Participant.FindComponentByClass<UVeyraProgressionComponent>()->AddExperience(VeyraProgression::ExperienceToNextLevel(1, Progression));
			Player = VeyraHud::DescribePlayer(Participant, Now);
			ASSERT_THAT(IsTrue(Player.Level == 2 && Player.UnspentSkillPoints == 1));
			ASSERT_THAT(IsTrue(Player.Slots[0].bCanRankUp && Player.Slots[1].bCanRankUp && !Player.Slots[3].bCanRankUp));
		}

		TEST_METHOD(TheHudShowsGoldTheRespawnWaitAndTeamFlux)
		{
			AVeyraPlayerState& Participant = *Caster->GetPlayerState<AVeyraPlayerState>();
			const double Now = RefreshedGreybox().GetServerNow();
			constexpr double Gold = 512.75;
			ASSERT_THAT(IsTrue(Participant.FindComponentByClass<UVeyraGoldComponent>()->Grant(Gold, EVeyraGoldReason::Developer)));
			FVeyraHudPlayer Player = VeyraHud::DescribePlayer(Participant, Now);
			ASSERT_THAT(IsTrue(Player.Gold == FMath::FloorToInt32(Gold) && !Player.bDead, TEXT("Gold is rounded down for display")));

			constexpr double WaitSeconds = 12.0;
			Participant.SetRespawnAt(Now + WaitSeconds);
			Participant.FindComponentByClass<UVeyraLifeComponent>()->SetState(EVeyraLifeState::Dead);
			Player = VeyraHud::DescribePlayer(Participant, Now);
			ASSERT_THAT(IsTrue(Player.bDead && FMath::IsNearlyEqual(Player.RespawnSeconds, WaitSeconds)));

			// Fixture Flux: a permanent total, a grant still counting and one that has lapsed.
			constexpr double Permanent = 50.0;
			constexpr double Grant = 25.0;
			constexpr double GrantSeconds = 30.0;
			FVeyraTeamFluxView Ours;
			Ours.Team = EVeyraTeam::A;
			Ours.Permanent = Permanent;
			FVeyraTemporaryFluxView& Counting = Ours.Temporary.AddDefaulted_GetRef();
			Counting.Amount = Grant;
			Counting.ExpiresAt = Now + GrantSeconds;
			FVeyraTemporaryFluxView& Lapsed = Ours.Temporary.AddDefaulted_GetRef();
			Lapsed.Amount = Grant;
			Lapsed.ExpiresAt = Now - StepSeconds;
			Spawner.SpawnActor<AVeyraTeamFluxState>().SetTeams({ Ours });
			const TArray<FVeyraHudTeamFlux> Teams = VeyraHud::DescribeTeamFlux(&Spawner.GetWorld(), Now);
			ASSERT_THAT(AreEqual(1, Teams.Num()));
			const FVeyraHudTeamFlux& Shown = Teams[0];
			ASSERT_THAT(IsTrue(Shown.Team == EVeyraTeam::A && Shown.Active == Permanent + Grant && Shown.Permanent == Permanent));
			ASSERT_THAT(IsTrue(Shown.TemporarySeconds.Num() == 1 && FMath::IsNearlyEqual(Shown.TemporarySeconds[0], GrantSeconds), TEXT("only the grant still counting")));
			const double Bonus = VeyraFlux::StrengthFor(Permanent + Grant, UVeyraFluxTuningSubsystem::Get().FluxbornScaling).HealthMultiplier - 1.0;
			ASSERT_THAT(IsTrue(Shown.FluxbornBonus == Bonus, TEXT("Flux's own rule")));
		}

		TEST_METHOD(TheHudShowsARecallChannelFilling)
		{
			AVeyraPlayerState& Participant = *Caster->GetPlayerState<AVeyraPlayerState>();
			UVeyraRecallComponent& Recall = *Participant.FindComponentByClass<UVeyraRecallComponent>();
			ASSERT_THAT(IsFalse(VeyraHud::DescribePlayer(Participant, RefreshedGreybox().GetServerNow()).bRecalling));

			// Fixture channel: a quarter of the way through.
			constexpr double ChannelSeconds = 8.0;
			constexpr double Passed = 2.0;
			Recall.Start(ChannelSeconds, FSimpleDelegate());
			const double StartedAt = Recall.GetChannel().StartedAt;
			const FVeyraHudPlayer Player = VeyraHud::DescribePlayer(Participant, StartedAt + Passed);
			ASSERT_THAT(IsTrue(Player.bRecalling));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Player.RecallSeconds, ChannelSeconds - Passed)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Player.RecallProgress, Passed / ChannelSeconds)));

			ASSERT_THAT(IsTrue(Recall.Interrupt()));
			ASSERT_THAT(IsFalse(VeyraHud::DescribePlayer(Participant, StartedAt + Passed).bRecalling, TEXT("an interrupted channel leaves the HUD")));
		}

		TEST_METHOD(TheHudShowsAnEmpowermentWaitingForTheNextAttack)
		{
			FArchetypeTestWorld World{ Spawner };
			const FVeyraContentId Heavy = ArchetypeTestId(TEXT("test_heavy"));
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::W, Heavy)));
			const AVeyraPlayerState& Participant = *Caster->GetPlayerState<AVeyraPlayerState>();
			const double Now = RefreshedGreybox().GetServerNow();
			ASSERT_THAT(IsTrue(VeyraHud::DescribePlayer(Participant, Now).Slots[1].EmpoweredSeconds == 0.0, TEXT("nothing waits before the cast")));

			ASSERT_THAT(IsTrue(VeyraAbilities::TryCast(*Caster->GetAbilitySystemComponent(), EVeyraAbilitySlot::W, FVeyraCastTarget()) == EVeyraCastRejection::None));
			const FVeyraHudPlayer Player = VeyraHud::DescribePlayer(Participant, Now);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Player.Slots[1].EmpoweredSeconds, LongSeconds, Tolerance), FString::SanitizeFloat(Player.Slots[1].EmpoweredSeconds)));
			ASSERT_THAT(IsTrue(Player.Slots[0].EmpoweredSeconds == 0.0, TEXT("only the slot that cast it")));
			ASSERT_THAT(IsTrue(VeyraHud::DescribePlayer(Participant, Now + LongSeconds + StepSeconds).Slots[1].EmpoweredSeconds == 0.0,
				TEXT("a lapsed empowerment shows nothing")));
		}

		TEST_METHOD(OutlinesTraceTheirShapes)
		{
			const FVector Origin(100.0, -50.0, 20.0);
			const FVector Direction = FVector(1.0, 1.0, 0.0).GetSafeNormal();
			const auto Distance2D = [&Origin](const FVector& Point) { return FVector::Dist2D(Point, Origin); };

			const TArray<FVeyraOutlineSegment> Circle = VeyraGreyboxOutline::Of(FVeyraPlacedShape{ CircleOf(OuterRadius), Origin, Direction }, CircleSegments);
			ASSERT_THAT(AreEqual(CircleSegments, Circle.Num()));
			for (const FVeyraOutlineSegment& Segment : Circle)
			{
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Distance2D(Segment.Start), OuterRadius, Tolerance) && Segment.Start.Z == Origin.Z));
			}

			FVeyraShape Sector;
			Sector.Kind = EVeyraShapeKind::Sector;
			Sector.Radius = InnerRadius;
			Sector.ArcDegrees = ArcDegrees;
			for (const FVeyraOutlineSegment& Segment : VeyraGreyboxOutline::Of(FVeyraPlacedShape{ Sector, Origin, Direction }, CircleSegments))
			{
				for (const FVector& Point : { Segment.Start, Segment.End })
				{
					const FVector Offset = (Point - Origin).GetSafeNormal2D();
					const bool bAtOrigin = Distance2D(Point) < Tolerance;
					const double Degrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Offset, Direction), -1.0, 1.0)));
					ASSERT_THAT(IsTrue(bAtOrigin || (Distance2D(Point) <= InnerRadius + Tolerance && Degrees <= ArcDegrees / 2.0 + Tolerance)));
				}
			}

			FVeyraShape Rectangle;
			Rectangle.Kind = EVeyraShapeKind::Rectangle;
			Rectangle.Length = OuterRadius;
			Rectangle.Width = InnerRadius;
			const TArray<FVeyraOutlineSegment> Sides = VeyraGreyboxOutline::Of(FVeyraPlacedShape{ Rectangle, Origin, FVector::ForwardVector }, CircleSegments);
			ASSERT_THAT(AreEqual(4, Sides.Num()));
			ASSERT_THAT(IsTrue(Sides[1].End.Equals(Origin + FVector(OuterRadius, InnerRadius / 2.0, 0.0), Tolerance), TEXT("the far corner")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
