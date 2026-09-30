// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

struct FVeyraBountyTuning;
struct FVeyraEconomyTuning;
struct FVeyraKillGoldTuning;
struct FVeyraExperienceRewardTuning;
struct FVeyraFluxRewardBonusTuning;

/**
 * The reward arithmetic of the Economy & Progression Bible (§3–§6, §8) as pure functions of the
 * tuning, so the bible's worked examples are tested without a world. UVeyraRewardSubsystem decides
 * who qualifies and pays through them.
 */
namespace VeyraRewards
{
	/**
	 * What the schema cannot check: every kind of Fluxborn with Gold has XP, and the reverse; the bounty
	 * never falls as a streak grows; devaluation starts at 1 and only falls.
	 */
	VEYRAECONOMY_API TArray<FString> Validate(const FVeyraEconomyTuning& Tuning);

	/**
	 * How much more an enemy Fluxborn pays for its team's active Team Flux when it died (§4): one step
	 * per FluxPerStep, each adding BonusPerStep, up to MaxBonus. 1 for none.
	 */
	VEYRAECONOMY_API double FluxBonusMultiplier(double ActiveFlux, const FVeyraFluxRewardBonusTuning& Bonus);

	/**
	 * Each eligible Vanguard's share of BaseXp from a Fluxborn (§3.3): all of it alone; with two or
	 * more, SharedPoolFraction of it split equally. 0 when nobody is eligible.
	 */
	VEYRAECONOMY_API double FarmXpShare(double BaseXp, int32 Eligible, double SharedPoolFraction);

	/** A Vanguard victim's base kill XP by its level (§6). */
	VEYRAECONOMY_API double KillExperience(int32 VictimLevel, const FVeyraExperienceRewardTuning& Experience);

	/**
	 * The kill XP pool (§6): Base × (1 + ParticipantBonus × (Participants − 1)), and the higher-level
	 * multiplier once when the victim outlevelled the killer.
	 */
	VEYRAECONOMY_API double KillExperiencePool(double Base, int32 Participants, bool bVictimOutlevelsKiller, const FVeyraExperienceRewardTuning& Experience);

	/** Each assister's share of the Assist Gold pool: AssistPoolFraction of the kill Gold, split evenly (§5.1). 0 for none. */
	VEYRAECONOMY_API double AssistShare(double KillGold, int32 Assisters, double AssistPoolFraction);

	/** A Vanguard's bounty for its KillStreak consecutive kills (§5.3); past the list, its last value. */
	VEYRAECONOMY_API double Bounty(int32 KillStreak, const FVeyraBountyTuning& Bounty);

	/** A Vanguard's base kill Gold, devalued by its DeathStreak (§5.4); past the list, the minimum. */
	VEYRAECONOMY_API double DevaluedKillGold(double BaseKillGold, int32 DeathStreak, const FVeyraKillGoldTuning& KillGold);

	/**
	 * A death streak after one more enemy-credited death: one step lower, never past the minimum, so the
	 * next takedown restores exactly one step (§5.4).
	 */
	VEYRAECONOMY_API int32 DeathStreakAfterDeath(int32 DeathStreak, const FVeyraKillGoldTuning& KillGold);

	/** A death streak after a takedown, a kill or an assist: one step restored (§5.4). */
	VEYRAECONOMY_API int32 DeathStreakAfterTakedown(int32 DeathStreak);
}
