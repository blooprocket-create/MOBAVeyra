// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Tuning/VeyraTuningProvenance.h"
#include "UObject/ObjectMacros.h"

#include "VeyraEconomyTuning.generated.h"

/** Individual Gold (Economy & Progression Bible §1, §3, §5, §8). */
USTRUCT()
struct FVeyraGoldTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Every participant's one guaranteed grant, when the match prepares (§1, §10). */
	UPROPERTY()
	double Starting = 0.0;

	/** Each kind of Fluxborn's listed Gold: all of it to the Vanguard who lands the last hit (§3.1). */
	UPROPERTY()
	TMap<FVeyraContentId, double> Fluxborn;

	/** The share of a Fluxborn's Gold each other nearby living ally gets when an allied Vanguard damaged it recently (§3.2). */
	UPROPERTY()
	double ParticipationFraction = 0.0;

	/** A Vanguard's base kill Gold, whatever its level (§5.1). */
	UPROPERTY()
	double VanguardKill = 0.0;

	/** The Assist Gold pool, as a fraction of the base kill Gold, split among the assisters (§5.1). */
	UPROPERTY()
	double AssistPoolFraction = 0.0;

	/** The match's first kill adds this fraction of the base kill Gold for the killer (§5.2). */
	UPROPERTY()
	double FirstBloodFraction = 0.0;

	/** A lane Spire's or base-defense tower's Gold, split among its recent contributors (§8.1). */
	UPROPERTY()
	double StructurePool = 0.0;

	/** The first Spire or base tower to fall adds this for every member of the destroying team (§8.1). */
	UPROPERTY()
	double FirstStructureBonus = 0.0;
};

/**
 * Passive Gold (author ruling, 2026-09-28, amending §1): every participant earns a steady income while
 * the match is live, dead or alive, as in League of Legends. There is still no passive XP.
 */
USTRUCT()
struct FVeyraPassiveGoldTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** The Gold each participant receives at each payment. */
	UPROPERTY()
	double PerPayment = 0.0;

	/** How often it is paid, in seconds of live match. */
	UPROPERTY()
	double IntervalSeconds = 0.0;

	/** How long after the match goes live the income begins, in seconds. */
	UPROPERTY()
	double StartSeconds = 0.0;
};

/** Individual XP rewards (§3.3, §6). The XP curve is Progression.json's. */
USTRUCT()
struct FVeyraExperienceRewardTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Each kind of Fluxborn's base XP for the nearby living allies (§3.3). */
	UPROPERTY()
	TMap<FVeyraContentId, double> Fluxborn;

	/** With two or more to share, one pool of this much of the base XP, split equally (§3.3). */
	UPROPERTY()
	double SharedPoolFraction = 0.0;

	/** A level-1 victim's base kill XP, and what each level above 1 adds (§6). */
	UPROPERTY()
	double KillBase = 0.0;

	UPROPERTY()
	double KillPerLevel = 0.0;

	/** The kill XP pool grows by this much of the base for each qualifying participant after the first (§6). */
	UPROPERTY()
	double ParticipantBonus = 0.0;

	/** Applied once to the pool when the victim outlevels the killer (§6). */
	UPROPERTY()
	double HigherLevelVictimMultiplier = 0.0;
};

/** Enemy Fluxborn strengthened by Team Flux pay a little more (§4). */
USTRUCT()
struct FVeyraFluxRewardBonusTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** Each this much of the Fluxborn's team's active Flux adds a step. */
	UPROPERTY()
	double FluxPerStep = 0.0;

	/** The Gold and XP each step adds, as a fraction. */
	UPROPERTY()
	double BonusPerStep = 0.0;

	/** The most the steps add. */
	UPROPERTY()
	double MaxBonus = 0.0;
};

/** Who is near enough, and recent enough, to share (§2, §3, §8). */
USTRUCT()
struct FVeyraRewardEligibilityTuning
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	/** How near a death a Vanguard must be for farm XP, participation Gold and kill XP, in units (§2). */
	UPROPERTY()
	double Radius = 0.0;

	/** How recently an allied Vanguard must have damaged a Fluxborn for nearby allies to share its Gold (§3.2). */
	UPROPERTY()
	double FluxbornParticipationSeconds = 0.0;

	/** How recently a Vanguard must have damaged a structure to share its pool (§8.1). */
	UPROPERTY()
	double StructureContributionSeconds = 0.0;
};

/** The Economy domain's tuning, bound from Game/Tuning/Economy.json (ADR-006 §6, ADR-011 §11). */
USTRUCT()
struct FVeyraEconomyTuning
{
	GENERATED_BODY()

	/** The Economy.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 2;

	UPROPERTY()
	FVeyraGoldTuning Gold;

	UPROPERTY()
	FVeyraPassiveGoldTuning PassiveGold;

	UPROPERTY()
	FVeyraExperienceRewardTuning Experience;

	UPROPERTY()
	FVeyraFluxRewardBonusTuning FluxBonus;

	UPROPERTY()
	FVeyraRewardEligibilityTuning Eligibility;
};
