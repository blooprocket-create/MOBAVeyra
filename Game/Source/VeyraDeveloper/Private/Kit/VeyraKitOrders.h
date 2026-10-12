// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "VeyraAbilityTypes.h"
#include "Content/VeyraContentId.h"

class AActor;
class AVeyraGameState;
class AVeyraPlayerController;
class AVeyraPlayerState;

/**
 * What a scripted player needs to cast a Vanguard's kit: the kit smoke (Smoke.ps1 -VeyraSmokeKit) and the skill
 * showcase recorder (ADR-074 §3) both order casts through these, so they aim and count casts alike.
 */
namespace VeyraKitOrders
{
	/** Whether a refused cast may succeed if tried again a moment later. */
	bool IsPassingRejection(EVeyraCastRejection Rejection);

	/** The living enemy Vanguard nearest From that Controller's client sees; null if none has a body here. */
	AActor* FindNearestEnemyBody(const AVeyraPlayerController& Controller, const AVeyraGameState& GameState, const FVector& From);

	/**
	 * Whether Ability is cast for an ally, as an escort's summon: such a cast goes to the caster itself, the one ally a
	 * scripted player has (ADR-035 §9), as the bots choose it.
	 */
	bool IsForAlly(const FVeyraContentId& Ability);

	/** How far from its target Ability may be cast, as the bots read it; 0 for one with no reach of its own. */
	double ReachOf(const FVeyraContentId& Ability);

	/** A cast aimed at Aim, where it stands now. */
	FVeyraCastTarget TargetAt(AActor& Aim);

	/**
	 * When Ability's cooldown is next due for Own. A cast counts once the server starts its cooldown, at Commit
	 * (ADR-008 §4), so a later due time than before the order means it committed. It is read by when it is due, not by
	 * what remains: a client's estimate of the server's clock can run seconds ahead, as a render-less client's does.
	 */
	double ReadyAtOf(const AVeyraPlayerState& Own, const FVeyraContentId& Ability);
}
