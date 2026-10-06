// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Feedback/VeyraKillFeedTypes.h"

#include "AbilitySystemComponent.h"
#include "Life/VeyraCombatEventSubsystem.h"
#include "Structures/VeyraStructure.h"
#include "VeyraPlayerState.h"

namespace VeyraKillFeedRules
{
namespace
{
	const AVeyraPlayerState* ParticipantOf(const UAbilitySystemComponent* Unit)
	{
		return Unit ? Cast<AVeyraPlayerState>(Unit->GetOwner()) : nullptr;
	}
}

TOptional<FVeyraKillFeedLine> LineFor(const FVeyraDeathEvent& Death)
{
	const UAbilitySystemComponent* Victim = Death.Victim.Get();
	if (!Victim)
	{
		return {};
	}
	FVeyraKillFeedLine Line;
	Line.Assists = Death.Assisters.Num();
	if (const AVeyraPlayerState* Killer = ParticipantOf(Death.CreditedKiller.Get()))
	{
		Line.KillerPlayerId = Killer->GetPlayerId();
		Line.KillerName = Killer->GetPlayerName();
		Line.KillerVanguard = Killer->GetVanguardId();
		Line.KillerSide = Killer->GetVeyraTeam();
	}
	if (const AVeyraPlayerState* Fallen = ParticipantOf(Victim))
	{
		Line.Kind = Line.KillerPlayerId != INDEX_NONE ? EVeyraKillFeedKind::Takedown : EVeyraKillFeedKind::Execution;
		Line.VictimPlayerId = Fallen->GetPlayerId();
		Line.VictimName = Fallen->GetPlayerName();
		Line.VictimVanguard = Fallen->GetVanguardId();
		Line.VictimSide = Fallen->GetVeyraTeam();
		return Line;
	}
	if (const AVeyraStructure* Structure = Cast<AVeyraStructure>(Victim->GetAvatarActor()))
	{
		Line.Kind = EVeyraKillFeedKind::Structure;
		Line.VictimSide = Structure->GetVeyraTeam();
		Line.StructureKind = Structure->GetStructureKind();
		Line.StructureOrder = Structure->GetOrder();
		if (const TOptional<EVeyraLane> Lane = Structure->GetLane())
		{
			Line.bHasLane = true;
			Line.Lane = Lane.GetValue();
		}
		return Line;
	}
	return {};
}
}
