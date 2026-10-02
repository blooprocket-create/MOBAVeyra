// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Echoes/VeyraEchoRules.h"

#include "Tuning/VeyraAbilitiesTuning.h"

namespace VeyraEchoRules
{
FVeyraEchoAim AimFrom(const FVector& EchoAt, const FVector& CasterAt, const FVector& Point, const FVector& Direction, double CastRange)
{
	FVector Aimed = Point - CasterAt;
	Aimed.Z = 0.0;
	if (Aimed.IsNearlyZero())
	{
		return FVeyraEchoAim{ EchoAt, Direction };
	}
	FVector Offset = Point - EchoAt;
	Offset.Z = 0.0;
	// A point beyond the ability's range from the Echo is brought within it, as any cast's is (ADR-008 §9).
	if (CastRange > 0.0 && Offset.Size() > CastRange)
	{
		Offset = Offset.GetSafeNormal() * CastRange;
	}
	return FVeyraEchoAim{ EchoAt + Offset, Offset.IsNearlyZero() ? Direction : Offset.GetSafeNormal() };
}

double TetherRadius(const FVeyraEchoProjectionTuning& Projection, double Integrity)
{
	const double Share = Projection.Integrity > 0.0 ? FMath::Clamp(Integrity / Projection.Integrity, 0.0, 1.0) : 0.0;
	return Projection.MinRadius + (Projection.MaxRadius - Projection.MinRadius) * FMath::Pow(Share, Projection.RadiusExponent);
}

double IntegrityLoss(const FVeyraEchoIntegrityLossTuning& Loss, EVeyraDamageDelivery Delivery, bool bFromVanguard)
{
	switch (Delivery)
	{
	case EVeyraDamageDelivery::BasicAttack:
		return bFromVanguard ? Loss.VanguardBasicAttack : Loss.UnitBasicAttack;
	case EVeyraDamageDelivery::Ability:
	case EVeyraDamageDelivery::Developer:
		return Loss.Ability;
	case EVeyraDamageDelivery::StructureAttack:
		return Loss.StructureAttack;
	case EVeyraDamageDelivery::Periodic:
		return Loss.Periodic;
	case EVeyraDamageDelivery::Proc:
		return Loss.Proc;
	case EVeyraDamageDelivery::Presence:
		return 0.0;
	}
	return 0.0;
}
}
