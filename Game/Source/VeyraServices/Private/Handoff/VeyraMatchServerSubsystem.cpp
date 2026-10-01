// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Handoff/VeyraMatchServerSubsystem.h"

#include "Backend/VeyraBackendProtocol.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Handoff/VeyraHandoff.h"
#include "Handoff/VeyraPipeLineReader.h"
#include "Handoff/VeyraServerAssignment.h"
#include "Join/VeyraMatchHostSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "Modules/ModuleManager.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "VeyraServicesLog.h"
#include "VeyraServicesSettings.h"

bool UVeyraMatchServerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!IsRunningDedicatedServer())
	{
		return false;
	}
	FString Channel;
	return UE_BUILD_SHIPPING || FParse::Value(FCommandLine::Get(), VeyraHandoff::AssignmentSwitch, Channel);
}

void UVeyraMatchServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The assignment's side sizes are checked against the Match tuning.
	Collection.InitializeDependency<UVeyraMatchTuningSubsystem>();
	Host = Collection.InitializeDependency<UVeyraMatchHostSubsystem>();
	// Loaded before the map, so no module appears while the match replicates.
	FModuleManager::Get().LoadModuleChecked(TEXT("HTTP"));

	if (const FString Problem = TakeAssignment(*Host); !Problem.IsEmpty())
	{
		Fail(Problem);
		return;
	}
	AcceptingHandle = Host->OnAcceptingPlayers.AddUObject(this, &UVeyraMatchServerSubsystem::OnAcceptingPlayers);
	EndedHandle = Host->OnMatchEnded.AddUObject(this, &UVeyraMatchServerSubsystem::OnMatchEnded);
}

void UVeyraMatchServerSubsystem::Deinitialize()
{
	if (Host)
	{
		Host->OnAcceptingPlayers.Remove(AcceptingHandle);
		Host->OnMatchEnded.Remove(EndedHandle);
	}
	FTSTicker::GetCoreTicker().RemoveTicker(RetryTicker);
	FTSTicker::GetCoreTicker().RemoveTicker(QuitTicker);
	Super::Deinitialize();
}

FString UVeyraMatchServerSubsystem::TakeAssignment(UVeyraMatchHostSubsystem& MatchHost)
{
	FString Channel;
	if (!FParse::Value(FCommandLine::Get(), VeyraHandoff::AssignmentSwitch, Channel))
	{
		return TEXT("a Shipping match server needs its assignment; start it with -VeyraAssignment=stdin");
	}
	if (!Channel.Equals(VeyraHandoff::StandardInput, ESearchCase::CaseSensitive))
	{
		return TEXT("-VeyraAssignment accepts only \"stdin\"; the assignment holds a credential, which never goes on a command line");
	}
	if (FParse::Param(FCommandLine::Get(), VeyraHandoff::ConsoleFromStandardInputSwitch))
	{
		return TEXT("-cmdstdin would run the assignment as a console command and log it; start the server without it");
	}
	const UVeyraServicesSettings& Settings = *GetDefault<UVeyraServicesSettings>();
	if (const TArray<FString> Problems = Settings.Validate(); !Problems.IsEmpty())
	{
		return TEXT("the Veyra Services settings are invalid: ") + FString::Join(Problems, TEXT("; "));
	}
	FString SchemaText;
	if (const TArray<FString> Problems = VeyraServerAssignment::ReadSchema(SchemaText); !Problems.IsEmpty())
	{
		return FString::Join(Problems, TEXT("; "));
	}

	FString Line;
	if (const FString Problem = WaitForLine(Line); !Problem.IsEmpty())
	{
		return Problem;
	}
	FVeyraServerAssignment Assignment;
	if (const TArray<FString> Problems = VeyraServerAssignment::Parse(Line, SchemaText, Assignment); !Problems.IsEmpty())
	{
		return TEXT("the assignment is invalid: ") + FString::Join(Problems, TEXT("; "));
	}
	MatchId = Assignment.Match.MatchId;
	if (const TArray<FString> Problems = MatchHost.SetAssignment(MoveTemp(Assignment.Match)); !Problems.IsEmpty())
	{
		return TEXT("this server cannot host the assignment: ") + FString::Join(Problems, TEXT("; "));
	}
	Backend = MakeUnique<FVeyraBackendClient>(Assignment.BackendUrl, Settings.RequestTimeoutSeconds);
	ServerCredential = MoveTemp(Assignment.ServerCredential);
	UE_LOG(LogVeyraServices, Display, TEXT("VeyraHandoff: took the assignment for match %s; reporting to %s."), *MatchId, *Backend->GetBaseUrl());
	return FString();
}

FString UVeyraMatchServerSubsystem::WaitForLine(FString& OutLine)
{
	// The game mode reads the roster when the first map loads, so the server waits here, before it.
	const UVeyraServicesSettings& Settings = *GetDefault<UVeyraServicesSettings>();
	FVeyraPipeLineReader Reader = FVeyraPipeLineReader::ForStandardInput();
	const double Deadline = FPlatformTime::Seconds() + Settings.AssignmentReadTimeoutSeconds;
	UE_LOG(LogVeyraServices, Display, TEXT("VeyraHandoff: waiting for the assignment on standard input."));
	for (;;)
	{
		switch (Reader.Poll(OutLine))
		{
		case EVeyraPipeRead::Line:
			return FString();
		case EVeyraPipeRead::Pending:
			if (FPlatformTime::Seconds() >= Deadline)
			{
				return TEXT("no assignment arrived on standard input within AssignmentReadTimeoutSeconds");
			}
			FPlatformProcess::Sleep(Settings.AssignmentPollIntervalSeconds);
			break;
		case EVeyraPipeRead::EndOfInput:
			return TEXT("standard input closed without an assignment");
		case EVeyraPipeRead::TooLong:
			return TEXT("the assignment is longer than a line may be");
		case EVeyraPipeRead::NotAPipe:
			return TEXT("standard input is not a pipe; the allocator must pass the assignment through one");
		default:
			return TEXT("reading standard input failed");
		}
	}
}

void UVeyraMatchServerSubsystem::OnAcceptingPlayers()
{
	if (bReadySent)
	{
		return;
	}
	bReadySent = true;
	// The route takes an empty JSON object.
	Report(TEXT("ready"), FString::Printf(TEXT("/v1/server/matches/%s/ready"), *MatchId), TEXT("{}"), 1,
		[this](bool bReported) {
			if (!bReported)
			{
				Fail(TEXT("the backend did not take the ready report"));
			}
		});
}

void UVeyraMatchServerSubsystem::OnMatchEnded(const FVeyraMatchResult& Result)
{
	if (bResultSent)
	{
		return;
	}
	bResultSent = true;
	const double EndedAt = FPlatformTime::Seconds();
	UE_LOG(LogVeyraServices, Display, TEXT("VeyraHandoff: match %s ended (%s after %.1f s); reporting the result."), *MatchId,
		LexToString(Result.EndReason), Result.DurationSeconds);
	Report(TEXT("result"), FString::Printf(TEXT("/v1/server/matches/%s/result"), *MatchId), VeyraBackendProtocol::BuildResultBody(Result), 1,
		[this, EndedAt](bool bReported) {
			if (!bReported)
			{
				Fail(TEXT("the backend did not take the result"));
				return;
			}
			// Its players watch the match end before they leave for the results (ADR-020 §1).
			const double Left = FMath::Max(0.0, EndedAt + UVeyraMatchTuningSubsystem::Get().Ending.ShowSeconds - FPlatformTime::Seconds());
			UE_LOG(LogVeyraServices, Display, TEXT("VeyraHandoff: the result is in; the match stays up %.1f s while its players watch the end."), Left);
			QuitTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float /*DeltaSeconds*/) {
				UE_LOG(LogVeyraServices, Display, TEXT("VeyraHandoff: the match server's work is done; quitting."));
				FPlatformMisc::RequestExit(/*bForce*/ false, TEXT("VeyraHandoff"));
				return false;
			}), static_cast<float>(Left));
		});
}

void UVeyraMatchServerSubsystem::Report(const FString& What, const FString& Path, const FString& Body, int32 Attempt, TFunction<void(bool)> OnDone)
{
	Backend->Post(Path, ServerCredential, Body,
		[WeakThis = TWeakObjectPtr<UVeyraMatchServerSubsystem>(this), What, Path, Body, Attempt, OnDone](const FVeyraBackendResponse& Response) {
			UVeyraMatchServerSubsystem* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			if (Response.IsSuccess())
			{
				UE_LOG(LogVeyraServices, Display, TEXT("VeyraHandoff: reported %s for match %s."), *What, *This->MatchId);
				OnDone(true);
				return;
			}
			const UVeyraServicesSettings& Settings = *GetDefault<UVeyraServicesSettings>();
			if (!Response.IsTransient() || Attempt >= Settings.ReportAttempts)
			{
				UE_LOG(LogVeyraServices, Error, TEXT("VeyraHandoff: the %s report failed after %d attempt(s): %s."), *What, Attempt, *Response.Describe());
				// What the backend refused, so the refusal can be read; a report carries no ticket or credential.
				if (!Response.IsTransient())
				{
					UE_LOG(LogVeyraServices, Warning, TEXT("VeyraHandoff: the refused %s report: %s"), *What, *Body);
				}
				OnDone(false);
				return;
			}
			UE_LOG(LogVeyraServices, Warning, TEXT("VeyraHandoff: the %s report failed (%s); trying again in %g s."), *What, *Response.Describe(),
				Settings.ReportRetryIntervalSeconds);
			This->RetryTicker = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateWeakLambda(This, [This, What, Path, Body, Attempt, OnDone](float) {
					This->RetryTicker.Reset();
					This->Report(What, Path, Body, Attempt + 1, OnDone);
					return false;
				}),
				Settings.ReportRetryIntervalSeconds);
		});
}

void UVeyraMatchServerSubsystem::Fail(const FString& Reason)
{
	UE_LOG(LogVeyraServices, Error, TEXT("VeyraHandoff: FAIL: %s."), *VeyraBackendProtocol::RedactCredentials(Reason));
	FPlatformMisc::RequestExitWithStatus(/*bForce*/ false, VeyraHandoff::FailedExitCode, TEXT("VeyraHandoff"));
}
