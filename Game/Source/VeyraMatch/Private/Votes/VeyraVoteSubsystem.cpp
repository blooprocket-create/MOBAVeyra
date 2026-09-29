// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Votes/VeyraVoteSubsystem.h"

#include "Absence/VeyraAbsenceSubsystem.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Tuning/VeyraMatchTuningSubsystem.h"
#include "VeyraGameState.h"
#include "VeyraMatchLog.h"
#include "VeyraPlayerState.h"

void UVeyraVoteSubsystem::Deinitialize()
{
	Stop();
	Super::Deinitialize();
}

void UVeyraVoteSubsystem::Start()
{
	if (bRunning)
	{
		return;
	}
	bRunning = true;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVeyraVoteSubsystem::Tick));
}

void UVeyraVoteSubsystem::Stop()
{
	if (!bRunning)
	{
		return;
	}
	bRunning = false;
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Box.Reset();
	IntermissionEndsAt.Reset();
	Publish({}, RealNow());
}

EVeyraVoteRefusal UVeyraVoteSubsystem::Request(const AVeyraPlayerState& Requester, EVeyraVoteKind Kind)
{
	const AVeyraGameState* GameState = GetWorld()->GetGameState<AVeyraGameState>();
	if (!bRunning || !GameState)
	{
		return EVeyraVoteRefusal::NotLive;
	}
	if (Requester.IsABot())
	{
		return EVeyraVoteRefusal::NotAVoter;
	}
	const FVeyraVotesTuning& Tuning = UVeyraMatchTuningSubsystem::Get().Votes;
	VeyraVotes::FStartContext Context;
	Context.Kind = Kind;
	Context.Team = Requester.GetVeyraTeam();
	Context.MatchClock = GameState->GetMatchClockSeconds();
	Context.Now = RealNow();
	Context.bStandard = GameState->GetMatchRules() == EVeyraMatchRules::Standard;
	Context.bLive = GameState->GetPhase() == EVeyraMatchPhase::Live;
	Context.bPaused = GameState->IsMatchPaused();
	Context.bVoteOpen = Box.IsSet();
	const EVeyraVoteRefusal Refusal = VeyraVotes::CheckStart(Context, Cooldowns, Tuning);
	if (Refusal != EVeyraVoteRefusal::None)
	{
		return Refusal;
	}
	FVeyraBallotBox& Opened = Box.Emplace();
	Opened.Kind = Kind;
	Opened.Team = VeyraVotes::IsUnanimous(Kind) ? EVeyraTeam::None : Requester.GetVeyraTeam();
	Opened.EndsAt = Context.Now + VeyraVotes::WindowSeconds(Kind, Tuning);
	// Starting a vote is voting for it.
	Opened.Ballots.Add(Requester.GetPlayerId(), true);
	UE_LOG(LogVeyraMatch, Log, TEXT("%s started a %s vote."), *Requester.GetPlayerName(), *StaticEnum<EVeyraVoteKind>()->GetNameStringByValue(static_cast<int64>(Kind)));
	Tick(0.0f);
	return EVeyraVoteRefusal::None;
}

EVeyraVoteRefusal UVeyraVoteSubsystem::CastBallot(const AVeyraPlayerState& Voter, bool bYes)
{
	if (!Box.IsSet())
	{
		return EVeyraVoteRefusal::NoVote;
	}
	const TArray<FVeyraVoter> Voters = GatherVoters();
	const FVeyraVoter* Found = Voters.FindByPredicate([&Voter](const FVeyraVoter& Candidate) { return Candidate.PlayerId == Voter.GetPlayerId(); });
	if (!Found || Found->bBot || !VeyraVotes::IsVoter(*Box, *Found))
	{
		return EVeyraVoteRefusal::NotAVoter;
	}
	if (Box->Ballots.Contains(Voter.GetPlayerId()))
	{
		return EVeyraVoteRefusal::AlreadyVoted;
	}
	Box->Ballots.Add(Voter.GetPlayerId(), bYes);
	UE_LOG(LogVeyraMatch, Log, TEXT("%s voted %s."), *Voter.GetPlayerName(), bYes ? TEXT("yes") : TEXT("no"));
	Tick(0.0f);
	return EVeyraVoteRefusal::None;
}

void UVeyraVoteSubsystem::BeginIntermission()
{
	IntermissionEndsAt = RealNow() + UVeyraMatchTuningSubsystem::Get().Votes.Pause.IntermissionSeconds;
	Publish(GatherVoters(), RealNow());
}

void UVeyraVoteSubsystem::EndIntermission()
{
	IntermissionEndsAt.Reset();
	// A resume vote still open has nothing left to decide.
	if (Box.IsSet() && Box->Kind == EVeyraVoteKind::Resume)
	{
		Box.Reset();
	}
	Publish(GatherVoters(), RealNow());
}

bool UVeyraVoteSubsystem::Tick(float /*DeltaSeconds*/)
{
	if (!bRunning)
	{
		return false;
	}
	const double Now = RealNow();
	const TArray<FVeyraVoter> Voters = GatherVoters();
	if (Box.IsSet())
	{
		// Absent players' and bots' ballots, locked once recorded (§9).
		for (const FVeyraVoter& Voter : Voters)
		{
			if (!VeyraVotes::IsVoter(*Box, Voter) || Box->Ballots.Contains(Voter.PlayerId))
			{
				continue;
			}
			if (const TOptional<bool> Automatic = VeyraVotes::AutomaticBallot(Box->Kind, Voter))
			{
				Box->Ballots.Add(Voter.PlayerId, Automatic.GetValue());
			}
		}
		const FVeyraVotesTuning& Tuning = UVeyraMatchTuningSubsystem::Get().Votes;
		const EVeyraVoteOutcome Outcome = VeyraVotes::Tally(*Box, Voters, Now, Tuning);
		if (Outcome != EVeyraVoteOutcome::Open)
		{
			const FVeyraBallotBox Closed = Box.GetValue();
			Box.Reset();
			UE_LOG(LogVeyraMatch, Log, TEXT("The %s vote %s."), *StaticEnum<EVeyraVoteKind>()->GetNameStringByValue(static_cast<int64>(Closed.Kind)),
				Outcome == EVeyraVoteOutcome::Passed ? TEXT("passed") : TEXT("failed"));
			if (Outcome == EVeyraVoteOutcome::Failed)
			{
				VeyraVotes::NoteFailed(Closed, Now, Tuning, Cooldowns);
			}
			Publish(Voters, Now);
			if (Outcome == EVeyraVoteOutcome::Passed)
			{
				OnVotePassed.Broadcast(Closed.Kind, Closed.Team);
			}
			return bRunning;
		}
	}
	if (IntermissionEndsAt.IsSet() && Now >= IntermissionEndsAt.GetValue())
	{
		IntermissionEndsAt.Reset();
		UE_LOG(LogVeyraMatch, Log, TEXT("The intermission is over."));
		OnIntermissionOver.Broadcast();
	}
	Publish(Voters, Now);
	return bRunning;
}

TArray<FVeyraVoter> UVeyraVoteSubsystem::GatherVoters() const
{
	TArray<FVeyraVoter> Voters;
	const AVeyraGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AVeyraGameState>() : nullptr;
	const UVeyraAbsenceSubsystem* Absence = GetWorld() ? GetWorld()->GetSubsystem<UVeyraAbsenceSubsystem>() : nullptr;
	if (!GameState)
	{
		return Voters;
	}
	for (const APlayerState* Member : GameState->PlayerArray)
	{
		const AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(Member);
		if (!Participant || Participant->GetVeyraTeam() == EVeyraTeam::None)
		{
			continue;
		}
		const FVeyraAbsenceRecord* Record = Absence ? Absence->Find(*Participant) : nullptr;
		Voters.Add({ Participant->GetPlayerId(), Participant->GetVeyraTeam(), Participant->IsABot(), Participant->IsInactive(),
			Record && Record->Absence == EVeyraAbsence::Afk });
	}
	return Voters;
}

void UVeyraVoteSubsystem::Publish(TConstArrayView<FVeyraVoter> Voters, double Now) const
{
	AVeyraGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AVeyraGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}
	FVeyraVoteState State;
	if (Box.IsSet())
	{
		State.bOpen = true;
		State.Kind = Box->Kind;
		State.Team = Box->Team;
		State.Needed = VeyraVotes::YesNeeded(*Box, Voters, UVeyraMatchTuningSubsystem::Get().Votes);
		// Whole seconds, so the countdown replicates once a second.
		State.SecondsLeft = static_cast<float>(FMath::CeilToDouble(FMath::Max(0.0, Box->EndsAt - Now)));
		for (const TPair<int32, bool>& Ballot : Box->Ballots)
		{
			(Ballot.Value ? State.Yes : State.No) += 1;
			State.Voted.Add(Ballot.Key);
		}
		State.Voted.Sort();
	}
	GameState->SetVote(State);
	GameState->SetIntermissionSecondsLeft(IntermissionEndsAt.IsSet() ? FMath::CeilToInt(FMath::Max(0.0, IntermissionEndsAt.GetValue() - Now)) : 0);
}

double UVeyraVoteSubsystem::RealNow()
{
	return FPlatformTime::Seconds();
}
