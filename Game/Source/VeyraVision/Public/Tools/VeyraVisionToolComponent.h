// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "VeyraVisionToolComponent.generated.h"

/** The vision tools a Vanguard may equip in its vision-tool slot (Vision Bible §3). */
UENUM()
enum class EVeyraVisionTool : uint8
{
	/** Places long-lived wards, invisible to the enemy, from regenerating charges (§4). */
	PersistentWard,
	/** True Sight around its owner for a while: enemy wards and Vanguards in fog, outlined (§5). */
	Sweeper,
	/** Ordinary vision over an area for a moment; over Dense Fog, a presence sensor (§6). */
	QuickSight,
};

/** Why a vision tool was not used, or None. */
UENUM()
enum class EVeyraVisionToolRejection : uint8
{
	None,
	/** The participant has no living Vanguard. */
	NoVanguard,
	/** Crowd control stops the Vanguard acting, as it stops casting (Combat Bible §8). */
	CrowdControlled,
	/** No ward charge is carried. */
	NoCharge,
	/** The equipped tool is cooling down. */
	CoolingDown,
	/** The point is not usable, for example not finite. */
	InvalidPoint,
};

VEYRAVISION_API const TCHAR* LexToString(EVeyraVisionToolRejection Rejection);

/**
 * A participant's vision tool (Vision Bible §3–§7; ADR-016 §6), on its PlayerState: the tool in its
 * slot, the ward charges and their recharge. The server decides everything; the owner's client sees
 * the charges and when the next comes back. Every match starts with Persistent Ward and its charges.
 */
UCLASS(ClassGroup = Vision)
class VEYRAVISION_API UVeyraVisionToolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVeyraVisionToolComponent();

	virtual void InitializeComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EVeyraVisionTool GetEquipped() const { return Equipped; }

	/** Ward charges carried now. */
	int32 GetWardCharges() const { return WardCharges; }

	/** When the next ward charge comes back, in the server's world time; below 0 while none is coming. */
	double GetNextChargeAt() const { return NextChargeAt; }

	/** When Tool is ready again, in the server's world time: its cooldown survives swaps and deaths (§7). */
	double GetReadyAt(EVeyraVisionTool Tool) const;

	/**
	 * Server: uses the equipped tool toward Point, from the participant's Vanguard. A Persistent Ward
	 * spends a charge and places a ward at Point, brought within its placement range; Sweeper grants
	 * True Sight around the Vanguard; Quick Sight lights Point, brought within its range.
	 */
	EVeyraVisionToolRejection Use(const FVector& Point);

	/**
	 * Server: puts Tool in the slot (the fountain swap routes here, ADR-016 §6). Persistent Ward comes
	 * with all its charges; the others keep their cooldowns.
	 */
	void Equip(EVeyraVisionTool Tool);

	/** Server: the fountain, a respawn or equipping Persistent Ward fills the charges (§4, §7). */
	void RefillWardCharges();

private:
	void ScheduleRecharge();
	void OnWardCharged();
	void SetWardCharges(int32 NewCharges);
	void SetNextChargeAt(double NewNextChargeAt);

	UPROPERTY(Replicated)
	EVeyraVisionTool Equipped = EVeyraVisionTool::PersistentWard;

	UPROPERTY(Replicated)
	int32 WardCharges = 0;

	UPROPERTY(Replicated)
	double NextChargeAt = -1.0;

	UPROPERTY(Replicated)
	double SweeperReadyAt = 0.0;

	UPROPERTY(Replicated)
	double QuickSightReadyAt = 0.0;

	FTimerHandle RechargeTimer;
};
