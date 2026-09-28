// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Client/VeyraClientFlow.h"

#include "Backend/VeyraBackendProtocol.h"
#include "VeyraServicesLog.h"
#include "VeyraServicesSettings.h"

namespace
{
	using VeyraBackendProtocol::ESelectState;
	using VeyraLaunchHandshake::EFailure;

	// The player routes (ADR-007 §10, ADR-010 §6–8).
	const TCHAR* const RedeemPath = TEXT("/v1/game-sessions");
	const TCHAR* const MyMatchPath = TEXT("/v1/me/match");
	const TCHAR* const MySelectPath = TEXT("/v1/me/select");
	const TCHAR* const ProfilePath = TEXT("/v1/me/profile");
	const TCHAR* const VanguardsPath = TEXT("/v1/me/vanguards");
	const TCHAR* const StarterPath = TEXT("/v1/me/starter");
	const TCHAR* const PracticePath = TEXT("/v1/practice");
	const TCHAR* const HoverPath = TEXT("/v1/me/select/hover");
	const TCHAR* const LockPath = TEXT("/v1/me/select/lock");

	FString SelectPath(const FString& SelectId)
	{
		return TEXT("/v1/me/selects/") + SelectId;
	}

	FString MatchOutcomePath(const FString& MatchId)
	{
		return TEXT("/v1/me/matches/") + MatchId;
	}

	/** The backend's error code, or "http_<status>" when it gave none. */
	FString RefusalCode(const FVeyraBackendResponse& Response)
	{
		const FString Code = VeyraBackendProtocol::ParseErrorCode(Response.Body);
		return Code.IsEmpty() ? FString::Printf(TEXT("http_%d"), Response.Status) : Code;
	}

	bool IsRefusal(const FVeyraBackendResponse& Response, const TCHAR* Code)
	{
		return !Response.IsSuccess() && VeyraBackendProtocol::ParseErrorCode(Response.Body).Equals(Code, ESearchCase::CaseSensitive);
	}
}

const TCHAR* LexToString(EVeyraClientState State)
{
	switch (State)
	{
	case EVeyraClientState::SigningIn:
		return TEXT("SigningIn");
	case EVeyraClientState::SignInFailed:
		return TEXT("SignInFailed");
	case EVeyraClientState::Loading:
		return TEXT("Loading");
	case EVeyraClientState::StarterChoice:
		return TEXT("StarterChoice");
	case EVeyraClientState::Shell:
		return TEXT("Shell");
	case EVeyraClientState::Selecting:
		return TEXT("Selecting");
	case EVeyraClientState::MatchStarting:
		return TEXT("MatchStarting");
	case EVeyraClientState::Connecting:
		return TEXT("Connecting");
	case EVeyraClientState::InMatch:
		return TEXT("InMatch");
	case EVeyraClientState::Returning:
		return TEXT("Returning");
	case EVeyraClientState::AwaitingResults:
		return TEXT("AwaitingResults");
	case EVeyraClientState::Results:
		return TEXT("Results");
	case EVeyraClientState::ReconnectOnly:
		return TEXT("ReconnectOnly");
	case EVeyraClientState::SessionEnded:
		return TEXT("SessionEnded");
	}
	return TEXT("Unknown");
}

const TCHAR* LexToString(EVeyraClientIntent Intent)
{
	switch (Intent)
	{
	case EVeyraClientIntent::ChooseStarter:
		return TEXT("ChooseStarter");
	case EVeyraClientIntent::StartPractice:
		return TEXT("StartPractice");
	case EVeyraClientIntent::HoverVanguard:
		return TEXT("HoverVanguard");
	case EVeyraClientIntent::LockVanguard:
		return TEXT("LockVanguard");
	case EVeyraClientIntent::Reconnect:
		return TEXT("Reconnect");
	case EVeyraClientIntent::ContinueFromResults:
		return TEXT("ContinueFromResults");
	case EVeyraClientIntent::Retry:
		return TEXT("Retry");
	case EVeyraClientIntent::Quit:
		return TEXT("Quit");
	}
	return TEXT("Unknown");
}

FVeyraClientFlowConfig FVeyraClientFlowConfig::FromSettings(const UVeyraServicesSettings& Settings, const FString& BuildVersion)
{
	FVeyraClientFlowConfig Config;
	Config.BuildVersion = BuildVersion;
	Config.LaunchCodeReadTimeoutSeconds = Settings.LaunchCodeReadTimeoutSeconds;
	Config.RequestAttempts = Settings.ClientRequestAttempts;
	Config.RetryIntervalSeconds = Settings.ClientRetryIntervalSeconds;
	Config.SelectPollIntervalSeconds = Settings.SelectPollIntervalSeconds;
	Config.MatchPollIntervalSeconds = Settings.MatchPollIntervalSeconds;
	Config.MatchWaitTimeoutSeconds = Settings.MatchWaitTimeoutSeconds;
	Config.ResultPollIntervalSeconds = Settings.ResultPollIntervalSeconds;
	Config.ResultWaitTimeoutSeconds = Settings.ResultWaitTimeoutSeconds;
	Config.ReconnectPollIntervalSeconds = Settings.ReconnectPollIntervalSeconds;
	return Config;
}

FVeyraClientFlow::FVeyraClientFlow(IVeyraBackendTransport& InBackend, IVeyraClientFlowHost& InHost, FVeyraClientFlowConfig InConfig)
	: Backend(InBackend)
	, Host(InHost)
	, Config(MoveTemp(InConfig))
	, Alive(MakeShared<bool>(true))
{
}

FVeyraClientFlow::~FVeyraClientFlow() = default;

void FVeyraClientFlow::Start(const FString& ConfigurationProblem)
{
	if (!ConfigurationProblem.IsEmpty())
	{
		FailSignIn(EFailure::Misconfigured, ConfigurationProblem);
		return;
	}
	bReadingLaunchCode = true;
	LaunchCodeDeadline = Host.Now() + Config.LaunchCodeReadTimeoutSeconds;
	Log(TEXT("waiting for the launch code on standard input."));
	// A launcher requests the code only now, so the code's short life starts once the game can read it.
	Host.WriteHandshake(VeyraLaunchHandshake::AwaitingLaunchCode);
	Broadcast();
}

void FVeyraClientFlow::Tick()
{
	if (bReadingLaunchCode)
	{
		TickLaunchCode();
	}

	const double Now = Host.Now();
	TArray<TFunction<void()>> Due;
	TArray<FWait> Pending = MoveTemp(Waits);
	Waits.Reset();
	for (FWait& Wait : Pending)
	{
		if (Wait.Epoch != Epoch)
		{
			continue;
		}
		if (Wait.At <= Now)
		{
			Due.Add(MoveTemp(Wait.Run));
		}
		else
		{
			Waits.Add(MoveTemp(Wait));
		}
	}
	const uint32 TickEpoch = Epoch;
	for (TFunction<void()>& Run : Due)
	{
		// An earlier wait may have changed the state, which cancels the rest.
		if (Epoch != TickEpoch)
		{
			break;
		}
		Run();
	}
}

bool FVeyraClientFlow::IsIntentAllowed(EVeyraClientState State, EVeyraClientIntent Intent)
{
	switch (Intent)
	{
	case EVeyraClientIntent::ChooseStarter:
		return State == EVeyraClientState::StarterChoice;
	case EVeyraClientIntent::StartPractice:
		return State == EVeyraClientState::Shell;
	case EVeyraClientIntent::HoverVanguard:
	case EVeyraClientIntent::LockVanguard:
		return State == EVeyraClientState::Selecting;
	case EVeyraClientIntent::Reconnect:
		return State == EVeyraClientState::ReconnectOnly;
	case EVeyraClientIntent::ContinueFromResults:
		return State == EVeyraClientState::Results;
	case EVeyraClientIntent::Retry:
	case EVeyraClientIntent::Quit:
		return true;
	}
	return false;
}

bool FVeyraClientFlow::CanIssue(EVeyraClientIntent Intent) const
{
	if (Intent == EVeyraClientIntent::Quit)
	{
		return true;
	}
	if (Intent == EVeyraClientIntent::Retry)
	{
		return !Snapshot.bBusy && Snapshot.Problem.IsSet() && Snapshot.Problem->bCanRetry;
	}
	if (Snapshot.bBusy || !IsIntentAllowed(Snapshot.State, Intent))
	{
		return false;
	}
	if (Intent == EVeyraClientIntent::HoverVanguard || Intent == EVeyraClientIntent::LockVanguard)
	{
		// A lock is permanent (Battleground Bible §15).
		const VeyraBackendProtocol::FSelectSeat* You = Snapshot.Select.FindYou();
		return Snapshot.Select.State == ESelectState::Picking && You && You->Locked.IsEmpty() && !Snapshot.AvailableVanguards.IsEmpty();
	}
	return true;
}

double FVeyraClientFlow::GetRemainingPickSeconds() const
{
	return Snapshot.State == EVeyraClientState::Selecting ? FMath::Max(0.0, Snapshot.PickEndsAt - Host.Now()) : 0.0;
}

// Signing in ------------------------------------------------------------------------------------

void FVeyraClientFlow::TickLaunchCode()
{
	FString Line;
	const EVeyraPipeRead Read = Host.PollLaunchCode(Line);
	if (Read == EVeyraPipeRead::Pending)
	{
		if (Host.Now() >= LaunchCodeDeadline)
		{
			FailSignIn(EFailure::NoLaunchCode, TEXT("no launch code arrived on standard input within LaunchCodeReadTimeoutSeconds"));
		}
		return;
	}

	bReadingLaunchCode = false;
	switch (Read)
	{
	case EVeyraPipeRead::Line:
		if (VeyraBackendProtocol::IsLaunchCode(Line))
		{
			Redeem(Line);
		}
		else
		{
			FailSignIn(EFailure::InvalidLaunchCode, TEXT("standard input did not hold a launch code"));
		}
		break;
	case EVeyraPipeRead::EndOfInput:
		FailSignIn(EFailure::NoLaunchCode, TEXT("standard input closed without a launch code"));
		break;
	case EVeyraPipeRead::TooLong:
		FailSignIn(EFailure::InvalidLaunchCode, TEXT("standard input held a line far too long to be a launch code"));
		break;
	case EVeyraPipeRead::NotAPipe:
		FailSignIn(EFailure::NoLaunchCode, TEXT("standard input is not a pipe; whoever starts the game must pass the launch code through one"));
		break;
	default:
		FailSignIn(EFailure::NoLaunchCode, TEXT("reading standard input failed"));
		break;
	}
}

void FVeyraClientFlow::Redeem(const FString& LaunchCode)
{
	Log(TEXT("redeeming the launch code."));
	// A launch code is single use, so a failed redemption is never tried again.
	Backend.Post(RedeemPath, FString(), VeyraBackendProtocol::BuildRedeemBody(LaunchCode, Config.BuildVersion),
		[this, WeakAlive = TWeakPtr<bool>(Alive)](const FVeyraBackendResponse& Response) {
			if (WeakAlive.IsValid())
			{
				OnRedeemed(Response);
			}
		});
}

void FVeyraClientFlow::OnRedeemed(const FVeyraBackendResponse& Response)
{
	if (!Response.IsSuccess())
	{
		FailSignIn(Response.IsTransient() ? EFailure::BackendUnreachable : EFailure::SignInRefused,
			TEXT("the backend did not redeem the launch code: ") + Response.Describe());
		return;
	}
	VeyraBackendProtocol::FGameSession Session;
	FString Problem;
	if (!VeyraBackendProtocol::ParseGameSession(Response.Body, Session, Problem))
	{
		FailSignIn(EFailure::BadAnswer, TEXT("the backend's answer to the launch code was not understood: ") + Problem);
		return;
	}
	GameSession = MoveTemp(Session.Token);
	Snapshot.DisplayName = Session.DisplayName;
	Host.WriteHandshake(VeyraLaunchHandshake::SignedIn);
	Log(FString::Printf(TEXT("signed in as %s."), *Session.DisplayName));
	Resume();
}

void FVeyraClientFlow::FailSignIn(EFailure Failure, const FString& Reason)
{
	bReadingLaunchCode = false;
	// The launcher waits for this line, then offers its own Retry with a new code.
	Host.WriteHandshake(VeyraLaunchHandshake::FailedLine(Failure));
	Enter(EVeyraClientState::SignInFailed);
	Snapshot.SignInFailure = Failure;
	Snapshot.Problem = FVeyraClientProblem{ VeyraLaunchHandshake::FailureCode(Failure), Reason, /*bCanRetry*/ false };
	UE_LOG(LogVeyraServices, Error, TEXT("VeyraClientFlow: signing in failed: %s."), *VeyraBackendProtocol::RedactCredentials(Reason));
	Broadcast();
}

// Where the player is ---------------------------------------------------------------------------

void FVeyraClientFlow::Resume()
{
	// A live match outranks everything, then a select in progress, then the starter choice (ADR-010 §2).
	Enter(EVeyraClientState::Loading);
	Broadcast();
	Call(EVerb::Get, MyMatchPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's match"), [this] { Resume(); });
			return;
		}
		VeyraBackendProtocol::FMyMatch Match;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMyMatch(Response.Body, Match, Problem))
		{
			ShowBadAnswer(TEXT("the player's match"), Problem, [this] { Resume(); });
			return;
		}
		if (Match.bHasMatch)
		{
			EnterReconnectOnly(Match.MatchId);
			return;
		}
		Call(EVerb::Get, MySelectPath, FString(), [this](const FVeyraBackendResponse& SelectResponse) {
			if (!SelectResponse.IsSuccess())
			{
				ShowRefusal(SelectResponse, TEXT("the player's champion select"), [this] { Resume(); });
				return;
			}
			TOptional<VeyraBackendProtocol::FSelect> Select;
			FString SelectProblem;
			if (!VeyraBackendProtocol::ParseSelect(SelectResponse.Body, Select, SelectProblem))
			{
				ShowBadAnswer(TEXT("the player's champion select"), SelectProblem, [this] { Resume(); });
				return;
			}
			if (Select.IsSet())
			{
				EnterSelecting(*Select);
				return;
			}
			LoadProfile();
		});
	});
}

void FVeyraClientFlow::LoadProfile()
{
	Call(EVerb::Get, ProfilePath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's profile"), [this] { Resume(); });
			return;
		}
		VeyraBackendProtocol::FProfile Profile;
		FString Problem;
		if (!VeyraBackendProtocol::ParseProfile(Response.Body, Profile, Problem))
		{
			ShowBadAnswer(TEXT("the player's profile"), Problem, [this] { Resume(); });
			return;
		}
		if (Profile.bTutorialCompleted)
		{
			Enter(EVeyraClientState::Shell);
			Broadcast();
			return;
		}
		Call(EVerb::Get, VanguardsPath, FString(), [this](const FVeyraBackendResponse& VanguardsResponse) {
			if (!VanguardsResponse.IsSuccess())
			{
				ShowRefusal(VanguardsResponse, TEXT("the starters"), [this] { Resume(); });
				return;
			}
			VeyraBackendProtocol::FVanguardAccess Access;
			FString AccessProblem;
			if (!VeyraBackendProtocol::ParseVanguardAccess(VanguardsResponse.Body, Access, AccessProblem))
			{
				ShowBadAnswer(TEXT("the starters"), AccessProblem, [this] { Resume(); });
				return;
			}
			Enter(EVeyraClientState::StarterChoice);
			Snapshot.Starters = MoveTemp(Access.Starters);
			Broadcast();
		});
	});
}

bool FVeyraClientFlow::ChooseStarter(const FString& VanguardId)
{
	if (!CanIssue(EVeyraClientIntent::ChooseStarter) || !Snapshot.Starters.Contains(VanguardId))
	{
		return false;
	}
	Log(FString::Printf(TEXT("choosing %s as the starter."), *VanguardId));
	SetBusy(true);
	Call(EVerb::Post, StarterPath, VeyraBackendProtocol::BuildVanguardBody(VanguardId), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (Response.IsSuccess())
		{
			Enter(EVeyraClientState::Shell);
			Broadcast();
		}
		else if (IsRefusal(Response, TEXT("already_completed")))
		{
			// An earlier attempt went through after all.
			Resume();
		}
		else
		{
			ShowRefusal(Response, TEXT("the starter"), nullptr);
		}
	});
	return true;
}

bool FVeyraClientFlow::StartPractice()
{
	if (!CanIssue(EVeyraClientIntent::StartPractice))
	{
		return false;
	}
	Log(TEXT("starting practice."));
	SetBusy(true);
	Call(EVerb::Post, PracticePath, FString(), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (IsRefusal(Response, TEXT("tutorial_required")) || IsRefusal(Response, TEXT("busy")))
		{
			// The player is somewhere else than this screen knew: find out where.
			Resume();
			return;
		}
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("practice"), nullptr);
			return;
		}
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (!VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem) || !Select.IsSet())
		{
			ShowBadAnswer(TEXT("the practice select"), Problem.IsEmpty() ? FString(TEXT("the select is null")) : Problem, [this] { Resume(); });
			return;
		}
		EnterSelecting(*Select);
	});
	return true;
}

void FVeyraClientFlow::EnterSelecting(const VeyraBackendProtocol::FSelect& Select)
{
	Enter(EVeyraClientState::Selecting);
	SelectId = Select.Id;
	Snapshot.AvailableVanguards.Reset();
	Log(FString::Printf(TEXT("in champion select %s (%s)."), *Select.Id, *Select.Mode));
	ApplySelect(Select);
	if (Snapshot.State != EVeyraClientState::Selecting)
	{
		return;
	}
	LoadAvailableVanguards();
	After(Config.SelectPollIntervalSeconds, [this] { PollSelect(); });
}

void FVeyraClientFlow::ApplySelect(const VeyraBackendProtocol::FSelect& Select)
{
	switch (Select.State)
	{
	case ESelectState::Started:
		EnterMatchStarting(Select.MatchId);
		return;
	case ESelectState::Cancelled:
		Log(FString::Printf(TEXT("champion select %s was cancelled (%s)."), *Select.Id, *Select.CancelReason));
		Enter(EVeyraClientState::Shell, Select.CancelReason);
		Broadcast();
		return;
	default:
		Snapshot.Select = Select;
		Snapshot.PickEndsAt = Host.Now() + Select.RemainingSeconds;
		Broadcast();
		return;
	}
}

void FVeyraClientFlow::LoadAvailableVanguards()
{
	Call(EVerb::Get, VanguardsPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the Vanguards the player may pick"), [this] { LoadAvailableVanguards(); });
			return;
		}
		VeyraBackendProtocol::FVanguardAccess Access;
		FString Problem;
		if (!VeyraBackendProtocol::ParseVanguardAccess(Response.Body, Access, Problem))
		{
			ShowBadAnswer(TEXT("the Vanguards the player may pick"), Problem, [this] { LoadAvailableVanguards(); });
			return;
		}
		Snapshot.AvailableVanguards = MoveTemp(Access.Available);
		Broadcast();
	});
}

void FVeyraClientFlow::PollSelect()
{
	Call(EVerb::Get, MySelectPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the champion select"), [this] { PollSelect(); });
			return;
		}
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (!VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem))
		{
			ShowBadAnswer(TEXT("the champion select"), Problem, [this] { PollSelect(); });
			return;
		}
		if (!Select.IsSet() || Select->Id != SelectId)
		{
			// The select is over.
			LearnHowSelectEnded();
			return;
		}
		ApplySelect(*Select);
		if (Snapshot.State == EVeyraClientState::Selecting)
		{
			After(Config.SelectPollIntervalSeconds, [this] { PollSelect(); });
		}
	});
}

void FVeyraClientFlow::LearnHowSelectEnded()
{
	Call(EVerb::Get, SelectPath(SelectId), FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("how the champion select ended"), [this] { LearnHowSelectEnded(); });
			return;
		}
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (!VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem) || !Select.IsSet() || Select->Id != SelectId)
		{
			ShowBadAnswer(TEXT("how the champion select ended"), Problem.IsEmpty() ? FString(TEXT("it is not the player's select")) : Problem,
				[this] { LearnHowSelectEnded(); });
			return;
		}
		ApplySelect(*Select);
		if (Snapshot.State == EVeyraClientState::Selecting)
		{
			After(Config.SelectPollIntervalSeconds, [this] { PollSelect(); });
		}
	});
}

bool FVeyraClientFlow::HoverVanguard(const FString& VanguardId)
{
	if (!CanIssue(EVeyraClientIntent::HoverVanguard) || !Snapshot.AvailableVanguards.Contains(VanguardId))
	{
		return false;
	}
	SetBusy(true);
	Call(EVerb::Put, HoverPath, VeyraBackendProtocol::BuildVanguardBody(VanguardId), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (Response.IsSuccess() && VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem) && Select.IsSet() && Select->Id == SelectId)
		{
			ApplySelect(*Select);
		}
		else if (Response.IsSuccess())
		{
			ShowBadAnswer(TEXT("the hover"), Problem.IsEmpty() ? FString(TEXT("it is not the player's select")) : Problem, nullptr);
		}
		else if (IsRefusal(Response, TEXT("not_available")))
		{
			ShowRefusal(Response, TEXT("the hover"), nullptr);
		}
		// Otherwise the select moved on, and the next read shows where.
	});
	return true;
}

bool FVeyraClientFlow::LockVanguard(const FString& VanguardId)
{
	if (!CanIssue(EVeyraClientIntent::LockVanguard) || !Snapshot.AvailableVanguards.Contains(VanguardId))
	{
		return false;
	}
	Log(FString::Printf(TEXT("locking in %s."), *VanguardId));
	SetBusy(true);
	Call(EVerb::Post, LockPath, VeyraBackendProtocol::BuildVanguardBody(VanguardId), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		TOptional<VeyraBackendProtocol::FSelect> Select;
		FString Problem;
		if (Response.IsSuccess() && VeyraBackendProtocol::ParseSelect(Response.Body, Select, Problem) && Select.IsSet() && Select->Id == SelectId)
		{
			// Once every pick is locked the backend creates the match before it answers.
			ApplySelect(*Select);
		}
		else if (Response.IsSuccess())
		{
			ShowBadAnswer(TEXT("the lock"), Problem.IsEmpty() ? FString(TEXT("it is not the player's select")) : Problem, nullptr);
		}
		else if (IsRefusal(Response, TEXT("not_available")))
		{
			ShowRefusal(Response, TEXT("the lock"), nullptr);
		}
		// Otherwise, such as a lock that an earlier attempt already made, the next read shows where the select is.
	});
	return true;
}

// The match -------------------------------------------------------------------------------------

void FVeyraClientFlow::EnterMatchStarting(const FString& MatchId)
{
	Enter(EVeyraClientState::MatchStarting);
	Snapshot.MatchId = MatchId;
	bMatchEnded = false;
	MatchDeadline = Host.Now() + Config.MatchWaitTimeoutSeconds;
	Log(FString::Printf(TEXT("match %s is starting."), *MatchId));
	Broadcast();
	PollMatch();
}

void FVeyraClientFlow::PollMatch()
{
	Call(EVerb::Get, MyMatchPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's match"), [this] { PollMatch(); });
			return;
		}
		VeyraBackendProtocol::FMyMatch Match;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMyMatch(Response.Body, Match, Problem))
		{
			ShowBadAnswer(TEXT("the player's match"), Problem, [this] { PollMatch(); });
			return;
		}
		if (!Match.bHasMatch)
		{
			// It ended before its server was ready; its outcome says why.
			bMatchEnded = true;
			EnterAwaitingResults();
			return;
		}
		Snapshot.MatchId = Match.MatchId;
		if (Match.bReady)
		{
			Connect(Match);
			return;
		}
		if (Host.Now() >= MatchDeadline)
		{
			ShowProblem(TEXT("match_not_ready"), TEXT("the match's server was not ready within MatchWaitTimeoutSeconds"), [this] {
				MatchDeadline = Host.Now() + Config.MatchWaitTimeoutSeconds;
				PollMatch();
			});
			return;
		}
		After(Config.MatchPollIntervalSeconds, [this] { PollMatch(); });
	});
}

void FVeyraClientFlow::Connect(const VeyraBackendProtocol::FMyMatch& Match)
{
	Enter(EVeyraClientState::Connecting);
	Snapshot.MatchId = Match.MatchId;
	bMatchEnded = false;
	const FString Address = FString::Printf(TEXT("%s:%d"), *Match.Host, Match.Port);
	Log(FString::Printf(TEXT("joining match %s at %s."), *Match.MatchId, *Address));
	if (!Host.TravelToMatch(Address, Match.Ticket))
	{
		ShowProblem(TEXT("travel_failed"), TEXT("the game could not travel to the match server"), nullptr);
		return;
	}
	Broadcast();
}

void FVeyraClientFlow::NotifyWorld(EVeyraClientWorld World)
{
	switch (Snapshot.State)
	{
	case EVeyraClientState::Connecting:
		if (World == EVeyraClientWorld::Match)
		{
			Enter(EVeyraClientState::InMatch);
			Log(FString::Printf(TEXT("in match %s."), *Snapshot.MatchId));
			Broadcast();
		}
		else
		{
			// The engine returns to the front end when a join fails.
			LeaveMatch(/*bEnded*/ false, TEXT("join_failed"));
		}
		break;
	case EVeyraClientState::InMatch:
		if (World == EVeyraClientWorld::FrontEnd)
		{
			LeaveMatch(/*bEnded*/ false, TEXT("connection_lost"));
		}
		break;
	case EVeyraClientState::Returning:
		if (World == EVeyraClientWorld::FrontEnd)
		{
			EnterAwaitingResults();
		}
		break;
	default:
		break;
	}
}

void FVeyraClientFlow::NotifyMatchPhase(EVeyraMatchPhase Phase)
{
	if (Phase == EVeyraMatchPhase::Ended && (Snapshot.State == EVeyraClientState::InMatch || Snapshot.State == EVeyraClientState::Connecting))
	{
		// The replicated end is not the result: the backend's verified one is (UX-15).
		LeaveMatch(/*bEnded*/ true, FString());
	}
}

void FVeyraClientFlow::NotifyConnectionFailed(const FString& Reason)
{
	if (Snapshot.State == EVeyraClientState::InMatch || Snapshot.State == EVeyraClientState::Connecting)
	{
		UE_LOG(LogVeyraServices, Warning, TEXT("VeyraClientFlow: the connection to match %s failed: %s."), *Snapshot.MatchId,
			*VeyraBackendProtocol::RedactCredentials(Reason));
		LeaveMatch(/*bEnded*/ false, Snapshot.State == EVeyraClientState::Connecting ? TEXT("join_failed") : TEXT("connection_lost"));
	}
}

void FVeyraClientFlow::LeaveMatch(bool bEnded, const FString& Notice)
{
	bMatchEnded = bEnded;
	Enter(EVeyraClientState::Returning, Notice);
	Log(FString::Printf(TEXT("leaving match %s%s."), *Snapshot.MatchId, bEnded ? TEXT(", which ended") : TEXT(", whose connection failed")));
	Host.TravelToFrontEnd();
	Broadcast();
}

void FVeyraClientFlow::EnterAwaitingResults()
{
	// A lost connection's notice stays, so the results screen can say why the player is there.
	const FString Notice = Snapshot.Notice;
	Enter(EVeyraClientState::AwaitingResults, Notice);
	ResultDeadline = Host.Now() + Config.ResultWaitTimeoutSeconds;
	Broadcast();
	PollResult();
}

void FVeyraClientFlow::PollResult()
{
	Call(EVerb::Get, MatchOutcomePath(Snapshot.MatchId), FString(), [this](const FVeyraBackendResponse& Response) {
		if (IsRefusal(Response, TEXT("match_not_found")))
		{
			ShowResults({});
			return;
		}
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the match's result"), [this] { PollResult(); });
			return;
		}
		VeyraBackendProtocol::FMatchOutcome Outcome;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMatchOutcome(Response.Body, Outcome, Problem))
		{
			ShowBadAnswer(TEXT("the match's result"), Problem, [this] { PollResult(); });
			return;
		}
		if (!Outcome.IsActive())
		{
			ShowResults(MoveTemp(Outcome));
			return;
		}
		if (!bMatchEnded)
		{
			// The player lost a match that still runs (UX-17).
			EnterReconnectOnly(Outcome.MatchId);
			return;
		}
		if (Host.Now() >= ResultDeadline)
		{
			ShowResults({});
			return;
		}
		After(Config.ResultPollIntervalSeconds, [this] { PollResult(); });
	});
}

void FVeyraClientFlow::ShowResults(TOptional<VeyraBackendProtocol::FMatchOutcome> Outcome)
{
	const FString Notice = Snapshot.Notice;
	Enter(EVeyraClientState::Results, Notice);
	if (Outcome.IsSet() && Outcome->bHasResult)
	{
		Log(FString::Printf(TEXT("match %s ended: %s, winner %s, %.1f s, as %s."), *Outcome->MatchId, *Outcome->EndReason,
			Outcome->Winner.IsEmpty() ? TEXT("none") : *Outcome->Winner, Outcome->DurationSeconds, *Outcome->VanguardId));
	}
	else if (Outcome.IsSet())
	{
		Log(FString::Printf(TEXT("match %s failed: %s."), *Outcome->MatchId, *Outcome->FailureReason));
	}
	else
	{
		Log(FString::Printf(TEXT("match %s has no verified result yet."), *Snapshot.MatchId));
	}
	Snapshot.Result = MoveTemp(Outcome);
	Broadcast();
}

bool FVeyraClientFlow::ContinueFromResults()
{
	if (!CanIssue(EVeyraClientIntent::ContinueFromResults))
	{
		return false;
	}
	Snapshot.Result.Reset();
	Resume();
	return true;
}

void FVeyraClientFlow::EnterReconnectOnly(const FString& MatchId)
{
	const FString Notice = Snapshot.Notice;
	Enter(EVeyraClientState::ReconnectOnly, Notice);
	Snapshot.MatchId = MatchId;
	bMatchEnded = false;
	Log(FString::Printf(TEXT("match %s runs; the player may only reconnect."), *MatchId));
	Broadcast();
	After(Config.ReconnectPollIntervalSeconds, [this] { PollReconnect(); });
}

void FVeyraClientFlow::PollReconnect()
{
	Call(EVerb::Get, MyMatchPath, FString(), [this](const FVeyraBackendResponse& Response) {
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's match"), [this] { PollReconnect(); });
			return;
		}
		VeyraBackendProtocol::FMyMatch Match;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMyMatch(Response.Body, Match, Problem))
		{
			ShowBadAnswer(TEXT("the player's match"), Problem, [this] { PollReconnect(); });
			return;
		}
		if (!Match.bHasMatch || Match.MatchId != Snapshot.MatchId)
		{
			// It ended without the player: show its result.
			bMatchEnded = true;
			EnterAwaitingResults();
			return;
		}
		After(Config.ReconnectPollIntervalSeconds, [this] { PollReconnect(); });
	});
}

bool FVeyraClientFlow::Reconnect()
{
	if (!CanIssue(EVeyraClientIntent::Reconnect))
	{
		return false;
	}
	Log(FString::Printf(TEXT("reconnecting to match %s."), *Snapshot.MatchId));
	SetBusy(true);
	Call(EVerb::Get, MyMatchPath, FString(), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the player's match"), nullptr);
			return;
		}
		VeyraBackendProtocol::FMyMatch Match;
		FString Problem;
		if (!VeyraBackendProtocol::ParseMyMatch(Response.Body, Match, Problem))
		{
			ShowBadAnswer(TEXT("the player's match"), Problem, nullptr);
			return;
		}
		if (!Match.bHasMatch)
		{
			bMatchEnded = true;
			EnterAwaitingResults();
		}
		else if (Match.bReady)
		{
			Connect(Match);
		}
		else
		{
			EnterMatchStarting(Match.MatchId);
		}
	});
	return true;
}

// Every state -----------------------------------------------------------------------------------

bool FVeyraClientFlow::Retry()
{
	if (!CanIssue(EVeyraClientIntent::Retry) || !RetryAction)
	{
		return false;
	}
	TFunction<void()> Step = MoveTemp(RetryAction);
	RetryAction = nullptr;
	Snapshot.Problem.Reset();
	Log(TEXT("retrying."));
	Broadcast();
	Step();
	return true;
}

bool FVeyraClientFlow::Quit()
{
	Log(TEXT("the player quits."));
	Host.QuitGame();
	return true;
}

void FVeyraClientFlow::EndSession()
{
	GameSession.Reset();
	Enter(EVeyraClientState::SessionEnded);
	Snapshot.Problem = FVeyraClientProblem{ TEXT("session_ended"), TEXT("the backend no longer accepts this game session; sign in again from the launcher"), false };
	UE_LOG(LogVeyraServices, Warning, TEXT("VeyraClientFlow: the backend ended the game session."));
	Broadcast();
}

void FVeyraClientFlow::Enter(EVeyraClientState NewState, const FString& Notice)
{
	++Epoch;
	Waits.Reset();
	RetryAction = nullptr;
	Snapshot.State = NewState;
	Snapshot.bBusy = false;
	Snapshot.Problem.Reset();
	Snapshot.Notice = Notice;
	Log(FString::Printf(TEXT("%s%s%s."), LexToString(NewState), Notice.IsEmpty() ? TEXT("") : TEXT(": "), *Notice));
}

void FVeyraClientFlow::After(double Seconds, TFunction<void()> Run)
{
	Waits.Add(FWait{ Host.Now() + Seconds, Epoch, MoveTemp(Run) });
}

void FVeyraClientFlow::Call(EVerb Verb, const FString& Path, const FString& Body, FAnswer OnAnswer, int32 Attempt)
{
	FVeyraBackendCallback OnDone = [this, WeakAlive = TWeakPtr<bool>(Alive), CallEpoch = Epoch, Verb, Path, Body, OnAnswer = MoveTemp(OnAnswer),
										  Attempt](const FVeyraBackendResponse& Response) mutable {
		if (!WeakAlive.IsValid() || CallEpoch != Epoch)
		{
			return;
		}
		if (Response.IsUnauthorized())
		{
			EndSession();
			return;
		}
		if (!Response.IsTransient())
		{
			OnAnswer(Response);
			return;
		}
		if (Attempt < Config.RequestAttempts)
		{
			After(Config.RetryIntervalSeconds, [this, Verb, Path, Body, OnAnswer = MoveTemp(OnAnswer), Attempt]() mutable {
				Call(Verb, Path, Body, MoveTemp(OnAnswer), Attempt + 1);
			});
			return;
		}
		SetBusy(false);
		ShowProblem(TEXT("backend_unreachable"), FString::Printf(TEXT("the backend did not answer %s after %d attempt(s): %s"), *Path, Attempt, *Response.Describe()),
			[this, Verb, Path, Body, OnAnswer = MoveTemp(OnAnswer)]() mutable { Call(Verb, Path, Body, MoveTemp(OnAnswer)); });
	};
	switch (Verb)
	{
	case EVerb::Get:
		Backend.Get(Path, GameSession, MoveTemp(OnDone));
		break;
	case EVerb::Post:
		Backend.Post(Path, GameSession, Body, MoveTemp(OnDone));
		break;
	case EVerb::Put:
		Backend.Put(Path, GameSession, Body, MoveTemp(OnDone));
		break;
	}
}

void FVeyraClientFlow::ShowProblem(const FString& Code, const FString& Message, TFunction<void()> RetryStep)
{
	Snapshot.Problem = FVeyraClientProblem{ Code, Message, static_cast<bool>(RetryStep) };
	RetryAction = MoveTemp(RetryStep);
	UE_LOG(LogVeyraServices, Warning, TEXT("VeyraClientFlow: problem in %s (%s): %s."), LexToString(Snapshot.State), *Code,
		*VeyraBackendProtocol::RedactCredentials(Message));
	Broadcast();
}

void FVeyraClientFlow::ShowRefusal(const FVeyraBackendResponse& Response, const TCHAR* What, TFunction<void()> RetryStep)
{
	ShowProblem(RefusalCode(Response), FString::Printf(TEXT("the backend refused %s: %s"), What, *Response.Describe()), MoveTemp(RetryStep));
}

void FVeyraClientFlow::ShowBadAnswer(const TCHAR* What, const FString& Problem, TFunction<void()> RetryStep)
{
	ShowProblem(TEXT("bad_answer"), FString::Printf(TEXT("the backend's answer about %s was not understood: %s"), What, *Problem), MoveTemp(RetryStep));
}

void FVeyraClientFlow::SetBusy(bool bBusy)
{
	if (Snapshot.bBusy != bBusy)
	{
		Snapshot.bBusy = bBusy;
		Broadcast();
	}
}

void FVeyraClientFlow::Broadcast()
{
	++Snapshot.Revision;
	Changed.Broadcast();
}

void FVeyraClientFlow::Log(const FString& Message) const
{
	UE_LOG(LogVeyraServices, Display, TEXT("VeyraClientFlow: %s"), *VeyraBackendProtocol::RedactCredentials(Message));
}
