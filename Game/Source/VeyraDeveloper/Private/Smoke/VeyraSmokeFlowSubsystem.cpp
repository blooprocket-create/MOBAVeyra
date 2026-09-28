// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Smoke/VeyraSmokeFlowSubsystem.h"

#include "Client/VeyraClientFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraVanguardCharacter.h"

#if WITH_VEYRA_UI
#include "Match/VeyraMatchMenu.h"
#include "Match/VeyraMatchMenuSubsystem.h"
#include "Shell/VeyraShellButton.h"
#include "Shell/VeyraShellModels.h"
#include "Shell/VeyraShellScreen.h"
#include "Shell/VeyraShellUISubsystem.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogVeyraSmokeFlow, Log, All);

namespace
{
	const TCHAR* const FlowSwitch = TEXT("VeyraSmokeFlow=");
	const TCHAR* const VanguardSwitch = TEXT("VeyraSmokeFlowVanguard=");
	const TCHAR* const ScreenshotSwitch = TEXT("VeyraSmokeFlowScreenshots=");
	// Harness settings, not gameplay: how long the whole practice script may take, how far the
	// Vanguard must walk toward the lane centre before its host ends the match, how long to wait for a
	// screenshot to be saved, and what the verified result of a practice its host ended must say
	// (ADR-010 §7).
	constexpr double FlowTimeoutRealSeconds = 300.0;
	constexpr double MoveProofDistance = 100.0;
	constexpr double ScreenshotHoldRealSeconds = 1.0;
	const TCHAR* const ExpectedEndReason = TEXT("host_ended");
	const TCHAR* const ExpectedRules = TEXT("practice");
	// The labels of the buttons the script clicks, as the shell and the menu show them.
	const TCHAR* const PlayLabel = TEXT("Play");
	const TCHAR* const PracticeLabel = TEXT("Practice");
	const TCHAR* const LockInLabel = TEXT("Lock In");
	const TCHAR* const ContinueLabel = TEXT("Continue");
	const TCHAR* const EndCustomMatchLabel = TEXT("End Custom Match");
}

bool UVeyraSmokeFlowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	FString Mode;
	return !IsRunningDedicatedServer() && FParse::Value(FCommandLine::Get(), FlowSwitch, Mode);
}

void UVeyraSmokeFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Mode;
	FParse::Value(FCommandLine::Get(), FlowSwitch, Mode);
	FParse::Value(FCommandLine::Get(), VanguardSwitch, WantedVanguard);
	FParse::Value(FCommandLine::Get(), ScreenshotSwitch, ScreenshotFolder);
	bPractice = Mode.Equals(TEXT("practice"), ESearchCase::CaseSensitive);
	StartRealTime = FPlatformTime::Seconds();
	if (!bPractice && !Mode.Equals(TEXT("join"), ESearchCase::CaseSensitive))
	{
		Finish(false, FString::Printf(TEXT("-VeyraSmokeFlow takes \"practice\" or \"join\", not \"%s\""), *Mode));
		return;
	}
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: started the %s flow script."), *Mode);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraSmokeFlowSubsystem::Tick));
}

void UVeyraSmokeFlowSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

bool UVeyraSmokeFlowSubsystem::Tick(float /*DeltaSeconds*/)
{
	if (bFinished)
	{
		return false;
	}
	UVeyraClientFlowSubsystem* FlowHost = GetGameInstance()->GetSubsystem<UVeyraClientFlowSubsystem>();
	if (!FlowHost)
	{
		Finish(false, TEXT("the game has no client flow; start it with -VeyraLaunchCode=stdin"));
		return false;
	}
	IVeyraClientIntents& Flow = FlowHost->GetClient();
	if (!bPractice)
	{
		// A script created the match before the game started, so it waits behind Reconnect (UX-17).
		if (Flow.CanIssue(EVeyraClientIntent::Reconnect))
		{
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: pressed Reconnect."));
			Flow.Reconnect();
			return false;
		}
		return true;
	}
	if (FPlatformTime::Seconds() - StartRealTime > FlowTimeoutRealSeconds)
	{
		Finish(false, FString::Printf(TEXT("timed out in %s"), LexToString(Flow.GetSnapshot().State)));
		return false;
	}
	if (FPlatformTime::Seconds() >= HoldUntil)
	{
		TickPractice(Flow);
	}
	return !bFinished;
}

void UVeyraSmokeFlowSubsystem::TickPractice(IVeyraClientIntents& Flow)
{
	const FVeyraClientSnapshot& Snapshot = Flow.GetSnapshot();
	if (Snapshot.Problem.IsSet())
	{
		Finish(false, FString::Printf(TEXT("the flow shows a problem in %s: %s (%s)"), LexToString(Snapshot.State), *Snapshot.Problem->Message, *Snapshot.Problem->Code));
		return;
	}

	switch (Snapshot.State)
	{
	case EVeyraClientState::ReconnectOnly:
		Finish(false, FString::Printf(TEXT("the player is still in match %s from an earlier run; try again once it ends"), *Snapshot.MatchId));
		break;

	case EVeyraClientState::StarterChoice:
		if (Flow.CanIssue(EVeyraClientIntent::ChooseStarter) && !Capture(TEXT("StarterChoice")))
		{
			const FString Starter = ChooseFrom(Snapshot.Starters);
			if (Starter.IsEmpty())
			{
				Finish(false, TEXT("no starter is on offer"));
				break;
			}
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: choosing %s as the starter."), *Starter);
			Click(VanguardLabel(Starter));
		}
		break;

	case EVeyraClientState::Shell:
		if (bSawResults)
		{
			Finish(true, FString::Printf(TEXT("clicked through the starter choice, Play and Practice, locked %s, ended the match from its menu as its host, ")
										 TEXT("saw its verified result and returned to the shell"),
				*LockedVanguard));
		}
		else if (bStartedPractice && !Snapshot.Notice.IsEmpty())
		{
			Finish(false, FString::Printf(TEXT("the practice select was cancelled (%s)"), *Snapshot.Notice));
		}
		else if (!bStartedPractice && Flow.CanIssue(EVeyraClientIntent::StartPractice))
		{
			if (!bOpenedPlay)
			{
				if (!Capture(TEXT("Home")) && Click(PlayLabel))
				{
					bOpenedPlay = true;
				}
			}
			else if (!Capture(TEXT("Play")) && Click(PracticeLabel))
			{
				bStartedPractice = true;
				UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: Play, Custom, Practice."));
			}
		}
		break;

	case EVeyraClientState::Selecting:
		if (Flow.CanIssue(EVeyraClientIntent::HoverVanguard))
		{
			const FString Pick = ChooseFrom(Snapshot.AvailableVanguards);
			if (Pick.IsEmpty())
			{
				Finish(false, TEXT("the wanted Vanguard is not available"));
				break;
			}
			if (Snapshot.Select.FindYou()->Hover != Pick)
			{
				UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: hovering %s with %.0f s on the timer."), *Pick, Flow.GetRemainingPickSeconds());
				Click(VanguardLabel(Pick));
			}
			else if (!Capture(TEXT("ChampionSelect")))
			{
				LockedVanguard = Pick;
				UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: locking in %s."), *Pick);
				Click(LockInLabel);
			}
		}
		break;

	case EVeyraClientState::InMatch:
		TickInMatch();
		break;

	case EVeyraClientState::Results:
		if (!bSawResults)
		{
			const TOptional<VeyraBackendProtocol::FMatchOutcome>& Result = Snapshot.Result;
			if (!Result.IsSet() || !Result->bHasResult)
			{
				Finish(false, FString::Printf(TEXT("match %s has no verified result"), *Snapshot.MatchId));
				break;
			}
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the verified result of match %s: %s, %s rules, winner %s, %.1f s, as %s, %s, %s at the end."),
				*Result->MatchId, *Result->EndReason, *Result->Rules, Result->Winner.IsEmpty() ? TEXT("none") : *Result->Winner, Result->DurationSeconds,
				*Result->VanguardId, Result->bJoined ? TEXT("joined") : TEXT("never joined"), Result->bConnectedAtEnd ? TEXT("connected") : TEXT("disconnected"));
			if (Result->EndReason != ExpectedEndReason || Result->Rules != ExpectedRules || !Result->Winner.IsEmpty() || Result->VanguardId != LockedVanguard
				|| !Result->bJoined || !Result->bConnectedAtEnd || Result->MatchId != Snapshot.MatchId)
			{
				Finish(false, TEXT("the verified result is not a host-ended practice, with no winner, of the locked Vanguard, joined and connected at the end"));
				break;
			}
			if (!Capture(TEXT("Results")) && Click(ContinueLabel))
			{
				bSawResults = true;
			}
		}
		break;

	default:
		break;
	}
}

void UVeyraSmokeFlowSubsystem::TickInMatch()
{
	const UWorld* World = GetGameInstance()->GetWorld();
	AVeyraPlayerController* Controller = Cast<AVeyraPlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
	const AVeyraGameState* GameState = World ? World->GetGameState<AVeyraGameState>() : nullptr;
	const AActor* Vanguard = Controller ? Controller->GetVanguard() : nullptr;
	if (!Controller || !GameState || !Vanguard || GameState->GetPhase() != EVeyraMatchPhase::Live)
	{
		return;
	}
	if (!bOrderedMove)
	{
		// Play a little: walk toward the lane centre.
		bOrderedMove = true;
		MoveStart = Vanguard->GetActorLocation();
		UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the practice match is live; walking toward the lane centre."));
		Controller->IssueMoveOrder(FVector(0.0, MoveStart.Y, 0.0));
		return;
	}
	if (Controller->GetOrderRejectionCount() > 0)
	{
		Finish(false, FString::Printf(TEXT("the server refused the move: %s"), LexToString(Controller->GetLastOrderRejection())));
		return;
	}
	if (bAskedToEnd)
	{
		if (Controller->GetEndCustomMatchRefusalCount() > 0)
		{
			Finish(false, FString::Printf(TEXT("the server refused to end the custom match: %s"), LexToString(Controller->GetLastEndCustomMatchRefusal())));
		}
		return;
	}
	if (FVector::Dist2D(Vanguard->GetActorLocation(), MoveStart) < MoveProofDistance)
	{
		return;
	}
#if WITH_VEYRA_UI
	// The host ends the practice match from the in-match menu (ADR-010 §4, §7), behind its confirmation.
	UVeyraMatchMenuSubsystem* Menus = GetGameInstance()->GetSubsystem<UVeyraMatchMenuSubsystem>();
	if (!Menus)
	{
		Finish(false, TEXT("the game has no in-match menu"));
		return;
	}
	if (!Menus->IsMenuOpen())
	{
		if (bOpenedMenu)
		{
			Finish(false, TEXT("the in-match menu closed before the match ended"));
			return;
		}
		// As its key does; the menu opens once its key is bound to this match's controller.
		Menus->ToggleMenu();
		bOpenedMenu = Menus->IsMenuOpen();
		if (bOpenedMenu)
		{
			UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: the Vanguard walks; opened the in-match menu."));
		}
		return;
	}
	UVeyraShellButton* EndButton = Menus->GetMenu()->FindButton(FText::FromString(EndCustomMatchLabel));
	if (!EndButton || !EndButton->GetIsEnabled())
	{
		Finish(false, TEXT("the in-match menu offers the practice match's host no End Custom Match"));
		return;
	}
	if (!bConfirmingEnd)
	{
		bConfirmingEnd = true;
		EndButton->Press();
		return;
	}
	if (Capture(TEXT("MatchMenu")))
	{
		return;
	}
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: confirmed End Custom Match."));
	bAskedToEnd = true;
	EndButton->Press();
#else
	Finish(false, TEXT("this build has no in-match menu"));
#endif
}

FString UVeyraSmokeFlowSubsystem::VanguardLabel(const FString& VanguardId)
{
#if WITH_VEYRA_UI
	// A Vanguard's button shows its name, not its content ID.
	return VeyraShellModels::NameOf(VanguardId).ToString();
#else
	return VanguardId;
#endif
}

bool UVeyraSmokeFlowSubsystem::Click(const FString& Label)
{
#if WITH_VEYRA_UI
	const UVeyraShellUISubsystem* Shell = GetGameInstance()->GetSubsystem<UVeyraShellUISubsystem>();
	UVeyraShellScreen* Screen = Shell ? Shell->GetScreen() : nullptr;
	UVeyraShellButton* Button = Screen ? Screen->FindButton(FText::FromString(Label)) : nullptr;
	if (!Button || !Button->GetIsEnabled())
	{
		Finish(false, FString::Printf(TEXT("the shell shows no enabled \"%s\" button%s"), *Label, Screen ? TEXT("") : TEXT(": it shows no screen at all")));
		return false;
	}
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: clicked \"%s\"."), *Label);
	Button->Press();
	return true;
#else
	Finish(false, TEXT("this build has no shell to click"));
	return false;
#endif
}

bool UVeyraSmokeFlowSubsystem::Capture(const TCHAR* Name)
{
	if (ScreenshotFolder.IsEmpty() || Captured.Contains(Name))
	{
		return false;
	}
	Captured.Add(Name);
	const FString Path = FPaths::Combine(ScreenshotFolder, FString::Printf(TEXT("Flow-%s.png"), Name));
	FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
	HoldUntil = FPlatformTime::Seconds() + ScreenshotHoldRealSeconds;
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: asked for a screenshot at %s."), *Path);
	return true;
}

FString UVeyraSmokeFlowSubsystem::ChooseFrom(const TArray<FString>& Offered) const
{
	if (WantedVanguard.IsEmpty())
	{
		return Offered.IsEmpty() ? FString() : Offered[0];
	}
	return Offered.Contains(WantedVanguard) ? WantedVanguard : FString();
}

void UVeyraSmokeFlowSubsystem::Finish(bool bPassed, const FString& Reason)
{
	bFinished = true;
	UE_LOG(LogVeyraSmokeFlow, Display, TEXT("VeyraSmoke: %s: %s."), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Reason);
	// The line above is the result; Game/Scripts/Smoke.ps1 reads it. The script then quits the way a
	// player would.
	if (UVeyraClientFlowSubsystem* FlowHost = GetGameInstance()->GetSubsystem<UVeyraClientFlowSubsystem>())
	{
		FlowHost->GetClient().Quit();
	}
	else
	{
		FPlatformMisc::RequestExit(/*bForce*/ false, TEXT("VeyraSmokeFlow"));
	}
}
