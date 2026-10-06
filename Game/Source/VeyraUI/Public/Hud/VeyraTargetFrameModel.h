// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Content/VeyraContentId.h"
#include "Hud/VeyraHudModel.h"
#include "Misc/Optional.h"
#include "Teams/VeyraTeam.h"
#include "Units/VeyraUnit.h"

class AActor;

/** A Flux Spell in the selected unit's frame (ADR-066 §3): locked until its team unlocks the slot, ready, or cooling down. */
struct FVeyraTargetFrameSpell
{
	/** Invalid for an empty slot. */
	FVeyraContentId Spell;

	bool bLocked = false;

	/** Seconds until it is ready, and what its cooldown started with; both 0 when it is ready. */
	double CooldownSeconds = 0.0;
	double CooldownTotal = 0.0;
};

/** What the selected unit's frame shows of it, as the viewer's side sees it (ADR-066 §3). */
struct FVeyraTargetFrame
{
	EVeyraUnitKind Kind = EVeyraUnitKind::Vanguard;

	/** Its name: a Vanguard's, a structure's place, a Fluxborn's, creature's or companion's kind. */
	FString Name;

	/** Under its name: a Vanguard's player. */
	FString Detail;

	EVeyraTeam Side = EVeyraTeam::None;
	bool bAlive = true;
	FVeyraHudVitals Vitals;

	/** A Vanguard's: its kit, for its face, and its Level; 0 for any other unit. */
	FVeyraContentId Vanguard;
	int32 Level = 0;

	/** A Vanguard's six item slots in order, an invalid ID for an empty one; empty for any other unit. */
	TArray<FVeyraContentId> Items;

	/** A Vanguard's Flux Spell slots in order; empty for any other unit. */
	TArray<FVeyraTargetFrameSpell> Spells;
};

/** The selected unit's frame, read from what every machine receives (ADR-066 §3–§4). Plain functions, so tests check them. */
namespace VeyraTargetFrame
{
	/**
	 * Unit's frame for a viewer on Viewer's side, its cooldowns at ServerNow in server gameplay time: what Unit presents, so
	 * a marker presenting as its owner shows its owner (ADR-030 §5). Nothing for what shows no frame, as a wall's marker.
	 */
	VEYRAUI_API TOptional<FVeyraTargetFrame> Describe(const AActor& Unit, EVeyraTeam Viewer, double ServerNow);

	/** A content ID's words, as a frame names a kind without text of its own: "spark_breaker" is "Spark Breaker". */
	VEYRAUI_API FString Words(const FString& Id);
}
