// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Water/VeyraWaterSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Layout/VeyraRiver.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Tuning/VeyraWorldTuning.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraUILog.h"
#include "Water/VeyraWaterSettings.h"

bool UVeyraWaterSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Presentation is for worlds someone watches: game and play-in-editor worlds, never a dedicated server's.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraWaterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UVeyraWaterSettings& Settings = *GetDefault<UVeyraWaterSettings>();
	TArray<FString> Problems = Settings.Validate();
	if (Problems.IsEmpty())
	{
		WakeSystem = Settings.WakeEffect.LoadSynchronous();
		SplashSystem = Settings.SplashEffect.LoadSynchronous();
		if (!WakeSystem || !SplashSystem)
		{
			Problems.Add(TEXT("WakeEffect: the wake or the splash does not load; run BuildEffects.ps1."));
		}
#if WITH_EDITORONLY_DATA
		// An editor build compiles a system's scripts the first time it plays, stalling that frame: done here instead, while
		// the world loads, so the first unit to wade does not hitch. A cooked build ships them compiled. Only where something
		// renders, and never waiting on GPU shaders, which a machine without a renderer never compiles (and which compile
		// in the background where one does).
		if (FApp::CanEverRender())
		{
			for (UNiagaraSystem* System : { WakeSystem.Get(), SplashSystem.Get() })
			{
				if (System)
				{
					System->WaitForCompilationComplete(/*bIncludingGPUShaders*/ false, /*bShowProgress*/ false);
				}
			}
		}
#endif
		if (!Settings.RippleMaterial.LoadSynchronous())
		{
			Problems.Add(TEXT("RippleMaterial: does not load; run BuildTerrainArt.ps1."));
		}
	}
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogVeyraUI, Error, TEXT("Water presentation: %s"), *Problem);
	}
	bReady = Problems.IsEmpty();
}

void UVeyraWaterSubsystem::Deinitialize()
{
	for (TPair<TWeakObjectPtr<const AActor>, FWader>& Wader : Waders)
	{
		Release(Wader.Value);
	}
	Waders.Reset();
	Super::Deinitialize();
}

TStatId UVeyraWaterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraWaterSubsystem, STATGROUP_Tickables);
}

void UVeyraWaterSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UWorld* World = GetWorld();
	const APlayerController* Viewer = World ? World->GetFirstPlayerController() : nullptr;
	if (!bReady || !World || !World->HasBegunPlay() || !Viewer || !Viewer->PlayerCameraManager)
	{
		return;
	}
	const double SurfaceZ = UVeyraWorldTuningSubsystem::Get().Layout.River.SurfaceZ;
	const APlayerCameraManager& Camera = *Viewer->PlayerCameraManager;
	Refresh(DeltaSeconds, VeyraWaterRipples::FocusOf(Camera.GetCameraLocation(), Camera.GetCameraRotation().Vector(), SurfaceZ));
}

void UVeyraWaterSubsystem::Refresh(float DeltaSeconds, const FVector2D& Focus)
{
	if (!bReady)
	{
		return;
	}
	if (!bSearched)
	{
		FindRiver();
	}
	if (!River.IsValid())
	{
		return;
	}
	const UVeyraWaterSettings& Settings = *GetDefault<UVeyraWaterSettings>();
	// A fixed step, so ripples spread the same at any frame rate; a slow frame catches up only so far.
	Owed += FMath::Max(0.0f, DeltaSeconds);
	const double StepSeconds = 1.0 / Settings.StepsPerSecond;
	const int32 Steps = FMath::Min(FMath::FloorToInt32(Owed / StepSeconds), Settings.MaxStepsPerFrame);
	Owed = FMath::Clamp(Owed - Steps * StepSeconds, 0.0, StepSeconds);
	// The window moves only with a step, which carries its ripples along; a frame that takes none leaves it where they are.
	if (Steps > 0 || !Window.bPlaced)
	{
		Window = VeyraWaterRipples::Follow(Window, Focus, Settings.WindowSize, Settings.Resolution);
	}
	else
	{
		Window.Shift = FIntPoint::ZeroValue;
	}
	Stir();
	// A machine that renders nothing (a test's, run without a renderer) has no targets to step; its wakes still follow.
	if (!CanRipple())
	{
		return;
	}
	Step(Steps);
	Surface->SetTextureParameterValue(Settings.HeightParameter, States[Current]);
	Surface->SetVectorParameterValue(Settings.WindowParameter,
		FLinearColor(static_cast<float>(Window.Origin.X), static_cast<float>(Window.Origin.Y), Settings.WindowSize, 1.0f / Settings.Resolution));
	Surface->SetScalarParameterValue(Settings.NormalStrengthParameter, Settings.NormalStrength);
}

bool UVeyraWaterSubsystem::CanRipple() const
{
	return Surface && Stepper && States[0] && States[1];
}

void UVeyraWaterSubsystem::FindRiver()
{
	bSearched = true;
	UWorld* World = GetWorld();
	const UVeyraWaterSettings& Settings = *GetDefault<UVeyraWaterSettings>();
	UPrimitiveComponent* Found = nullptr;
	for (TActorIterator<AActor> It(World); It && !Found; ++It)
	{
		if (It->ActorHasTag(Settings.RiverTag))
		{
			Found = It->FindComponentByClass<UPrimitiveComponent>();
		}
	}
	// A battleground drawn without its authored river (a flat test floor) has no water to answer.
	if (!Found)
	{
		return;
	}
	River = Found;
	Surface = Found->CreateDynamicMaterialInstance(0);
	Stepper = UMaterialInstanceDynamic::Create(Settings.RippleMaterial.Get(), this);
	for (TObjectPtr<UTextureRenderTarget2D>& State : States)
	{
		// None where nothing renders: the ripples are then not stepped, and the river shows its still water.
		State = UKismetRenderingLibrary::CreateRenderTarget2D(World, Settings.Resolution, Settings.Resolution, RTF_RG16f, FLinearColor::Black, false);
		if (State)
		{
			State->AddressX = TA_Clamp;
			State->AddressY = TA_Clamp;
			UKismetRenderingLibrary::ClearRenderTarget2D(World, State, FLinearColor::Black);
		}
	}
	if (Stepper)
	{
		Stepper->SetScalarParameterValue(Settings.TexelParameter, 1.0f / Settings.Resolution);
		Stepper->SetScalarParameterValue(Settings.DampingParameter, Settings.Damping);
		Stepper->SetScalarParameterValue(Settings.EdgeFadeParameter, Settings.EdgeFade);
	}
}

void UVeyraWaterSubsystem::Stir()
{
	UWorld* World = GetWorld();
	const UVeyraWaterSettings& Settings = *GetDefault<UVeyraWaterSettings>();
	const UVeyraGreyboxSettings& Effects = *GetDefault<UVeyraGreyboxSettings>();
	const FVeyraBattlegroundLayout& Layout = UVeyraWorldTuningSubsystem::Get().Layout;
	const FVeyraRiverShape& Shape = VeyraRiver::ShapeOf(Layout);
	const double SurfaceZ = Layout.River.SurfaceZ;
	const FVector2D Centre = Window.Origin + FVector2D(Settings.WindowSize * 0.5);

	struct FStir
	{
		double Distance;
		FLinearColor Source;
	};
	TArray<FStir> Stirs;
	TSet<TWeakObjectPtr<const AActor>> Wading;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		const APawn& Unit = **It;
		const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(&Unit);
		// Every unit this machine knows of stirs the water, seen or not: ripples are a tell for whoever is watching the
		// river (author ruling, 2026-10-12). Structures never wade.
		if (!Kind.IsSet() || Kind.GetValue() == EVeyraUnitKind::Structure)
		{
			continue;
		}
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
		const FVector Feet = Unit.GetActorLocation() - FVector(0.0, 0.0, HalfHeight);
		if (!VeyraWaterRipples::Wades(Shape, SurfaceZ, Feet))
		{
			continue;
		}
		const FVector Velocity = Unit.GetVelocity();
		const double Speed = Velocity.Size2D();
		const double Strength = VeyraWaterRipples::StrengthAt(Speed, Settings.StillStrength, Settings.MovingStrength, Settings.FullSpeed);
		const FVector2D At(Feet);
		if (const TOptional<FLinearColor> Source = VeyraWaterRipples::SourceIn(Window, Settings.WindowSize, At, Settings.SourceRadius, Strength))
		{
			Stirs.Add({ FVector2D::Distance(At, Centre), Source.GetValue() });
		}

		// Its wake, on the water at its feet, turned to its heading; and a splash if it ran in.
		Wading.Add(&Unit);
		FWader& Wader = Waders.FindOrAdd(&Unit);
		const FVector OnWater(At, SurfaceZ);
		const FRotator Heading = Speed > UE_KINDA_SMALL_NUMBER ? FRotator(0.0, Velocity.Rotation().Yaw, 0.0) : Unit.GetActorRotation();
		if (!Wader.bWading && Speed >= Settings.SplashSpeed)
		{
			if (UNiagaraComponent* Splash = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, SplashSystem, OnWater, Heading))
			{
				Splash->SetVariableLinearColor(Effects.EffectColorParameter, Settings.FoamColor);
				Splash->SetVariableFloat(Effects.EffectScaleParameter, Settings.SplashScale);
			}
			++Splashes;
		}
		Wader.bWading = true;
		UNiagaraComponent* Wake = Wader.Wake.Get();
		if (!Wake)
		{
			Wake = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, WakeSystem, OnWater, Heading, FVector::OneVector, /*bAutoDestroy*/ false);
			if (Wake)
			{
				Wake->SetVariableLinearColor(Effects.EffectColorParameter, Settings.FoamColor);
				Wake->SetVariableFloat(Effects.EffectScaleParameter, Settings.WakeScale);
			}
			Wader.Wake = Wake;
		}
		if (Wake)
		{
			Wake->SetWorldLocationAndRotation(OnWater, Heading);
			const double Range = Settings.MovingStrength - Settings.StillStrength;
			Wake->SetVariableFloat(Settings.WakeStrengthParameter, Range > 0.0 ? static_cast<float>((Strength - Settings.StillStrength) / Range) : 1.0f);
		}
	}

	// Units that left the water, or are gone, leave their wake to settle.
	for (auto It = Waders.CreateIterator(); It; ++It)
	{
		if (!Wading.Contains(It->Key))
		{
			Release(It->Value);
			It.RemoveCurrent();
		}
	}

	// The nearest the view, as many as the ripple material takes.
	Stirs.Sort([](const FStir& A, const FStir& B) { return A.Distance < B.Distance; });
	Sources.Reset();
	for (int32 Index = 0; Index < Stirs.Num() && Index < Settings.MaxSources; ++Index)
	{
		Sources.Add(Stirs[Index].Source);
	}
}

void UVeyraWaterSubsystem::Step(int32 Steps)
{
	UWorld* World = GetWorld();
	const UVeyraWaterSettings& Settings = *GetDefault<UVeyraWaterSettings>();
	for (int32 Slot = 0; Slot < UVeyraWaterSettings::SourceSlots; ++Slot)
	{
		Stepper->SetVectorParameterValue(Settings.SourceParameter(Slot), Sources.IsValidIndex(Slot) ? Sources[Slot] : FLinearColor::Transparent);
	}
	const double StepSeconds = 1.0 / Settings.StepsPerSecond;
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		// The window's move is carried once, by its first step; the rest step it where it stands.
		const FIntPoint Shift = Index == 0 ? Window.Shift : FIntPoint::ZeroValue;
		Phase = FMath::Fmod(Phase + UE_TWO_PI * Settings.RingsPerSecond * StepSeconds, UE_TWO_PI);
		Stepper->SetTextureParameterValue(Settings.StateParameter, States[Current]);
		Stepper->SetVectorParameterValue(Settings.ShiftParameter, FLinearColor(static_cast<float>(Shift.X), static_cast<float>(Shift.Y), 0.0f, 0.0f));
		Stepper->SetScalarParameterValue(Settings.PhaseParameter, static_cast<float>(Phase));
		UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, States[1 - Current], Stepper);
		Current = 1 - Current;
	}
}

void UVeyraWaterSubsystem::Release(FWader& Wader)
{
	if (UNiagaraComponent* Wake = Wader.Wake.Get())
	{
		Wake->SetAutoDestroy(true);
		Wake->Deactivate();
	}
	Wader.Wake.Reset();
}

UTextureRenderTarget2D* UVeyraWaterSubsystem::GetRipples() const
{
	return CanRipple() ? States[Current].Get() : nullptr;
}

bool UVeyraWaterSubsystem::IsWading(const AActor& Unit) const
{
	const FWader* Wader = Waders.Find(&Unit);
	return Wader && Wader->bWading;
}

UNiagaraComponent* UVeyraWaterSubsystem::FindWake(const AActor& Unit) const
{
	const FWader* Wader = Waders.Find(&Unit);
	return Wader ? Wader->Wake.Get() : nullptr;
}
