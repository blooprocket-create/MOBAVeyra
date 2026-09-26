// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Replay/VeyraReplayCheckSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "VeyraGameState.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogVeyraReplayCheck, Log, All);

namespace
{
	// Harness settings, not gameplay: how long the check may take, how much faster than real time
	// the replay plays, how far a Vanguard must travel to count as moving, how close to the end
	// playback must get, and how many Vanguards the smoke match has.
	constexpr double TimeoutRealSeconds = 180.0;
	constexpr float PlaybackSpeed = 4.0f;
	constexpr double MovementThreshold = 100.0;
	constexpr double EndToleranceSeconds = 0.5;
	constexpr int32 ExpectedVanguards = 2;
	constexpr double ProgressLogRealSeconds = 2.0;

	const TCHAR* const ReplayOption = TEXT("VeyraReplayCheck=");
}

bool UVeyraReplayCheckSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	FString Name;
	return FParse::Value(FCommandLine::Get(), ReplayOption, Name) && !Name.IsEmpty();
}

void UVeyraReplayCheckSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FParse::Value(FCommandLine::Get(), ReplayOption, ReplayName);
	StartRealTime = FPlatformTime::Seconds();
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraReplayCheckSubsystem::Tick));
	UE_LOG(LogVeyraReplayCheck, Display, TEXT("VeyraReplayCheck: will play %s."), *ReplayName);
}

void UVeyraReplayCheckSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

bool UVeyraReplayCheckSubsystem::Tick(float /*DeltaSeconds*/)
{
	if (bFinished)
	{
		return false;
	}
	if (FPlatformTime::Seconds() - StartRealTime > TimeoutRealSeconds)
	{
		Finish(false, FString::Printf(TEXT("timed out; playing: %d"), bPlaying ? 1 : 0));
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance->GetWorld();
	if (!World)
	{
		return true;
	}
	if (!bStarted)
	{
		bStarted = true;
		if (!GameInstance->PlayReplay(ReplayName))
		{
			Finish(false, FString::Printf(TEXT("could not start playing %s"), *ReplayName));
			return false;
		}
		return true;
	}

	const UDemoNetDriver* Replay = World->GetDemoNetDriver();
	if (!Replay || !Replay->IsPlaying() || Replay->GetDemoTotalTime() <= 0.0f)
	{
		return true;
	}
	if (!bPlaying)
	{
		bPlaying = true;
		if (AWorldSettings* Settings = World->GetWorldSettings())
		{
			Settings->DemoPlayTimeDilation = PlaybackSpeed;
		}
		UE_LOG(LogVeyraReplayCheck, Display, TEXT("VeyraReplayCheck: playing %s, %.1f s long."), *ReplayName, Replay->GetDemoTotalTime());
	}

	// The engine advances replay time only while the world has no pauser, and a replay records the
	// match's world pause, so playback would stop for good at the first pause. A viewer clears the
	// recorded pauser and plays on; the match's own pause still arrives through the GameState, which
	// Observe checks (ADR-006 §5).
	if (AWorldSettings* Settings = World->GetWorldSettings(); Settings && Settings->GetPauserPlayerState())
	{
		bClearedRecordedPause = true;
		Settings->SetPauserPlayerState(nullptr);
	}

	Observe(*World);

	const double Now = FPlatformTime::Seconds();
	if (Now - LastProgressRealTime >= ProgressLogRealSeconds)
	{
		LastProgressRealTime = Now;
		UE_LOG(LogVeyraReplayCheck, Display, TEXT("VeyraReplayCheck: at %.2f of %.2f s; world paused %d."),
			Replay->GetDemoCurrentTime(), Replay->GetDemoTotalTime(), World->IsPaused() ? 1 : 0);
	}

	if (Replay->GetDemoCurrentTime() >= Replay->GetDemoTotalTime() - EndToleranceSeconds)
	{
		const bool bPassed = MostVanguards >= ExpectedVanguards && bSawMovement && bSawDamage && bSawPause;
		Finish(bPassed, FString::Printf(TEXT("%d Vanguard(s), movement %d, damage %d, pause %d (recorded world pause cleared %d), over %.1f s"),
			MostVanguards, bSawMovement ? 1 : 0, bSawDamage ? 1 : 0, bSawPause ? 1 : 0, bClearedRecordedPause ? 1 : 0, Replay->GetDemoTotalTime()));
		return false;
	}
	return true;
}

void UVeyraReplayCheckSubsystem::Observe(const UWorld& World)
{
	int32 Vanguards = 0;
	for (TActorIterator<AVeyraVanguardCharacter> It(&World); It; ++It)
	{
		++Vanguards;
		const FVector Location = It->GetActorLocation();
		const FVector& First = FirstLocations.FindOrAdd(TWeakObjectPtr<AActor>(*It), Location);
		bSawMovement |= FVector::Dist2D(First, Location) >= MovementThreshold;
	}
	MostVanguards = FMath::Max(MostVanguards, Vanguards);

	if (const AVeyraGameState* GameState = World.GetGameState<AVeyraGameState>())
	{
		bSawPause |= GameState->IsMatchPaused();
		for (const APlayerState* Candidate : GameState->PlayerArray)
		{
			const AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(Candidate);
			const UAbilitySystemComponent* Abilities = Participant ? Participant->GetAbilitySystemComponent() : nullptr;
			if (Abilities)
			{
				const double MaxHealth = Abilities->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute());
				bSawDamage |= MaxHealth > 0.0 && Abilities->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) < MaxHealth;
			}
		}
	}
}

void UVeyraReplayCheckSubsystem::Finish(bool bPassed, const FString& Reason)
{
	bFinished = true;
	UE_LOG(LogVeyraReplayCheck, Display, TEXT("VeyraReplayCheck: %s: %s."), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Reason);
	// The line above is the result, as for the smoke client: a clean exit on Windows returns 0.
	FPlatformMisc::RequestExit(/*bForce*/ false, TEXT("VeyraReplayCheck"));
}
