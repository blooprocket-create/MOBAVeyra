// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraGreyboxSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Casting/VeyraCastTelegraphs.h"
#include "Components/LineBatchComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Delivery/VeyraDelayedArea.h"
#include "Delivery/VeyraLingeringArea.h"
#include "Delivery/VeyraProjectile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Greybox/VeyraGreyboxOutline.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Hud/VeyraHudModel.h"
#include "Hud/VeyraHudOverlay.h"
#include "Layout/VeyraLayout.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Structures/VeyraStructure.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraGameState.h"
#include "VeyraUILog.h"

namespace
{
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

void UVeyraGreyboxSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	TArray<FString> Problems = Settings.Validate();
	if (Problems.IsEmpty())
	{
		BodyMesh = Settings.BodyMesh.LoadSynchronous();
		ProjectileMesh = Settings.ProjectileMesh.LoadSynchronous();
		ShapeMaterial = Settings.ShapeMaterial.LoadSynchronous();
		GroundMesh = Settings.GroundMesh.LoadSynchronous();
		PadMesh = Settings.PadMesh.LoadSynchronous();
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
	}
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogVeyraUI, Error, TEXT("The grey-box presentation is off: %s"), *Problem);
	}
	bReady = Problems.IsEmpty();
}

void UVeyraGreyboxSubsystem::Deinitialize()
{
	if (AHUD* Hud = OverlayHud.Get(); Hud && HudOverlay.IsValid())
	{
		Hud->RemovePostRenderedActor(HudOverlay.Get());
	}
	if (TelegraphLines && TelegraphLines->IsRegistered())
	{
		TelegraphLines->UnregisterComponent();
	}
	TelegraphLines = nullptr;
	// Bodies and projectile spheres belong to their actors, which the world destroys with them.
	Bodies.Reset();
	Projectiles.Reset();
	Telegraphs.Reset();
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
	AttachHudOverlay();
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

FLinearColor UVeyraGreyboxSubsystem::ColorOfSide(EVeyraTeam Team) const
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	if (Team == EVeyraTeam::None)
	{
		return Settings.NeutralColor;
	}
	const EVeyraTeam ViewerTeam = GetViewerTeam();
	const EVeyraTeam Allies = ViewerTeam == EVeyraTeam::None ? EVeyraTeam::A : ViewerTeam;
	return Team == Allies ? Settings.AllyColor : Settings.EnemyColor;
}

FLinearColor UVeyraGreyboxSubsystem::SideColorOf(const AActor& Unit) const
{
	// The viewer's Vanguard carries the viewer's PlayerState (ADR-006 §7).
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = Cast<APawn>(&Unit);
	if (Viewer && Viewer->PlayerState && Pawn && Pawn->GetPlayerState() == Viewer->PlayerState)
	{
		return GetDefault<UVeyraGreyboxSettings>()->OwnColor;
	}
	return ColorOfSide(VeyraTeams::TeamOf(&Unit));
}

FLinearColor UVeyraGreyboxSubsystem::BodyColorOf(const AActor& Unit) const
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FLinearColor Side = SideColorOf(Unit);
	const TArray<FVeyraHudStatus> Statuses = VeyraHud::StatusesOf(Unit, GetServerNow());
	const auto Has = [&Statuses](EVeyraStatusKind Kind) {
		return Statuses.ContainsByPredicate([Kind](const FVeyraHudStatus& Status) { return Status.Kind == Kind; });
	};
	// A stun matters more than a slow.
	if (Has(EVeyraStatusKind::Stun))
	{
		return FLinearColor::LerpUsingHSV(Side, Settings.StunColor, Settings.StatusTintStrength);
	}
	if (Has(EVeyraStatusKind::Slow))
	{
		return FLinearColor::LerpUsingHSV(Side, Settings.SlowColor, Settings.StatusTintStrength);
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

void UVeyraGreyboxSubsystem::RefreshBodies()
{
	const FName ColorParameter = GetDefault<UVeyraGreyboxSettings>()->ColorParameter;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn& Unit = **It;
		if (!VeyraUnits::KindOf(&Unit).IsSet())
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
	// The river crosses the floor corner to corner along Y = -X (Battleground Bible §2).
	const double Diagonal = Layout.HalfExtent * UE_DOUBLE_SQRT_2;
	AddGroundMarking(*Owner, *GroundMesh, Settings.RiverColor, FVector2D::ZeroVector, -45.0, FVector2D(Diagonal, Layout.RiverWidth / 2.0), 1);
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
	// The Dense Fog, the battleground's bush, on top: a player sees where it lies, not who is in it.
	for (const FVeyraFogPlacement& Fog : VeyraLayout::DenseFog(Layout))
	{
		AddGroundMarking(*Owner, *PadMesh, Settings.DenseFogColor, Fog.Center, 0.0, FVector2D(Fog.Radius), 4);
	}
}

FLinearColor UVeyraGreyboxSubsystem::BaseColorOf(EVeyraTeam Team) const
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	return ColorOfSide(Team) == Settings.AllyColor ? Settings.AllyBaseColor : Settings.EnemyBaseColor;
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
		Telegraphs.Add(FVeyraTelegraph{ Area.GetPlacedShape(), EVeyraTelegraphSource::LingeringArea, Area.GetVeyraTeam(),
			FMath::Max(0.0, Area.GetEndsAt() - Now) });
	}
}

FVector UVeyraGreyboxSubsystem::GroundUnder(const FVector& Location) const
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FVector Lift = FVector::UpVector * Settings.TelegraphLift;
	FCollisionObjectQueryParams Ground;
	Ground.AddObjectTypesToQuery(ECC_WorldStatic);
	Ground.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit;
	const FVector Start = Location + Lift;
	const FVector End = Location - FVector::UpVector * Settings.GroundProbeDistance;
	const bool bFound = GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Ground, FCollisionQueryParams(SCENE_QUERY_STAT(VeyraGreyboxGround), false));
	return (bFound ? FVector(Hit.ImpactPoint) : Location) + Lift;
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
	for (const FVeyraTelegraph& Telegraph : Telegraphs)
	{
		FVeyraPlacedShape OnGround = Telegraph.Placed;
		OnGround.Origin = GroundUnder(Telegraph.Placed.Origin);
		const FLinearColor Color = ColorOfSide(Telegraph.Team);
		for (const FVeyraOutlineSegment& Segment : VeyraGreyboxOutline::Of(OnGround, Settings.CircleSegments))
		{
			// A lifetime of 0 keeps the line until the next refresh flushes it.
			TelegraphLines->DrawLine(Segment.Start, Segment.End, Color, SDPG_World, Settings.TelegraphThickness, 0.0f);
		}
	}
}

