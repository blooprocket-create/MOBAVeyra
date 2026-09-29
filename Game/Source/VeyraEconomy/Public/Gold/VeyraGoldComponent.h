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
	/** An item sold at the fountain (§12). */
	Sale,
	/** A purchase undone at the fountain, for all it cost (§12). */
	Undo,
};

VEYRAECONOMY_API const TCHAR* LexToString(EVeyraGoldReason Reason);

/**
 * Gold set aside for a purchase not yet delivered (§11.1): spent already, and refunded in full if the
 * purchase is cancelled (§11.3). The owner sees its holds, so the shop can show what waits.
 */
USTRUCT()
struct FVeyraGoldHold
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	UPROPERTY()
	double Amount = 0.0;
};

/**
 * One participant's Gold (Economy & Progression Bible §1): a fractional balance that only the server
 * changes, through explained grants, purchases and refunds, and that only its owner sees. It sits on
 * the PlayerState, beside progression, so it survives death. The HUD rounds it for display; nothing
 * else does. It owns purchase accounting (§16): what items cost is Items' to say, never to change.
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

	/**
	 * Server only: pays Amount for a purchase delivered at once (§10). Refused, changing nothing, unless
	 * finite, at least 0 and within the balance: Gold never goes negative (§11.1).
	 */
	bool Spend(double Amount);

	/**
	 * Server only: pays Amount for a purchase that waits in the queue (§11.1), holding it until the
	 * purchase is delivered or cancelled. Returns the hold's ID, or nothing if refused as Spend is.
	 */
	TOptional<int32> Hold(double Amount);

	/** Server only: the held purchase was cancelled; all it held comes back (§11.3). Returns what came back. */
	double ReleaseHold(int32 Id);

	/** Server only: the held purchase was delivered; its Gold is spent for good. */
	void SettleHold(int32 Id);

	/** Owner and server only. */
	double GetGold() const { return Gold; }

	/** Owner and server only: the Gold held for purchases not yet delivered. */
	const TArray<FVeyraGoldHold>& GetHolds() const { return Holds; }

private:
	void SetGold(double NewGold);

	UPROPERTY(Replicated)
	double Gold = 0.0;

	UPROPERTY(Replicated)
	TArray<FVeyraGoldHold> Holds;

	int32 NextHoldId = 1;
};
