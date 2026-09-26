// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Smoke/VeyraSmokeClientSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
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
	constexpr double TimeoutRealSeconds = 180.0;
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
	FParse::Value(FCommandLine::Get(), TEXT("VeyraSmokeStay="), StaySeconds);
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
			const FVeyraTargetedDamageAbilityTuning* Ability =
				UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(UVeyraMatchTuningSubsystem::Get().DeveloperLoadout.AbilityQ);
			if (!Ability)
			{
				Finish(false, TEXT("the developer loadout's Q ability is not a targeted damage ability"));
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
			Finish(true, TEXT("the Vanguard moved, its Q ability hit the enemy, and the match paused and resumed"));
		}
		break;

	case EStep::Finished:
		break;
	}
	return Step != EStep::Finished;
}

void UVeyraSmokeClientSubsystem::AfterHit()
{
	if (bCheckPause)
	{
		GetController()->RequestDeveloperPause(true);
		Advance(EStep::WaitForPause, TEXT("the cast hit the enemy; asked for a pause"));
	}
	else
	{
		Finish(true, TEXT("the Vanguard moved, and its Q ability hit the enemy"));
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
