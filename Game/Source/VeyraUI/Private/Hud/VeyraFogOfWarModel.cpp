// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraFogOfWarModel.h"

#include "Engine/World.h"
#include "Misc/Crc.h"
#include "State/VeyraVisionTeamState.h"

namespace VeyraFogOfWar
{
const FVeyraSeenGround* OwnGround(const UWorld& World, EVeyraTeam Viewer)
{
	if (Viewer != EVeyraTeam::A && Viewer != EVeyraTeam::B)
	{
		return nullptr;
	}
	const AVeyraVisionTeamState* Side = AVeyraVisionTeamState::Find(&World, Viewer);
	const FVeyraSeenGround* Ground = Side ? &Side->GetSeenGround() : nullptr;
	return Ground && Ground->CellsAcross > 0 && !Ground->Cells.IsEmpty() ? Ground : nullptr;
}

TArray<FVeyraUnseenRun> UnseenRuns(const FVeyraSeenGround& Ground)
{
	TArray<FVeyraUnseenRun> Runs;
	for (int32 Row = 0; Row < Ground.CellsAcross; ++Row)
	{
		int32 Start = INDEX_NONE;
		for (int32 Column = 0; Column <= Ground.CellsAcross; ++Column)
		{
			const bool bUnseen = Column < Ground.CellsAcross && !Ground.IsSeen(Column, Row);
			if (bUnseen && Start == INDEX_NONE)
			{
				Start = Column;
			}
			else if (!bUnseen && Start != INDEX_NONE)
			{
				Runs.Add(FVeyraUnseenRun{ Row, Start, Column - 1 });
				Start = INDEX_NONE;
			}
		}
	}
	return Runs;
}

FBox2D BoundsOf(const FVeyraSeenGround& Ground, const FVeyraUnseenRun& Run)
{
	const FVector2D Low = Ground.Min + FVector2D(Run.First * Ground.CellSize, Run.Row * Ground.CellSize);
	const FVector2D High = Ground.Min + FVector2D((Run.Last + 1) * Ground.CellSize, (Run.Row + 1) * Ground.CellSize);
	return FBox2D(Low, High);
}

uint32 SignatureOf(const FVeyraSeenGround* Ground)
{
	if (!Ground)
	{
		return 0;
	}
	uint32 Crc = FCrc::MemCrc32(Ground->Cells.GetData(), Ground->Cells.Num());
	Crc = FCrc::MemCrc32(&Ground->CellsAcross, sizeof(Ground->CellsAcross), Crc);
	Crc = FCrc::MemCrc32(&Ground->CellSize, sizeof(Ground->CellSize), Crc);
	Crc = FCrc::MemCrc32(&Ground->Min, sizeof(Ground->Min), Crc);
	// Never 0, which stands for no ground.
	return Crc == 0 ? 1 : Crc;
}
}