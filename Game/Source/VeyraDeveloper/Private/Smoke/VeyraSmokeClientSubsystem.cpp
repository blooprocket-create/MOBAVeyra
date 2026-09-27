// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Smoke/VeyraSmokeClientSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "UnrealClient.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogVeyraSmoke, Log, All);

namespace
{
	// Harness settings, not gameplay: how long the whole script may take, how far along its move the
	// Vanguard must get to count as moving, and how close to the lane centre each Vanguard stops, as
	// a fraction of the Q ability's cast range, so the two end in range without meeting.
	constexpr double SmokeTimeoutRealSeconds = 180.0;
	constexpr double MoveProgressFraction = 0.5;
	constexpr double StopFromCentreFractionOfCastRange = 0.25;
	// How long the paused world must stay paused before this client asks to resume. Long enough for
	// a replay, which samples a few times a second, to record the pause.
	constexpr double PauseHoldRealSeconds = 1.0;
}

bool UVeyraSmokeClientSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return FParse::Param(FCommandLine::Get(), TEXT("VeyraSmoke"));
}

void UVeyraSmokeClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bCheckPause = FParse::Param(FCommandLine::Get(), TEXT("VeyraSmokePause"));
	bEndMatch = FParse::Param(FCommandLine::Get(), TEXT("VeyraSmokeEndMatch"));
	bWaitForEnd = bEndMatch || FParse::Param(FCommandLine::Get(), TEXT("VeyraSmokeWaitForEnd"));
	FParse::Value(FCommandLine::Get(), TEXT("VeyraSmokeStay="), StaySeconds);
	FParse::Value(FCommandLine::Get(), TEXT("VeyraSmokeScreenshot="), ScreenshotPath);
	StartRealTime = FPlatformTime::Seconds();
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraSmokeClientSubsystem::Tick));
	NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UVeyraSmokeClientSubsystem::OnNetworkFailure);
	TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UVeyraSmokeClientSubsystem::OnTravelFailure);
	UE_LOG(LogVeyraSmoke, Display, TEXT("VeyraSmoke: started%s%s."), bCheckPause ? TEXT(", with the pause check") : TEXT(""),
		bEndMatch ? TEXT(", ending the match") : (bWaitForEnd ? TEXT(", until the match ends") : TEXT("")));
}

void UVeyraSmokeClientSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	Super::Deinitialize();
}

void UVeyraSmokeClientSubsystem::OnNetworkFailure(UWorld* /*World*/, UNetDriver* /*NetDriver*/, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (Step != EStep::Finished)
	{
		Finish(false, FString::Printf(TEXT("the connection failed (%s): %s"), ENetworkFailure::ToString(FailureType), *ErrorString));
	}
}

void UVeyraSmokeClientSubsystem::OnTravelFailure(UWorld* /*World*/, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (Step != EStep::Finished)
	{
		Finish(false, FString::Printf(TEXT("travel to the server failed (%s): %s"), ETravelFailure::ToString(FailureType), *ErrorString));
	}
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

const AVeyraPlayerState* UVeyraSmokeClientSubsystem::FindEnemy(const AVeyraPlayerController& Controller, const AVeyraGameState& GameState) const
{
	const AVeyraPlayerState* Self = Controller.GetPlayerState<AVeyraPlayerState>();
	for (const APlayerState* Participant : GameState.PlayerArray)
	{
		const AVeyraPlayerState* Candidate = Cast<AVeyraPlayerState>(Participant);
		if (Self && Candidate && Candidate->GetVeyraTeam() != Self->GetVeyraTeam() && Candidate->GetPawn())
		{
			return Candidate;
		}
	}
	return nullptr;
}

bool UVeyraSmokeClientSubsystem::Tick(float /*DeltaSeconds*/)
{
	if (Step == EStep::Finished)
	{
		return false;
	}
	if (FPlatformTime::Seconds() - StartRealTime > SmokeTimeoutRealSeconds)
	{
		Finish(false, FString::Printf(TEXT("timed out waiting at step %d"), static_cast<int32>(Step)));
		return false;
	}

	AVeyraPlayerController* Controller = GetController();
	const AVeyraGameState* GameState = GetGameState();
	// Only a networked client counts. A local game would pass every step below: a client that joins
	// through the handoff starts in one, and a client whose connection fails falls back to one (the
	// failure handlers fail the script then).
	if (GameState && GameState->GetNetMode() != NM_Client)
	{
		return true;
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
			// The smoke plays a Vanguard whose Q is a targeted damage ability (Smoke.ps1 asks for test_vanguard).
			const AVeyraPlayerState* Own = Controller->GetPlayerState<AVeyraPlayerState>();
			const UVeyraAbilityLoadoutComponent* Loadout = Own ? Own->FindComponentByClass<UVeyraAbilityLoadoutComponent>() : nullptr;
			const FVeyraLoadoutEntry* SlotQ = Loadout ? Loadout->FindSlot(EVeyraAbilitySlot::Q) : nullptr;
			const FVeyraTargetedDamageAbilityTuning* Ability = SlotQ ? UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(SlotQ->Ability) : nullptr;
			if (!Ability)
			{
				Finish(false, TEXT("the Vanguard's Q ability is not a targeted damage ability; run the smoke with -VeyraVanguard=test_vanguard"));
				break;
			}
			CastRange = Ability->CastRange;
			// Toward the lane centre, stopping short of it so the two Vanguards end in range of each other.
			MoveStart = Vanguard->GetActorLocation();
			const double StopX = FMath::Min(FMath::Abs(MoveStart.X), CastRange * StopFromCentreFractionOfCastRange);
			MoveDestination = FVector(FMath::Sign(MoveStart.X) * StopX, MoveStart.Y, 0.0);
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
			Advance(EStep::WaitForRange, TEXT("the Vanguard moves"));
		}
		break;

	case EStep::WaitForRange:
		if (const AVeyraPlayerState* Target = FindEnemy(*Controller, *GameState))
		{
			AActor* TargetBody = Target->GetPawn();
			if (VeyraTargeting::EdgeToEdgeDistance(*Vanguard, *TargetBody) <= CastRange)
			{
				Enemy = Target;
				Controller->IssueCastOrder(EVeyraAbilitySlot::Q, TargetBody);
				Advance(EStep::WaitForHit, TEXT("the enemy is in range; cast Q at it"));
			}
		}
		break;

	case EStep::WaitForHit:
		if (Controller->GetCastRejectionCount() > 0)
		{
			Finish(false, FString::Printf(TEXT("the server refused the cast: %s"), LexToString(Controller->GetLastCastRejection())));
		}
		else if (!Enemy.IsValid())
		{
			Finish(false, TEXT("the enemy left the match"));
		}
		else
		{
			const UAbilitySystemComponent* EnemyAbilities = Enemy->GetAbilitySystemComponent();
			if (EnemyAbilities->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) < EnemyAbilities->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()))
			{
				AfterHit();
			}
		}
		break;

	case EStep::WaitForPause:
		if (World->IsPaused() && GameState->IsMatchPaused())
		{
			PausedRealTime = FPlatformTime::Seconds();
			Advance(EStep::HoldPause, TEXT("this client's world is paused"));
		}
		break;

	case EStep::HoldPause:
		if (!World->IsPaused() || !GameState->IsMatchPaused())
		{
			Finish(false, TEXT("the match resumed before this client asked"));
		}
		else if (FPlatformTime::Seconds() - PausedRealTime >= PauseHoldRealSeconds)
		{
			Controller->RequestDeveloperPause(false);
			Advance(EStep::WaitForResume, TEXT("the world stayed paused; asked to resume"));
		}
		break;

	case EStep::WaitForResume:
		if (!World->IsPaused() && !GameState->IsMatchPaused())
		{
			AfterScript(TEXT("the Vanguard moved, its Q ability hit the enemy, and the match paused and resumed"));
		}
		break;

	case EStep::WaitToBeHit:
		// Ending the match only after the enemy's cast has landed lets both clients finish their script.
		if (const UAbilitySystemComponent* OwnAbilities = Controller->GetPlayerState<AVeyraPlayerState>()->GetAbilitySystemComponent();
			OwnAbilities->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) < OwnAbilities->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()))
		{
			Controller->RequestDeveloperEndMatch();
			Advance(EStep::WaitForEnd, TEXT("the enemy's cast hit this Vanguard; asked to end the match"));
		}
		break;

	case EStep::WaitForEnd:
		if (GameState->GetPhase() == EVeyraMatchPhase::Ended)
		{
			Finish(true, ScriptSummary + TEXT("; the match ended"));
		}
		break;

	case EStep::Finished:
		break;
	}
	return Step != EStep::Finished;
}

void UVeyraSmokeClientSubsystem::AfterHit()
{
	// A rendering client shows the grey-box presentation mid-match (ADR-008 §1). The viewport saves
	// it at the end of its next frame, so the client must stay a moment (-VeyraSmokeStay=).
	if (!ScreenshotPath.IsEmpty())
	{
		FScreenshotRequest::RequestScreenshot(ScreenshotPath, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogVeyraSmoke, Display, TEXT("VeyraSmoke: asked for a screenshot at %s."), *ScreenshotPath);
	}
	if (bCheckPause)
	{
		GetController()->RequestDeveloperPause(true);
		Advance(EStep::WaitForPause, TEXT("the cast hit the enemy; asked for a pause"));
	}
	else
	{
		AfterScript(TEXT("the Vanguard moved, and its Q ability hit the enemy"));
	}
}

void UVeyraSmokeClientSubsystem::AfterScript(const TCHAR* Summary)
{
	ScriptSummary = Summary;
	if (bEndMatch)
	{
		Advance(EStep::WaitToBeHit, TEXT("the script is done; waiting for the enemy's cast before ending the match"));
	}
	else if (bWaitForEnd)
	{
		Advance(EStep::WaitForEnd, TEXT("the script is done; waiting for the match to end"));
	}
	else
	{
		Finish(true, ScriptSummary);
	}
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
	// The line above is the result; Game/Scripts/Smoke.ps1 reads it. On Windows a clean exit cannot
	// carry a status (the engine loop returns its own exit code), and a forced exit would skip the
	// clean disconnect the server should see. A failed client quits at once; a passing one may stay.
	if (bPassed && StaySeconds > 0.0)
	{
		// Staying connected after the script, if asked, so the server can be measured.
		UE_LOG(LogVeyraSmoke, Display, TEXT("VeyraSmoke: staying connected for %g s."), StaySeconds);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) {
			FPlatformMisc::RequestExit(/*bForce*/ false, TEXT("VeyraSmoke"));
			return false;
		}), static_cast<float>(StaySeconds));
		return;
	}
	FPlatformMisc::RequestExit(/*bForce*/ false, TEXT("VeyraSmoke"));
}
