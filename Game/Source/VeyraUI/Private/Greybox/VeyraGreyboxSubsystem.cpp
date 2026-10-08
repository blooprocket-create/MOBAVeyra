// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraGreyboxSubsystem.h"

#include "Engine/Font.h"
#include "Styling/CoreStyle.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Camera/VeyraCameraRig.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Casting/VeyraCastTelegraphs.h"
#include "Components/LineBatchComponent.h"
#include "Companions/VeyraCompanion.h"
#include "Cues/VeyraCombatCueSubsystem.h"
#include "Echoes/VeyraEcho.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Greybox/VeyraBodyLead.h"
#include "Input/VeyraOrderMark.h"
#include "Statuses/VeyraStatusTypes.h"
#include "VeyraCombatVerbs.h"
#include "Components/StaticMeshComponent.h"
#include "Delivery/VeyraDelayedArea.h"
#include "Delivery/VeyraLingeringArea.h"
#include "Delivery/VeyraProjectile.h"
#include "Entities/VeyraPlacedMarker.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fog/VeyraDenseFogBank.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Greybox/VeyraFountainShop.h"
#include "Greybox/VeyraGreyboxOutline.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraUnitArtSet.h"
#include "Greybox/VeyraVanguardAnimInstance.h"
#include "Greybox/VeyraVanguardArtSet.h"
#include "Greybox/VeyraVanguardSkin.h"
#include "Components/SkeletalMeshComponent.h"
#include "Hud/VeyraHudModel.h"
#include "Hud/VeyraHudOverlay.h"
#include "Engine/NetConnection.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Hud/VeyraFogOfWarModel.h"
#include "Kit/VeyraSkillEffectsSubsystem.h"
#include "Greybox/VeyraCastGlow.h"
#include "Settings/VeyraDisplayRules.h"
#include "State/VeyraVisionTeamState.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Components/DirectionalLightComponent.h"
#include "Greybox/VeyraHitFeel.h"
#include "Greybox/VeyraTelegraphFill.h"
#include "Greybox/VeyraToonLight.h"
#include "Movement/VeyraDrawnBody.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Structures/VeyraStructure.h"
#include "Targeting/VeyraTargeting.h"
#include "Terrain/VeyraTerrainWall.h"
#include "Terrain/VeyraSurfacePlacement.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "VeyraGameState.h"
#include "Match/VeyraMatchMenuSubsystem.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamStart.h"
#include "VeyraVanguardCharacter.h"
#include "VeyraUILog.h"

namespace
{
	/** Dense Fog lies on top of the ground's other markings, the map's and an ability's alike. */
	constexpr int32 FogMarkingLayer = 4;

	/** The cast state beside Unit's Ability System Component: on a Vanguard, its participant's. */
	const UVeyraCastStateComponent* FindGreyboxCastState(const AActor& Unit)
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
		const AActor* Owner = AbilitySystem ? AbilitySystem->GetOwner() : nullptr;
		return Owner ? Owner->FindComponentByClass<UVeyraCastStateComponent>() : nullptr;
	}

	/** Scales a mesh of Mesh's bounds so its box has the half extents HalfSize, centred on its parent. */
	void FitGreyboxShape(UStaticMeshComponent& Shape, const FVector& HalfSize)
	{
		const FBoxSphereBounds Bounds = Shape.GetStaticMesh()->GetBounds();
		const FVector Scale = HalfSize / Bounds.BoxExtent.ComponentMax(FVector(UE_KINDA_SMALL_NUMBER));
		Shape.SetRelativeScale3D(Scale);
		Shape.SetRelativeLocation(-Bounds.Origin * Scale);
	}
}

bool UVeyraGreyboxSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Presentation is for worlds someone watches: game and play-in-editor worlds, never a dedicated server's.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraGreyboxSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Veyra.AuthoredTerrain"))) { bAuthoredTerrain = true; break; }
	}
}

void UVeyraGreyboxSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The shell's type, from the engine's own font files; they load as the HUD first draws with them.
	HudFont = NewObject<UFont>(this);
	HudFont->FontCacheType = EFontCacheType::Runtime;
	HudFont->GetMutableInternalCompositeFont() = *FCoreStyle::GetDefaultFont();
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	TArray<FString> Problems = Settings.Validate();
	if (Problems.IsEmpty())
	{
		BodyMesh = Settings.BodyMesh.LoadSynchronous();
		ProjectileMesh = Settings.ProjectileMesh.LoadSynchronous();
		ShapeMaterial = Settings.ShapeMaterial.LoadSynchronous();
		GroundMesh = Settings.GroundMesh.LoadSynchronous();
		PadMesh = Settings.PadMesh.LoadSynchronous();
		// The structure kit's art set must dress every kind (ADR-006 §6: asset references by stable ID).
		StructureArt = Settings.StructureArt.LoadSynchronous();
		if (!StructureArt)
		{
			Problems.Add(FString::Printf(TEXT("StructureArt: %s does not load."), *Settings.StructureArt.ToString()));
		}
		else
		{
			TArray<FName> Kinds;
			for (const EVeyraStructureKind Kind : { EVeyraStructureKind::LaneSpire, EVeyraStructureKind::BaseTower, EVeyraStructureKind::Inhibitor, EVeyraStructureKind::PrimeWell })
			{
				Kinds.Add(UVeyraGreyboxSettings::StructureArtId(Kind));
			}
			for (const FString& Problem : StructureArt->Validate(Kinds))
			{
				Problems.Add(TEXT("StructureArt ") + Problem);
			}
		}
		// The Fluxborn kit's set need not dress every kind: one without art keeps its body.
		FluxbornArt = Settings.FluxbornArt.LoadSynchronous();
		if (!FluxbornArt)
		{
			Problems.Add(FString::Printf(TEXT("FluxbornArt: %s does not load."), *Settings.FluxbornArt.ToString()));
		}
		else
		{
			for (const FString& Problem : FluxbornArt->Validate(TConstArrayView<FName>()))
			{
				Problems.Add(TEXT("FluxbornArt ") + Problem);
			}
		}
		// Nor need the Vanguards' set dress every Vanguard: one without art keeps its body (ADR-064 §1).
		VanguardArt = Settings.VanguardArt.LoadSynchronous();
		if (!VanguardArt)
		{
			Problems.Add(FString::Printf(TEXT("VanguardArt: %s does not load."), *Settings.VanguardArt.ToString()));
		}
		else
		{
			for (const FString& Problem : VanguardArt->Validate())
			{
				Problems.Add(TEXT("VanguardArt ") + Problem);
			}
		}
		if (!GroundMesh)
		{
			Problems.Add(FString::Printf(TEXT("GroundMesh: %s does not load."), *Settings.GroundMesh.ToString()));
		}
		if (!PadMesh)
		{
			Problems.Add(FString::Printf(TEXT("PadMesh: %s does not load."), *Settings.PadMesh.ToString()));
		}
		if (!BodyMesh)
		{
			Problems.Add(FString::Printf(TEXT("BodyMesh: %s does not load."), *Settings.BodyMesh.ToString()));
		}
		if (!ProjectileMesh)
		{
			Problems.Add(FString::Printf(TEXT("ProjectileMesh: %s does not load."), *Settings.ProjectileMesh.ToString()));
		}
		if (!ShapeMaterial)
		{
			Problems.Add(FString::Printf(TEXT("ShapeMaterial: %s does not load."), *Settings.ShapeMaterial.ToString()));
		}
		HitFlashMaterial = Settings.HitFlashMaterial.LoadSynchronous();
		if (!HitFlashMaterial)
		{
			Problems.Add(FString::Printf(TEXT("HitFlashMaterial: %s does not load; run BuildPresentationMaterials.ps1."), *Settings.HitFlashMaterial.ToString()));
		}
		ImpactSound = Settings.ImpactSound.LoadSynchronous();
		SwingSound = Settings.SwingSound.LoadSynchronous();
		CastSound = Settings.CastSound.LoadSynchronous();
		DeathSound = Settings.DeathSound.LoadSynchronous();
		ClickSound = Settings.ClickSound.LoadSynchronous();
		LevelUpSound = Settings.LevelUpSound.LoadSynchronous();
		if (!ImpactSound || !SwingSound || !CastSound || !DeathSound || !ClickSound || !LevelUpSound)
		{
			Problems.Add(TEXT("ImpactSound: the cue sounds do not all load; run BuildCueSounds.ps1."));
		}
		// Heard whole near the camera's focus and fading beyond, at most so many at once, the quietest giving way.
		CueAttenuation = NewObject<USoundAttenuation>(this);
		CueAttenuation->Attenuation.bAttenuate = true;
		CueAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		CueAttenuation->Attenuation.AttenuationShapeExtents = FVector(Settings.SoundAudibleRadius, 0.0, 0.0);
		CueAttenuation->Attenuation.FalloffDistance = Settings.SoundFalloffDistance;
		CueConcurrency = NewObject<USoundConcurrency>(this);
		CueConcurrency->Concurrency.MaxCount = Settings.MaxCueSounds;
		CueConcurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopQuietest;
		ImpactEffect = Settings.ImpactEffect.LoadSynchronous();
		CastEffect = Settings.CastEffect.LoadSynchronous();
		DeathEffect = Settings.DeathEffect.LoadSynchronous();
		LevelUpEffect = Settings.LevelUpEffect.LoadSynchronous();
		TrailEffect = Settings.TrailEffect.LoadSynchronous();
		if (!ImpactEffect || !CastEffect || !DeathEffect || !LevelUpEffect || !TrailEffect)
		{
			Problems.Add(TEXT("ImpactEffect: the impact, cast, death, level-up and trail effects do not all load; run BuildEffects.ps1."));
		}
		HoverOutlineMaterial = Settings.HoverOutlineMaterial.LoadSynchronous();
		if (!HoverOutlineMaterial)
		{
			Problems.Add(FString::Printf(TEXT("HoverOutlineMaterial: %s does not load; run BuildPresentationMaterials.ps1."), *Settings.HoverOutlineMaterial.ToString()));
		}
		else
		{
			// The stencils are the generated material's own, so the pass and the bodies agree.
			const TPair<FName, int32*> Stencils[] = { { Settings.HoverEnemyStencilParameter, &EnemyStencil }, { Settings.HoverAllyStencilParameter, &AllyStencil },
				{ Settings.HoverNeutralStencilParameter, &NeutralStencil } };
			for (const TPair<FName, int32*>& Stencil : Stencils)
			{
				float Value = 0.0f;
				if (!HoverOutlineMaterial->GetScalarParameterDefaultValue(FHashedMaterialParameterInfo(Stencil.Key), Value) || Value < 1.0f)
				{
					Problems.Add(FString::Printf(TEXT("HoverOutlineMaterial: no stencil in its %s parameter."), *Stencil.Key.ToString()));
				}
				*Stencil.Value = FMath::RoundToInt32(Value);
			}
		}
		// The toon characters' ink and light (ADR-068 §2-3); the ink's stencil is the generated material's own.
		ToonInkMaterial = Settings.ToonInkMaterial.LoadSynchronous();
		float Ink = 0.0f;
		if (!ToonInkMaterial || !ToonInkMaterial->GetScalarParameterDefaultValue(FHashedMaterialParameterInfo(Settings.ToonInkStencilParameter), Ink) || Ink < 1.0f)
		{
			Problems.Add(FString::Printf(TEXT("ToonInkMaterial: %s does not load with a stencil in its %s parameter; run BuildPresentationMaterials.ps1."),
				*Settings.ToonInkMaterial.ToString(), *Settings.ToonInkStencilParameter.ToString()));
		}
		InkStencil = FMath::RoundToInt32(Ink);
		TelegraphFillMaterial = Settings.TelegraphFillMaterial.LoadSynchronous();
		TelegraphFillMesh = Settings.TelegraphFillMesh.LoadSynchronous();
		if (!TelegraphFillMaterial || !TelegraphFillMesh)
		{
			Problems.Add(FString::Printf(TEXT("TelegraphFillMaterial: %s or its quad %s does not load; run BuildPresentationMaterials.ps1."),
				*Settings.TelegraphFillMaterial.ToString(), *Settings.TelegraphFillMesh.ToString()));
		}
		ToonLight = Settings.ToonLight.LoadSynchronous();
		if (!ToonLight || !ToonLight->GetVectorParameterByName(Settings.ToonSunDirectionParameter) || !ToonLight->GetVectorParameterByName(Settings.ToonSunColorParameter))
		{
			Problems.Add(FString::Printf(TEXT("ToonLight: %s does not load with its %s and %s parameters; run BuildVanguardBodies.ps1."), *Settings.ToonLight.ToString(),
				*Settings.ToonSunDirectionParameter.ToString(), *Settings.ToonSunColorParameter.ToString()));
		}
	}
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogVeyraUI, Error, TEXT("The grey-box presentation is off: %s"), *Problem);
	}
	bReady = Problems.IsEmpty();
	// Bodies answer the fight's moments (ADR-063 §2).
	if (UVeyraCombatCueSubsystem* Cues = Collection.InitializeDependency<UVeyraCombatCueSubsystem>())
	{
		CueHandle = Cues->OnCue.AddUObject(this, &UVeyraGreyboxSubsystem::OnCombatCue);
	}
}

void UVeyraGreyboxSubsystem::OnCombatCue(const FVeyraCombatCue& Cue)
{
	if (FBody* Body = Cue.Unit.IsValid() ? Bodies.Find(Cue.Unit) : nullptr)
	{
		VeyraBodyFeedback::Note(Body->Feedback, Cue, GetWorld()->GetRealTimeSeconds(), GetServerNow());
		// An animated body acts it out, its windup timed to end as the attack or cast commits (ADR-064 §3), a skill with a
		// clip of its own playing it (ADR-072 §1).
		if (UVeyraVanguardAnimInstance* Animation = Body->Skin.IsValid() ? Cast<UVeyraVanguardAnimInstance>(Body->Skin->GetAnimInstance()) : nullptr)
		{
			Animation->NoteCue(Cue.Kind, static_cast<float>(FMath::Max(0.0, Cue.EndsAt - GetServerNow())),
				Cue.Ability.IsValid() ? FName(*Cue.Ability.ToString()) : NAME_None);
		}
	}
	// Hit feel (ADR-068 §4): a struck generated body holds its pose a moment, and the player's own Vanguard's heavy hit or
	// fall kicks their camera as hard as their Screen Shake allows.
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const double RealNow = GetWorld()->GetRealTimeSeconds();
	if (FBody* Struck = Cue.Kind == EVeyraCombatCueKind::Hit && Cue.Unit.IsValid() ? Bodies.Find(Cue.Unit) : nullptr; Struck && Struck->Skin.IsValid())
	{
		Struck->HitStopUntil = RealNow + Settings.HitStopSeconds;
	}
	if (IsViewersVanguard(Cue.Unit.Get()))
	{
		const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Cue.Unit.Get());
		const double MaxHealth = Abilities ? Abilities->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) : 0.0;
		const float Scale = VeyraInterfacePreferences::Resolve(Settings, VeyraInterfacePreferences::StoreOf(this)).ScreenShakeScale;
		const double Amplitude = VeyraHitFeel::AmplitudeOf(Cue.Kind, Cue.Amount, MaxHealth, Settings.HeavyHitShare, Settings.HitShakeAmplitude,
			Settings.DeathShakeAmplitude, Scale);
		// A weaker blow does not cut a stronger kick short.
		if (Amplitude > 0.0 && !VeyraHitFeel::Outshakes(ShakeAmplitude, RealNow - ShakeStartedAt, Settings.HitShakeSeconds, Amplitude))
		{
			ShakeAmplitude = Amplitude;
			ShakeStartedAt = RealNow;
		}
	}
	PlayEffect(Cue);
	PlaySound(Cue);
	NoteSwing(Cue);
	// The player's own level-up is announced on the HUD (ADR-065 §5).
	if (Cue.Kind == EVeyraCombatCueKind::LevelUp && IsViewersVanguard(Cue.Unit.Get()))
	{
		OwnLevelUp = FVeyraLevelUpMoment{ static_cast<int32>(Cue.Amount), FPlatformTime::Seconds() };
	}
}

FVeyraBodyLead UVeyraGreyboxSubsystem::OwnLeadOf(const APawn& Unit, double Radius) const
{
	// Only the body the player's orders move, and only while it may move (ADR-067 §2).
	const AVeyraPlayerController* Local = Cast<AVeyraPlayerController>(GetWorld()->GetFirstPlayerController());
	const TOptional<FVeyraOrderMark>& Mark = Local ? Local->GetOrderMark() : TOptional<FVeyraOrderMark>();
	const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
	if (!Mark || !Abilities || Local->GetCommandedBody() != &Unit || !VeyraTargeting::IsAlive(&Unit)
		|| EnumHasAnyFlags(VeyraCombat::GetActionBlocks(*Abilities), EVeyraActionBlocks::Move))
	{
		return FVeyraBodyLead();
	}
	// A move or an Attack Move is run toward, and so is an attack's target beyond the body's reach, which it chases; one
	// within reach is only faced.
	const AActor* Target = Mark->Target.Get();
	const FVector Point = Target ? Target->GetActorLocation() : Mark->Location;
	const TOptional<double> Reach = VeyraHud::AttackReachOf(Unit);
	const bool bRun = Mark->Kind != EVeyraOrderMarkKind::Attack || (Target && Reach && FVector::Dist2D(Point, Unit.GetActorLocation()) > *Reach);
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	return VeyraBodyLead::For(Point, GetWorld()->GetRealTimeSeconds() - Mark->GivenAt, Unit.GetActorLocation(), Unit.GetVelocity(),
		Abilities->GetNumericAttribute(UVeyraMobilitySet::GetMoveSpeedAttribute()), bRun, Settings.OwnLeadSeconds, Settings.OwnLeadAlignDegrees, Radius);
}

bool UVeyraGreyboxSubsystem::IsViewersVanguard(const AActor* Unit) const
{
	// The local controller possesses nothing: its participant's Vanguard is the player's own (ADR-006 §6).
	const AVeyraPlayerController* Viewer = Cast<AVeyraPlayerController>(GetWorld()->GetFirstPlayerController());
	return Unit && Viewer && Unit == Viewer->GetVanguard();
}

void UVeyraGreyboxSubsystem::NoteSwing(const FVeyraCombatCue& Cue)
{
	const AActor* Attacker = Cue.Unit.Get();
	const AActor* Target = Cue.Target.Get();
	if (Cue.Kind != EVeyraCombatCueKind::AttackCommit || !Attacker || !Target)
	{
		return;
	}
	// A swing only where the attack reaches across: a ranged attack shows its projectile's trail instead.
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const double Apart = FVector::Dist2D(Attacker->GetActorLocation(), Target->GetActorLocation());
	const double TargetRadius = Target->GetSimpleCollisionRadius();
	if (Apart - Attacker->GetSimpleCollisionRadius() - TargetRadius > Settings.SwingArcReach)
	{
		return;
	}
	SwingArcs.Add(FVeyraSwingArc{ Attacker, (Target->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D(), Apart + TargetRadius,
		GetWorld()->GetRealTimeSeconds(), SideColorOf(*Attacker) });
}

void UVeyraGreyboxSubsystem::DrawSwingArcs()
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const double Now = GetWorld()->GetRealTimeSeconds();
	SwingArcs.RemoveAll([&Settings, Now](const FVeyraSwingArc& Swing) { return !Swing.Attacker.IsValid() || Now - Swing.At >= Settings.SwingArcSeconds; });
	if (!TelegraphLines)
	{
		return;
	}
	for (const FVeyraSwingArc& Swing : SwingArcs)
	{
		FVeyraShape Arc;
		Arc.Kind = EVeyraShapeKind::Sector;
		Arc.Radius = Swing.Radius;
		Arc.ArcDegrees = Settings.SwingArcDegrees;
		FLinearColor Color = Swing.Color;
		Color.A *= static_cast<float>(1.0 - (Now - Swing.At) / Settings.SwingArcSeconds);
		const FVeyraPlacedShape OnGround{ Arc, GroundUnder(Swing.Attacker->GetActorLocation()), Swing.Direction };
		for (const FVeyraOutlineSegment& Segment : VeyraGreyboxOutline::Of(OnGround, Settings.CircleSegments))
		{
			TelegraphLines->DrawLine(Segment.Start, Segment.End, Color, SDPG_World, Settings.TelegraphThickness, 0.0f);
		}
	}
}

USoundBase* UVeyraGreyboxSubsystem::SoundFor(EVeyraCombatCueKind Kind) const
{
	switch (Kind)
	{
	case EVeyraCombatCueKind::Hit:
		return ImpactSound;
	case EVeyraCombatCueKind::AttackCommit:
		return SwingSound;
	case EVeyraCombatCueKind::CastCommit:
		return CastSound;
	case EVeyraCombatCueKind::Death:
		return DeathSound;
	case EVeyraCombatCueKind::LevelUp:
		return LevelUpSound;
	case EVeyraCombatCueKind::AttackWindup:
	case EVeyraCombatCueKind::CastWindup:
	case EVeyraCombatCueKind::ProjectileEnd:
		break;
	}
	return nullptr;
}

void UVeyraGreyboxSubsystem::PlaySound(const FVeyraCombatCue& Cue)
{
	const AActor* Unit = Cue.Unit.Get();
	USoundBase* Sound = SoundFor(Cue.Kind);
	if (!Unit || !Sound || !bReady)
	{
		return;
	}
	// A level-up is heard only by the player whose Vanguard it is, as the HUD's own sounds are (ADR-065 §5).
	const bool bOwnLevelUp = Cue.Kind == EVeyraCombatCueKind::LevelUp;
	if (bOwnLevelUp && !IsViewersVanguard(Unit))
	{
		return;
	}
	const float Volume = VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), VeyraInterfacePreferences::StoreOf(this)).EffectsVolume;
	if (Volume <= 0.0f)
	{
		return;
	}
	if (bOwnLevelUp)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), Sound, Volume);
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, Unit->GetActorLocation(), FRotator::ZeroRotator, Volume, /*PitchMultiplier*/ 1.0f,
		/*StartTime*/ 0.0f, CueAttenuation, CueConcurrency);
}

TArray<AVeyraFountainShop*> UVeyraGreyboxSubsystem::GetShops() const
{
	TArray<AVeyraFountainShop*> Standing;
	for (const TWeakObjectPtr<AVeyraFountainShop>& Shop : Shops)
	{
		if (AVeyraFountainShop* Each = Shop.Get())
		{
			Standing.Add(Each);
		}
	}
	return Standing;
}

void UVeyraGreyboxSubsystem::RefreshShops()
{
	if (!bReady || !Shops.IsEmpty())
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	for (TActorIterator<AVeyraTeamStart> It(GetWorld()); It; ++It)
	{
		const AVeyraTeamStart& Start = **It;
		// In front of the fountain, toward the battleground's centre, on its floor.
		const FVector From = Start.GetActorLocation();
		const FVector Toward = FVector(-From.X, -From.Y, 0.0).GetSafeNormal();
		const FVector Floor = GroundUnder(From + Toward * Settings.ShopOffset) - FVector::UpVector * Settings.TelegraphLift;
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		AVeyraFountainShop* Shop = GetWorld()->SpawnActor<AVeyraFountainShop>(Floor, FRotator::ZeroRotator, Spawn);
		if (!Shop)
		{
			continue;
		}
		Shop->SetTeam(Start.GetVeyraTeam());
		Shop->Build(*BodyMesh, *ProjectileMesh, *ShapeMaterial, Settings.ColorParameter, Settings.ShopColor, Settings.ShopRadius, Settings.ShopHeight);
		Shops.Add(Shop);
	}
}

bool UVeyraGreyboxSubsystem::HoverShop(AVeyraFountainShop* Under, EVeyraTeam Viewer, bool bClicked)
{
	// Only the player's own side's shop answers them; a viewer on no side has none.
	AVeyraFountainShop* Own = Under && Viewer != EVeyraTeam::None && Under->GetTeam() == Viewer ? Under : nullptr;
	if (AVeyraFountainShop* Was = HoveredShop.Get(); Was && Was != Own)
	{
		Was->SetOutlined(false, AllyStencil);
	}
	HoveredShop = Own;
	if (!Own)
	{
		return false;
	}
	Own->SetOutlined(true, AllyStencil);
	// The shop opens as its key opens it: browsing is the player's anywhere, buying the server's to allow (ADR-012 §11).
	UGameInstance* Game = GetWorld()->GetGameInstance();
	UVeyraMatchMenuSubsystem* Menus = Game ? Game->GetSubsystem<UVeyraMatchMenuSubsystem>() : nullptr;
	if (bClicked && Menus && !Menus->IsShopOpen())
	{
		Menus->ToggleShop();
	}
	return true;
}

void UVeyraGreyboxSubsystem::RefreshSound()
{
	AVeyraPlayerController* Local = Cast<AVeyraPlayerController>(GetWorld()->GetFirstPlayerController());
	const AVeyraCameraRig* Rig = Local ? Local->GetCameraRig() : nullptr;
	if (!Local || !Rig || !bReady)
	{
		return;
	}
	if (ListeningFrom.Get() != Local)
	{
		Local->SetAudioListenerOverride(Rig->GetRootComponent(), FVector::ZeroVector, FRotator::ZeroRotator);
		ListeningFrom = Local;
	}
	// The player's own order clicks once, at once, with its mark.
	const TOptional<FVeyraOrderMark>& Mark = Local->GetOrderMark();
	if (Mark && Mark->GivenAt != ClickedFor)
	{
		ClickedFor = Mark->GivenAt;
		const float Volume = VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), VeyraInterfacePreferences::StoreOf(this)).EffectsVolume;
		if (Volume > 0.0f)
		{
			UGameplayStatics::PlaySound2D(GetWorld(), ClickSound, Volume);
		}
	}
}

UNiagaraSystem* UVeyraGreyboxSubsystem::EffectFor(EVeyraCombatCueKind Kind) const
{
	switch (Kind)
	{
	case EVeyraCombatCueKind::Hit:
		return ImpactEffect;
	case EVeyraCombatCueKind::CastCommit:
		return CastEffect;
	case EVeyraCombatCueKind::Death:
		return DeathEffect;
	case EVeyraCombatCueKind::LevelUp:
		return LevelUpEffect;
	case EVeyraCombatCueKind::AttackWindup:
	case EVeyraCombatCueKind::AttackCommit:
	case EVeyraCombatCueKind::CastWindup:
	case EVeyraCombatCueKind::ProjectileEnd:
		break;
	}
	return nullptr;
}

UNiagaraComponent* UVeyraGreyboxSubsystem::PlayEffect(const FVeyraCombatCue& Cue)
{
	const AActor* Unit = Cue.Unit.Get();
	UNiagaraSystem* Effect = EffectFor(Cue.Kind);
	// An ability with its own commit effect shows it in place of the shared flash (ADR-072 §4).
	float OwnScale = 0.0f;
	bool bAtTarget = false;
	const UVeyraSkillEffectsSubsystem* Skills = Cue.Kind == EVeyraCombatCueKind::CastCommit ? GetWorld()->GetSubsystem<UVeyraSkillEffectsSubsystem>() : nullptr;
	UNiagaraSystem* Own = Skills ? Skills->CommitEffectOf(Cue.Ability, OwnScale, bAtTarget) : nullptr;
	Effect = Own ? Own : Effect;
	if (!Unit || !Effect || !bReady)
	{
		return nullptr;
	}
	// A cast flashes from its caster toward where it was aimed, or where it was aimed if its effect lands there; the rest
	// where their unit stands.
	const FVector From = Unit->GetActorLocation();
	const FVector At = VeyraSkillEffects::CommitPlacement(From, Cue.Location, bAtTarget);
	const FVector Toward = Cue.Kind == EVeyraCombatCueKind::CastCommit ? (Cue.Location - From).GetSafeNormal2D() : FVector::ZeroVector;
	const FRotator Facing = Toward.IsZero() ? Unit->GetActorRotation() : Toward.Rotation();
	// Pooled: a battleground's waves raise many hits a second.
	UNiagaraComponent* Played = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Effect, At, Facing, FVector::OneVector, /*bAutoDestroy*/ true,
		/*bAutoActivate*/ true, ENCPoolMethod::AutoRelease);
	if (Played)
	{
		Played->SetVariableLinearColor(GetDefault<UVeyraGreyboxSettings>()->EffectColorParameter, SideColorOf(*Unit));
		if (Own)
		{
			Played->SetVariableFloat(GetDefault<UVeyraGreyboxSettings>()->EffectScaleParameter, OwnScale);
		}
	}
	return Played;
}

double UVeyraGreyboxSubsystem::GetHealthLossPerSecond(const AActor& Unit) const
{
	const FBody* Body = Bodies.Find(&Unit);
	return Body ? Body->Loss.PerSecond(GetWorld()->GetRealTimeSeconds(), GetDefault<UVeyraGreyboxSettings>()->LastHitLossWindowSeconds) : 0.0;
}

float UVeyraGreyboxSubsystem::GetFlashOf(const AActor& Unit) const
{
	const FBody* Body = Bodies.Find(&Unit);
	const UMaterialInstanceDynamic* Flash = Body && Body->Flashing.IsValid() ? Body->Flash.Get() : nullptr;
	float Strength = 0.0f;
	if (Flash)
	{
		Flash->GetScalarParameterValue(FHashedMaterialParameterInfo(GetDefault<UVeyraGreyboxSettings>()->HitFlashStrengthParameter), Strength);
	}
	return Strength;
}

void UVeyraGreyboxSubsystem::ShowHover(const AActor* NewHovered)
{
	if (const AActor* Was = Hovered.Get(); Was && Was != NewHovered)
	{
		SetStencils(*Was, false);
	}
	Hovered = NewHovered;
	// Every frame: the unit's art may change under the cursor, as a structure falls.
	if (NewHovered)
	{
		SetStencils(*NewHovered, true);
	}
}

int32 UVeyraGreyboxSubsystem::HoverStencilOf(const AActor& Unit) const
{
	const EVeyraTeam Team = VeyraTeams::TeamOf(&Unit);
	if (Team == EVeyraTeam::None)
	{
		return NeutralStencil;
	}
	const EVeyraTeam ViewerTeam = GetViewerTeam();
	const EVeyraTeam Allies = ViewerTeam == EVeyraTeam::None ? EVeyraTeam::A : ViewerTeam;
	return Team == Allies ? AllyStencil : EnemyStencil;
}

void UVeyraGreyboxSubsystem::SetStencils(const AActor& Unit, bool bHovered) const
{
	const FBody* Body = Bodies.Find(&Unit);
	if (!Body)
	{
		return;
	}
	// Characters are inked: a generated body, and a creature's art; a structure is part of the painted world (ADR-068 §1).
	// A veiled body draws none, so its shimmer reads as a ghost rather than a dithered outline (§6).
	UPrimitiveComponent* Inked[] = { Body->Veil > 0.0 ? nullptr : Body->Skin.Get(), Unit.IsA<AVeyraStructure>() ? nullptr : Body->Art.Get() };
	const int32 Hover = HoverStencilOf(Unit);
	for (UPrimitiveComponent* Shape : { static_cast<UPrimitiveComponent*>(Body->Mesh.Get()), static_cast<UPrimitiveComponent*>(Body->Art.Get()),
			 static_cast<UPrimitiveComponent*>(Body->Skin.Get()) })
	{
		if (!Shape)
		{
			continue;
		}
		const bool bInked = InkStencil > 0 && (Shape == Inked[0] || Shape == Inked[1]);
		const bool bWrites = bHovered || bInked;
		if (Shape->bRenderCustomDepth != bWrites)
		{
			Shape->SetRenderCustomDepth(bWrites);
		}
		const int32 Stencil = bHovered ? Hover : InkStencil;
		if (bWrites && Shape->CustomDepthStencilValue != Stencil)
		{
			Shape->SetCustomDepthStencilValue(Stencil);
		}
	}
}

void UVeyraGreyboxSubsystem::RefreshInkPass()
{
	const AVeyraPlayerController* Local = Cast<AVeyraPlayerController>(GetWorld()->GetFirstPlayerController());
	const AVeyraCameraRig* Rig = Local ? Local->GetCameraRig() : nullptr;
	UCameraComponent* Camera = Rig ? Rig->GetCamera() : nullptr;
	if (Camera && ToonInkMaterial && InkCamera.Get() != Camera)
	{
		Camera->PostProcessSettings.AddBlendable(ToonInkMaterial, 1.0f);
		InkCamera = Camera;
	}
}

void UVeyraGreyboxSubsystem::RefreshCameraShake()
{
	const AVeyraPlayerController* Local = Cast<AVeyraPlayerController>(GetWorld()->GetFirstPlayerController());
	const AVeyraCameraRig* Rig = Local ? Local->GetCameraRig() : nullptr;
	UCameraComponent* Camera = Rig ? Rig->GetCamera() : nullptr;
	if (!Camera)
	{
		return;
	}
	// The rig places only its arm: the camera's own offset from the arm's end is the kick's alone.
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FVector Offset = VeyraHitFeel::OffsetAt(ShakeAmplitude, GetWorld()->GetRealTimeSeconds() - ShakeStartedAt, Settings.HitShakeSeconds,
		Settings.HitShakeFrequency);
	if (!Offset.Equals(Camera->GetRelativeLocation()))
	{
		Camera->SetRelativeLocation(Offset);
	}
}

void UVeyraGreyboxSubsystem::RefreshToonLight()
{
	if (!ToonLight)
	{
		return;
	}
	// The map's brightest sun, sought until one streams in.
	if (!Sun.IsValid())
	{
		Sun = UVeyraToonLight::BrightestSun(*GetWorld());
	}
	const UDirectionalLightComponent* Light = Sun.Get();
	if (!Light)
	{
		return;
	}
	const FVeyraToonSun Lit = UVeyraToonLight::Of(*Light);
	if ((!Lit.ToSun.Equals(ToonSunShown) || !Lit.Color.Equals(ToonColorShown)) && UVeyraToonLight::Apply(*GetWorld(), Lit))
	{
		ToonSunShown = Lit.ToSun;
		ToonColorShown = Lit.Color;
	}
}

void UVeyraGreyboxSubsystem::RefreshHoverPass()
{
	const AVeyraPlayerController* Local = Cast<AVeyraPlayerController>(GetWorld()->GetFirstPlayerController());
	const AVeyraCameraRig* Rig = Local ? Local->GetCameraRig() : nullptr;
	UCameraComponent* Camera = Rig ? Rig->GetCamera() : nullptr;
	if (!Camera || !HoverOutlineMaterial)
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	if (!HoverOutline)
	{
		HoverOutline = UMaterialInstanceDynamic::Create(HoverOutlineMaterial, this);
	}
	if (HoverCamera.Get() != Camera)
	{
		Camera->PostProcessSettings.AddBlendable(HoverOutline, 0.0f);
		HoverCamera = Camera;
	}
	// The player's own side colours, as their colour vision gives them (Settings Bible §4.1).
	const FVeyraSideColors& Colors = GetSideColors();
	HoverOutline->SetVectorParameterValue(Settings.HoverEnemyColorParameter, Colors.Enemy);
	HoverOutline->SetVectorParameterValue(Settings.HoverAllyColorParameter, Colors.Ally);
	HoverOutline->SetVectorParameterValue(Settings.HoverNeutralColorParameter, Colors.Neutral);
	// The pass costs nothing while it weighs nothing: only while something is hovered.
	for (FWeightedBlendable& Blendable : Camera->PostProcessSettings.WeightedBlendables.Array)
	{
		if (Blendable.Object == HoverOutline)
		{
			Blendable.Weight = Hovered.IsValid() ? 1.0f : 0.0f;
		}
	}
}

void UVeyraGreyboxSubsystem::ApplyBodyPose(const APawn& Unit, FBody& Body, bool bReduceFlashing)
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	if (VeyraTargeting::IsAlive(&Unit))
	{
		VeyraBodyFeedback::NoteAlive(Body.Feedback);
	}
	const FVeyraBodyPose Pose = VeyraBodyFeedback::PoseAt(Body.Feedback, GetWorld()->GetRealTimeSeconds(), bReduceFlashing, Settings);
	UStaticMeshComponent* Shape = Body.Mesh.Get();
	UStaticMeshComponent* Art = Body.Art.IsValid() && Body.Art->IsVisible() ? Body.Art.Get() : nullptr;
	USkeletalMeshComponent* Skin = Body.Skin.IsValid() && Body.Skin->IsVisible() ? Body.Skin.Get() : nullptr;
	const USceneComponent* Anchor = VeyraDrawnBody::AnchorOf(Unit);
	// A structure stands still, and an animated body's animation acts out its fight (ADR-064 §3): they only flash.
	if (Anchor && !Unit.IsA<AVeyraStructure>() && !Skin)
	{
		// The pose moves on the ground, whichever way the drawn body faces.
		const FVector Local = Anchor->GetComponentTransform().InverseTransformVectorNoScale(Pose.Offset);
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
		if (Shape)
		{
			// Squashed from the ground up: its foot stays where it stands.
			FVector Scale = Shape->GetRelativeScale3D();
			Scale.Z *= Pose.HeightShare;
			Shape->SetRelativeScale3D(Scale);
			Shape->SetRelativeLocation(Shape->GetRelativeLocation() + Local - FVector(0.0, 0.0, HalfHeight * (1.0 - Pose.HeightShare)));
		}
		if (Art)
		{
			// Art that has its own fallen form keeps it whole.
			const double Height = Body.Feedback.DiedAt ? 1.0 : Pose.HeightShare;
			Art->SetRelativeScale3D(FVector(1.0, 1.0, Height));
			Art->SetRelativeLocation(Art->GetRelativeLocation() + Local);
		}
	}
	// The flash lies over whatever shows, and is taken off once it has faded.
	UMeshComponent* Shown = Skin ? static_cast<UMeshComponent*>(Skin) : Art ? static_cast<UMeshComponent*>(Art) : Shape;
	if (UMeshComponent* Was = Body.Flashing.Get(); Was && (Was != Shown || Pose.Flash <= 0.0))
	{
		Was->SetOverlayMaterial(nullptr);
		Body.Flashing.Reset();
	}
	if (!Shown || Pose.Flash <= 0.0 || !HitFlashMaterial)
	{
		return;
	}
	if (!Body.Flash.IsValid())
	{
		Body.Flash = UMaterialInstanceDynamic::Create(HitFlashMaterial, this);
		Body.Flash->SetVectorParameterValue(Settings.HitFlashColorParameter, Settings.HitFlashColor);
	}
	Body.Flash->SetScalarParameterValue(Settings.HitFlashStrengthParameter, static_cast<float>(Pose.Flash));
	if (Body.Flashing.Get() != Shown)
	{
		Shown->SetOverlayMaterial(Body.Flash.Get());
		Body.Flashing = Shown;
	}
}

void UVeyraGreyboxSubsystem::Deinitialize()
{
	if (UVeyraCombatCueSubsystem* Cues = GetWorld() ? GetWorld()->GetSubsystem<UVeyraCombatCueSubsystem>() : nullptr)
	{
		Cues->OnCue.Remove(CueHandle);
	}
	CueHandle.Reset();
	if (AHUD* Hud = OverlayHud.Get(); Hud && HudOverlay.IsValid())
	{
		Hud->RemovePostRenderedActor(HudOverlay.Get());
	}
	if (TelegraphLines && TelegraphLines->IsRegistered())
	{
		TelegraphLines->UnregisterComponent();
	}
	TelegraphLines = nullptr;
	// Registered with the world, not owned by an actor: the world's cleanup expects it gone.
	if (FogOfWarSheet && FogOfWarSheet->IsRegistered())
	{
		FogOfWarSheet->UnregisterComponent();
	}
	FogOfWarSheet = nullptr;
	FogOfWarDrawn = 0;
	// Bodies and projectile spheres belong to their actors, which the world destroys with them.
	Bodies.Reset();
	DenseFog.Reset();
	MapFog.Reset();
	// The fills belong to their owner, which the world destroys with it.
	TelegraphFills.Reset();
	TelegraphFillOwner.Reset();
	Projectiles.Reset();
	Telegraphs.Reset();
	if (AVeyraPlayerController* Source = CombatTextSource.Get())
	{
		Source->OnCombatText.Remove(CombatTextHandle);
	}
	CombatTextSource.Reset();
	CombatText.Reset();
	if (AVeyraPlayerController* Source = KillFeedSource.Get())
	{
		Source->OnKillFeed.Remove(KillFeedHandle);
	}
	KillFeedSource.Reset();
	KillFeed.Reset();
	bReady = false;
	Super::Deinitialize();
}

void UVeyraGreyboxSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Refresh();
}

TStatId UVeyraGreyboxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraGreyboxSubsystem, STATGROUP_Tickables);
}

void UVeyraGreyboxSubsystem::Refresh()
{
	if (!bReady)
	{
		return;
	}
	RefreshBattleground();
	RefreshBodies();
	RefreshProjectiles();
	RefreshTelegraphs();
	DrawTelegraphs();
	DrawVisionMarks();
	DrawChains();
	DrawEchoTethers();
	DrawOrderMark();
	DrawSwingArcs();
	const AVeyraPlayerController* Local = Cast<AVeyraPlayerController>(GetWorld()->GetFirstPlayerController());
	ShowHover(Local ? Local->GetHoveredUnit() : nullptr);
	RefreshShops();
	if (AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		FHitResult Under;
		Player->GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex*/ false, Under);
		// The player's own shop takes the hand cursor over the controller's choice, which this frame has already made.
		if (HoverShop(Cast<AVeyraFountainShop>(Under.GetActor()), GetViewerTeam(), Player->WasInputKeyJustPressed(Player->GetKeys().SelectKey)))
		{
			Player->CurrentMouseCursor = EMouseCursor::Hand;
		}
	}
	RefreshHoverPass();
	RefreshInkPass();
	RefreshToonLight();
	RefreshCameraShake();
	RefreshSound();
	RefreshCombatText();
	RefreshFogOfWar();
	RefreshWarnings();
	AttachHudOverlay();
}

// The engine's running average frame rate, which it declares in no public header.
extern ENGINE_API float GAverageFPS;

void UVeyraGreyboxSubsystem::RefreshWarnings()
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FVeyraSettingsStore* Store = VeyraInterfacePreferences::StoreOf(this);
	const FVeyraInterfacePreferences Preferences = VeyraInterfacePreferences::Resolve(Settings, Store);
	const double Now = FPlatformTime::Seconds();
	// The connection: a client's link to its match server, losing packets or lagging (SET-21).
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	const UNetConnection* Connection = Viewer && GetWorld()->GetNetMode() == NM_Client ? Viewer->GetNetConnection() : nullptr;
	bool bConnectionTrouble = false;
	if (Connection && Preferences.bConnectionWarning)
	{
		// Despite its name, the engine's average loss is a fraction from 0 to 1, lost packets over sent (NetAnalyticsTypes.h),
		// as the threshold is.
		const double Loss = FMath::Max(Connection->GetInLossPercentage().GetAvgLossPercentage(), Connection->GetOutLossPercentage().GetAvgLossPercentage());
		bConnectionTrouble = VeyraHudWarnings::IsConnectionTroubled(Loss, Connection->AvgLag * 1000.0, Settings.ConnectionWarningLossFraction, Settings.ConnectionWarningRoundTripMs);
	}
	VeyraHudWarnings::Update(ConnectionWarning, bConnectionTrouble, Now, Settings.WarningStartSeconds, Settings.WarningClearSeconds);
	// The frame rate, against the cap the player chose; in the background its own cap rules, so never then (Proposal 110).
	const bool bForeground = FPlatformApplicationMisc::IsThisApplicationForeground();
	const double Cap = Store ? VeyraDisplayRules::Resolve(*Store, /*bForeground*/ true).FrameCap : 0.0;
	const bool bPerformanceTrouble = Preferences.bPerformanceWarning
		&& VeyraHudWarnings::IsPerformanceTroubled(bForeground, GAverageFPS, Cap, Settings.UncappedReferenceFps, Settings.PerformanceWarningFraction);
	VeyraHudWarnings::Update(PerformanceWarning, bPerformanceTrouble, Now, Settings.WarningStartSeconds, Settings.WarningClearSeconds);
}

void UVeyraGreyboxSubsystem::RefreshFogOfWar()
{
	const FVeyraSeenGround* Ground = VeyraFogOfWar::OwnGround(*GetWorld(), GetViewerTeam());
	const uint32 Signature = VeyraFogOfWar::SignatureOf(Ground);
	if (Signature == FogOfWarDrawn && FogOfWarSheet)
	{
		return;
	}
	FogOfWarDrawn = Signature;
	if (!FogOfWarSheet)
	{
		FogOfWarSheet = NewObject<ULineBatchComponent>(this, NAME_None, RF_Transient);
		FogOfWarSheet->bCalculateAccurateBounds = false;
		FogOfWarSheet->RegisterComponentWithWorld(GetWorld());
	}
	FogOfWarSheet->Flush();
	if (!Ground)
	{
		return;
	}
	// One translucent sheet for every unseen run, above the ground's markings and under the telegraphs. Each quad is
	// wound both ways, so it shows from any camera.
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	TArray<FVector> Vertices;
	TArray<int32> Indices;
	for (const FVeyraUnseenRun& Run : VeyraFogOfWar::UnseenRuns(*Ground))
	{
		const FBox2D Box = VeyraFogOfWar::BoundsOf(*Ground, Run);
		const int32 Subdivisions = bAuthoredTerrain ? FMath::Max(1, FMath::CeilToInt(Ground->CellSize / Settings.FogOfWarSurfaceStep)) : 1;
		const int32 Columns = (Run.Last - Run.First + 1) * Subdivisions;
		const double Step = Ground->CellSize / Subdivisions;
		auto Vertex = [&](double X, double Y) {
			const FVector2D Point(X, Y);
			double* Cached = FogSurfaceHeights.Find(Point);
			if (!Cached)
			{
				Cached = &FogSurfaceHeights.Add(Point, GroundUnder(FVector(Point, 0.0)).Z - Settings.TelegraphLift);
			}
			return FVector(Point, *Cached + Settings.FogOfWarLift);
		};
		// The run's quads share their corners: a lattice of (Columns + 1) by (Subdivisions + 1) points. The line batcher
		// rebuilds its meshes every frame, so every vertex saved is saved on each one.
		const int32 Base = Vertices.Num();
		const int32 Across = Columns + 1;
		for (int32 Y = 0; Y <= Subdivisions; ++Y)
		{
			for (int32 X = 0; X <= Columns; ++X)
			{
				Vertices.Add(Vertex(Box.Min.X + X * Step, Box.Min.Y + Y * Step));
			}
		}
		for (int32 Y = 0; Y < Subdivisions; ++Y)
		{
			for (int32 X = 0; X < Columns; ++X)
			{
				const int32 Low = Base + Y * Across + X;
				const int32 High = Low + Across;
				Indices.Append({ Low, Low + 1, High + 1, Low, High + 1, High, Low, High + 1, Low + 1, Low, High, High + 1 });
			}
		}
	}
	if (!Vertices.IsEmpty())
	{
		// A lifetime of 0 keeps it until the next change flushes it.
		FogOfWarSheet->DrawMesh(Vertices, Indices, Settings.FogOfWarColor.ToFColor(/*bSRGB*/ true), SDPG_World, 0.0f);
	}
}

void UVeyraGreyboxSubsystem::RefreshCombatText()
{
	AVeyraPlayerController* Local = Cast<AVeyraPlayerController>(GetWorld()->GetFirstPlayerController());
	if (Local != CombatTextSource.Get())
	{
		if (AVeyraPlayerController* Previous = CombatTextSource.Get())
		{
			Previous->OnCombatText.Remove(CombatTextHandle);
		}
		CombatTextSource = Local;
		CombatTextHandle = Local ? Local->OnCombatText.AddUObject(this, &UVeyraGreyboxSubsystem::OnCombatText) : FDelegateHandle();
		if (AVeyraPlayerController* Previous = KillFeedSource.Get())
		{
			Previous->OnKillFeed.Remove(KillFeedHandle);
		}
		KillFeedSource = Local;
		KillFeedHandle = Local ? Local->OnKillFeed.AddUObject(this, &UVeyraGreyboxSubsystem::OnKillFeed) : FDelegateHandle();
	}
	// Forgotten as they are drawn: a running total keeps all of its parts while it shows.
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FVeyraInterfacePreferences Preferences = VeyraInterfacePreferences::Resolve(Settings, VeyraInterfacePreferences::StoreOf(GetWorld()));
	VeyraCombatTextView::Forget(CombatText, FPlatformTime::Seconds(), Preferences.CombatText);
	VeyraKillFeedView::Forget(KillFeed, FPlatformTime::Seconds(), FMath::Max(Settings.KillFeedSeconds, Settings.AnnouncementSeconds));
}

void UVeyraGreyboxSubsystem::OnKillFeed(const FVeyraKillFeedLine& Line)
{
	KillFeed.Add(FVeyraKillFeedArrival{ Line, FPlatformTime::Seconds() });
}

void UVeyraGreyboxSubsystem::OnCombatText(const FVeyraCombatTextLine& Line)
{
	CombatText.Add(FVeyraCombatTextArrival{ Line, FPlatformTime::Seconds() });
}

void UVeyraGreyboxSubsystem::AttachHudOverlay()
{
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	AHUD* Hud = Viewer ? Viewer->GetHUD() : nullptr;
	if (!Hud || (Hud == OverlayHud.Get() && HudOverlay.IsValid()))
	{
		return;
	}
	if (!HudOverlay.IsValid())
	{
		FActorSpawnParameters Parameters;
		Parameters.ObjectFlags |= RF_Transient;
		AVeyraHudOverlay* Overlay = GetWorld()->SpawnActor<AVeyraHudOverlay>(Parameters);
		if (!Overlay)
		{
			return;
		}
		Overlay->SetGreybox(*this);
		HudOverlay = Overlay;
	}
	// The HUD renders its overlay actors on its own canvas, which the menus cover.
	Hud->bShowOverlays = true;
	Hud->AddPostRenderedActor(HudOverlay.Get());
	OverlayHud = Hud;
}

UStaticMeshComponent* UVeyraGreyboxSubsystem::FindBody(const AActor& Unit) const
{
	const FBody* Body = Bodies.Find(&Unit);
	return Body ? Body->Mesh.Get() : nullptr;
}

UStaticMeshComponent* UVeyraGreyboxSubsystem::FindArt(const AActor& Unit) const
{
	const FBody* Body = Bodies.Find(&Unit);
	return Body ? Body->Art.Get() : nullptr;
}

USkeletalMeshComponent* UVeyraGreyboxSubsystem::FindSkin(const AActor& Unit) const
{
	const FBody* Body = Bodies.Find(&Unit);
	return Body ? Body->Skin.Get() : nullptr;
}

TArray<UNiagaraComponent*> UVeyraGreyboxSubsystem::FindBodyEffects(const AActor& Unit) const
{
	TArray<UNiagaraComponent*> Effects;
	if (const FBody* Body = Bodies.Find(&Unit))
	{
		for (const TWeakObjectPtr<UNiagaraComponent>& Effect : Body->BodyEffects)
		{
			if (UNiagaraComponent* Live = Effect.Get())
			{
				Effects.Add(Live);
			}
		}
	}
	return Effects;
}

void UVeyraGreyboxSubsystem::RefreshBodyEffects(FBody& Body, USkeletalMeshComponent& Skin, const FVeyraVanguardBody& Worn)
{
	if (Body.BodyEffectsOf.Get() == Worn.Mesh && Body.BodyEffects.Num() == Worn.EffectBones.Num())
	{
		return;
	}
	// The body it wears changed (its own, or a status's): what it poured gives way to what this one pours.
	for (const TWeakObjectPtr<UNiagaraComponent>& Effect : Body.BodyEffects)
	{
		if (UNiagaraComponent* Live = Effect.Get())
		{
			Live->DestroyComponent();
		}
	}
	Body.BodyEffects.Reset();
	Body.BodyEffectsOf = Worn.Mesh;
	if (!Worn.Effect)
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	for (const FName& Bone : Worn.EffectBones)
	{
		// Attached at its bone and simulated in the world, so it pours off the animated skeleton and trails behind it.
		// Made directly rather than through the spawning helper, which makes nothing where nothing renders.
		UNiagaraComponent* Effect = NewObject<UNiagaraComponent>(Skin.GetOwner(), NAME_None, RF_Transient);
		Effect->SetAsset(Worn.Effect);
		Effect->SetCanEverAffectNavigation(false);
		Effect->SetupAttachment(&Skin, Bone);
		// Its frame is its body's (forward and up as the body stands at rest), turned only as the bone turns from there:
		// a bone's own axes lie as its rig drew it, so an effect's local offsets would otherwise point wherever they do.
		if (const USkinnedAsset* Asset = Skin.GetSkinnedAsset(); Asset && Skin.GetBoneIndex(Bone) != INDEX_NONE)
		{
			const FTransform Rest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Asset->GetRefSkeleton(), Skin.GetBoneIndex(Bone));
			Effect->SetRelativeRotation(Rest.GetRotation().Inverse());
		}
		Effect->RegisterComponent();
		Effect->SetVariableLinearColor(Settings.EffectColorParameter, Worn.EffectColor);
		Effect->SetVariableFloat(Settings.EffectScaleParameter, Worn.EffectScale);
		Effect->Activate(/*bReset*/ true);
		Body.BodyEffects.Add(Effect);
	}
	// New effects start as their body made them: a veil the body wears is laid on them afresh.
	Body.VeilShown = -1.0;
}

UStaticMeshComponent* UVeyraGreyboxSubsystem::FindProjectileVisual(const AVeyraProjectile& Projectile) const
{
	const FProjectileVisual* Visual = Projectiles.Find(&Projectile);
	return Visual ? Visual->Mesh.Get() : nullptr;
}

double UVeyraGreyboxSubsystem::GetServerNow() const
{
	const UWorld* World = GetWorld();
	if (const AVeyraGameState* GameState = World->GetGameState<AVeyraGameState>())
	{
		return GameState->GetGameplayServerTime();
	}
	// A world without a match, such as a test's, is its own server.
	return World->GetTimeSeconds();
}

EVeyraTeam UVeyraGreyboxSubsystem::GetViewerTeam() const
{
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	return Viewer ? VeyraTeams::TeamOf(Viewer->PlayerState) : EVeyraTeam::None;
}

const FVeyraSideColors& UVeyraGreyboxSubsystem::GetSideColors() const
{
	if (SideColorsFrame != GFrameCounter)
	{
		SideColors = VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), VeyraInterfacePreferences::StoreOf(this)).SideColors;
		SideColorsFrame = GFrameCounter;
	}
	return SideColors;
}

FLinearColor UVeyraGreyboxSubsystem::ColorOfSide(EVeyraTeam Team) const
{
	const FVeyraSideColors& Colors = GetSideColors();
	if (Team == EVeyraTeam::None)
	{
		return Colors.Neutral;
	}
	const EVeyraTeam ViewerTeam = GetViewerTeam();
	const EVeyraTeam Allies = ViewerTeam == EVeyraTeam::None ? EVeyraTeam::A : ViewerTeam;
	return Team == Allies ? Colors.Ally : Colors.Enemy;
}

FLinearColor UVeyraGreyboxSubsystem::SideColorOf(const AActor& Unit) const
{
	// The viewer's Vanguard carries the viewer's PlayerState (ADR-006 §7), and the viewer's companion
	// names it as its owner's (ADR-034 §3), as the viewer's Echo names its holder's (ADR-050 §7).
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = Cast<APawn>(&Unit);
	const AVeyraCompanion* Companion = Cast<AVeyraCompanion>(&Unit);
	const AVeyraEcho* Echo = Cast<AVeyraEcho>(&Unit);
	const APlayerState* Whose = Companion ? Companion->GetOwnerState() : Echo ? Echo->GetHolderState() : (Pawn ? Pawn->GetPlayerState() : nullptr);
	if (Viewer && Viewer->PlayerState && Whose == Viewer->PlayerState)
	{
		return GetSideColors().Own;
	}
	return ColorOfSide(VeyraTeams::TeamOf(&Unit));
}

FLinearColor UVeyraGreyboxSubsystem::BodyColorOf(const AActor& Unit) const
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FLinearColor Side = SideColorOf(Unit);
	const TArray<FVeyraHudStatus> Statuses = VeyraHud::StatusesOf(Unit, GetServerNow(), GetViewerTeam());
	const auto Has = [&Statuses](EVeyraStatusKind Kind) {
		return Statuses.ContainsByPredicate([Kind](const FVeyraHudStatus& Status) { return Status.Kind == Kind; });
	};
	// A stun matters more than a slow, and either more than a Camouflage; a Fear or a Knockup reads as a stun.
	if (Has(EVeyraStatusKind::Stun) || Has(EVeyraStatusKind::Fear) || Has(EVeyraStatusKind::Knockup))
	{
		return FLinearColor::LerpUsingHSV(Side, Settings.StunColor, Settings.StatusTintStrength);
	}
	if (Has(EVeyraStatusKind::Slow))
	{
		return FLinearColor::LerpUsingHSV(Side, Settings.SlowColor, Settings.StatusTintStrength);
	}
	if (Has(EVeyraStatusKind::Camouflage) || Has(EVeyraStatusKind::Invisible))
	{
		return FLinearColor::LerpUsingHSV(Side, Settings.CamouflageColor, Settings.StatusTintStrength);
	}
	return Side;
}

void UVeyraGreyboxSubsystem::EaseBody(APawn& Unit)
{
	// A machine that only shows the unit steps its capsule to each update from the server and eases its mesh, and so its
	// drawn body, after it (ADR-065 §12). The server's own copy never eases.
	const TOptional<FVector2f> Ease = GetDefault<UVeyraGreyboxSettings>()->EaseOf(Unit);
	const ACharacter* Character = Cast<ACharacter>(&Unit);
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Ease || !Movement)
	{
		return;
	}
	Movement->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
	Movement->NetworkSimulatedSmoothLocationTime = Ease->X;
	Movement->NetworkSimulatedSmoothRotationTime = Ease->Y;
}

UStaticMeshComponent* UVeyraGreyboxSubsystem::AddShape(AActor& Owner, UStaticMesh& Mesh, UMaterialInstanceDynamic*& OutMaterial) const
{
	// It hangs from where the body is drawn, which glides after the capsule on machines that only show it (ADR-065 §12).
	USceneComponent* Anchor = VeyraDrawnBody::AnchorOf(Owner);
	if (!Anchor)
	{
		return nullptr;
	}
	// Presentation only: it blocks nothing, overlaps nothing and never shapes navigation.
	UStaticMeshComponent* Shape = NewObject<UStaticMeshComponent>(&Owner, NAME_None, RF_Transient);
	Shape->SetStaticMesh(&Mesh);
	Shape->SetMobility(EComponentMobility::Movable);
	Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shape->SetGenerateOverlapEvents(false);
	Shape->SetCanEverAffectNavigation(false);
	Shape->SetupAttachment(Anchor);
	Shape->RegisterComponent();
	OutMaterial = Shape->CreateDynamicMaterialInstance(0, ShapeMaterial);
	return Shape;
}

void UVeyraGreyboxSubsystem::RefreshStructureArt(const AVeyraStructure& Structure, FBody& Body)
{
	// Standing, or the wreck it leaves (Battleground Bible §5); an inhibitor rebuilt stands again.
	const FVeyraUnitArt* Art = StructureArt ? StructureArt->Find(UVeyraGreyboxSettings::StructureArtId(Structure.GetStructureKind())) : nullptr;
	if (UStaticMesh* Mesh = Art ? (Structure.IsDestroyed() ? Art->Fallen : Art->Intact).Get() : nullptr)
	{
		ShowArt(Structure, Body, *Mesh, *StructureArt, SideColorOf(Structure));
		// Drawn larger than its capsule, from its foot (ADR-065 §11).
		if (UStaticMeshComponent* Shown = Body.Art.Get())
		{
			Shown->SetRelativeScale3D(FVector(GetDefault<UVeyraGreyboxSettings>()->VisualScaleOf(Structure)));
		}
	}
}

void UVeyraGreyboxSubsystem::RefreshFluxbornArt(const AVeyraFluxborn& Unit, FBody& Body)
{
	// Its kind arrives with it; until then, and for a kind without art, its body shows.
	const FVeyraUnitArt* Art = FluxbornArt && Unit.GetKind().IsValid() ? FluxbornArt->Find(FName(Unit.GetKind().ToString())) : nullptr;
	// Active, or collapsed where it fell, for its corpse's moment (Battleground Bible §4).
	if (UStaticMesh* Mesh = Art ? (Unit.IsAlive() ? Art->Intact : Art->Fallen).Get() : nullptr)
	{
		// Its Flux shows what its body would: its side, tinted while it is crowd controlled.
		ShowArt(Unit, Body, *Mesh, *FluxbornArt, BodyColorOf(Unit));
	}
}

void UVeyraGreyboxSubsystem::RefreshVanguardArt(const APawn& Unit, FBody& Body)
{
	// A Vanguard's arrives with its participant, a companion's with its definition; until then, and for one without
	// art, its body shows.
	const FVeyraVanguardArt* Art = nullptr;
	if (!VanguardArt)
	{
		return;
	}
	if (const AVeyraCompanion* Companion = Cast<AVeyraCompanion>(&Unit))
	{
		Art = Companion->GetDefinitionId().IsValid() ? VanguardArt->FindCompanion(FName(Companion->GetDefinitionId().ToString())) : nullptr;
	}
	else if (const AVeyraPlayerState* Participant = Unit.GetPlayerState<AVeyraPlayerState>(); Participant && Participant->GetVanguardId().IsValid())
	{
		Art = VanguardArt->Find(FName(Participant->GetVanguardId().ToString()));
	}
	if (!Art || !Art->Mesh)
	{
		return;
	}
	if (!Body.Skin.IsValid())
	{
		Body.Skin = VeyraVanguardSkin::Attach(const_cast<APawn&>(Unit));
	}
	USkeletalMeshComponent* Skin = Body.Skin.Get();
	if (!Skin)
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	// The body its statuses call for: a rider on its mount while its ride lasts, on foot otherwise (ADR-064 §1); and
	// whatever that body is made of where no mesh shows it, as Nix's smoke.
	const FVeyraVanguardBody& Worn = VeyraVanguardSkin::BodyOf(Unit, *Art);
	VeyraVanguardSkin::Dress(*Skin, Worn, VeyraVanguardSkin::ShapeOf(Settings));
	RefreshBodyEffects(Body, *Skin, Worn);
	// It stands at the capsule's foot, which its Vanguard's definition shapes once it arrives (ADR-008 §2), drawn larger than
	// the capsule from there (ADR-065 §11).
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
	Skin->SetRelativeLocation(FVector(0.0, 0.0, -HalfHeight));
	Skin->SetRelativeScale3D(FVector(Settings.VisualScaleOf(Unit)));
	// The player's own body leads its latest order until the server's movement reaches it (ADR-067 §2).
	const FVeyraBodyLead Lead = OwnLeadOf(Unit, Radius);
	FVeyraVanguardAnimInputs Inputs = VeyraVanguardSkin::InputsOf(Unit, GetViewerTeam(), GetServerNow());
	Inputs.GroundSpeed = static_cast<float>(VeyraBodyLead::GroundSpeedOf(Lead, Inputs.GroundSpeed));
	RefreshBodyLook(Unit, Body, *Skin, Worn, Inputs.bAlive && Inputs.bCastHeld);
	if (UVeyraVanguardAnimInstance* Animation = Cast<UVeyraVanguardAnimInstance>(Skin->GetAnimInstance()))
	{
		Animation->SetInputs(Inputs);
	}
	// Struck, it holds its pose a moment, so the blow lands (ADR-068 §4).
	Skin->GlobalAnimRateScale = GetWorld()->GetRealTimeSeconds() < Body.HitStopUntil ? 0.0f : 1.0f;
	// It turns toward the lead from its mesh's facing at the lead's own rate, and back as the server's facing arrives.
	const double TargetYaw = Lead.bLeads ? FRotator::NormalizeAxis(Lead.Yaw - Skin->GetAttachParent()->GetComponentRotation().Yaw) : 0.0;
	Body.LeadYaw = FMath::FixedTurn(Body.LeadYaw, TargetYaw, Settings.OwnLeadTurnDegreesPerSecond * GetWorld()->GetDeltaSeconds());
	Skin->SetRelativeRotation(FRotator(0.0, Body.LeadYaw, 0.0));
	// Its body lies under its feet as a disc, still showing its side and its status tint.
	if (UStaticMeshComponent* Shape = Body.Mesh.Get())
	{
		const double DiscHalfHeight = Settings.VanguardFootDiscHeight * 0.5;
		FitGreyboxShape(*Shape, FVector(Radius, Radius, DiscHalfHeight));
		Shape->SetRelativeLocation(Shape->GetRelativeLocation() + FVector(0.0, 0.0, DiscHalfHeight - HalfHeight));
	}
}

EVeyraHiddenKind UVeyraGreyboxSubsystem::HiddenKindOf(const AActor& Unit) const
{
	// Its own side sees why it is hidden, and a viewer on no side sees every side's; the other side sees nothing of it.
	const EVeyraTeam Viewer = GetViewerTeam();
	const bool bShows = Viewer == EVeyraTeam::None || VeyraTeams::TeamOf(&Unit) == Viewer;
	if (!bShows)
	{
		return EVeyraHiddenKind::None;
	}
	const TArray<FVeyraHudStatus> Statuses = VeyraHud::StatusesOf(Unit, GetServerNow(), Viewer);
	const auto Has = [&Statuses](EVeyraStatusKind Kind) {
		return Statuses.ContainsByPredicate([Kind](const FVeyraHudStatus& Status) { return Status.Kind == Kind; });
	};
	const bool bInFog = VeyraVisionRules::CircleAt(DenseFog, FVector2D(Unit.GetActorLocation())) != INDEX_NONE;
	return VeyraHiddenBody::KindOf(bShows, Has(EVeyraStatusKind::Invisible), Has(EVeyraStatusKind::Camouflage), bInFog);
}

double UVeyraGreyboxSubsystem::GetVeilOf(const AActor& Unit) const
{
	const FBody* Body = Bodies.Find(&Unit);
	return Body ? Body->Veil : 0.0;
}

void UVeyraGreyboxSubsystem::RefreshBodyLook(const APawn& Unit, FBody& Body, USkeletalMeshComponent& Skin, const FVeyraVanguardBody& Worn, bool bCastHeld)
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const double DeltaSeconds = GetWorld()->GetDeltaSeconds();
	const EVeyraHiddenKind Kind = HiddenKindOf(Unit);
	Body.Veil = VeyraHiddenBody::StepVeil(Body.Veil, Kind != EVeyraHiddenKind::None, DeltaSeconds, Settings.VeilFadeSeconds);
	Body.CastGlow = VeyraCastGlow::Step(Body.CastGlow, bCastHeld, DeltaSeconds, Settings.CastGlowRiseSeconds, Settings.CastGlowFallSeconds);
	// A veil fading away keeps the colour it had.
	if (Kind != EVeyraHiddenKind::None)
	{
		Body.VeilKind = Kind;
	}
	// A body is given a material of its own only once it is first veiled or strains; one never hidden nor casting keeps
	// the shared one.
	const FLinearColor Tint = VeyraHiddenBody::TintOf(Body.VeilKind, Settings);
	const bool bVeilAtRest = (Body.Veil <= 0.0 && Body.VeilShown <= 0.0) || (Body.Veil == Body.VeilShown && Tint.Equals(Body.VeilTintShown));
	if (bVeilAtRest && Body.CastGlow == Body.CastGlowShown)
	{
		return;
	}
	const float Glow = static_cast<float>(VeyraCastGlow::Multiplier(Body.CastGlow, Settings.CastGlowGain));
	for (int32 Slot = 0; Slot < Skin.GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* Own = Skin.CreateDynamicMaterialInstance(Slot))
		{
			Own->SetScalarParameterValue(Settings.BodyVeilParameter, static_cast<float>(Body.Veil));
			Own->SetVectorParameterValue(Settings.BodyVeilTintParameter, Tint);
			Own->SetScalarParameterValue(Settings.BodyCastGlowParameter, Glow);
		}
	}
	// What it pours veils with it, or its smoke would show it plainly; and surges with its glow while it casts.
	const float Veil = static_cast<float>(Body.Veil);
	const float Surge = static_cast<float>(VeyraCastGlow::Multiplier(Body.CastGlow, Settings.CastEffectGain));
	for (const TWeakObjectPtr<UNiagaraComponent>& Effect : Body.BodyEffects)
	{
		if (UNiagaraComponent* Live = Effect.Get())
		{
			Live->SetVariableLinearColor(Settings.EffectColorParameter, FMath::Lerp(Worn.EffectColor, Tint, Veil));
			Live->SetVariableFloat(Settings.EffectScaleParameter, Worn.EffectScale * FMath::Lerp(1.0f, Settings.VeiledEffectScale, Veil) * Surge);
		}
	}
	Body.VeilShown = Body.Veil;
	Body.VeilTintShown = Tint;
	Body.CastGlowShown = Body.CastGlow;
}

void UVeyraGreyboxSubsystem::ShowArt(const APawn& Unit, FBody& Body, UStaticMesh& Mesh, const UVeyraUnitArtSet& Set, const FLinearColor& Color)
{
	USceneComponent* Anchor = VeyraDrawnBody::AnchorOf(Unit);
	if (!Anchor)
	{
		return;
	}
	if (!Body.Art.IsValid())
	{
		// Presentation only, as a body is.
		UStaticMeshComponent* Art = NewObject<UStaticMeshComponent>(const_cast<APawn*>(&Unit), NAME_None, RF_Transient);
		Art->SetMobility(EComponentMobility::Movable);
		Art->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Art->SetGenerateOverlapEvents(false);
		Art->SetCanEverAffectNavigation(false);
		Art->SetupAttachment(Anchor);
		Art->RegisterComponent();
		Body.Art = Art;
		Body.ArtMesh = nullptr;
	}
	UStaticMeshComponent* Art = Body.Art.Get();
	// It stands on the floor, its pivot at the capsule's foot; a Fluxborn's capsule takes its kind's shape once that arrives.
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
	Art->SetRelativeLocation(FVector(0.0, 0.0, -HalfHeight));
	if (Body.ArtMesh.Get() != &Mesh)
	{
		Art->SetStaticMesh(&Mesh);
		const int32 Slot = Art->GetMaterialIndex(Set.FluxSlot);
		Body.ArtFlux = Slot != INDEX_NONE ? Art->CreateDynamicMaterialInstance(Slot) : nullptr;
		Body.ArtMesh = &Mesh;
		Body.ArtShown = FLinearColor::Transparent;
	}
	if (UStaticMeshComponent* Shape = Body.Mesh.Get())
	{
		Shape->SetVisibility(false);
	}
	if (UMaterialInstanceDynamic* Flux = Body.ArtFlux.Get(); Flux && !Color.Equals(Body.ArtShown))
	{
		Flux->SetVectorParameterValue(Set.FluxParameter, Color);
		Body.ArtShown = Color;
	}
}

void UVeyraGreyboxSubsystem::RefreshBodies()
{
	const FName ColorParameter = GetDefault<UVeyraGreyboxSettings>()->ColorParameter;
	const bool bReduceFlashing = VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), VeyraInterfacePreferences::StoreOf(this)).bReduceFlashing;
	// The Dense Fog bodies may stand in: the map's, and what abilities have laid and not yet lifted (ADR-036 §1).
	DenseFog = MapFog;
	for (TActorIterator<AVeyraDenseFogBank> It(GetWorld()); It; ++It)
	{
		DenseFog.Append(It->GetCircles());
	}
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn& Unit = **It;
		if (!VeyraUnits::KindOf(&Unit).IsSet())
		{
			continue;
		}
		// A wall's marker shows nothing of its own: the terrain it holds is drawn as terrain (ADR-032 §4).
		const AVeyraPlacedMarker* Marker = Cast<AVeyraPlacedMarker>(&Unit);
		if (Marker && Marker->IsWall())
		{
			continue;
		}
		FBody* Body = Bodies.Find(&Unit);
		if (!Body || !Body->Mesh.IsValid())
		{
			UMaterialInstanceDynamic* Material = nullptr;
			UStaticMeshComponent* Mesh = AddShape(Unit, *BodyMesh, Material);
			if (!Mesh)
			{
				continue;
			}
			Body = &Bodies.Add(&Unit, FBody{ Mesh, Material });
			EaseBody(Unit);
		}
		// Its capsule, which its Vanguard's definition shapes (ADR-008 §2).
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
		FitGreyboxShape(*Body->Mesh, FVector(Radius, Radius, HalfHeight));
		// How fast its Health, not its shields, is falling, for the last-hit cue's Ready stage (ADR-071 §5).
		if (const UAbilitySystemComponent* Abilities = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit))
		{
			Body->Loss.Sample(GetWorld()->GetRealTimeSeconds(), Abilities->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()),
				GetDefault<UVeyraGreyboxSettings>()->LastHitLossWindowSeconds);
		}

		const FLinearColor Color = BodyColorOf(Unit);
		if (UMaterialInstanceDynamic* Material = Body->Material.Get(); Material && !Color.Equals(Body->Shown))
		{
			Material->SetVectorParameterValue(ColorParameter, Color);
			Body->Shown = Color;
		}
		if (const AVeyraStructure* Structure = Cast<AVeyraStructure>(&Unit))
		{
			RefreshStructureArt(*Structure, *Body);
		}
		else if (const AVeyraFluxborn* Fluxborn = Cast<AVeyraFluxborn>(&Unit))
		{
			RefreshFluxbornArt(*Fluxborn, *Body);
		}
		else if (Unit.IsA<AVeyraVanguardCharacter>() || Unit.IsA<AVeyraCompanion>())
		{
			RefreshVanguardArt(Unit, *Body);
		}
		// Its art may have just arrived or changed: a character's is inked (ADR-068 §3), and the hovered keeps its outline.
		SetStencils(Unit, &Unit == Hovered.Get());
		ApplyBodyPose(Unit, *Body, bReduceFlashing);
	}
	// Runtime terrain stands as a block across the way it faces, in the neutral colour (ADR-032 §4).
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	for (TActorIterator<AVeyraTerrainWall> It(GetWorld()); It; ++It)
	{
		AVeyraTerrainWall& Wall = **It;
		if ((bAuthoredTerrain && Wall.IsMapTerrain()) || Wall.GetHalfExtent().GetMin() <= 0.0 || (Bodies.Contains(&Wall) && Bodies.FindChecked(&Wall).Mesh.IsValid()))
		{
			continue;
		}
		UMaterialInstanceDynamic* Material = nullptr;
		UStaticMeshComponent* Mesh = AddShape(Wall, *GroundMesh, Material);
		if (!Mesh)
		{
			continue;
		}
		FitGreyboxShape(*Mesh, Wall.GetHalfExtent());
		if (Material)
		{
			Material->SetVectorParameterValue(ColorParameter, Settings.NeutralColor);
		}
		Bodies.Add(&Wall, FBody{ Mesh, Material, Settings.NeutralColor });
	}
	// Fog an ability laid lies on the ground as the map's does, once its circles have arrived (ADR-036 §1).
	for (TActorIterator<AVeyraDenseFogBank> It(GetWorld()); It; ++It)
	{
		AVeyraDenseFogBank& Bank = **It;
		const TArray<FVeyraFogCircle> Circles = Bank.GetCircles();
		if (Circles.IsEmpty() || DrawnFogBanks.Contains(&Bank))
		{
			continue;
		}
		for (const FVeyraFogCircle& Circle : Circles)
		{
			AddGroundMarking(Bank, *PadMesh, Settings.DenseFogColor, Circle.Center, 0.0, FVector2D(Circle.Radius), FogMarkingLayer);
		}
		DrawnFogBanks.Add(&Bank);
	}
	for (auto It = DrawnFogBanks.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = Bodies.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void UVeyraGreyboxSubsystem::RefreshBattleground()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	if (GroundMarkings.IsValid())
	{
		// The viewer's side may arrive after the ground is drawn: each base takes the colour it has for them.
		const EVeyraTeam Viewer = GetViewerTeam();
		if (Viewer != PadsDrawnFor)
		{
			for (const TPair<EVeyraTeam, TWeakObjectPtr<UMaterialInstanceDynamic>>& Pad : PadMaterials)
			{
				if (UMaterialInstanceDynamic* Material = Pad.Value.Get())
				{
					Material->SetVectorParameterValue(Settings.ColorParameter, BaseColorOf(Pad.Key));
				}
			}
			PadsDrawnFor = Viewer;
		}
		return;
	}
	// The battleground is the map whose structures replicate; others, such as the grey box, have none.
	TActorIterator<AVeyraStructure> Structures(World);
	if (!Structures)
	{
		return;
	}
	FActorSpawnParameters Parameters;
	Parameters.ObjectFlags |= RF_Transient;
	AActor* Owner = World->SpawnActor<AActor>(Parameters);
	if (!Owner)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Owner, NAME_None, RF_Transient);
	Owner->SetRootComponent(Root);
	Root->RegisterComponent();
	GroundMarkings = Owner;

	const FVeyraBattlegroundLayout& Layout = UVeyraWorldTuningSubsystem::Get().Layout;
	if (!bAuthoredTerrain)
	{
		// Flat fixtures still show the authored river; production terrain supplies its own water.
		for (const FVeyraRiverChannel& Channel : VeyraRiver::ShapeOf(Layout).GetChannels())
		{
			const TArray<FVeyraRiverSample>& Samples = Channel.Samples;
			for (int32 I = 1; I < Samples.Num(); ++I)
			{
				const auto& A = Samples[I - 1];
				const auto& B = Samples[I];
				const FVector2D Along = B.Point - A.Point;
				AddGroundMarking(*Owner, *GroundMesh, Settings.RiverColor, (A.Point + B.Point) / 2.0,
					FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), FVector2D(Along.Size() / 2.0, (A.Width + B.Width) / 4.0), 1);
			}
		}
		// Each stretch of road reaches a half width past its ends, so the bends join.
		for (const FVeyraLaneLayout& Lane : Layout.Lanes)
		{
			for (int32 Index = 0; Index + 1 < Lane.Points.Num(); ++Index)
			{
				const FVector2D From = VeyraLayout::ToVector(Lane.Points[Index]);
				const FVector2D To = VeyraLayout::ToVector(Lane.Points[Index + 1]);
				const FVector2D Along = To - From;
				const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X));
				AddGroundMarking(*Owner, *GroundMesh, Settings.LaneColor, (From + To) / 2.0, Yaw, FVector2D((Along.Size() + Lane.Width) / 2.0, Lane.Width / 2.0), 2);
			}
		}
		const FVector2D PrimeWell = VeyraLayout::ToVector(Layout.Base.PrimeWell);
		PadMaterials.Reset();
		PadsDrawnFor = GetViewerTeam();
		for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
		{
			UMaterialInstanceDynamic* Material =
				AddGroundMarking(*Owner, *PadMesh, BaseColorOf(Team), VeyraLayout::ForTeam(PrimeWell, Team), 0.0, FVector2D(Layout.Base.PadRadius), 3);
			PadMaterials.Add(Team, Material);
		}
	}
	// Dense Fog is marked on top: a player sees where it lies, not who is in it.
	for (const FVeyraFogPlacement& Fog : VeyraLayout::DenseFog(Layout))
	{
		AddGroundMarking(*Owner, *PadMesh, Settings.DenseFogColor, Fog.Center, 0.0, FVector2D(Fog.Radius), FogMarkingLayer);
		// And a body in it on the viewer's side shows that it is hidden (ADR-068 §6).
		MapFog.Add(FVeyraFogCircle{ Fog.Center, Fog.Radius });
	}
}

FLinearColor UVeyraGreyboxSubsystem::BaseColorOf(EVeyraTeam Team) const
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const EVeyraTeam ViewerTeam = GetViewerTeam();
	return Team == (ViewerTeam == EVeyraTeam::None ? EVeyraTeam::A : ViewerTeam) ? Settings.AllyBaseColor : Settings.EnemyBaseColor;
}

UMaterialInstanceDynamic* UVeyraGreyboxSubsystem::AddGroundMarking(AActor& Owner, UStaticMesh& Mesh, const FLinearColor& Color, const FVector2D& Centre,
	double Yaw, const FVector2D& HalfSize, int32 Layer) const
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	UMaterialInstanceDynamic* Material = nullptr;
	UStaticMeshComponent* Shape = AddShape(Owner, Mesh, Material);
	if (!Shape)
	{
		return nullptr;
	}
	if (Material)
	{
		Material->SetVectorParameterValue(Settings.ColorParameter, Color);
	}
	const double HalfThickness = Settings.GroundMarkingThickness / 2.0;
	FitGreyboxShape(*Shape, FVector(HalfSize, HalfThickness));
	// It lies on the floor, each layer a lift above the one below, all under the telegraphs.
	const double Floor = GroundUnder(FVector(Centre, 0.0)).Z - Settings.TelegraphLift;
	Shape->SetRelativeRotation(FRotator(0.0, Yaw, 0.0));
	Shape->SetRelativeLocation(FVector(Centre, Floor + HalfThickness + Settings.GroundMarkingLift * Layer));
	return Material;
}

void UVeyraGreyboxSubsystem::RefreshProjectiles()
{
	const FName ColorParameter = GetDefault<UVeyraGreyboxSettings>()->ColorParameter;
	const double Now = GetServerNow();
	for (TActorIterator<AVeyraProjectile> It(GetWorld()); It; ++It)
	{
		AVeyraProjectile& Projectile = **It;
		FProjectileVisual* Visual = Projectiles.Find(&Projectile);
		if (!Visual || !Visual->Mesh.IsValid())
		{
			UMaterialInstanceDynamic* Material = nullptr;
			UStaticMeshComponent* Mesh = AddShape(Projectile, *ProjectileMesh, Material);
			if (!Mesh)
			{
				continue;
			}
			if (Material)
			{
				Material->SetVectorParameterValue(ColorParameter, ColorOfSide(Projectile.GetVeyraTeam()));
			}
			const double Radius = Projectile.GetRadius();
			FitGreyboxShape(*Mesh, FVector(Radius));
			// Clients receive launch data only and never move the actor, so the drawing places the sphere.
			Mesh->SetUsingAbsoluteLocation(true);
			Visual = &Projectiles.Add(&Projectile, FProjectileVisual{ Mesh, Projectile.GetLaunchedFrom(), Projectile.GetLaunchedAt() });
			Visual->Ability = Projectile.GetAbility();
			Visual->Color = ColorOfSide(Projectile.GetVeyraTeam());
			// A trail follows the sphere in its side's colour, at its own size whatever the sphere's (ADR-063 §4): its
			// ability's own, if it has one (ADR-072 §4).
			float OwnScale = 0.0f;
			const UVeyraSkillEffectsSubsystem* Skills = GetWorld()->GetSubsystem<UVeyraSkillEffectsSubsystem>();
			UNiagaraSystem* OwnTrail = Skills ? Skills->TravelEffectOf(Visual->Ability, OwnScale) : nullptr;
			if (UNiagaraSystem* Trailing = OwnTrail ? OwnTrail : TrailEffect.Get())
			{
				Visual->Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(Trailing, Mesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
					EAttachLocation::SnapToTarget, /*bAutoDestroy*/ true);
				if (UNiagaraComponent* Trail = Visual->Trail.Get())
				{
					Trail->SetUsingAbsoluteScale(true);
					Trail->SetVariableLinearColor(GetDefault<UVeyraGreyboxSettings>()->EffectColorParameter, Visual->Color);
					if (OwnTrail)
					{
						Trail->SetVariableFloat(GetDefault<UVeyraGreyboxSettings>()->EffectScaleParameter, OwnScale);
					}
				}
			}
		}

		FVector Location = Projectile.GetLineLocationAt(Now);
		if (Projectile.GetFlight() == EVeyraProjectileFlight::Homing)
		{
			// After its target at its speed, in server time, as the server moves it.
			const double Step = Projectile.GetSpeed() * FMath::Max(0.0, Now - Visual->PresentedAt);
			if (const AActor* Target = Projectile.GetHomingTarget())
			{
				Visual->Position += (Target->GetActorLocation() - Visual->Position).GetClampedToMaxSize(Step);
			}
			Location = Visual->Position;
		}
		Visual->PresentedAt = Now;
		Visual->Mesh->SetWorldLocation(Location);
		// Seen here, so its impact may show where the server ends it (ADR-072 §4).
		if (UVeyraSkillEffectsSubsystem* Skills = GetWorld()->GetSubsystem<UVeyraSkillEffectsSubsystem>())
		{
			Skills->NoteProjectileDrawn(Projectile.GetAbility(), Projectile.GetCastId(), Location, Projectile.GetSpeed(), Visual->Color);
		}
	}
	for (auto It = Projectiles.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void UVeyraGreyboxSubsystem::RefreshTelegraphs()
{
	Telegraphs.Reset();
	const double Now = GetServerNow();
	const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn& Caster = **It;
		const UVeyraCastStateComponent* CastState = FindGreyboxCastState(Caster);
		if (!CastState)
		{
			continue;
		}
		const FVeyraCastState& State = CastState->GetState();
		EVeyraTelegraphSource Source = EVeyraTelegraphSource::Windup;
		if (State.Phase == EVeyraCastPhase::Channel)
		{
			Source = EVeyraTelegraphSource::Channel;
		}
		else if (State.Phase != EVeyraCastPhase::Windup)
		{
			continue;
		}
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Caster.GetSimpleCollisionCylinder(Radius, HalfHeight);
		const EVeyraTeam Team = VeyraTeams::TeamOf(&Caster);
		const double Remaining = FMath::Max(0.0, State.PhaseEndsAt - Now);
		for (const FVeyraPlacedShape& Placed : VeyraCastTelegraphs::ForCast(Tuning, State, Caster.GetActorLocation(), Radius))
		{
			Telegraphs.Add(FVeyraTelegraph{ Placed, Source, Team, Remaining });
		}
	}
	for (TActorIterator<AVeyraDelayedArea> It(GetWorld()); It; ++It)
	{
		const AVeyraDelayedArea& Area = **It;
		const double Remaining = FMath::Max(0.0, Area.GetResolvesAt() - Now);
		for (const FVeyraShape& Shape : Area.GetShapes())
		{
			Telegraphs.Add(FVeyraTelegraph{ FVeyraPlacedShape{ Shape, Area.GetActorLocation(), Area.GetDirection() }, EVeyraTelegraphSource::DelayedArea,
				Area.GetVeyraTeam(), Remaining });
		}
	}
	for (TActorIterator<AVeyraLingeringArea> It(GetWorld()); It; ++It)
	{
		const AVeyraLingeringArea& Area = **It;
		Telegraphs.Add(FVeyraTelegraph{ Area.GetPlacedShape(), Area.IsEndNear(Now) ? EVeyraTelegraphSource::LingeringAreaEnding : EVeyraTelegraphSource::LingeringArea,
			Area.GetVeyraTeam(), FMath::Max(0.0, Area.GetEndsAt() - Now) });
	}
	if (const AVeyraPlayerController* Local = Cast<AVeyraPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		AddIndicator(*Local, Tuning);
		AddAttackRange(*Local);
		AddSelectionRing(*Local);
	}
	AddProjectileLanes(Now);
}

void UVeyraGreyboxSubsystem::AddProjectileLanes(double Now)
{
	// A line projectile's lane: the rest of its flight on the ground, as wide as it is, in its side's colour (ADR-067 §3). A
	// homing one follows its target and shows none.
	for (TActorIterator<AVeyraProjectile> It(GetWorld()); It; ++It)
	{
		const AVeyraProjectile& Projectile = **It;
		const double Left = Projectile.GetRange() - FMath::Max(0.0, Now - Projectile.GetLaunchedAt()) * Projectile.GetSpeed();
		if (Projectile.GetFlight() != EVeyraProjectileFlight::Line || Left <= 0.0)
		{
			continue;
		}
		FVeyraShape Lane;
		Lane.Kind = EVeyraShapeKind::Rectangle;
		Lane.Length = Left;
		Lane.Width = Projectile.GetRadius() * 2.0;
		Telegraphs.Add(FVeyraTelegraph{ FVeyraPlacedShape{ Lane, Projectile.GetLineLocationAt(Now), Projectile.GetDirection() }, EVeyraTelegraphSource::ProjectileLane,
			Projectile.GetVeyraTeam(), 0.0 });
	}
}

void UVeyraGreyboxSubsystem::AddIndicator(const AVeyraPlayerController& Local, const FVeyraAbilitiesTuning& Tuning)
{
	const TOptional<FVeyraCastIndicator>& Shown = Local.GetCastIndicator();
	const AVeyraVanguardCharacter* Body = Local.GetVanguard();
	if (!Shown || !Body || !Local.PlayerState)
	{
		return;
	}
	const UVeyraAbilityLoadoutComponent* Loadout = Local.PlayerState->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
	const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Shown->Slot) : nullptr;
	FHitResult Ground;
	if (!Entry || !Local.GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex*/ false, Ground))
	{
		return;
	}
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Body->GetSimpleCollisionCylinder(Radius, HalfHeight);
	for (const FVeyraPlacedShape& Placed : VeyraCastTelegraphs::ForAim(Tuning, Entry->Ability, Body->GetActorLocation(), Radius, Ground.Location))
	{
		Telegraphs.Add(FVeyraTelegraph{ Placed, EVeyraTelegraphSource::Indicator, VeyraTeams::TeamOf(Body), 0.0 });
	}
}

void UVeyraGreyboxSubsystem::AddAttackRange(const AVeyraPlayerController& Local)
{
	// A guide in the indicator's appearance: no target is acquired and no attack follows (ADR-052 §4).
	const APawn* Body = Local.GetCommandedBody();
	const TOptional<double> Reach = Body && Local.IsShowingAttackRange() ? VeyraHud::AttackReachOf(*Body) : TOptional<double>();
	if (!Reach)
	{
		return;
	}
	FVeyraShape Ring;
	Ring.Kind = EVeyraShapeKind::Circle;
	Ring.Radius = Reach.GetValue();
	Telegraphs.Add(FVeyraTelegraph{ FVeyraPlacedShape{ Ring, Body->GetActorLocation(), FVector::ForwardVector }, EVeyraTelegraphSource::AttackRange, VeyraTeams::TeamOf(Body), 0.0 });
}

void UVeyraGreyboxSubsystem::AddSelectionRing(const AVeyraPlayerController& Local)
{
	const AActor* Selected = Local.GetSelectedUnit();
	if (!Selected)
	{
		return;
	}
	// Just outside the body as it is drawn, larger than its capsule and easing after it (ADR-065 §11–§12).
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Selected->GetSimpleCollisionCylinder(Radius, HalfHeight);
	FVeyraShape Ring;
	Ring.Kind = EVeyraShapeKind::Circle;
	Ring.Radius = Radius * Settings.VisualScaleOf(*Selected) + Settings.SelectionRingMargin;
	Telegraphs.Add(FVeyraTelegraph{ FVeyraPlacedShape{ Ring, VeyraDrawnBody::LocationOf(*Selected), FVector::ForwardVector }, EVeyraTelegraphSource::Selection,
		VeyraTeams::TeamOf(Selected), 0.0 });
}

FVector UVeyraGreyboxSubsystem::GroundUnder(const FVector& Location) const
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FVector Lift = FVector::UpVector * Settings.TelegraphLift;
	// Everything drawn here lies over the ground, a ridge's cliff faces included, so it takes the surface however steep.
	FVector Surface;
	return (VeyraSurfacePlacement::Drape(*GetWorld(), FVector2D(Location), UVeyraWorldTuningSubsystem::Get().Layout.Surface, Surface) ? Surface : Location) + Lift;
}

void UVeyraGreyboxSubsystem::DrawVisionMarks()
{
	// DrawTelegraphs made the line batch and flushed it this refresh; these join it.
	if (!TelegraphLines)
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FVeyraHudVision Vision = VeyraHud::DescribeVision(GetWorld(), GetViewerTeam(), GetServerNow());
	const auto DrawRing = [this, &Settings](const FVector& Centre, double Radius, const FLinearColor& Color) {
		FVeyraShape Circle;
		Circle.Kind = EVeyraShapeKind::Circle;
		Circle.Radius = Radius;
		const FVeyraPlacedShape OnGround{ Circle, GroundUnder(Centre), FVector::ForwardVector };
		for (const FVeyraOutlineSegment& Segment : VeyraGreyboxOutline::Of(OnGround, Settings.CircleSegments))
		{
			TelegraphLines->DrawLine(Segment.Start, Segment.End, Color, SDPG_World, Settings.TelegraphThickness, 0.0f);
		}
	};
	for (const FVeyraHudPing& Ping : Vision.Pings)
	{
		FLinearColor Color = Settings.PresencePingColor;
		Color.A *= static_cast<float>(Ping.Fade);
		DrawRing(FVector(Ping.Centre, 0.0), Ping.Radius, Color);
	}
	for (const FVector& Outline : Vision.Outlines)
	{
		DrawRing(Outline, Settings.OutlineMarkerRadius, Settings.OutlineColor);
	}
}

void UVeyraGreyboxSubsystem::DrawChains()
{
	// A chain joins a companion to its owner while it lasts, in its side's colour (ADR-034 §7).
	if (!TelegraphLines)
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	for (TActorIterator<AVeyraCompanion> It(GetWorld()); It; ++It)
	{
		const AVeyraCompanion& Companion = **It;
		const APlayerState* Whose = Companion.GetOwnerState();
		const APawn* OwnerBody = Whose ? Whose->GetPawn() : nullptr;
		if (!Companion.IsChained() || Companion.IsHidden() || !OwnerBody)
		{
			continue;
		}
		TelegraphLines->DrawLine(GroundUnder(Companion.GetActorLocation()), GroundUnder(OwnerBody->GetActorLocation()), ColorOfSide(Companion.GetVeyraTeam()),
			SDPG_World, Settings.TelegraphThickness, 0.0f);
	}
}

void UVeyraGreyboxSubsystem::DrawEchoTethers()
{
	// A projected Echo's tether is gameplay information (Item Bible §11; ADR-050 §7): the circle it must stay within,
	// around the Stasis body, and the stream from the body to it, strained as its Integrity runs low.
	if (!TelegraphLines)
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	for (TActorIterator<AVeyraEcho> It(GetWorld()); It; ++It)
	{
		const AVeyraEcho& Echo = **It;
		const UAbilitySystemComponent* Abilities = Echo.GetAbilitySystemComponent();
		if (Echo.IsWithdrawn() || Echo.IsHidden() || !(Echo.GetRadius() > 0.0) || !Abilities)
		{
			continue;
		}
		const double Max = Abilities->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
		const double Share = Max > 0.0 ? Abilities->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) / Max : 0.0;
		const FLinearColor Color = Share < Settings.EchoStrainShare ? Settings.EchoStrainColor : ColorOfSide(Echo.GetVeyraTeam());
		FVeyraShape Circle;
		Circle.Kind = EVeyraShapeKind::Circle;
		Circle.Radius = Echo.GetRadius();
		const FVeyraPlacedShape OnGround{ Circle, GroundUnder(Echo.GetAnchor()), FVector::ForwardVector };
		for (const FVeyraOutlineSegment& Segment : VeyraGreyboxOutline::Of(OnGround, Settings.CircleSegments))
		{
			TelegraphLines->DrawLine(Segment.Start, Segment.End, Color, SDPG_World, Settings.TelegraphThickness, 0.0f);
		}
		TelegraphLines->DrawLine(GroundUnder(Echo.GetAnchor()), GroundUnder(Echo.GetActorLocation()), Color, SDPG_World, Settings.TelegraphThickness, 0.0f);
	}
}

void UVeyraGreyboxSubsystem::DrawOrderMark()
{
	OrderMarkRing.Reset();
	const AVeyraPlayerController* Local = Cast<AVeyraPlayerController>(GetWorld()->GetFirstPlayerController());
	if (!TelegraphLines || !Local || !Local->GetOrderMark())
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	// The player may turn click markers off (Settings Bible §3.3).
	const FVeyraInterfacePreferences Preferences = VeyraInterfacePreferences::Resolve(Settings, VeyraInterfacePreferences::StoreOf(this));
	if (!Preferences.bClickMarkers)
	{
		return;
	}
	OrderMarkRing = VeyraOrderMarks::Describe(Local->GetOrderMark().GetValue(), GetWorld()->GetRealTimeSeconds(), Preferences.bReduceUiAnimation, Settings);
	if (!OrderMarkRing)
	{
		return;
	}
	FVeyraShape Circle;
	Circle.Kind = EVeyraShapeKind::Circle;
	Circle.Radius = OrderMarkRing->Radius;
	const FVeyraPlacedShape OnGround{ Circle, GroundUnder(OrderMarkRing->Centre), FVector::ForwardVector };
	for (const FVeyraOutlineSegment& Segment : VeyraGreyboxOutline::Of(OnGround, Settings.CircleSegments))
	{
		TelegraphLines->DrawLine(Segment.Start, Segment.End, OrderMarkRing->Color, SDPG_World, Settings.TelegraphThickness, 0.0f);
	}
}

void UVeyraGreyboxSubsystem::DrawTelegraphs()
{
	if (!TelegraphLines)
	{
		TelegraphLines = NewObject<ULineBatchComponent>(this, NAME_None, RF_Transient);
		TelegraphLines->bCalculateAccurateBounds = false;
		TelegraphLines->RegisterComponentWithWorld(GetWorld());
	}
	TelegraphLines->Flush();
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const float IndicatorThickness = VeyraInterfacePreferences::Resolve(Settings, VeyraInterfacePreferences::StoreOf(this)).IndicatorThickness;
	for (const FVeyraTelegraph& Telegraph : Telegraphs)
	{
		FVeyraPlacedShape OnGround = Telegraph.Placed;
		OnGround.Origin = GroundUnder(Telegraph.Placed.Origin);
		// An end about to land is marked in one colour for every side, so it reads as a warning (ADR-026 §4);
		// the player's own indicator in its own.
		const bool bIndicator = Telegraph.Source == EVeyraTelegraphSource::Indicator || Telegraph.Source == EVeyraTelegraphSource::AttackRange;
		const FLinearColor Color = bIndicator ? Settings.IndicatorColor
			: Telegraph.Source == EVeyraTelegraphSource::LingeringAreaEnding ? Settings.EndingColor : ColorOfSide(Telegraph.Team);
		const float Thickness = bIndicator ? IndicatorThickness : Settings.TelegraphThickness;
		for (const FVeyraOutlineSegment& Segment : VeyraGreyboxOutline::Of(OnGround, Settings.CircleSegments))
		{
			// A lifetime of 0 keeps the line until the next refresh flushes it.
			TelegraphLines->DrawLine(GroundUnder(Segment.Start), GroundUnder(Segment.End), Color, SDPG_World, Thickness, 0.0f);
		}
	}
	DrawTelegraphFills();
}

void UVeyraGreyboxSubsystem::DrawTelegraphFills()
{
	if (!TelegraphFillMaterial || !TelegraphFillMesh)
	{
		return;
	}
	AActor* Owner = TelegraphFillOwner.Get();
	if (!Owner)
	{
		// Presentation only, drawn by this machine alone.
		FActorSpawnParameters Parameters;
		Parameters.ObjectFlags |= RF_Transient;
		Owner = GetWorld()->SpawnActor<AActor>(Parameters);
		if (!Owner)
		{
			return;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Owner, NAME_None, RF_Transient);
		Owner->SetRootComponent(Root);
		Root->RegisterComponent();
		TelegraphFillOwner = Owner;
		TelegraphFills.Reset();
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	// The quad's own half-size, which each fill's scale stretches to its shape's.
	const FVector Extent = TelegraphFillMesh->GetBounds().BoxExtent;
	int32 Used = 0;
	for (const FVeyraTelegraph& Telegraph : Telegraphs)
	{
		if (!VeyraTelegraphFill::IsFilled(Telegraph.Source))
		{
			continue;
		}
		if (Used == TelegraphFills.Num())
		{
			UStaticMeshComponent* Quad = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
			Quad->SetStaticMesh(TelegraphFillMesh);
			Quad->SetMobility(EComponentMobility::Movable);
			Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Quad->SetGenerateOverlapEvents(false);
			Quad->SetCanEverAffectNavigation(false);
			Quad->SetCastShadow(false);
			Quad->SetupAttachment(Owner->GetRootComponent());
			Quad->RegisterComponent();
			Quad->CreateDynamicMaterialInstance(0, TelegraphFillMaterial);
			TelegraphFills.Add(Quad);
		}
		UStaticMeshComponent* Quad = TelegraphFills[Used++];
		const FVeyraTelegraphFill Fill = VeyraTelegraphFill::Of(Telegraph.Placed, GroundUnder(Telegraph.Placed.Origin));
		Quad->SetWorldLocationAndRotation(GroundUnder(Fill.Centre), FRotator(0.0, Fill.Yaw, 0.0));
		Quad->SetWorldScale3D(FVector(Fill.HalfSize.X / FMath::Max(Extent.X, UE_KINDA_SMALL_NUMBER), Fill.HalfSize.Y / FMath::Max(Extent.Y, UE_KINDA_SMALL_NUMBER), 1.0));
		// In the outline's own colour.
		const bool bIndicator = Telegraph.Source == EVeyraTelegraphSource::Indicator;
		const FLinearColor Color = bIndicator ? Settings.IndicatorColor
			: Telegraph.Source == EVeyraTelegraphSource::LingeringAreaEnding ? Settings.EndingColor : ColorOfSide(Telegraph.Team);
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Quad->GetMaterial(0)))
		{
			Material->SetScalarParameterValue(Settings.TelegraphShapeParameter, static_cast<float>(Fill.Shape));
			Material->SetScalarParameterValue(Settings.TelegraphHalfArcParameter, static_cast<float>(Fill.HalfArc));
			Material->SetScalarParameterValue(Settings.TelegraphLandingParameter,
				static_cast<float>(VeyraTelegraphFill::LandingOf(Telegraph.Source, Telegraph.RemainingSeconds, Settings.TelegraphLandingSeconds)));
			Material->SetVectorParameterValue(Settings.TelegraphColorParameter, Color.CopyWithNewOpacity(1.0f));
			Material->SetVectorParameterValue(Settings.TelegraphSizeParameter, FLinearColor(Fill.HalfSize.X, Fill.HalfSize.Y, 0.0f, 0.0f));
		}
		Quad->SetVisibility(true);
	}
	for (int32 Index = Used; Index < TelegraphFills.Num(); ++Index)
	{
		if (TelegraphFills[Index])
		{
			TelegraphFills[Index]->SetVisibility(false);
		}
	}
}

TArray<UStaticMeshComponent*> UVeyraGreyboxSubsystem::GetTelegraphFills() const
{
	TArray<UStaticMeshComponent*> Shown;
	for (UStaticMeshComponent* Quad : TelegraphFills)
	{
		if (Quad && Quad->IsVisible())
		{
			Shown.Add(Quad);
		}
	}
	return Shown;
}

