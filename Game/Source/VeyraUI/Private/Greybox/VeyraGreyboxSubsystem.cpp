// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraGreyboxSubsystem.h"

#include "Engine/Font.h"
#include "Styling/CoreStyle.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Casting/VeyraCastTelegraphs.h"
#include "Components/LineBatchComponent.h"
#include "Companions/VeyraCompanion.h"
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
#include "Greybox/VeyraGreyboxOutline.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraUnitArtSet.h"
#include "Hud/VeyraHudModel.h"
#include "Hud/VeyraHudOverlay.h"
#include "Hud/VeyraFogOfWarModel.h"
#include "State/VeyraVisionTeamState.h"
#include "Layout/VeyraLayout.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Structures/VeyraStructure.h"
#include "Terrain/VeyraTerrainWall.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
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
	RefreshCombatText();
	RefreshFogOfWar();
	AttachHudOverlay();
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
		const double Z = GroundUnder(FVector(Box.GetCenter(), 0.0)).Z - Settings.TelegraphLift + Settings.FogOfWarLift;
		const int32 Base = Vertices.Num();
		Vertices.Append({ FVector(Box.Min.X, Box.Min.Y, Z), FVector(Box.Max.X, Box.Min.Y, Z), FVector(Box.Max.X, Box.Max.Y, Z), FVector(Box.Min.X, Box.Max.Y, Z) });
		Indices.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3, Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
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
	}
	// Runtime terrain stands as a block across the way it faces, in the neutral colour (ADR-032 §4).
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	for (TActorIterator<AVeyraTerrainWall> It(GetWorld()); It; ++It)
	{
		AVeyraTerrainWall& Wall = **It;
		if (Wall.GetHalfExtent().GetMin() <= 0.0 || (Bodies.Contains(&Wall) && Bodies.FindChecked(&Wall).Mesh.IsValid()))
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
			TelegraphLines->DrawLine(Segment.Start, Segment.End, Color, SDPG_World, Thickness, 0.0f);
		}
	}
}

