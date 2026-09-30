// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "UObject/WeakObjectPtr.h"

#include "VeyraMarkerTypes.generated.h"

class AActor;
class UAbilitySystemComponent;

/** Why a placed marker ended (ADR-030 §5). */
UENUM()
enum class EVeyraMarkerEndReason : uint8
{
	/** Its time ran out. */
	Expired,
	/** Enemy hits destroyed it. */
	Destroyed,
	/** Its owner's ability took it back, as a recast does. */
	Recalled,
	/** Its owner died. */
	OwnerDied,
};

/** What a placed marker is, as its owner's ability places it (ADR-030 §5). */
struct FVeyraMarkerSpec
{
	/** Which marker it is, for the ability that reads its events. */
	FVeyraContentId Id;
	/** How long it stands, in seconds. */
	double LifetimeSeconds = 0.0;
	/** The hits that destroy it; 0 for one no one can target. */
	int32 HitsToDestroy = 0;
	/** Enemies see it as its owner: its owner's body, bars and minimap mark (a decoy). */
	bool bPresentsAsOwner = false;
};

/** A placed marker's end, announced on the server. */
struct FVeyraMarkerEnd
{
	TWeakObjectPtr<AActor> Marker;
	TWeakObjectPtr<UAbilitySystemComponent> Owner;
	FVeyraContentId Id;
	EVeyraMarkerEndReason Reason = EVeyraMarkerEndReason::Expired;
	/** Where it stood as it ended. */
	FVector Location = FVector::ZeroVector;
	/** For Destroyed: whoever landed the last hit. */
	TWeakObjectPtr<UAbilitySystemComponent> Destroyer;
};
