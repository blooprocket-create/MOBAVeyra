// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Showcase/VeyraShowcaseRecorderSubsystem.h"

#include "Algo/AllOf.h"
#include "Camera/VeyraCameraPreferences.h"
#include "Camera/VeyraCameraRig.h"
#include "Casting/VeyraCastStateComponent.h"
#include "DevCommands/VeyraDevCommands.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kit/VeyraKitOrders.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Showcase/VeyraShowcaseSettings.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraSettingsRegistry.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.h"
#include "VeyraVanguardCharacter.h"

// The frame grabber and the image writer read and write the window's pixels: client builds only.
#if WITH_VEYRA_UI
#include "Engine/GameEngine.h"
#include "FrameGrabber.h"
#include "ImagePixelData.h"
#include "ImageWriteQueue.h"
#include "ImageWriteTask.h"
#include "Modules/ModuleManager.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogVeyraShowcaseRecorder, Log, All);

namespace
{
	// Harness settings, not gameplay: between tries after a refusal, as the server turns away orders that come too
	// often; and how long the frames still in flight may take to be written once the last clip ends.
	constexpr double RetryRealSeconds = 0.25;
	constexpr double FlushRealSeconds = 30.0;

#if WITH_VEYRA_UI
	/** What a captured frame carries to where it is written: its clip, its file and its server time. */
	struct FShowcaseFrame : IFramePayload
	{
		int32 Clip = 0;
		FString File;
		double Time = 0.0;
	};
#endif
}

bool UVeyraShowcaseRecorderSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	FString Directory;
	return WITH_VEYRA_UI && !UE_BUILD_SHIPPING && FParse::Value(FCommandLine::Get(), TEXT("VeyraShowcase="), Directory) && !Directory.IsEmpty();
}

void UVeyraShowcaseRecorderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FParse::Value(FCommandLine::Get(), TEXT("VeyraShowcase="), OutputDir);
	OutputDir = FPaths::ConvertRelativePathToFull(OutputDir);
	StartRealTime = FPlatformTime::Seconds();
	StepSince = StartRealTime;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraShowcaseRecorderSubsystem::Tick));
	const TArray<FString> Problems = GetDefault<UVeyraShowcaseSettings>()->Validate();
	if (!Problems.IsEmpty())
	{
		Finish(false, FString::Printf(TEXT("the showcase settings are off: %s"), *FString::Join(Problems, TEXT("; "))));
		return;
	}
	UE_LOG(LogVeyraShowcaseRecorder, Display, TEXT("VeyraShowcase: recording into %s."), *OutputDir);
}

void UVeyraShowcaseRecorderSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
#if WITH_VEYRA_UI
	if (Grabber)
	{
		Grabber->Shutdown();
		Grabber.Reset();
	}
#endif
	Super::Deinitialize();
}

AVeyraPlayerController* UVeyraShowcaseRecorderSubsystem::GetController() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World ? Cast<AVeyraPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

AVeyraGameState* UVeyraShowcaseRecorderSubsystem::GetGameState() const
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World ? World->GetGameState<AVeyraGameState>() : nullptr;
}

double UVeyraShowcaseRecorderSubsystem::ServerNow() const
{
	const AVeyraGameState* GameState = GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : 0.0;
}

void UVeyraShowcaseRecorderSubsystem::Advance(EStep Next, const TCHAR* Description)
{
	Step = Next;
	StepSince = FPlatformTime::Seconds();
	UE_LOG(LogVeyraShowcaseRecorder, Display, TEXT("VeyraShowcase: %s."), Description);
}

void UVeyraShowcaseRecorderSubsystem::Finish(bool bDone, const FString& Reason)
{
	if (Step == EStep::Finished)
	{
		return;
	}
	StopCapture();
	Step = EStep::Finished;
	if (bDone)
	{
		UE_LOG(LogVeyraShowcaseRecorder, Display, TEXT("VeyraShowcase: DONE: %s"), *Reason);
	}
	else
	{
		UE_LOG(LogVeyraShowcaseRecorder, Error, TEXT("VeyraShowcase: FAIL: %s"), *Reason);
	}
	FPlatformMisc::RequestExit(/*bForce*/ false, TEXT("VeyraShowcase"));
}

bool UVeyraShowcaseRecorderSubsystem::Tick(float /*DeltaSeconds*/)
{
	if (Step == EStep::Finished)
	{
		return false;
	}
	const UVeyraShowcaseSettings& Settings = *GetDefault<UVeyraShowcaseSettings>();
	const double Now = FPlatformTime::Seconds();
	if (Now - StartRealTime > Settings.TimeoutSeconds)
	{
		Finish(false, FString::Printf(TEXT("timed out at step %d, slot %d"), static_cast<int32>(Step), SlotIndex));
		return false;
	}
	CaptureFrame();
	WriteCapturedFrames();

	AVeyraPlayerController* Controller = GetController();
	const AVeyraGameState* GameState = GetGameState();
	// Only a client connected to the stage's server records.
	if (!Controller || !GameState || GameState->GetNetMode() != NM_Client)
	{
		return true;
	}
	const AVeyraVanguardCharacter* Vanguard = Controller->GetVanguard();
	AVeyraPlayerState* Own = Controller->GetPlayerState<AVeyraPlayerState>();
	const UVeyraProgressionComponent* Progression = Own ? Own->FindComponentByClass<UVeyraProgressionComponent>() : nullptr;
	const UVeyraCastStateComponent* CastState = Own ? Own->FindComponentByClass<UVeyraCastStateComponent>() : nullptr;
	if (!Vanguard || !Progression || !CastState)
	{
		return true;
	}
	const AActor* Dummy = VeyraKitOrders::FindNearestEnemyBody(*Controller, *GameState, Vanguard->GetActorLocation());

	switch (Step)
	{
	case EStep::WaitForLiveMatch:
	{
		// The whole kit needs the first ultimate rank: developer levels up to it (ADR-008 §6).
		const TArray<int32>& UltimateLevels = UVeyraProgressionTuningSubsystem::Get().UltimateRankLevels;
		if (GameState->GetPhase() == EVeyraMatchPhase::Live && Progression->IsInitialized() && !UltimateLevels.IsEmpty())
		{
			KitLevel = UltimateLevels[0];
			VeyraDevCommands::Request(*Controller, TEXT("GrantLevels"), { FString::FromInt(KitLevel - Progression->GetLevel()) });
			Advance(EStep::WaitForLevels, TEXT("the match is live; asked for developer levels"));
		}
		break;
	}

	case EStep::WaitForLevels:
		if (Progression->GetLevel() >= KitLevel)
		{
			// A slot ranked already, as an R learnt from the start (ADR-031 §2), takes no point.
			for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
			{
				if (Progression->GetRank(Slot) == 0)
				{
					Controller->RequestRankUp(Slot);
				}
			}
			Advance(EStep::WaitForRanks, TEXT("reached the ultimate's level; asked to rank every ability"));
		}
		break;

	case EStep::WaitForRanks:
		if (Controller->GetLastRankUpRefusal() != EVeyraRankRefusal::None)
		{
			Finish(false, FString::Printf(TEXT("the server refused a rank: %s"), LexToString(Controller->GetLastRankUpRefusal())));
		}
		else if (Algo::AllOf(VeyraAbilitySlots::All, [Progression](EVeyraAbilitySlot Slot) { return Progression->GetRank(Slot) > 0; }))
		{
			FrameTheStage(*Controller);
			bRehearsing = Settings.bRehearse;
			Advance(EStep::Reset, bRehearsing ? TEXT("every ability is ranked; the stage is framed; rehearsing the kit")
											  : TEXT("every ability is ranked; the stage is framed"));
		}
		break;

	case EStep::Reset:
		// The last clip's frames are all read back before the next starts.
		if (HasOutstandingFrames())
		{
			break;
		}
		VeyraDevCommands::Request(*Controller, TEXT("ResetShowcase"));
		Advance(EStep::Settle, *FString::Printf(TEXT("asked to reset the stage for slot %d"), SlotIndex));
		break;

	case EStep::Settle:
		if (Now - StepSince >= Settings.SettleSeconds && Dummy)
		{
			// Within the ability's reach of the dummy before the clip starts, so it shows the cast, not a walk.
			const UVeyraAbilityLoadoutComponent* Loadout = Own->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
			const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(VeyraAbilitySlots::All[SlotIndex]) : nullptr;
			if (!Entry)
			{
				Finish(false, FString::Printf(TEXT("slot %d holds no ability"), SlotIndex));
				break;
			}
			const double Reach = VeyraKitOrders::ReachOf(Entry->Ability);
			const FVector Toward = (Vanguard->GetActorLocation() - Dummy->GetActorLocation()).GetSafeNormal2D();
			if (!VeyraKitOrders::IsForAlly(Entry->Ability) && Reach > 0.0 && FVector::Dist2D(Vanguard->GetActorLocation(), Dummy->GetActorLocation()) > Reach)
			{
				Controller->IssueMoveOrder(Dummy->GetActorLocation() + Toward * Reach * Settings.ApproachShareOfReach);
				Advance(EStep::Approach, *FString::Printf(TEXT("stepping within %s's reach"), *Entry->Ability.ToString()));
				break;
			}
			OrderCast(*Controller, *Vanguard);
		}
		break;

	case EStep::Approach:
	{
		const UVeyraAbilityLoadoutComponent* Loadout = Own->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(VeyraAbilitySlots::All[SlotIndex]) : nullptr;
		if (Entry && Dummy && FVector::Dist2D(Vanguard->GetActorLocation(), Dummy->GetActorLocation()) <= VeyraKitOrders::ReachOf(Entry->Ability))
		{
			OrderCast(*Controller, *Vanguard);
		}
		else if (Now - StepSince > Settings.CastPatienceSeconds)
		{
			Finish(false, FString::Printf(TEXT("could not step within reach of the dummy for slot %d"), SlotIndex));
		}
		break;
	}

	case EStep::Cast:
		TickCast(*Controller);
		break;

	case EStep::Record:
	{
		// The clip runs while the cast holds its caster, then for the tail, within its limit.
		if (CastState->IsBusy())
		{
			LastBusyAt = ServerNow();
		}
		const FClip& Clip = Clips.Last();
		const double Since = ServerNow() - Clip.OrderedAt;
		if ((!CastState->IsBusy() && ServerNow() - LastBusyAt >= Settings.TailSeconds) || Since >= Settings.MaxClipSeconds)
		{
			StopCapture();
			const FString Recorded = FString::Printf(TEXT("%s %s in %d frame(s) over %.2f s"), bRehearsing ? TEXT("rehearsed") : TEXT("recorded"),
				*Clip.Ability.ToString(), Clip.Frames.Num(), Since);
			const bool bLast = ++SlotIndex == UE_ARRAY_COUNT(VeyraAbilitySlots::All);
			if (bLast && bRehearsing)
			{
				// The rehearsal leaves nothing behind: the recording starts again from the first slot.
				bRehearsing = false;
				SlotIndex = 0;
				Clips.Reset();
			}
			Advance(bLast && !bRehearsing && Clips.Num() > 0 ? EStep::Flush : EStep::Reset, *Recorded);
		}
		break;
	}

	case EStep::Flush:
	{
#if WITH_VEYRA_UI
		const bool bWritten = !HasOutstandingFrames() && FModuleManager::LoadModuleChecked<IImageWriteQueueModule>(TEXT("ImageWriteQueue")).GetWriteQueue().GetNumPendingTasks() == 0;
#else
		const bool bWritten = true;
#endif
		if (bWritten || Now - StepSince > FlushRealSeconds)
		{
			if (!WriteManifest())
			{
				Finish(false, TEXT("could not write manifest.json"));
			}
			else
			{
				Finish(bWritten, bWritten ? FString::Printf(TEXT("recorded %d clip(s) into %s"), Clips.Num(), *OutputDir) : FString(TEXT("frames were still being written")));
			}
		}
		break;
	}

	default:
		break;
	}
	return true;
}

void UVeyraShowcaseRecorderSubsystem::FrameTheStage(AVeyraPlayerController& Controller)
{
	// A scripted client's cursor rests wherever the desktop left it, so the camera stops scrolling at the screen's
	// edge and follows the Vanguard instead.
	if (UVeyraSettingsSubsystem* Settings = UVeyraSettingsSubsystem::Get(this))
	{
		Settings->GetStore().Set(VeyraCameraPreferences::EdgeScroll(), VeyraSettings::Off(), /*bInLiveMatch*/ true);
	}
	if (AVeyraCameraRig* Rig = Controller.GetCameraRig())
	{
		Rig->SetMode(EVeyraCameraMode::Locked);
	}
	if (Controller.MyHUD)
	{
		Controller.MyHUD->bShowHUD = GetDefault<UVeyraShowcaseSettings>()->bShowHud;
	}
	// The engine's own notices, as of effects still preparing, would print over the clips.
	Controller.ConsoleCommand(TEXT("DisableAllScreenMessages"));
	// One frame in FrameStride then comes at a steady rate, whatever the machine could draw.
	if (IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
	{
		MaxFps->Set(static_cast<float>(GetDefault<UVeyraShowcaseSettings>()->FrameRateLimit), ECVF_SetByCode);
	}
}

void UVeyraShowcaseRecorderSubsystem::OrderCast(AVeyraPlayerController& Controller, const AVeyraVanguardCharacter& Vanguard)
{
	const AVeyraGameState* GameState = GetGameState();
	const AVeyraPlayerState* Own = Controller.GetPlayerState<AVeyraPlayerState>();
	const UVeyraAbilityLoadoutComponent* Loadout = Own ? Own->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
	const EVeyraAbilitySlot Slot = VeyraAbilitySlots::All[SlotIndex];
	const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(Slot) : nullptr;
	AActor* Aim = Entry && VeyraKitOrders::IsForAlly(Entry->Ability) ? Controller.GetVanguard()
		: (GameState ? VeyraKitOrders::FindNearestEnemyBody(Controller, *GameState, Vanguard.GetActorLocation()) : nullptr);
	if (!Entry || !Aim)
	{
		return;
	}
	if (Step != EStep::Cast)
	{
		// The clip starts with the first order, so it holds the windup.
		FClip& Clip = Clips.AddDefaulted_GetRef();
		Clip.Ability = Entry->Ability;
		Clip.Slot = StaticEnum<EVeyraAbilitySlot>()->GetNameStringByValue(static_cast<int64>(Slot));
		Clip.OrderedAt = ServerNow();
		LastBusyAt = Clip.OrderedAt;
		FirstRefusedAt.Reset();
		bAskedForResource = false;
		if (!bRehearsing)
		{
			StartCapture();
		}
		Advance(EStep::Cast, *FString::Printf(TEXT("casting %s"), *Entry->Ability.ToString()));
	}
	RejectionsBefore = Controller.GetCastRejectionCount();
	OrderRejectionsBefore = Controller.GetOrderRejectionCount();
	ReadyAtBefore = VeyraKitOrders::ReadyAtOf(*Own, Entry->Ability);
	CastAbility = Entry->Ability;
	bCastPending = true;
	Controller.IssueCastOrder(Slot, VeyraKitOrders::TargetAt(*Aim));
}

void UVeyraShowcaseRecorderSubsystem::TickCast(AVeyraPlayerController& Controller)
{
	const AVeyraPlayerState* Own = Controller.GetPlayerState<AVeyraPlayerState>();
	const AVeyraVanguardCharacter* Vanguard = Controller.GetVanguard();
	const double Now = FPlatformTime::Seconds();
	if (!Own || !Vanguard)
	{
		return;
	}
	if (!bCastPending)
	{
		if (Now >= NextTryAt)
		{
			OrderCast(Controller, *Vanguard);
		}
		return;
	}
	if (VeyraKitOrders::ReadyAtOf(*Own, CastAbility) > ReadyAtBefore)
	{
		bCastPending = false;
		Clips.Last().CommittedAt = ServerNow();
		Advance(EStep::Record, *FString::Printf(TEXT("%s committed"), *CastAbility.ToString()));
		return;
	}
	const bool bRefused = Controller.GetCastRejectionCount() > RejectionsBefore;
	if (!bRefused && Controller.GetOrderRejectionCount() <= OrderRejectionsBefore)
	{
		return;
	}
	// Refused while something passing held the Vanguard, as crowd control still running from the last clip, or the
	// order itself turned away: try again shortly, for as long as the settings allow.
	const EVeyraCastRejection Rejection = Controller.GetLastCastRejection();
	if (!FirstRefusedAt.IsSet())
	{
		FirstRefusedAt = Now;
	}
	if ((bRefused && !VeyraKitOrders::IsPassingRejection(Rejection)) || Now - FirstRefusedAt.GetValue() >= GetDefault<UVeyraShowcaseSettings>()->CastPatienceSeconds)
	{
		Finish(false, FString::Printf(TEXT("the server refused %s: %s"), *CastAbility.ToString(), bRefused ? LexToString(Rejection) : TEXT("the order")));
		return;
	}
	// A resource only effects restore may hold nothing yet: the stage's reset fills it, once for the clip.
	if (bRefused && Rejection == EVeyraCastRejection::InsufficientResource && !bAskedForResource)
	{
		VeyraDevCommands::Request(Controller, TEXT("ResetShowcase"));
		bAskedForResource = true;
	}
	bCastPending = false;
	NextTryAt = Now + RetryRealSeconds;
}

void UVeyraShowcaseRecorderSubsystem::StartCapture()
{
#if WITH_VEYRA_UI
	const UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (!GameEngine || !GameEngine->SceneViewport.IsValid())
	{
		Finish(false, TEXT("there is no game window to capture: run the recorder in a rendering -game client"));
		return;
	}
	if (!Grabber)
	{
		Grabber = MakeShared<FFrameGrabber>(GameEngine->SceneViewport.ToSharedRef(), GetDefault<UVeyraShowcaseSettings>()->CaptureSize);
	}
	Grabber->StartCapturingFrames();
	FrameCount = 0;
#endif
}

void UVeyraShowcaseRecorderSubsystem::CaptureFrame()
{
#if WITH_VEYRA_UI
	if (!Grabber || !Grabber->IsCapturingFrames() || Clips.IsEmpty() || (Step != EStep::Cast && Step != EStep::Record))
	{
		return;
	}
	if (FrameCount++ % GetDefault<UVeyraShowcaseSettings>()->FrameStride != 0)
	{
		return;
	}
	TSharedRef<FShowcaseFrame, ESPMode::ThreadSafe> Frame = MakeShared<FShowcaseFrame, ESPMode::ThreadSafe>();
	Frame->Clip = Clips.Num() - 1;
	Frame->File = FString::Printf(TEXT("%s/%04d.png"), *Clips.Last().Ability.ToString(), Clips.Last().Frames.Num());
	Frame->Time = ServerNow();
	Clips.Last().Frames.Emplace(Frame->File, Frame->Time);
	Grabber->CaptureThisFrame(Frame);
#endif
}

void UVeyraShowcaseRecorderSubsystem::WriteCapturedFrames()
{
#if WITH_VEYRA_UI
	if (!Grabber)
	{
		return;
	}
	for (FCapturedFrameData& Captured : Grabber->GetCapturedFrames())
	{
		const FShowcaseFrame* Frame = Captured.GetPayload<FShowcaseFrame>();
		if (!Frame)
		{
			continue;
		}
		TUniquePtr<FImageWriteTask> Task = MakeUnique<FImageWriteTask>();
		Task->Format = EImageFormat::PNG;
		Task->Filename = FPaths::Combine(OutputDir, Frame->File);
		Task->bOverwriteFile = true;
		Task->PixelData = MakeUnique<TImagePixelData<FColor>>(Captured.BufferSize, TArray64<FColor>(Captured.ColorBuffer));
		// The window's alpha means nothing in a picture of it.
		Task->AddPreProcessorToSetAlphaOpaque();
		FModuleManager::LoadModuleChecked<IImageWriteQueueModule>(TEXT("ImageWriteQueue")).GetWriteQueue().Enqueue(MoveTemp(Task));
	}
#endif
}

void UVeyraShowcaseRecorderSubsystem::StopCapture()
{
#if WITH_VEYRA_UI
	// The frames still being read back keep arriving over the next frames, and each tick hands them to the writer.
	if (Grabber && Grabber->IsCapturingFrames())
	{
		Grabber->StopCapturingFrames();
	}
#endif
}

bool UVeyraShowcaseRecorderSubsystem::HasOutstandingFrames() const
{
#if WITH_VEYRA_UI
	return Grabber && Grabber->HasOutstandingFrames();
#else
	return false;
#endif
}

bool UVeyraShowcaseRecorderSubsystem::WriteManifest() const
{
	const UVeyraShowcaseSettings& Settings = *GetDefault<UVeyraShowcaseSettings>();
	const AVeyraPlayerController* Controller = GetController();
	const AVeyraPlayerState* Own = Controller ? Controller->GetPlayerState<AVeyraPlayerState>() : nullptr;
	TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
	Manifest->SetNumberField(TEXT("schemaVersion"), 1);
	Manifest->SetStringField(TEXT("vanguard"), Own ? Own->GetVanguardId().ToString() : FString());
	Manifest->SetNumberField(TEXT("frameStride"), Settings.FrameStride);
	Manifest->SetNumberField(TEXT("width"), Settings.CaptureSize.X);
	Manifest->SetNumberField(TEXT("height"), Settings.CaptureSize.Y);
	TArray<TSharedPtr<FJsonValue>> ClipValues;
	for (const FClip& Clip : Clips)
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("ability"), Clip.Ability.ToString());
		Entry->SetStringField(TEXT("slot"), Clip.Slot);
		Entry->SetNumberField(TEXT("orderedAt"), Clip.OrderedAt);
		Entry->SetNumberField(TEXT("committedAt"), Clip.CommittedAt);
		TArray<TSharedPtr<FJsonValue>> FrameValues;
		for (const TPair<FString, double>& Frame : Clip.Frames)
		{
			TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
			Value->SetStringField(TEXT("file"), Frame.Key);
			Value->SetNumberField(TEXT("time"), Frame.Value);
			FrameValues.Add(MakeShared<FJsonValueObject>(Value));
		}
		Entry->SetArrayField(TEXT("frames"), FrameValues);
		ClipValues.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Manifest->SetArrayField(TEXT("clips"), ClipValues);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	return FJsonSerializer::Serialize(Manifest, Writer) && FFileHelper::SaveStringToFile(Text, *FPaths::Combine(OutputDir, TEXT("manifest.json")));
}
