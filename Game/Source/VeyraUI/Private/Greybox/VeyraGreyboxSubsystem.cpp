// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraGreyboxSubsystem.h"

#include "Engine/Font.h"
#include "Styling/CoreStyle.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraComponent.h"
#include "Camera/VeyraCameraRig.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Casting/VeyraCastTelegraphs.h"
#include "Components/LineBatchComponent.h"
#include "Companions/VeyraCompanion.h"
#include "Cues/VeyraCombatCueSubsystem.h"
#include "Echoes/VeyraEcho.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Components/StaticMeshComponent.h"
#include "Delivery/VeyraDelayedArea.h"
#include "Delivery/VeyraLingeringArea.h"
#include "Delivery/VeyraProjectile.h"
#include "Entities/VeyraPlacedMarker.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fog/VeyraDenseFogBank.h"
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
#include "Settings/VeyraDisplayRules.h"
#include "State/VeyraVisionTeamState.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
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
		if (!ImpactSound || !SwingSound || !CastSound || !DeathSound || !ClickSound)
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
		TrailEffect = Settings.TrailEffect.LoadSynchronous();
		if (!ImpactEffect || !CastEffect || !DeathEffect || !TrailEffect)
		{
			Problems.Add(TEXT("ImpactEffect: the impact, cast, death and trail effects do not all load; run BuildEffects.ps1."));
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
		// An animated body acts it out, its windup timed to end as the attack commits (ADR-064 §3).
		if (UVeyraVanguardAnimInstance* Animation = Body->Skin.IsValid() ? Cast<UVeyraVanguardAnimInstance>(Body->Skin->GetAnimInstance()) : nullptr)
		{
			Animation->NoteCue(Cue.Kind, static_cast<float>(FMath::Max(0.0, Cue.EndsAt - GetServerNow())));
		}
	}
	PlayEffect(Cue);
	PlaySound(Cue);
	NoteSwing(Cue);
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
	case EVeyraCombatCueKind::AttackWindup:
	case EVeyraCombatCueKind::CastWindup:
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
	const float Volume = VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), VeyraInterfacePreferences::StoreOf(this)).EffectsVolume;
	if (Volume > 0.0f)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, Unit->GetActorLocation(), FRotator::ZeroRotator, Volume, /*PitchMultiplier*/ 1.0f,
			/*StartTime*/ 0.0f, CueAttenuation, CueConcurrency);
	}
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
	case EVeyraCombatCueKind::AttackWindup:
	case EVeyraCombatCueKind::AttackCommit:
	case EVeyraCombatCueKind::CastWindup:
		break;
	}
	return nullptr;
}

UNiagaraComponent* UVeyraGreyboxSubsystem::PlayEffect(const FVeyraCombatCue& Cue)
{
	const AActor* Unit = Cue.Unit.Get();
	UNiagaraSystem* Effect = EffectFor(Cue.Kind);
	if (!Unit || !Effect || !bReady)
	{
		return nullptr;
	}
	// A cast flashes from its caster toward where it was aimed; the rest where their unit stands.
	const FVector At = Unit->GetActorLocation();
	const FVector Toward = Cue.Kind == EVeyraCombatCueKind::CastCommit ? (Cue.Location - At).GetSafeNormal2D() : FVector::ZeroVector;
	const FRotator Facing = Toward.IsZero() ? Unit->GetActorRotation() : Toward.Rotation();
	// Pooled: a battleground's waves raise many hits a second.
	UNiagaraComponent* Played = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Effect, At, Facing, FVector::OneVector, /*bAutoDestroy*/ true,
		/*bAutoActivate*/ true, ENCPoolMethod::AutoRelease);
	if (Played)
	{
		Played->SetVariableLinearColor(GetDefault<UVeyraGreyboxSettings>()->EffectColorParameter, SideColorOf(*Unit));
	}
	return Played;
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
		SetOutlined(*Was, false);
	}
	Hovered = NewHovered;
	// Every frame: the unit's art may change under the cursor, as a structure falls.
	if (NewHovered)
	{
		SetOutlined(*NewHovered, true);
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

void UVeyraGreyboxSubsystem::SetOutlined(const AActor& Unit, bool bOutlined) const
{
	const FBody* Body = Bodies.Find(&Unit);
	if (!Body)
	{
		return;
	}
	const int32 Stencil = HoverStencilOf(Unit);
	for (UPrimitiveComponent* Shape : { static_cast<UPrimitiveComponent*>(Body->Mesh.Get()), static_cast<UPrimitiveComponent*>(Body->Art.Get()),
			 static_cast<UPrimitiveComponent*>(Body->Skin.Get()) })
	{
		if (!Shape)
		{
			continue;
		}
		if (Shape->bRenderCustomDepth != bOutlined)
		{
			Shape->SetRenderCustomDepth(bOutlined);
		}
		if (bOutlined && Shape->CustomDepthStencilValue != Stencil)
		{
			Shape->SetCustomDepthStencilValue(Stencil);
		}
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
	const USceneComponent* Root = Unit.GetRootComponent();
	// A structure stands still, and an animated body's animation acts out its fight (ADR-064 §3): they only flash.
	if (Root && !Unit.IsA<AVeyraStructure>() && !Skin)
	{
		// The pose moves on the ground, whichever way the unit faces.
		const FVector Local = Root->GetComponentTransform().InverseTransformVectorNoScale(Pose.Offset);
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
	Projectiles.Reset();
	Telegraphs.Reset();
	if (AVeyraPlayerController* Source = CombatTextSource.Get())
	{
		Source->OnCombatText.Remove(CombatTextHandle);
	}
	CombatTextSource.Reset();
	CombatText.Reset();
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
	}
	// Forgotten as they are drawn: a running total keeps all of its parts while it shows.
	const FVeyraInterfacePreferences Preferences = VeyraInterfacePreferences::Resolve(*GetDefault<UVeyraGreyboxSettings>(), VeyraInterfacePreferences::StoreOf(GetWorld()));
	VeyraCombatTextView::Forget(CombatText, FPlatformTime::Seconds(), Preferences.CombatText);
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

UStaticMeshComponent* UVeyraGreyboxSubsystem::AddShape(AActor& Owner, UStaticMesh& Mesh, UMaterialInstanceDynamic*& OutMaterial) const
{
	USceneComponent* Root = Owner.GetRootComponent();
	if (!Root)
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
	Shape->SetupAttachment(Root);
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

void UVeyraGreyboxSubsystem::RefreshVanguardArt(const AVeyraVanguardCharacter& Unit, FBody& Body)
{
	// Its Vanguard arrives with its participant; until then, and for a Vanguard without art, its body shows.
	const AVeyraPlayerState* Participant = Unit.GetPlayerState<AVeyraPlayerState>();
	const FVeyraVanguardArt* Art = VanguardArt && Participant && Participant->GetVanguardId().IsValid()
		? VanguardArt->Find(FName(Participant->GetVanguardId().ToString()))
		: nullptr;
	if (!Art || !Art->Mesh)
	{
		return;
	}
	if (!Body.Skin.IsValid())
	{
		Body.Skin = VeyraVanguardSkin::Attach(const_cast<AVeyraVanguardCharacter&>(Unit));
	}
	USkeletalMeshComponent* Skin = Body.Skin.Get();
	if (!Skin)
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	VeyraVanguardSkin::Dress(*Skin, *Art, VeyraVanguardSkin::ShapeOf(Settings));
	// It stands at the capsule's foot, which its Vanguard's definition shapes once it arrives (ADR-008 §2).
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
	Skin->SetRelativeLocation(FVector(0.0, 0.0, -HalfHeight));
	if (UVeyraVanguardAnimInstance* Animation = Cast<UVeyraVanguardAnimInstance>(Skin->GetAnimInstance()))
	{
		Animation->SetInputs(VeyraVanguardSkin::InputsOf(Unit, GetViewerTeam()));
	}
	// Its body lies under its feet as a disc, still showing its side and its status tint.
	if (UStaticMeshComponent* Shape = Body.Mesh.Get())
	{
		const double DiscHalfHeight = Settings.VanguardFootDiscHeight * 0.5;
		FitGreyboxShape(*Shape, FVector(Radius, Radius, DiscHalfHeight));
		Shape->SetRelativeLocation(Shape->GetRelativeLocation() + FVector(0.0, 0.0, DiscHalfHeight - HalfHeight));
	}
}

void UVeyraGreyboxSubsystem::ShowArt(const APawn& Unit, FBody& Body, UStaticMesh& Mesh, const UVeyraUnitArtSet& Set, const FLinearColor& Color)
{
	USceneComponent* Root = Unit.GetRootComponent();
	if (!Root)
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
		Art->SetupAttachment(Root);
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
		}
		// Its capsule, which its Vanguard's definition shapes (ADR-008 §2).
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
		FitGreyboxShape(*Body->Mesh, FVector(Radius, Radius, HalfHeight));

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
		else if (const AVeyraVanguardCharacter* Vanguard = Cast<AVeyraVanguardCharacter>(&Unit))
		{
			RefreshVanguardArt(*Vanguard, *Body);
		}
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
			// A trail follows the sphere in its side's colour, at its own size whatever the sphere's (ADR-063 §4).
			if (TrailEffect)
			{
				Visual->Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(TrailEffect, Mesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
					EAttachLocation::SnapToTarget, /*bAutoDestroy*/ true);
				if (UNiagaraComponent* Trail = Visual->Trail.Get())
				{
					Trail->SetUsingAbsoluteScale(true);
					Trail->SetVariableLinearColor(GetDefault<UVeyraGreyboxSettings>()->EffectColorParameter, ColorOfSide(Projectile.GetVeyraTeam()));
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
	Telegraphs.Add(FVeyraTelegraph{ FVeyraPlacedShape{ Ring, Body->GetActorLocation(), FVector::ForwardVector }, EVeyraTelegraphSource::Indicator, VeyraTeams::TeamOf(Body), 0.0 });
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
		const bool bIndicator = Telegraph.Source == EVeyraTelegraphSource::Indicator;
		const FLinearColor Color = bIndicator ? Settings.IndicatorColor
			: Telegraph.Source == EVeyraTelegraphSource::LingeringAreaEnding ? Settings.EndingColor : ColorOfSide(Telegraph.Team);
		const float Thickness = bIndicator ? IndicatorThickness : Settings.TelegraphThickness;
		for (const FVeyraOutlineSegment& Segment : VeyraGreyboxOutline::Of(OnGround, Settings.CircleSegments))
		{
			// A lifetime of 0 keeps the line until the next refresh flushes it.
			TelegraphLines->DrawLine(GroundUnder(Segment.Start), GroundUnder(Segment.End), Color, SDPG_World, Thickness, 0.0f);
		}
	}
}

