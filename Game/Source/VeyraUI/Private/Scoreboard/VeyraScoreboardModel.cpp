// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Scoreboard/VeyraScoreboardModel.h"

#include "GameFramework/PlayerState.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Statistics/VeyraScoreComponent.h"
#include "VeyraPlayerState.h"

#define LOCTEXT_NAMESPACE "VeyraScoreboard"

namespace VeyraScoreboardModel
{
FVeyraScoreboardView Describe(TConstArrayView<const APlayerState*> Participants, const APlayerState* Local)
{
	const EVeyraTeam LocalSide = VeyraTeams::TeamOf(Local);
	const EVeyraTeam First = LocalSide == EVeyraTeam::None ? EVeyraTeam::A : LocalSide;
	FVeyraScoreboardView View;
	for (const EVeyraTeam Team : { First, VeyraTeams::Opposing(First) })
	{
		FVeyraScoreboardSide& Side = View.Sides.AddDefaulted_GetRef();
		Side.Team = Team;
		Side.bAllies = Team == LocalSide;
		TArray<const APlayerState*> Seated;
		for (const APlayerState* Participant : Participants)
		{
			if (Participant && VeyraTeams::TeamOf(Participant) == Team)
			{
				Seated.Add(Participant);
			}
		}
		Seated.Sort([](const APlayerState& A, const APlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
		for (const APlayerState* Participant : Seated)
		{
			FVeyraScoreboardRow& Row = Side.Rows.AddDefaulted_GetRef();
			Row.Name = Participant->GetPlayerName();
			Row.bLocal = Participant == Local;
			if (const AVeyraPlayerState* Veyra = Cast<AVeyraPlayerState>(Participant))
			{
				Row.Vanguard = Veyra->GetVanguardId();
			}
			if (const UVeyraProgressionComponent* Progression = Participant->FindComponentByClass<UVeyraProgressionComponent>())
			{
				Row.Level = Progression->GetLevel();
			}
			if (const UVeyraScoreComponent* Score = Participant->FindComponentByClass<UVeyraScoreComponent>())
			{
				const FVeyraScore& Public = Score->GetScore();
				Row.Kills = Public.Kills;
				Row.Deaths = Public.Deaths;
				Row.Assists = Public.Assists;
				Row.CreepScore = Public.MinionKills + Public.JungleKills;
			}
			if (const UVeyraInventoryComponent* Inventory = Participant->FindComponentByClass<UVeyraInventoryComponent>())
			{
				for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
				{
					Row.Items.Add(Slot.IsEmpty() ? FVeyraContentId() : Slot.Item);
				}
			}
			Side.Kills += Row.Kills;
		}
	}
	return View;
}

FText KdaText(const FVeyraScoreboardRow& Row)
{
	return FText::Format(LOCTEXT("Kda", "{0} / {1} / {2}"), FText::AsNumber(Row.Kills), FText::AsNumber(Row.Deaths), FText::AsNumber(Row.Assists));
}
}

#undef LOCTEXT_NAMESPACE
