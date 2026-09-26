// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Smoke/VeyraSmokeClientSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraVanguardCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogVeyraSmoke, Log, All);

namespace
{
	// Harness settings, not gameplay: how long the whole script may take, and how far along its
	// move the Vanguard must get to count as moving.
	constexpr double TimeoutRealSeconds = 180.0;
	constexpr double MoveProgressFraction = 0.5;
}

bool UVeyraSmokeClientSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return FParse::Param(FCommandLine::Get(), TEXT("VeyraSmoke"));
}

void UVeyraSmokeClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bCheckPause = FParse::Param(FCommandLine::Get(), TEXT("VeyraSmokePause"));
	StartRealTime = FPlatformTime::Seconds();
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraSmokeClientSubsystem::Tick));
	UE_LOG(LogVeyraSmoke, Display, TEXT("VeyraSmoke: started%s."), bCheckPause ? TEXT(", with the pause check") : TEXT(""));
}

void UVeyraSmokeClientSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

AVeyraPlayerController* UVeyraSmokeClientSubsystem::GetController() const
{
	return Cast<AVeyraPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
}

AVeyraGameState* UVeyraSmokeClientSubsystem::GetGameState() const
{
	const UWorld* World = GetGameInstance()->GetWorld();
	return World ? World->GetGameState<AVeyraGameState>() : nullptr;
}

bool UVeyraSmokeClientSubsystem::Tick(float /*DeltaSeconds*/)
{
	if (Step == EStep::Finished)
	{
		return false;
	}
	if (FPlatformTime::Seconds() - StartRealTime > TimeoutRealSeconds)
	{
		Finish(false, FString::Printf(TEXT("timed out waiting at step %d"), static_cast<int32>(Step)));
		return false;
	}

	AVeyraPlayerController* Controller = GetController();
	const AVeyraGameState* GameState = GetGameState();
	// A client that fails to connect falls back to a local game on the default map, which would
	// pass every step below; only a networked client counts.
	if (GameState && GameState->GetNetMode() != NM_Client)
	{
		Finish(false, TEXT("the client is not connected to a server; it is running a local game"));
		return false;
	}
	const AVeyraVanguardCharacter* Vanguard = Controller ? Controller->GetVanguard() : nullptr;
	if (!Controller || !GameState || !Vanguard)
	{
		return true;
	}
	const UWorld* World = Controller->GetWorld();

	switch (Step)
	{
	case EStep::WaitForLiveMatch:
		if (GameState->GetPhase() == EVeyraMatchPhase::Live)
		{
			// Halfway to the lane centre, so the two Vanguards never meet.
			MoveStart = Vanguard->GetActorLocation();
			MoveDestination = FVector(MoveStart.X / 2.0, MoveStart.Y, 0.0);
			Controller->IssueMoveOrder(MoveDestination);
			Advance(EStep::WaitForMove, TEXT("the match is live; ordered a move"));
		}
		break;

	case EStep::WaitForMove:
		if (Controller->GetOrderRejectionCount() > 0)
		{
			Finish(false, FString::Printf(TEXT("the server refused the move: %s"), LexToString(Controller->GetLastOrderRejection())));
		}
		else if (FVector::Dist2D(Vanguard->GetActorLocation(), MoveStart) >= FVector::Dist2D(MoveStart, MoveDestination) * MoveProgressFraction)
		{
			if (bCheckPause)
			{
				Controller->RequestDeveloperPause(true);
				Advance(EStep::WaitForPause, TEXT("the Vanguard moves; asked for a pause"));
			}
			else
			{
				Finish(true, TEXT("the Vanguard moves"));
			}
		}
		break;

	case EStep::WaitForPause:
		if (World->IsPaused() && GameState->IsMatchPaused())
		{
			Controller->RequestDeveloperPause(false);
			Advance(EStep::WaitForResume, TEXT("this client's world is paused; asked to resume"));
		}
		break;

	case EStep::WaitForResume:
		if (!World->IsPaused() && !GameState->IsMatchPaused())
		{
			Finish(true, TEXT("the Vanguard moved, and the match paused and resumed"));
		}
		break;

	case EStep::Finished:
		break;
	}
	return Step != EStep::Finished;
}

void UVeyraSmokeClientSubsystem::Advance(EStep NextStep, const TCHAR* Description)
{
	UE_LOG(LogVeyraSmoke, Display, TEXT("VeyraSmoke: %s."), Description);
	Step = NextStep;
}

void UVeyraSmokeClientSubsystem::Finish(bool bPassed, const FString& Reason)
{
	Step = EStep::Finished;
	UE_LOG(LogVeyraSmoke, Display, TEXT("VeyraSmoke: %s: %s."), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Reason);
	FPlatformMisc::RequestExitWithStatus(/*bForce*/ false, bPassed ? 0 : 1);
}
