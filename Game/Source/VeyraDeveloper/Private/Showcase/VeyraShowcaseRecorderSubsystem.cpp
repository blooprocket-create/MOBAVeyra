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
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Showcase/VeyraShowcaseRules.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Showcase/VeyraShowcaseSettings.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
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
	// How long a cast may go unanswered, neither committed nor refused, with its caster free again, before it is ordered
	// again, as the kit smoke does.
	constexpr double UnansweredRealSeconds = 3.0;

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
	AActor* Dummy = VeyraKitOrders::FindNearestEnemyBody(*Controller, *GameState, Vanguard->GetActorLocation());

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
		ResetSince = Now;
		SetAt.Reset();
		VeyraDevCommands::Request(*Controller, TEXT("ResetShowcase"), { FString::FromInt(++ResetToken) });
		Advance(EStep::Settle, *FString::Printf(TEXT("asked to reset the stage for slot %d"), SlotIndex));
		break;

	case EStep::Settle:
	{
		// The settle starts once the server says this reset set the stage; a stage it could not set yet is asked again.
		const VeyraShowcase::EResetAnswer Answer = VeyraShowcase::ReadResetReply(Controller->GetLastDeveloperCommandReply(), ResetToken);
		if (!SetAt.IsSet() && Answer == VeyraShowcase::EResetAnswer::Set)
		{
			SetAt = Now;
		}
		else if (!SetAt.IsSet())
		{
			if (Now - ResetSince > Settings.CastPatienceSeconds && bRecordingAttack)
			{
				FailAttack(FString::Printf(TEXT("the stage could not be reset: %s"), *Controller->GetLastDeveloperCommandReply()));
			}
			else if (Now - ResetSince > Settings.CastPatienceSeconds)
			{
				const UVeyraAbilityLoadoutComponent* Loadout = Own->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
				const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(VeyraAbilitySlots::All[SlotIndex]) : nullptr;
				SkipAbility(Entry ? Entry->Ability : FVeyraContentId(), FString::Printf(TEXT("the stage could not be reset: %s"), *Controller->GetLastDeveloperCommandReply()));
			}
			else if (Answer == VeyraShowcase::EResetAnswer::NotSet && Now - StepSince >= RetryRealSeconds)
			{
				VeyraDevCommands::Request(*Controller, TEXT("ResetShowcase"), { FString::FromInt(++ResetToken) });
				StepSince = Now;
			}
			break;
		}
		// The caster starts clean: a state the last clip left on it, as a mode its ultimate turns on, changes what its
		// abilities are. One that never ends, as a passive's, is waited out no longer than the patience.
		const UVeyraStatusComponent* Statuses = Own->FindComponentByClass<UVeyraStatusComponent>();
		const bool bClean = VeyraShowcase::IsStageClean(Statuses && !Statuses->GetLedger().Entries.IsEmpty(), Now - SetAt.GetValue(), Settings.SettleSeconds,
			Settings.StatusPatienceSeconds);
		if (bRecordingAttack && Now - SetAt.GetValue() >= Settings.SettleSeconds && bClean && Dummy)
		{
			// An attack order left from priming the dummy gives way to standing still, and the clip waits for its swing to end.
			const UVeyraBasicAttackComponent* Swings = Own->FindComponentByClass<UVeyraBasicAttackComponent>();
			if (!VeyraShowcase::MayStartAttackClip(Swings ? Swings->GetState().Phase : EVeyraAttackPhase::None))
			{
				Controller->IssueMoveOrder(Vanguard->GetActorLocation());
				break;
			}
			// The basic attack, within its own reach of the dummy first.
			const double Reach = AttackReach(*Controller);
			if (Reach > 0.0 && FVector::Dist2D(Vanguard->GetActorLocation(), Dummy->GetActorLocation()) > Reach)
			{
				const FVector Toward = (Vanguard->GetActorLocation() - Dummy->GetActorLocation()).GetSafeNormal2D();
				Controller->IssueMoveOrder(Dummy->GetActorLocation() + Toward * Reach * Settings.ApproachShareOfReach);
				Advance(EStep::Approach, TEXT("stepping within the basic attack's reach"));
				break;
			}
			OrderAttack(*Controller, *Dummy);
			break;
		}
		if (Now - SetAt.GetValue() >= Settings.SettleSeconds && bClean && Dummy)
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
	}

	case EStep::Approach:
	{
		if (bRecordingAttack)
		{
			const UVeyraBasicAttackComponent* Swings = Own->FindComponentByClass<UVeyraBasicAttackComponent>();
			const bool bIdle = VeyraShowcase::MayStartAttackClip(Swings ? Swings->GetState().Phase : EVeyraAttackPhase::None);
			if (bIdle && Dummy && FVector::Dist2D(Vanguard->GetActorLocation(), Dummy->GetActorLocation()) <= AttackReach(*Controller))
			{
				OrderAttack(*Controller, *Dummy);
			}
			else if (Now - StepSince > Settings.CastPatienceSeconds)
			{
				FailAttack(TEXT("could not step within the basic attack's reach of the dummy"));
			}
			break;
		}
		const UVeyraAbilityLoadoutComponent* Loadout = Own->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		const FVeyraLoadoutEntry* Entry = Loadout ? Loadout->FindSlot(VeyraAbilitySlots::All[SlotIndex]) : nullptr;
		if (Entry && Dummy && FVector::Dist2D(Vanguard->GetActorLocation(), Dummy->GetActorLocation()) <= VeyraKitOrders::ReachOf(Entry->Ability))
		{
			OrderCast(*Controller, *Vanguard);
		}
		else if (Now - StepSince > Settings.CastPatienceSeconds)
		{
			SkipAbility(Entry ? Entry->Ability : FVeyraContentId(), TEXT("could not step within its reach of the dummy"));
		}
		break;
	}

	case EStep::Cast:
		TickCast(*Controller);
		break;

	case EStep::Attack:
	{
		// The clip runs until the attack has swung through and the tail has passed, within its limit.
		const UVeyraBasicAttackComponent* Attacks = Own->FindComponentByClass<UVeyraBasicAttackComponent>();
		const EVeyraAttackPhase Phase = Attacks ? Attacks->GetState().Phase : EVeyraAttackPhase::None;
		FClip& Clip = Clips.Last();
		if (Phase == EVeyraAttackPhase::Backswing && !bAttackCommitted)
		{
			bAttackCommitted = true;
			Clip.CommittedAt = ServerNow();
		}
		// The clip holds one swing, from its windup until the tail after its backswing: once it has swung, the attack
		// order gives way to standing where it is, or it would swing at the dummy again when its interval passes.
		if (bAttackCommitted && Phase != EVeyraAttackPhase::Backswing && !bFirstSwingDone)
		{
			bFirstSwingDone = true;
			Controller->IssueMoveOrder(Vanguard->GetActorLocation());
		}
		if (!bFirstSwingDone)
		{
			LastBusyAt = ServerNow();
		}
		const double Since = ServerNow() - Clip.OrderedAt;
		const bool bThrough = bAttackCommitted && ServerNow() - LastBusyAt >= Settings.TailSeconds;
		if (bThrough || Since >= Settings.MaxClipSeconds)
		{
			StopCapture();
			if (!bAttackCommitted)
			{
				Clip.Failure = TEXT("the basic attack never committed");
			}
			Advance(EStep::Flush, *FString::Printf(TEXT("recorded the basic attack in %d frame(s) over %.2f s"), Clip.Frames.Num(), Since));
		}
		break;
	}

	case EStep::Prime:
	{
		// The dummy is hurt by a basic attack, as an ability that needs a target hurt a moment ago asks; once the attack
		// lands, the cast goes again at once, as a new clip.
		const UVeyraBasicAttackComponent* Attacks = Own->FindComponentByClass<UVeyraBasicAttackComponent>();
		if (Attacks && Attacks->GetState().Phase == EVeyraAttackPhase::Backswing)
		{
			OrderCast(*Controller, *Vanguard);
		}
		else if (Now - StepSince > Settings.CastPatienceSeconds)
		{
			SkipAbility(CastAbility, TEXT("the dummy could not be primed with a basic attack"));
		}
		else if (Now >= PrimeRetryAt && Dummy)
		{
			Controller->IssueAttackOrder(Dummy);
			PrimeRetryAt = Now + RetryRealSeconds * 4.0;
		}
		break;
	}

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
			NextSlot(FString::Printf(TEXT("%s %s in %d frame(s) over %.2f s"), bRehearsing ? TEXT("rehearsed") : TEXT("recorded"),
				*Clip.Ability.ToString(), Clip.Frames.Num(), Since));
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
				// A recording passes only whole: every ability recorded and every frame written.
				TArray<FString> LeftOut;
				for (const FClip& Clip : Clips)
				{
					if (!Clip.Failure.IsEmpty())
					{
						LeftOut.Add(FString::Printf(TEXT("%s (%s)"), *Clip.Ability.ToString(), *Clip.Failure));
					}
				}
				Finish(bWritten && LeftOut.IsEmpty(), !bWritten ? FString(TEXT("frames were still being written"))
					: LeftOut.IsEmpty() ? FString::Printf(TEXT("recorded %d clip(s) into %s"), Clips.Num(), *OutputDir)
					: FString::Printf(TEXT("recorded into %s without %s"), *OutputDir, *FString::Join(LeftOut, TEXT("; "))));
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
		bClipForSlot = true;
		FClip& Clip = Clips.AddDefaulted_GetRef();
		Clip.Ability = Entry->Ability;
		Clip.Slot = StaticEnum<EVeyraAbilitySlot>()->GetNameStringByValue(static_cast<int64>(Slot));
		Clip.OrderedAt = ServerNow();
		LastBusyAt = Clip.OrderedAt;
		CastSince = FPlatformTime::Seconds();
		bOrderedAgain = false;
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
	PendingSince = FPlatformTime::Seconds();
	const UVeyraCastStateComponent* CastState = Own->FindComponentByClass<UVeyraCastStateComponent>();
	CastIdBefore = CastState ? CastState->GetState().CastId : 0;
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
	if (Now - CastSince > GetDefault<UVeyraShowcaseSettings>()->CastPatienceSeconds)
	{
		SkipAbility(CastAbility, TEXT("it was never seen to commit"));
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
	// The order is taken when its cooldown starts, or when its caster is seen in a new cast of its ability: a follow-up
	// in the slot, such as a dash a mode grants, may start no cooldown of its own.
	const UVeyraCastStateComponent* CastState = Own->FindComponentByClass<UVeyraCastStateComponent>();
	const bool bNewCast = CastState && CastState->GetState().Ability == CastAbility && CastState->GetState().CastId != CastIdBefore
		&& CastState->GetState().Phase != EVeyraCastPhase::None;
	if (VeyraKitOrders::ReadyAtOf(*Own, CastAbility) > ReadyAtBefore || bNewCast)
	{
		bCastPending = false;
		Clips.Last().CommittedAt = ServerNow();
		Advance(EStep::Record, *FString::Printf(TEXT("%s committed"), *CastAbility.ToString()));
		return;
	}
	const bool bRefused = Controller.GetCastRejectionCount() > RejectionsBefore;
	if (!bRefused && Controller.GetOrderRejectionCount() <= OrderRejectionsBefore)
	{
		// Neither committed nor refused, with nothing holding the caster: the server let it go. Order it again.
		if (CastState && !CastState->IsBusy() && Now - PendingSince >= UnansweredRealSeconds)
		{
			UE_LOG(LogVeyraShowcaseRecorder, Display, TEXT("VeyraShowcase: %s went unanswered; ordering it again."), *CastAbility.ToString());
			bCastPending = false;
			bOrderedAgain = true;
			NextTryAt = Now + RetryRealSeconds;
		}
		return;
	}
	// Refused while something passing held the Vanguard, as crowd control still running from the last clip, or the
	// order itself turned away: try again shortly, for as long as the settings allow.
	const EVeyraCastRejection Rejection = Controller.GetLastCastRejection();
	// Sent again after going unanswered, and refused as cooling down: the first order was taken after all, though nothing
	// this client saw showed it. The clip has held it since that order.
	if (bRefused && bOrderedAgain && Rejection == EVeyraCastRejection::OnCooldown)
	{
		bCastPending = false;
		Clips.Last().CommittedAt = Clips.Last().OrderedAt;
		Advance(EStep::Record, *FString::Printf(TEXT("%s was taken at its first order (its retry is cooling down)"), *CastAbility.ToString()));
		return;
	}
	if (!FirstRefusedAt.IsSet())
	{
		FirstRefusedAt = Now;
	}
	const VeyraShowcase::EAfterRefusal After = bRefused ? VeyraShowcase::AfterRefusal(Rejection, bPrimed) : VeyraShowcase::EAfterRefusal::Retry;
	if (After == VeyraShowcase::EAfterRefusal::Prime)
	{
		// A target the ability needs prepared: this clip is dropped, the dummy is hurt, and the cast goes again as a new one.
		StopCapture();
		Clips.Pop();
		bClipForSlot = false;
		bPrimed = true;
		bCastPending = false;
		PrimeRetryAt = 0.0;
		Advance(EStep::Prime, *FString::Printf(TEXT("%s needs its target prepared (%s); priming the dummy"), *CastAbility.ToString(), LexToString(Rejection)));
		return;
	}
	if (After == VeyraShowcase::EAfterRefusal::Skip || Now - FirstRefusedAt.GetValue() >= GetDefault<UVeyraShowcaseSettings>()->CastPatienceSeconds)
	{
		SkipAbility(CastAbility, FString::Printf(TEXT("the server refused it: %s"), bRefused ? LexToString(Rejection) : TEXT("the order")));
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

void UVeyraShowcaseRecorderSubsystem::SkipAbility(const FVeyraContentId& Ability, const FString& Reason)
{
	StopCapture();
	bCastPending = false;
	if (!bClipForSlot)
	{
		FClip& Clip = Clips.AddDefaulted_GetRef();
		Clip.Ability = Ability;
		Clip.Slot = StaticEnum<EVeyraAbilitySlot>()->GetNameStringByValue(static_cast<int64>(VeyraAbilitySlots::All[SlotIndex]));
	}
	Clips.Last().Failure = Reason;
	UE_LOG(LogVeyraShowcaseRecorder, Warning, TEXT("VeyraShowcase: left %s out: %s"), *Clips.Last().Ability.ToString(), *Reason);
	NextSlot(FString::Printf(TEXT("left %s out"), *Clips.Last().Ability.ToString()));
}

void UVeyraShowcaseRecorderSubsystem::NextSlot(const FString& Description)
{
	bClipForSlot = false;
	bPrimed = false;
	const bool bLast = ++SlotIndex == UE_ARRAY_COUNT(VeyraAbilitySlots::All);
	if (bLast && bRehearsing)
	{
		// The rehearsal leaves nothing behind: the recording starts again from the first slot.
		bRehearsing = false;
		SlotIndex = 0;
		Clips.Reset();
		Advance(EStep::Reset, *Description);
		return;
	}
	// After the kit, its basic attack, from a stage set again.
	if (bLast)
	{
		bRecordingAttack = true;
		SlotIndex = UE_ARRAY_COUNT(VeyraAbilitySlots::All) - 1;
	}
	Advance(EStep::Reset, *Description);
}

void UVeyraShowcaseRecorderSubsystem::OrderAttack(AVeyraPlayerController& Controller, AActor& Dummy)
{
	FClip& Clip = Clips.AddDefaulted_GetRef();
	Clip.Ability = FVeyraContentId::FromText(TEXT("basic_attack")).GetValue();
	Clip.Slot = TEXT("Attack");
	Clip.OrderedAt = ServerNow();
	LastBusyAt = Clip.OrderedAt;
	bAttackCommitted = false;
	bFirstSwingDone = false;
	StartCapture();
	Controller.IssueAttackOrder(&Dummy);
	Advance(EStep::Attack, TEXT("attacking the dummy"));
}

void UVeyraShowcaseRecorderSubsystem::FailAttack(const FString& Reason)
{
	StopCapture();
	FClip& Clip = Clips.AddDefaulted_GetRef();
	Clip.Ability = FVeyraContentId::FromText(TEXT("basic_attack")).GetValue();
	Clip.Slot = TEXT("Attack");
	Clip.Failure = Reason;
	UE_LOG(LogVeyraShowcaseRecorder, Warning, TEXT("VeyraShowcase: left the basic attack out: %s"), *Reason);
	Advance(EStep::Flush, TEXT("left the basic attack out"));
}

double UVeyraShowcaseRecorderSubsystem::AttackReach(const AVeyraPlayerController& Controller) const
{
	const AVeyraPlayerState* Own = Controller.GetPlayerState<AVeyraPlayerState>();
	const FVeyraVanguardDefinition* Definition = Own ? UVeyraVanguardsTuningSubsystem::FindVanguard(Own->GetVanguardId()) : nullptr;
	return Definition ? Definition->BasicAttack.Range : 0.0;
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
	if (!Grabber || !Grabber->IsCapturingFrames() || Clips.IsEmpty() || (Step != EStep::Cast && Step != EStep::Record && Step != EStep::Attack))
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
		if (!Clip.Failure.IsEmpty())
		{
			Entry->SetStringField(TEXT("failure"), Clip.Failure);
		}
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
