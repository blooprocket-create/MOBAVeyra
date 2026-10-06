// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Battleground/VeyraBattlegroundTypes.h"
#include "Content/VeyraContentId.h"
#include "Teams/VeyraTeam.h"

#include "VeyraKillFeedTypes.generated.h"

/** What a kill feed line tells (ADR-065 §10). */
UENUM()
enum class EVeyraKillFeedKind : uint8
{
	/** A Vanguard fell to an enemy Vanguard credited with the kill. */
	Takedown,
	/** A Vanguard fell with no enemy Vanguard to credit: to a structure, a Fluxborn or a creature. */
	Execution,
	/** A structure fell. */
	Structure,
};

/**
 * One line of the kill feed, as the server sends it to every player (ADR-065 §10): who fell, to whom, with how many
 * assisting. A death everyone may know of, as the scoreboard's kills are; presentation only, nothing reads it to decide
 * anything.
 */
USTRUCT()
struct VEYRAMATCH_API FVeyraKillFeedLine
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraKillFeedKind Kind = EVeyraKillFeedKind::Takedown;

	/** The credited killer, for a takedown or a structure a Vanguard took; INDEX_NONE for none. */
	UPROPERTY()
	int32 KillerPlayerId = INDEX_NONE;

	UPROPERTY()
	FString KillerName;

	UPROPERTY()
	FVeyraContentId KillerVanguard;

	UPROPERTY()
	EVeyraTeam KillerSide = EVeyraTeam::None;

	/** The fallen Vanguard; INDEX_NONE for a structure. */
	UPROPERTY()
	int32 VictimPlayerId = INDEX_NONE;

	UPROPERTY()
	FString VictimName;

	UPROPERTY()
	FVeyraContentId VictimVanguard;

	/** The fallen Vanguard's or structure's side. */
	UPROPERTY()
	EVeyraTeam VictimSide = EVeyraTeam::None;

	UPROPERTY()
	int32 Assists = 0;

	/** The match's first takedown. */
	UPROPERTY()
	bool bFirstBlood = false;

	/** A fallen structure: its kind, its lane when it has one, and its order there, the outer Spire 0. */
	UPROPERTY()
	EVeyraStructureKind StructureKind = EVeyraStructureKind::LaneSpire;

	UPROPERTY()
	bool bHasLane = false;

	UPROPERTY()
	EVeyraLane Lane = EVeyraLane::Mid;

	UPROPERTY()
	int32 StructureOrder = 0;
};

struct FVeyraDeathEvent;

/** Which deaths make kill feed lines (ADR-065 §10). */
namespace VeyraKillFeedRules
{
	/**
	 * Death's line: a Vanguard's, a takedown when an enemy Vanguard is credited and an execution otherwise; a structure's,
	 * with the Vanguard credited with it when there is one. Nothing for any other death. First Blood is the caller's to mark.
	 */
	VEYRAMATCH_API TOptional<FVeyraKillFeedLine> LineFor(const FVeyraDeathEvent& Death);
}
