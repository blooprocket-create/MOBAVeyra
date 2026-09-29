// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "State/VeyraVisionTeamState.h"

#include "EngineUtils.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

AVeyraVisionTeamState::AVeyraVisionTeamState()
{
	bReplicates = true;
	// Always relevant, so replays record it; the fog gate lets only its side's clients receive it (ADR-016 §3).
	bAlwaysRelevant = true;
}

void AVeyraVisionTeamState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraVisionTeamState, Team, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraVisionTeamState, Pings, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraVisionTeamState, Outlines, Params);
}

void AVeyraVisionTeamState::SetVeyraTeam(EVeyraTeam InTeam)
{
	Team = InTeam;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraVisionTeamState, Team, this);
}

void AVeyraVisionTeamState::AddPing(const FVeyraPresencePing& Ping, double Now, double KeepSeconds)
{
	Pings.RemoveAll([Now, KeepSeconds](const FVeyraPresencePing& Old) { return Now - Old.At > KeepSeconds; });
	Pings.Add(Ping);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraVisionTeamState, Pings, this);
}

void AVeyraVisionTeamState::SetOutlines(TArray<FVeyraOutline> InOutlines)
{
	if (InOutlines.Num() == 0 && Outlines.Num() == 0)
	{
		return;
	}
	Outlines = MoveTemp(InOutlines);
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraVisionTeamState, Outlines, this);
}

AVeyraVisionTeamState* AVeyraVisionTeamState::Find(const UWorld* World, EVeyraTeam Team)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AVeyraVisionTeamState> It(World); It; ++It)
	{
		if (It->GetVeyraTeam() == Team)
		{
			return *It;
		}
	}
	return nullptr;
}
