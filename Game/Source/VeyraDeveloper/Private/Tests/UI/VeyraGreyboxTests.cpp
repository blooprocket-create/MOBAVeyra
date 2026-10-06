// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "AbilitySystemComponent.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "VeyraCombatVerbs.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Attacks/VeyraBasicAttackComponent.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Cues/VeyraCombatCueSubsystem.h"
#include "Greybox/VeyraFountainShop.h"
#include "VeyraTeamStart.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Delivery/VeyraLingeringArea.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/StaticMesh.h"
#include "Echoes/VeyraEchoSubsystem.h"
#include "EngineUtils.h"
#include "Entities/VeyraPlacedMarker.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "Greybox/VeyraGreyboxOutline.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Gold/VeyraGoldComponent.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Greybox/VeyraUnitArtSet.h"
#include "Hud/VeyraHudModel.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
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
		static constexpr double HitAmount = 50.0;
		static constexpr double ResourceCost = 40.0;

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

			// The same mortar with a cost.
			FVeyraAreaAbilityTuning DearMortar = Mortar;
			DearMortar.Cast.ResourceCostByRank = { ResourceCost };
			Tuning.Area.Add(ArchetypeTestId(TEXT("test_dear_mortar")), DearMortar);

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

		/** The structure kit's art set, as the settings name it. */
		static const UVeyraUnitArtSet& StructureArt()
		{
			return *GetDefault<UVeyraGreyboxSettings>()->StructureArt.LoadSynchronous();
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
			// The structure kit's art set dresses every kind, standing and wrecked, by its stable ID (ADR-006 §6).
			const UVeyraUnitArtSet* Structures = Settings.StructureArt.LoadSynchronous();
			ASSERT_THAT(IsNotNull(Structures));
			TArray<FName> Kinds;
			for (const EVeyraStructureKind Kind : { EVeyraStructureKind::LaneSpire, EVeyraStructureKind::BaseTower, EVeyraStructureKind::Inhibitor, EVeyraStructureKind::PrimeWell })
			{
				Kinds.Add(UVeyraGreyboxSettings::StructureArtId(Kind));
			}
			const TArray<FString> ArtProblems = Structures->Validate(Kinds);
			ASSERT_THAT(IsTrue(ArtProblems.IsEmpty(), FString::Join(ArtProblems, TEXT(" "))));
			const FVeyraUnitArt* Spire = Structures->Find(TEXT("laneSpire"));
			ASSERT_THAT(IsTrue(Spire->Intact->GetMaterialIndex(Structures->FluxSlot) != INDEX_NONE, TEXT("the meshes have the Flux slot")));
			// The generated hit flash takes its colour and strength (ADR-063 §2).
			const UMaterialInterface* Flash = Settings.HitFlashMaterial.LoadSynchronous();
			ASSERT_THAT(IsNotNull(Flash, TEXT("run BuildPresentationMaterials.ps1")));
			float Strength = 0.0f;
			ASSERT_THAT(IsTrue(Flash->GetVectorParameterValue(FHashedMaterialParameterInfo(Settings.HitFlashColorParameter), Unused)
				&& Flash->GetScalarParameterValue(FHashedMaterialParameterInfo(Settings.HitFlashStrengthParameter), Strength)));
			// The generated hover outline takes each side's colour and names each side's stencil (ADR-063 §3).
			const UMaterialInterface* Outline = Settings.HoverOutlineMaterial.LoadSynchronous();
			ASSERT_THAT(IsNotNull(Outline, TEXT("run BuildPresentationMaterials.ps1")));
			for (const FName& Parameter : { Settings.HoverEnemyColorParameter, Settings.HoverAllyColorParameter, Settings.HoverNeutralColorParameter })
			{
				ASSERT_THAT(IsTrue(Outline->GetVectorParameterValue(FHashedMaterialParameterInfo(Parameter), Unused), *Parameter.ToString()));
			}
			for (const FName& Parameter : { Settings.HoverEnemyStencilParameter, Settings.HoverAllyStencilParameter, Settings.HoverNeutralStencilParameter })
			{
				float Stencil = 0.0f;
				ASSERT_THAT(IsTrue(Outline->GetScalarParameterDefaultValue(FHashedMaterialParameterInfo(Parameter), Stencil) && Stencil >= 1.0f, *Parameter.ToString()));
			}
		}

		TEST_METHOD(TheHoveredUnitAloneIsOutlinedInItsSidesStencil)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CastRange, 0.0, 0.0));
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const int32 EnemyStencil = Presentation.HoverStencilOf(Enemy);
			const int32 AllyStencil = Presentation.HoverStencilOf(*Caster);
			ASSERT_THAT(IsTrue(EnemyStencil > 0 && AllyStencil > 0 && EnemyStencil != AllyStencil, TEXT("each side has its own stencil")));
			Presentation.ShowHover(&Enemy);
			const UStaticMeshComponent* EnemyBody = Presentation.FindBody(Enemy);
			const UStaticMeshComponent* AllyBody = Presentation.FindBody(*Caster);
			ASSERT_THAT(IsTrue(EnemyBody->bRenderCustomDepth && EnemyBody->CustomDepthStencilValue == EnemyStencil));
			ASSERT_THAT(IsFalse(AllyBody->bRenderCustomDepth));
			Presentation.ShowHover(Caster);
			ASSERT_THAT(IsTrue(!EnemyBody->bRenderCustomDepth && AllyBody->bRenderCustomDepth && AllyBody->CustomDepthStencilValue == AllyStencil, TEXT("the outline moves")));
			Presentation.ShowHover(nullptr);
			ASSERT_THAT(IsFalse(AllyBody->bRenderCustomDepth, TEXT("and leaves with the cursor")));
		}

		TEST_METHOD(EachMomentPlaysItsEffectAndEachEffectTakesItsSidesColour)
		{
			// Which effect each moment plays (ADR-063 §4). A test runs where nothing renders, and Niagara plays nothing there.
			const UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			ASSERT_THAT(IsTrue(Presentation.EffectFor(EVeyraCombatCueKind::Hit) == Settings.ImpactEffect.Get()));
			ASSERT_THAT(IsTrue(Presentation.EffectFor(EVeyraCombatCueKind::CastCommit) == Settings.CastEffect.Get()));
			ASSERT_THAT(IsTrue(Presentation.EffectFor(EVeyraCombatCueKind::Death) == Settings.DeathEffect.Get()));
			for (const EVeyraCombatCueKind Bodily : { EVeyraCombatCueKind::AttackWindup, EVeyraCombatCueKind::AttackCommit, EVeyraCombatCueKind::CastWindup })
			{
				ASSERT_THAT(IsNull(Presentation.EffectFor(Bodily), TEXT("the body shows it, with no effect")));
			}
			// And the sound each moment makes (ADR-063 §5); a windup is silent.
			ASSERT_THAT(IsTrue(Presentation.SoundFor(EVeyraCombatCueKind::Hit) == Settings.ImpactSound.Get()));
			ASSERT_THAT(IsTrue(Presentation.SoundFor(EVeyraCombatCueKind::AttackCommit) == Settings.SwingSound.Get()));
			ASSERT_THAT(IsTrue(Presentation.SoundFor(EVeyraCombatCueKind::CastCommit) == Settings.CastSound.Get()));
			ASSERT_THAT(IsTrue(Presentation.SoundFor(EVeyraCombatCueKind::Death) == Settings.DeathSound.Get()));
			ASSERT_THAT(IsTrue(!Presentation.SoundFor(EVeyraCombatCueKind::AttackWindup) && !Presentation.SoundFor(EVeyraCombatCueKind::CastWindup)));
			for (const TSoftObjectPtr<USoundBase>* Sound : { &Settings.ImpactSound, &Settings.SwingSound, &Settings.CastSound, &Settings.DeathSound, &Settings.ClickSound })
			{
				ASSERT_THAT(IsNotNull(Sound->Get(), TEXT("run BuildCueSounds.ps1")));
			}
			// Each generated system takes the side colour the presentation gives it.
			const FNiagaraVariableBase Color(FNiagaraTypeDefinition::GetColorDef(), FName(TEXT("User.") + Settings.EffectColorParameter.ToString()));
			for (const UNiagaraSystem* Effect : { Settings.ImpactEffect.Get(), Settings.CastEffect.Get(), Settings.DeathEffect.Get(), Settings.TrailEffect.Get() })
			{
				ASSERT_THAT(IsNotNull(Effect, TEXT("run BuildEffects.ps1")));
				ASSERT_THAT(IsTrue(Effect->GetExposedParameters().IndexOf(Color) != INDEX_NONE, *Effect->GetName()));
				// And draws its sprites and ribbons with the project's generated materials, never the engine's unlit defaults,
				// which the Crucible's physical sun and manual exposure show black.
				for (const FNiagaraEmitterHandle& Handle : Effect->GetEmitterHandles())
				{
					const FVersionedNiagaraEmitterData* Emitter = Handle.GetEmitterData();
					ASSERT_THAT(IsNotNull(Emitter, *Handle.GetName().ToString()));
					for (const UNiagaraRendererProperties* Renderer : Emitter->GetRenderers())
					{
						const UMaterialInterface* Material = nullptr;
						if (const UNiagaraSpriteRendererProperties* Sprites = Cast<UNiagaraSpriteRendererProperties>(Renderer))
						{
							Material = Sprites->Material;
						}
						else if (const UNiagaraRibbonRendererProperties* Ribbons = Cast<UNiagaraRibbonRendererProperties>(Renderer))
						{
							Material = Ribbons->Material;
						}
						else
						{
							continue;
						}
						ASSERT_THAT(IsTrue(Material && Material->GetPathName().StartsWith(TEXT("/Game/")),
							*FString::Printf(TEXT("%s's %s draws with %s; run BuildEffects.ps1"), *Effect->GetName(), *Handle.GetName().ToString(), *GetPathNameSafe(Material))));
					}
				}
			}
		}
		TEST_METHOD(AShopStandsByEachFountainAndOnlyTheOwnSidesAnswers)
		{
			// ADR-063 §6. Fixture value: where each side's team start stands, mirrored about the centre.
			const FVector StartAt(-4000.0, -3000.0, 0.0);
			for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
			{
				Spawner.SpawnActorAt<AVeyraTeamStart>(Team == EVeyraTeam::A ? StartAt : -StartAt, FRotator::ZeroRotator).SetVeyraTeam(Team);
			}
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			const TArray<AVeyraFountainShop*> Shops = Presentation.GetShops();
			ASSERT_THAT(AreEqual(Shops.Num(), 2));
			AVeyraFountainShop* Own = Shops[0]->GetTeam() == EVeyraTeam::A ? Shops[0] : Shops[1];
			AVeyraFountainShop* Enemy = Own == Shops[0] ? Shops[1] : Shops[0];
			ASSERT_THAT(IsTrue(Enemy->GetTeam() == EVeyraTeam::B));
			// In front of its fountain, toward the centre.
			ASSERT_THAT(IsNear(FVector::Dist2D(Own->GetActorLocation(), StartAt), static_cast<double>(Settings.ShopOffset), Tolerance));
			ASSERT_THAT(IsTrue(Own->GetActorLocation().Size2D() < StartAt.Size2D()));
			ASSERT_THAT(IsTrue(Presentation.HoverShop(Own, EVeyraTeam::A, /*bClicked*/ false) && Own->IsOutlined(), TEXT("the player's own answers")));
			ASSERT_THAT(IsFalse(Presentation.HoverShop(Enemy, EVeyraTeam::A, false), TEXT("the other side's does not")));
			ASSERT_THAT(IsTrue(!Enemy->IsOutlined() && !Own->IsOutlined(), TEXT("and the outline left with the cursor")));
			ASSERT_THAT(IsFalse(Presentation.HoverShop(Own, EVeyraTeam::None, false), TEXT("a viewer on no side has no shop")));
		}

		TEST_METHOD(AMeleeCommitSwingsAnArcThatFadesAndARangedOneNone)
		{
			// ADR-063 §2. Fixture values: a target at arm's length, and one far beyond any melee reach.
			constexpr double Near = 150.0;
			constexpr double Far = 3000.0;
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Close = World.Spawn(EVeyraTeam::B, FVector(Near, 0.0, 0.0));
			AVeyraVanguardCharacter& Distant = World.Spawn(EVeyraTeam::B, FVector(0.0, Far, 0.0));
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			UVeyraCombatCueSubsystem* Cues = Spawner.GetWorld().GetSubsystem<UVeyraCombatCueSubsystem>();
			FVeyraCombatCue Commit;
			Commit.Kind = EVeyraCombatCueKind::AttackCommit;
			Commit.Unit = Caster;
			Commit.Target = &Distant;
			Cues->OnCue.Broadcast(Commit);
			ASSERT_THAT(IsTrue(Presentation.GetSwingArcs().IsEmpty(), TEXT("a ranged attack swings no arc")));
			Commit.Target = &Close;
			Cues->OnCue.Broadcast(Commit);
			ASSERT_THAT(AreEqual(Presentation.GetSwingArcs().Num(), 1));
			const FVeyraSwingArc& Swing = Presentation.GetSwingArcs()[0];
			ASSERT_THAT(IsTrue(Swing.Direction.Equals(FVector::ForwardVector, Tolerance), TEXT("toward its target")));
			ASSERT_THAT(IsNear(Swing.Radius, Near + Close.GetSimpleCollisionRadius(), Tolerance, TEXT("to the target's far edge")));
			ASSERT_THAT(IsTrue(Swing.Color.Equals(Presentation.SideColorOf(*Caster))));
			Wait(FMath::CeilToInt32(GetDefault<UVeyraGreyboxSettings>()->SwingArcSeconds / StepSeconds) + 1);
			ASSERT_THAT(IsTrue(RefreshedGreybox().GetSwingArcs().IsEmpty(), TEXT("then it fades")));
		}

		TEST_METHOD(AHitFlashesTheBodyUntilItFades)
		{
			FArchetypeTestWorld World{ Spawner };
			AVeyraVanguardCharacter& Enemy = World.Spawn(EVeyraTeam::B, FVector(CastRange, 0.0, 0.0));
			UVeyraCombatCueSubsystem* Cues = Spawner.GetWorld().GetSubsystem<UVeyraCombatCueSubsystem>();
			ASSERT_THAT(IsNotNull(Cues));
			// The first sighting raises nothing; the next, after the hit, raises it.
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();
			Cues->Refresh();
			ASSERT_THAT(IsTrue(Presentation.GetFlashOf(Enemy) == 0.0f));
			FVeyraRawDamageEvent Hit;
			Hit.Components.Add({ EVeyraDamageType::TrueDamage, HitAmount });
			ASSERT_THAT(IsTrue(VeyraCombat::DealDamage(*Caster->GetAbilitySystemComponent(), *Enemy.GetAbilitySystemComponent(), Hit)));
			Cues->Refresh();
			RefreshedGreybox();
			ASSERT_THAT(IsTrue(Presentation.GetFlashOf(Enemy) > 0.0f, TEXT("the body flashes at once")));
			ASSERT_THAT(IsTrue(Presentation.FindBody(Enemy)->GetOverlayMaterial() != nullptr));
			ASSERT_THAT(IsTrue(Presentation.GetFlashOf(*Caster) == 0.0f, TEXT("the dealer is not hit")));
			Wait(FMath::CeilToInt32(GetDefault<UVeyraGreyboxSettings>()->HitFlashSeconds / StepSeconds) + 1);
			RefreshedGreybox();
			ASSERT_THAT(IsTrue(Presentation.GetFlashOf(Enemy) == 0.0f && Presentation.FindBody(Enemy)->GetOverlayMaterial() == nullptr, TEXT("then it fades")));
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
			VeyraWorldTests::SpawnCompactGround(Spawner.GetWorld());
			Battleground->SpawnStructures(VeyraWorldTests::CompactBattleground());
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();

			// The river, each stretch of each lane's road, both bases' pads, and both halves' Dense Fog, from
			// World.json's layout.
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
			int32 RiverStretches = 0;
			for (const FVeyraRiverChannel& Channel : VeyraRiver::ShapeOf(Layout).GetChannels())
			{
				RiverStretches += Channel.Samples.Num() - 1;
			}
			ASSERT_THAT(AreEqual(RiverStretches + Stretches + 2 + VeyraLayout::DenseFog(Layout).Num(), Markings.Num()));
			for (const UStaticMeshComponent* Marking : Markings)
			{
				ASSERT_THAT(IsTrue(Marking->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Marking->CanEverAffectNavigation(),
					TEXT("the ground decides nothing")));
			}
			for (const AVeyraStructure* Structure : Battleground->GetStructures())
			{
				ASSERT_THAT(IsNotNull(Presentation.FindBody(*Structure), TEXT("every structure has a body")));
				// Its kind's art stands in for its body, on the floor, deciding nothing.
				const UStaticMeshComponent* Art = Presentation.FindArt(*Structure);
				const FVeyraUnitArt* KindArt = StructureArt().Find(UVeyraGreyboxSettings::StructureArtId(Structure->GetStructureKind()));
				ASSERT_THAT(IsTrue(Art && KindArt && Art->GetStaticMesh() == KindArt->Intact));
				ASSERT_THAT(IsTrue(Art->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Art->CanEverAffectNavigation()));
				float Radius = 0.0f;
				float HalfHeight = 0.0f;
				Structure->GetSimpleCollisionCylinder(Radius, HalfHeight);
				ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Art->GetComponentLocation().Z, Structure->GetActorLocation().Z - HalfHeight, Tolerance), TEXT("standing on the floor")));
				ASSERT_THAT(IsFalse(Presentation.FindBody(*Structure)->IsVisible(), TEXT("its body hides behind it")));
			}

			// Each structure's bar names it and says whether the structures before it protect it.
			const double Now = Presentation.GetServerNow();
			const TOptional<FVeyraHudStructure> Outer = VeyraHud::StructureOf(*Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 0), Now);
			const TOptional<FVeyraHudStructure> Middle = VeyraHud::StructureOf(*Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 1), Now);
			ASSERT_THAT(IsTrue(Outer.IsSet() && Outer->Kind == EVeyraStructureKind::LaneSpire && !Outer->bInvulnerable));
			ASSERT_THAT(IsTrue(Middle.IsSet() && Middle->bInvulnerable, TEXT("the outer Spire still stands")));
			ASSERT_THAT(IsFalse(VeyraHud::StructureOf(*Caster, Now).IsSet(), TEXT("a Vanguard is no structure")));

			// A destroyed structure shows its wreck.
			AVeyraStructure* Outermost = Battleground->FindStructure(EVeyraTeam::B, EVeyraStructureKind::LaneSpire, EVeyraLane::Mid, 0);
			FVeyraRawDamageEvent Lethal;
			Lethal.Components.Add({ EVeyraDamageType::TrueDamage, Outermost->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) });
			Lethal.Delivery = EVeyraDamageDelivery::Developer;
			VeyraCombat::DealDamage(*Caster->GetAbilitySystemComponent(), *Outermost->GetAbilitySystemComponent(), Lethal);
			ASSERT_THAT(IsTrue(Outermost->IsDestroyed()));
			RefreshedGreybox();
			ASSERT_THAT(IsTrue(Presentation.FindArt(*Outermost)->GetStaticMesh() == StructureArt().Find(TEXT("laneSpire"))->Fallen));
		}

		TEST_METHOD(EachFluxbornWearsItsKindsArtAndCollapsesWhereItFalls)
		{
			// Every kind World.json defines has art in the Fluxborn kit's set (Art Direction, Fluxborn greybox meshes).
			const UVeyraUnitArtSet* Fluxborn = GetDefault<UVeyraGreyboxSettings>()->FluxbornArt.LoadSynchronous();
			ASSERT_THAT(IsNotNull(Fluxborn));
			TArray<FName> Kinds;
			for (const TPair<FVeyraContentId, FVeyraFluxbornDefinition>& Kind : UVeyraWorldTuningSubsystem::Get().Fluxborn.Units)
			{
				Kinds.Add(FName(Kind.Key.ToString()));
			}
			const TArray<FString> Problems = Fluxborn->Validate(Kinds);
			ASSERT_THAT(IsTrue(Problems.IsEmpty(), FString::Join(Problems, TEXT(" "))));
			const FVeyraUnitArt* StriderArt = Fluxborn->Find(TEXT("strider"));
			ASSERT_THAT(IsNotNull(StriderArt));

			UVeyraBattlegroundSubsystem* Battleground = Spawner.GetWorld().GetSubsystem<UVeyraBattlegroundSubsystem>();
			const FVeyraBattlegroundLayout Layout = VeyraWorldTests::CompactBattleground();
			VeyraWorldTests::SpawnCompactGround(Spawner.GetWorld());
			Battleground->SpawnStructures(Layout);
			AVeyraFluxborn* Strider = Battleground->SpawnFluxborn(FVeyraContentId::FromText(TEXT("strider")).GetValue(), EVeyraTeam::B, Layout.Lanes[0].Lane);
			ASSERT_THAT(IsNotNull(Strider));
			UVeyraGreyboxSubsystem& Presentation = RefreshedGreybox();

			// Its kind's art stands in for its body, on the floor, deciding nothing; its Flux shows its side.
			const UStaticMeshComponent* Art = Presentation.FindArt(*Strider);
			ASSERT_THAT(IsTrue(Art && Art->GetStaticMesh() == StriderArt->Intact));
			ASSERT_THAT(IsTrue(Art->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !Art->CanEverAffectNavigation()));
			float Radius = 0.0f;
			float HalfHeight = 0.0f;
			Strider->GetSimpleCollisionCylinder(Radius, HalfHeight);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Art->GetComponentLocation().Z, Strider->GetActorLocation().Z - HalfHeight, Tolerance), TEXT("standing on the floor")));
			ASSERT_THAT(IsFalse(Presentation.FindBody(*Strider)->IsVisible(), TEXT("its body hides behind it")));
			const int32 FluxSlot = Art->GetMaterialIndex(Fluxborn->FluxSlot);
			ASSERT_THAT(IsTrue(FluxSlot != INDEX_NONE, TEXT("the art has the Flux slot")));
			FLinearColor Flux = FLinearColor::Transparent;
			ASSERT_THAT(IsTrue(Art->GetMaterial(FluxSlot)->GetVectorParameterValue(FHashedMaterialParameterInfo(Fluxborn->FluxParameter), Flux)));
			ASSERT_THAT(IsTrue(Flux.Equals(GetDefault<UVeyraGreyboxSettings>()->EnemyColor), TEXT("side B is the enemy of a viewer on no side")));

			// Once dead, it lies collapsed where it fell, for its corpse's moment (Battleground Bible §4).
			FVeyraRawDamageEvent Lethal;
			Lethal.Components.Add({ EVeyraDamageType::TrueDamage, Strider->GetAbilitySystemComponent()->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) });
			Lethal.Delivery = EVeyraDamageDelivery::Developer;
			VeyraCombat::DealDamage(*Caster->GetAbilitySystemComponent(), *Strider->GetAbilitySystemComponent(), Lethal);
			ASSERT_THAT(IsFalse(Strider->IsAlive()));
			RefreshedGreybox();
			ASSERT_THAT(IsTrue(Presentation.FindArt(*Strider)->GetStaticMesh() == StriderArt->Fallen));
		}

		TEST_METHOD(NeutralUnitsAreDrawnGreyAndNamedOnTheHud)
		{
			// A creature says what it is; a Flux Well where it stands in its cycle (ADR-014 §2, §4).
			constexpr double UntilOpen = 10.0;
			AVeyraWildlife& Creature = Spawner.SpawnActor<AVeyraWildlife>();
			const FVeyraContentId Species = FVeyraContentId::FromText(TEXT("ashfang")).GetValue();
			Creature.Configure(Species, 0, Creature.GetActorLocation(), FVector2D(Creature.GetActorLocation()), OuterRadius);
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

		TEST_METHOD(ALingeringAreaIsMarkedAsItsEndDrawsNear)
		{
			UAbilitySystemComponent& Self = *Caster->GetAbilitySystemComponent();
			const auto Linger = [this, &Self](const FVector& At, FVeyraLingerEffects Effects) {
				AVeyraLingeringArea& Area = Spawner.SpawnActorAt<AVeyraLingeringArea>(At, FRotator::ZeroRotator);
				FVeyraEffectFrame Placement;
				Placement.Origin = At;
				Area.Arm(Self, Placement, CircleOf(InnerRadius), FVeyraLingerStatuses(), MoveTemp(Effects), LongSeconds, LongSeconds, ArchetypeTestId(TEXT("test_field")));
			};
			// One whose end hits and is warned of for its whole life, and one whose end does nothing.
			FVeyraLingerEffects Rupture;
			Rupture.End.Add(FVeyraPreparedZone{ CircleOf(InnerRadius), FVeyraPreparedEffects() });
			Rupture.EndWarningSeconds = LongSeconds;
			const FVector Ending(CastRange, 0.0, 0.0);
			Linger(Ending, MoveTemp(Rupture));
			Linger(FVector(0.0, CastRange, 0.0), FVeyraLingerEffects());

			const TArray<FVeyraTelegraph>& Telegraphs = RefreshedGreybox().GetTelegraphs();
			ASSERT_THAT(AreEqual(2, Telegraphs.Num()));
			const FVeyraTelegraph* Warned = Telegraphs.FindByPredicate([](const FVeyraTelegraph& Each) { return Each.Source == EVeyraTelegraphSource::LingeringAreaEnding; });
			ASSERT_THAT(IsTrue(Warned != nullptr && FVector::Dist2D(Warned->Placed.Origin, Ending) < Tolerance, TEXT("its end is marked (ADR-026 §4)")));
			ASSERT_THAT(AreEqual(1, Telegraphs.FilterByPredicate([](const FVeyraTelegraph& Each) { return Each.Source == EVeyraTelegraphSource::LingeringArea; }).Num()));
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
			// What it started with, which its sweep measures against (ADR-059 §3).
			ASSERT_THAT(IsTrue(Q.CooldownTotal >= Q.CooldownSeconds && Q.CooldownTotal == Participant.FindComponentByClass<UVeyraCooldownComponent>()->GetDurationSeconds(Mortar)));
			ASSERT_THAT(IsFalse(Player.Slots[1].Ability.IsValid() || Q.bCanRankUp, TEXT("W is empty, and no skill point is left")));
			const TOptional<FVeyraHudVitals> Vitals = VeyraHud::VitalsOf(*Caster, EVeyraTeam::A);
			ASSERT_THAT(IsTrue(Vitals.IsSet() && Player.Vitals.Health == Vitals->Health && Player.Vitals.MaxHealth == VeyraCombatTests::ExampleStats().MaxHealth));

			// A level's skill point opens Q, W and E, but not the ultimate before its level.
			Participant.FindComponentByClass<UVeyraProgressionComponent>()->AddExperience(VeyraProgression::ExperienceToNextLevel(1, Progression));
			Player = VeyraHud::DescribePlayer(Participant, Now);
			ASSERT_THAT(IsTrue(Player.Level == 2 && Player.UnspentSkillPoints == 1));
			ASSERT_THAT(IsTrue(Player.Slots[0].bCanRankUp && Player.Slots[1].bCanRankUp && !Player.Slots[3].bCanRankUp));
		}

		TEST_METHOD(TheHudSaysWhenAReadyAbilityCostsMoreThanItsOwnerHolds)
		{
			FArchetypeTestWorld World{ Spawner };
			const FVeyraContentId Dear = ArchetypeTestId(TEXT("test_dear_mortar"));
			ASSERT_THAT(IsTrue(World.Learn(*Caster, EVeyraAbilitySlot::Q, Dear)));
			const AVeyraPlayerState& Participant = *Caster->GetPlayerState<AVeyraPlayerState>();
			UAbilitySystemComponent& Abilities = *Caster->GetAbilitySystemComponent();
			const double Now = RefreshedGreybox().GetServerNow();
			Abilities.SetNumericAttributeBase(UVeyraResourceSet::GetResourceAttribute(), static_cast<float>(ResourceCost / 2.0));
			const FVeyraHudSlot Short = VeyraHud::DescribePlayer(Participant, Now).Slots[0];
			ASSERT_THAT(IsTrue(!Short.bAffordable && Short.CooldownSeconds == 0.0, TEXT("ready, but unaffordable (ADR-066 §1)")));
			ASSERT_THAT(IsNear(Short.Cost, ResourceCost, Tolerance, TEXT("with what it costs")));
			Abilities.SetNumericAttributeBase(UVeyraResourceSet::GetResourceAttribute(), static_cast<float>(ResourceCost));
			ASSERT_THAT(IsTrue(VeyraHud::DescribePlayer(Participant, Now).Slots[0].bAffordable));
			ASSERT_THAT(IsTrue(VeyraHud::DescribePlayer(Participant, Now).Slots[1].bAffordable, TEXT("an empty slot is never short")));
		}

		TEST_METHOD(ADecoyShowsItsOwnersBarsAndStatusesButNotTheirStealth)
		{
			UAbilitySystemComponent& Own = *Caster->GetAbilitySystemComponent();
			FVeyraMarkerSpec Spec;
			Spec.Id = ArchetypeTestId(TEXT("test_illusion"));
			Spec.LifetimeSeconds = 60.0;
			Spec.HitsToDestroy = 1;
			Spec.bPresentsAsOwner = true;
			AVeyraPlacedMarker* Decoy = AVeyraPlacedMarker::Place(Spawner.GetWorld(), Own, Spec, FTransform(FVector(CastRange, 0.0, 0.0)));
			ASSERT_THAT(IsNotNull(Decoy));
			// Its owner's enemies see its owner; its owner's side sees the illusion.
			ASSERT_THAT(IsTrue(&VeyraHud::PresentedUnitOf(*Decoy, EVeyraTeam::B) == Caster->GetPlayerState()));
			ASSERT_THAT(IsTrue(&VeyraHud::PresentedUnitOf(*Decoy, EVeyraTeam::A) == Decoy));
			const TOptional<FVeyraHudVitals> ToAllies = VeyraHud::VitalsOf(*Decoy, EVeyraTeam::A);
			ASSERT_THAT(IsTrue(ToAllies.IsSet() && ToAllies->MaxHealth == static_cast<double>(Spec.HitsToDestroy), TEXT("allies see its own single point")));
			FVeyraStatusSpec Hide;
			Hide.Id = ArchetypeTestId(TEXT("test_hidden"));
			Hide.Kind = EVeyraStatusKind::Invisible;
			Hide.DurationSeconds = 60.0;
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Own, Own, Hide)));
			const TOptional<FVeyraHudVitals> Shown = VeyraHud::VitalsOf(*Decoy, EVeyraTeam::B);
			const TOptional<FVeyraHudVitals> Real = VeyraHud::VitalsOf(*Caster, EVeyraTeam::B);
			ASSERT_THAT(IsTrue(Shown.IsSet() && Real.IsSet() && Shown->MaxHealth == Real->MaxHealth && Shown->Health == Real->Health,
				TEXT("its owner's Health, not its own single point")));
			const double Now = RefreshedGreybox().GetServerNow();
			ASSERT_THAT(IsTrue(VeyraHud::StatusesOf(*Caster, Now, EVeyraTeam::A).ContainsByPredicate([](const FVeyraHudStatus& Each) { return Each.Kind == EVeyraStatusKind::Invisible; })));
			ASSERT_THAT(IsFalse(VeyraHud::StatusesOf(*Decoy, Now, EVeyraTeam::B).ContainsByPredicate([](const FVeyraHudStatus& Each) { return Each.Kind == EVeyraStatusKind::Invisible; }),
				TEXT("the stealth it covers for stays hidden")));
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

		TEST_METHOD(TheHudShowsACommandedEchosIntegrityAndWhatItMayCast)
		{
			// Fixture projection: forming for a second, immune for two, its Integrity decaying from 100 at 10 a second.
			constexpr double Forming = 1.0;
			constexpr double Immune = 2.0;
			constexpr double Integrity = 100.0;
			constexpr double Reach = 800.0;
			const FVeyraContentId Stasis = ArchetypeTestId(TEXT("test_hud_stasis"));
			const FVeyraContentId Project = ArchetypeTestId(TEXT("test_hud_echo"));
			Tuning.Statuses.Add(Stasis, StatusOf(EVeyraStatusKind::Stasis, 0.0, Integrity));
			FVeyraEchoAbilityTuning& Echo = Tuning.Echo.Add(Project);
			Echo.Cast = InstantCast(Reach, 0.0, 0.0);
			Echo.DamageCoefficient = 0.25;
			Echo.Slots = { EVeyraAbilitySlot::Q, EVeyraAbilitySlot::E };
			Echo.Repeats = 1;
			FVeyraEchoProjectionTuning& Projection = Echo.Projection.AddDefaulted_GetRef();
			Projection.Stasis = Stasis;
			Projection.FormationSeconds = Forming;
			Projection.ImmunitySeconds = Immune;
			Projection.Integrity = Integrity;
			Projection.DecayPerSecond = Integrity / 10.0;
			Projection.MaxRadius = Reach;
			Projection.MinRadius = Reach / 2.0;
			Projection.RadiusExponent = 1.0;
			Projection.UpdateSeconds = 0.1;

			AVeyraPlayerState& Participant = *Caster->GetPlayerState<AVeyraPlayerState>();
			// The Echo's times are the server's world time, as the HUD's clock is.
			const double Now = Spawner.GetWorld().GetTimeSeconds();
			ASSERT_THAT(IsFalse(VeyraHud::DescribePlayer(Participant, Now).Echo.IsSet()));
			UVeyraEchoSubsystem* Echoes = Spawner.GetWorld().GetSubsystem<UVeyraEchoSubsystem>();
			ASSERT_THAT(IsNotNull(Echoes->Project(*Participant.GetAbilitySystemComponent(), Project, Caster->GetActorLocation() + FVector(Reach / 2.0, 0.0, 0.0))));
			const FVeyraHudPlayer Player = VeyraHud::DescribePlayer(Participant, Now);
			ASSERT_THAT(IsTrue(Player.Echo.IsSet()));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Player.Echo->IntegrityShare, 1.0), TEXT("whole as it forms")));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Player.Echo->FormingSeconds, Forming) && FMath::IsNearlyEqual(Player.Echo->ImmuneSeconds, Immune)));
			ASSERT_THAT(IsTrue(Player.Echo->RepeatsLeft == 1 && Player.Echo->Slots == Echo.Slots, TEXT("what it may cast, and how often")));

			Echoes->End(*Participant.GetAbilitySystemComponent(), EVeyraEchoEnd::Faded);
			ASSERT_THAT(IsFalse(VeyraHud::DescribePlayer(Participant, Now).Echo.IsSet(), TEXT("an ended Echo leaves the HUD")));
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

		// ADR-052 §4: the ring follows the reach as it is now, statuses included, out to a target's edge.
		TEST_METHOD(ShowAttackRangeRingsTheBasicAttacksReachAsItIsNow)
		{
			constexpr double Reach = 525.0;
			constexpr double Extra = 75.0;
			// A Vanguard's attack is its participant's.
			UVeyraBasicAttackComponent* Attacks = Caster->GetPlayerState()->FindComponentByClass<UVeyraBasicAttackComponent>();
			ASSERT_THAT(IsNotNull(Attacks));
			FVeyraBasicAttackProfile Profile;
			Profile.Range = Reach;
			Profile.DamageType = EVeyraDamageType::Physical;
			Profile.PhysicalPowerRatio = 1.0;
			Profile.WindupFraction = 0.25;
			Profile.AcquisitionRadius = Reach;
			ASSERT_THAT(IsTrue(Attacks->SetProfile(Profile)));
			const double Body = Caster->GetSimpleCollisionRadius();
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraHud::AttackReachOf(*Caster).Get(0.0), Reach + Body), TEXT("the reach, out from the body's edge")));

			FVeyraStatusSpec Longer;
			Longer.Id = ArchetypeTestId(TEXT("test_reach"));
			Longer.Kind = EVeyraStatusKind::AttackRange;
			Longer.Magnitude = Extra;
			Longer.DurationSeconds = LongSeconds;
			UAbilitySystemComponent& Self = *Caster->GetAbilitySystemComponent();
			ASSERT_THAT(IsTrue(VeyraCombat::ApplyStatus(Self, Self, Longer)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(VeyraHud::AttackReachOf(*Caster).Get(0.0), Reach + Extra + Body), TEXT("a longer reach rings further at once")));
			ASSERT_THAT(IsFalse(VeyraHud::AttackReachOf(Spawner.SpawnActor<AActor>()).IsSet(), TEXT("nothing for a unit with no basic attack")));
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
