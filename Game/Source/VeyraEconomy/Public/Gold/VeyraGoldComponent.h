// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "VeyraGoldComponent.generated.h"

/** Why Gold was granted, for the log and, later, the match statistics (Economy & Progression Bible §1). */
UENUM()
enum class EVeyraGoldReason : uint8
{
	Starting,
	/** An enemy Fluxborn's last hit (§3.1). */
	LastHit,
	/** A share of a nearby Fluxborn an ally fought (§3.2). */
	Participation,
	Kill,
	Assist,
	FirstBlood,
	/** A share of a fallen Spire's or base tower's pool (§8.1). */
	StructurePool,
	/** The first Spire or base tower of the match, for the whole team (§8.1). */
	FirstStructure,
	Developer,
};

VEYRAECONOMY_API const TCHAR* LexToString(EVeyraGoldReason Reason);

/**
 * One participant's Gold (Economy & Progression Bible §1): a fractional balance that only the server
 * changes, through explained grants, and that only its owner sees. It sits on the PlayerState, beside
 * progression, so it survives death. The HUD rounds it for display; nothing else does.
 */
UCLASS()
class VEYRAECONOMY_API UVeyraGoldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraGoldComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only: adds Amount Gold for Reason and logs it. Refused unless finite and above 0. */
	bool Grant(double Amount, EVeyraGoldReason Reason);

	/** Owner and server only. */
	double GetGold() const { return Gold; }

private:
	UPROPERTY(Replicated)
	double Gold = 0.0;
};
