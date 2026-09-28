// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "State/VeyraTeamFluxState.h"

#include "EngineUtils.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

double FVeyraTeamFluxView::ActiveAt(double Now) const
{
	double Total = Permanent;
	for (const FVeyraTemporaryFluxView& Grant : Temporary)
	{
		Total += Grant.ExpiresAt > Now ? Grant.Amount : 0.0;
	}
	return Total;
}

AVeyraTeamFluxState::AVeyraTeamFluxState()
{
	bReplicates = true;
	// Team Flux is public, like the score: everyone sees both teams' (ADR-011 §10).
	bAlwaysRelevant = true;
}

void AVeyraTeamFluxState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraTeamFluxState, Teams, Params);
}

void AVeyraTeamFluxState::SetTeams(TArray<FVeyraTeamFluxView> InTeams)
{
	Teams = MoveTemp(InTeams);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraTeamFluxState, Teams, this);
}

const FVeyraTeamFluxView* AVeyraTeamFluxState::Find(EVeyraTeam Team) const
{
	return Teams.FindByPredicate([Team](const FVeyraTeamFluxView& View) { return View.Team == Team; });
}

AVeyraTeamFluxState* AVeyraTeamFluxState::Find(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<AVeyraTeamFluxState> It(World);
	return It ? *It : nullptr;
}
