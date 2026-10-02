// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Damage/VeyraDamageTypes.h"

struct FVeyraEchoIntegrityLossTuning;
struct FVeyraEchoProjectionTuning;

/** Where an Echo's repeat of a cast is aimed (ADR-050 §5). */
struct FVeyraEchoAim
{
	FVector Point = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
};

/** The Echo's rules (ADR-050 §3–§5), as plain functions. */
namespace VeyraEchoRules
{
	/**
	 * How an Echo standing at EchoAt repeats a cast its holder aimed from CasterAt at Point, facing Direction: toward
	 * the same point, brought within CastRange of the Echo when the ability has one. A cast with no point of its own,
	 * aimed where its caster stood, is repeated where the Echo stands, facing the same way. Judged on the ground plane.
	 */
	VEYRAABILITIES_API FVeyraEchoAim AimFrom(const FVector& EchoAt, const FVector& CasterAt, const FVector& Point, const FVector& Direction, double CastRange);

	/**
	 * A projected Echo's tether radius at Integrity (ADR-050 §4): min + (max - min) x share ^ exponent, its share of the
	 * Integrity it formed with, so its minimum while any remains.
	 */
	VEYRAABILITIES_API double TetherRadius(const FVeyraEchoProjectionTuning& Projection, double Integrity);

	/**
	 * The Integrity one enemy hit removes (ADR-050 §3), by how it was delivered and whether a Vanguard dealt it. A
	 * developer's damage counts as an ability's; a neutral objective's presence drain never reaches an Echo.
	 */
	VEYRAABILITIES_API double IntegrityLoss(const FVeyraEchoIntegrityLossTuning& Loss, EVeyraDamageDelivery Delivery, bool bFromVanguard);
}
