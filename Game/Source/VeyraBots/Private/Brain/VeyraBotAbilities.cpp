// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Brain/VeyraBotAbilities.h"

#include "Shapes/VeyraShapes.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

namespace VeyraBotAbilities
{
namespace
{
	/** How far from its origin a shape reaches. */
	double ExtentOf(const FVeyraShape& Shape)
	{
		return Shape.Kind == EVeyraShapeKind::Rectangle ? Shape.Length : Shape.Radius;
	}

	double LargestZone(TConstArrayView<FVeyraAreaZoneTuning> Zones)
	{
		double Extent = 0.0;
		for (const FVeyraAreaZoneTuning& Zone : Zones)
		{
			Extent = FMath::Max(Extent, ExtentOf(Zone.Shape));
		}
		return Extent;
	}
}

TOptional<FVeyraBotAbilityProfile> ProfileOf(const FVeyraContentId& Ability, double AttackRange)
{
	FVeyraBotAbilityProfile Profile;
	if (const FVeyraTargetedDamageAbilityTuning* Targeted = UVeyraAbilitiesTuningSubsystem::FindTargetedDamage(Ability))
	{
		Profile.Targeting = EVeyraBotTargeting::Unit;
		Profile.Reach = Targeted->CastRange;
		Profile.CostByRank = { Targeted->ResourceCost };
		Profile.Damage = Targeted->DamageAmount;
		Profile.bTrueDamage = Targeted->DamageType == EVeyraDamageType::TrueDamage;
		Profile.TargetKinds = Targeted->TargetKinds;
		return Profile;
	}
	if (const FVeyraAreaAbilityTuning* Area = UVeyraAbilitiesTuningSubsystem::FindArea(Ability))
	{
		// A point-placed area reaches its cast range and then its shape; one at the caster, its shape.
		Profile.Targeting = EVeyraBotTargeting::Point;
		Profile.Reach = (Area->Origin == EVeyraAreaOrigin::TargetPoint ? Area->Cast.CastRange : 0.0) + LargestZone(Area->Zones);
		Profile.LeadSeconds = Area->Cast.WindupSeconds + Area->DelaySeconds;
		Profile.CostByRank = Area->Cast.ResourceCostByRank;
		return Profile;
	}
	if (const FVeyraSkillshotAbilityTuning* Skillshot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Ability))
	{
		Profile.Targeting = EVeyraBotTargeting::Point;
		Profile.Reach = Skillshot->Projectile.Range;
		Profile.LeadSeconds = Skillshot->Cast.WindupSeconds;
		Profile.ProjectileSpeed = Skillshot->Projectile.Speed;
		Profile.CostByRank = Skillshot->Cast.ResourceCostByRank;
		return Profile;
	}
	if (const FVeyraDashAbilityTuning* Dash = UVeyraAbilitiesTuningSubsystem::FindDash(Ability))
	{
		// A dash reaches as far as it carries the caster, or as far as its opening areas reach.
		Profile.Targeting = EVeyraBotTargeting::Point;
		Profile.Reach = FMath::Max(Dash->Distance, LargestZone(Dash->StartZones));
		Profile.LeadSeconds = Dash->Cast.WindupSeconds;
		Profile.bAwayFromPoint = Dash->Direction == EVeyraDashDirection::AwayFromPoint;
		Profile.CostByRank = Dash->Cast.ResourceCostByRank;
		return Profile;
	}
	if (const FVeyraSelfBuffAbilityTuning* SelfBuff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability))
	{
		Profile.Targeting = EVeyraBotTargeting::Self;
		Profile.CostByRank = SelfBuff->Cast.ResourceCostByRank;
		return Profile;
	}
	if (const FVeyraEmpoweredAttackAbilityTuning* Empowered = UVeyraAbilitiesTuningSubsystem::FindEmpoweredAttack(Ability))
	{
		Profile.Targeting = EVeyraBotTargeting::Self;
		Profile.Reach = AttackRange;
		Profile.CostByRank = Empowered->Cast.ResourceCostByRank;
		return Profile;
	}
	return {};
}
}
