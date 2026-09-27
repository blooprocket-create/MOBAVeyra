// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Smoke/VeyraSmokeClientSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Algo/AllOf.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Progression/VeyraProgressionTuningSubsystem.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
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
	// -VeyraSmokeKit: how many times one ability may be refused for a passing reason before the
	// script gives up.
	constexpr int32 KitCastAttemptLimit = 20;

	/** -VeyraSmokeKit: whether a refused cast may succeed if tried again a moment later. */
	bool IsPassingKitRejection(EVeyraCastRejection Rejection)
	{
		switch (Rejection)
		{
		case EVeyraCastRejection::CasterDead:
		case EVeyraCastRejection::CrowdControlled:
		case EVeyraCastRejection::Busy:
		case EVeyraCastRejection::InsufficientResource:
		case EVeyraCastRejection::TargetDead:
		case EVeyraCastRejection::OutOfRange:
		case EVeyraCastRejection::Paused:
			return true;
		default:
			return false;
		}
	}
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
	bKit = FParse::Param(FCommandLine::Get(), TEXT("VeyraSmokeKit"));
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
		if (GameState->GetPhase() == EVeyraMatchPhase::Live && bKit)
		{
			// The whole kit needs the first ultimate rank: developer levels up to it (ADR-008 §6).
			const UVeyraProgressionComponent* Progression = Controller->GetPlayerState<AVeyraPlayerState>()->FindComponentByClass<UVeyraProgressionComponent>();
			const TArray<int32>& UltimateLevels = UVeyraProgressionTuningSubsystem::Get().UltimateRankLevels;
			if (!Progression || !Progression->IsInitialized() || UltimateLevels.IsEmpty())
			{
				break;
			}
			KitLevel = UltimateLevels[0];
			Controller->RequestDeveloperLevels(KitLevel - Progression->GetLevel());
			Advance(EStep::WaitForLevels, TEXT("the match is live; asked for developer levels"));
		}
		else if (GameState->GetPhase() == EVeyraMatchPhase::Live)
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
			StartMove(*Controller, *Vanguard, CastRange);
			Advance(EStep::WaitForMove, TEXT("the match is live; ordered a move"));
		}
		break;

	case EStep::WaitForLevels:
		if (const UVeyraProgressionComponent* Progression = Controller->GetPlayerState<AVeyraPlayerState>()->FindComponentByClass<UVeyraProgressionComponent>();
			Progression && Progression->GetLevel() >= KitLevel)
		{
			for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
			{
				Controller->RequestRankUp(Slot);
			}
			Advance(EStep::WaitForRanks, TEXT("reached the ultimate's level; asked to rank every ability"));
		}
		break;

	case EStep::WaitForRanks:
	{
		const AVeyraPlayerState* Own = Controller->GetPlayerState<AVeyraPlayerState>();
		const UVeyraProgressionComponent* Progression = Own->FindComponentByClass<UVeyraProgressionComponent>();
		if (Controller->GetLastRankUpRefusal() != EVeyraRankRefusal::None)
		{
			Finish(false, FString::Printf(TEXT("the server refused a rank: %s"), LexToString(Controller->GetLastRankUpRefusal())));
		}
		else if (Controller->GetOrderRejectionCount() > 0)
		{
			Finish(false, FString::Printf(TEXT("the server refused a rank-up order: %s"), LexToString(Controller->GetLastOrderRejection())));
		}
		else if (Progression && Algo::AllOf(VeyraAbilitySlots::All, [Progression](EVeyraAbilitySlot Slot) { return Progression->GetRank(Slot) > 0; }))
		{
			// Toward the lane centre, stopping within the Vanguard's own attack range of it.
			const FVeyraVanguardDefinition* Definition = UVeyraVanguardsTuningSubsystem::FindVanguard(Own->GetVanguardId());
			CastRange = Definition ? Definition->BasicAttack.Range : 0.0;
			StartMove(*Controller, *Vanguard, CastRange);
			Advance(EStep::WaitForMove, TEXT("every ability is ranked; ordered a move"));
		}
		break;
	}

	case EStep::WaitForMove:
		if (Controller->GetOrderRejectionCount() > 0)
		{
			Finish(false, FString::Printf(TEXT("the server refused the move: %s"), LexToString(Controller->GetLastOrderRejection())));
		}
		else if (FVector::Dist2D(Vanguard->GetActorLocation(), MoveStart) >= FVector::Dist2D(MoveStart, MoveDestination) * MoveProgressFraction)
		{
			Advance(bKit ? EStep::KitCast : EStep::WaitForRange, TEXT("the Vanguard moves"));
		}
		break;

	case EStep::KitCast:
		TickKitCast(*Controller, *GameState, *Vanguard);
		break;

	case EStep::KitAttack:
	{
		const UVeyraBasicAttackComponent* Attacks = Controller->GetPlayerState<AVeyraPlayerState>()->FindComponentByClass<UVeyraBasicAttackComponent>();
		if (Attacks && Attacks->GetState().Phase == EVeyraAttackPhase::Backswing)
		{
			Advance(EStep::KitDamage, TEXT("a basic attack committed"));
		}
		else if (Controller->GetOrderRejectionCount() > KitRejectionsBefore)
		{
			// The order was refused, perhaps while crowd controlled: order it again at whoever is nearest.
			KitRejectionsBefore = Controller->GetOrderRejectionCount();
			Controller->IssueAttackOrder(FindNearestEnemyBody(*Controller, *GameState, Vanguard->GetActorLocation()));
		}
		break;
	}

	case EStep::KitDamage:
		if (HasAnEnemyTakenDamage(*Controller, *GameState))
		{
			AfterScript(TEXT("the Vanguard took its levels, ranked its kit, moved, cast Q, W, E and R, attacked, and the enemies took damage"));
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

void UVeyraSmokeClientSubsystem::StartMove(AVeyraPlayerController& Controller, const AVeyraVanguardCharacter& Vanguard, double StopDistance)
{
	// Toward the lane centre, stopping short of it so the Vanguards end in range of each other.
	MoveStart = Vanguard.GetActorLocation();
	const double StopX = FMath::Min(FMath::Abs(MoveStart.X), StopDistance * StopFromCentreFractionOfCastRange);
	MoveDestination = FVector(FMath::Sign(MoveStart.X) * StopX, MoveStart.Y, 0.0);
	Controller.IssueMoveOrder(MoveDestination);
}

AActor* UVeyraSmokeClientSubsystem::FindNearestEnemyBody(const AVeyraPlayerController& Controller, const AVeyraGameState& GameState, const FVector& From) const
{
	const AVeyraPlayerState* Self = Controller.GetPlayerState<AVeyraPlayerState>();
	AActor* Nearest = nullptr;
	double NearestDistance = TNumericLimits<double>::Max();
	for (const APlayerState* Participant : GameState.PlayerArray)
	{
		const AVeyraPlayerState* Candidate = Cast<AVeyraPlayerState>(Participant);
		APawn* Body = Candidate ? Candidate->GetPawn() : nullptr;
		if (!Self || !Body || Candidate->GetVeyraTeam() == Self->GetVeyraTeam() || !VeyraTargeting::IsAlive(Body))
		{
			continue;
		}
		const double Distance = FVector::Dist2D(From, Body->GetActorLocation());
		if (Distance < NearestDistance)
		{
			Nearest = Body;
			NearestDistance = Distance;
		}
	}
	return Nearest;
}

void UVeyraSmokeClientSubsystem::TickKitCast(AVeyraPlayerController& Controller, const AVeyraGameState& GameState, const AVeyraVanguardCharacter& Vanguard)
{
	const AVeyraPlayerState* Own = Controller.GetPlayerState<AVeyraPlayerState>();
	const UVeyraCastStateComponent* CastState = Own->FindComponentByClass<UVeyraCastStateComponent>();
	const UVeyraCooldownComponent* Cooldowns = Own->FindComponentByClass<UVeyraCooldownComponent>();
	const UVeyraAbilityLoadoutComponent* Loadout = Own->FindComponentByClass<UVeyraAbilityLoadoutComponent>();
	if (!CastState || !Cooldowns || !Loadout)
	{
		return;
	}
	// A rendering client shows a telegraph mid-cast (ADR-008 §1).
	if (CastState->GetState().Phase == EVeyraCastPhase::Windup || CastState->GetState().Phase == EVeyraCastPhase::Channel)
	{
		RequestScreenshot();
	}

	const EVeyraAbilitySlot Slot = VeyraAbilitySlots::All[KitSlotIndex];
	const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Slot);
	if (!Entry)
	{
		Finish(false, FString::Printf(TEXT("slot %d holds no ability"), KitSlotIndex));
		return;
	}
	if (bKitCastPending)
	{
		// A cast counts once the server starts its cooldown, at Commit (ADR-008 §4).
		if (Cooldowns->GetRemainingSecondsNow(Entry->Ability) > 0.0)
		{
			UE_LOG(LogVeyraSmoke, Display, TEXT("VeyraSmoke: %s committed after %d attempt(s)."), *Entry->Ability.ToString(), KitCastAttempts);
			bKitCastPending = false;
			KitCastAttempts = 0;
			if (++KitSlotIndex == UE_ARRAY_COUNT(VeyraAbilitySlots::All))
			{
				KitRejectionsBefore = Controller.GetOrderRejectionCount();
				Controller.IssueAttackOrder(FindNearestEnemyBody(Controller, GameState, Vanguard.GetActorLocation()));
				Advance(EStep::KitAttack, TEXT("cast Q, W, E and R; ordered a basic attack"));
			}
		}
		else if (Controller.GetCastRejectionCount() > KitRejectionsBefore)
		{
			const EVeyraCastRejection Rejection = Controller.GetLastCastRejection();
			if (!IsPassingKitRejection(Rejection) || KitCastAttempts >= KitCastAttemptLimit)
			{
				Finish(false, FString::Printf(TEXT("the server refused %s %d time(s), last: %s"), *Entry->Ability.ToString(), KitCastAttempts, LexToString(Rejection)));
				return;
			}
			// Refused while something passing held the Vanguard, such as another cast or crowd control: try again.
			UE_LOG(LogVeyraSmoke, Display, TEXT("VeyraSmoke: %s refused (%s); trying again."), *Entry->Ability.ToString(), LexToString(Rejection));
			bKitCastPending = false;
		}
		return;
	}
	AActor* Target = FindNearestEnemyBody(Controller, GameState, Vanguard.GetActorLocation());
	if (!Target || CastState->IsBusy())
	{
		return;
	}
	FVeyraCastTarget CastTarget;
	CastTarget.Actor = Target;
	CastTarget.bHasLocation = true;
	CastTarget.Location = Target->GetActorLocation();
	KitRejectionsBefore = Controller.GetCastRejectionCount();
	++KitCastAttempts;
	bKitCastPending = true;
	Controller.IssueCastOrder(Slot, CastTarget);
}

bool UVeyraSmokeClientSubsystem::HasAnEnemyTakenDamage(const AVeyraPlayerController& Controller, const AVeyraGameState& GameState) const
{
	const AVeyraPlayerState* Self = Controller.GetPlayerState<AVeyraPlayerState>();
	for (const APlayerState* Participant : GameState.PlayerArray)
	{
		const AVeyraPlayerState* Candidate = Cast<AVeyraPlayerState>(Participant);
		const UAbilitySystemComponent* Abilities = Candidate ? Candidate->GetAbilitySystemComponent() : nullptr;
		if (Self && Abilities && Candidate->GetVeyraTeam() != Self->GetVeyraTeam()
			&& Abilities->GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()) < Abilities->GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()))
		{
			return true;
		}
	}
	return false;
}

void UVeyraSmokeClientSubsystem::RequestScreenshot()
{
	// A rendering client shows the grey-box presentation mid-match (ADR-008 §1). The viewport saves
	// it at the end of its next frame, so the client must stay a moment (-VeyraSmokeStay=).
	if (!ScreenshotPath.IsEmpty() && !bScreenshotRequested)
	{
		bScreenshotRequested = true;
		FScreenshotRequest::RequestScreenshot(ScreenshotPath, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogVeyraSmoke, Display, TEXT("VeyraSmoke: asked for a screenshot at %s."), *ScreenshotPath);
	}
}

void UVeyraSmokeClientSubsystem::AfterHit()
{
	RequestScreenshot();
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
