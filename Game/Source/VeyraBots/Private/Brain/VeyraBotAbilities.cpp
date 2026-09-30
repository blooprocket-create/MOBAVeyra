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
		// A dash reaches as far as it carries the caster and its landing's areas reach beyond, or as far
		// as its opening areas reach.
		Profile.Targeting = EVeyraBotTargeting::Point;
		Profile.Reach = FMath::Max(Dash->Distance + LargestZone(Dash->EndZones), LargestZone(Dash->StartZones));
		Profile.LeadSeconds = Dash->Cast.WindupSeconds;
		Profile.bAwayFromPoint = Dash->Direction == EVeyraDashDirection::AwayFromPoint;
		Profile.CostByRank = Dash->Cast.ResourceCostByRank;
		// Through a unit it names, within its cast range (ADR-030 §6).
		if (Dash->Direction == EVeyraDashDirection::ThroughTarget)
		{
			Profile.Targeting = EVeyraBotTargeting::Unit;
			Profile.Reach = Dash->Cast.CastRange;
		}
		return Profile;
	}
	if (const FVeyraAmbushAbilityTuning* Ambush = UVeyraAbilitiesTuningSubsystem::FindAmbush(Ability))
	{
		// At an enemy Vanguard within its cast range; the cast itself refuses one its caster has not hurt lately.
		Profile.Targeting = EVeyraBotTargeting::Unit;
		Profile.Reach = Ambush->Cast.CastRange;
		Profile.CostByRank = Ambush->Cast.ResourceCostByRank;
		Profile.TargetKinds = { EVeyraUnitKind::Vanguard };
		return Profile;
	}
	if (const FVeyraSelfBuffAbilityTuning* SelfBuff = UVeyraAbilitiesTuningSubsystem::FindSelfBuff(Ability))
	{
		Profile.Targeting = EVeyraBotTargeting::Self;
		Profile.CostByRank = SelfBuff->Cast.ResourceCostByRank;
		Profile.AllyReach = SelfBuff->Recipient == EVeyraBuffRecipient::CasterOrAlly ? SelfBuff->Cast.CastRange : 0.0;
		return Profile;
	}
	if (const FVeyraEmpoweredAttackAbilityTuning* Empowered = UVeyraAbilitiesTuningSubsystem::FindEmpoweredAttack(Ability))
	{
		Profile.Targeting = EVeyraBotTargeting::Self;
		Profile.Reach = AttackRange;
		Profile.CostByRank = Empowered->Cast.ResourceCostByRank;
		return Profile;
	}
	if (const FVeyraVolleyAbilityTuning* Volley = UVeyraAbilitiesTuningSubsystem::FindVolley(Ability))
	{
		// A lane is opened toward an enemy its shots can reach.
		const FVeyraSkillshotAbilityTuning* Shot = UVeyraAbilitiesTuningSubsystem::FindSkillshot(Volley->Shot);
		Profile.Targeting = EVeyraBotTargeting::Point;
		Profile.Reach = Shot ? Shot->Projectile.Range : 0.0;
		Profile.LeadSeconds = Volley->Cast.WindupSeconds;
		Profile.CostByRank = Volley->Cast.ResourceCostByRank;
		return Profile;
	}
	if (const FVeyraTetherAbilityTuning* Tether = UVeyraAbilitiesTuningSubsystem::FindTether(Ability))
	{
		Profile.Targeting = EVeyraBotTargeting::Unit;
		Profile.Reach = Tether->Cast.CastRange;
		Profile.LeadSeconds = Tether->Cast.WindupSeconds;
		Profile.CostByRank = Tether->Cast.ResourceCostByRank;
		Profile.TargetKinds = Tether->TargetKinds;
		return Profile;
	}
	if (const FVeyraAttachAbilityTuning* Attach = UVeyraAbilitiesTuningSubsystem::FindAttach(Ability))
	{
		// A leap at the unit it holds on to.
		Profile.Targeting = EVeyraBotTargeting::Unit;
		Profile.Reach = Attach->Cast.CastRange;
		Profile.LeadSeconds = Attach->Cast.WindupSeconds;
		Profile.CostByRank = Attach->Cast.ResourceCostByRank;
		Profile.TargetKinds = Attach->TargetKinds;
		return Profile;
	}
	if (const FVeyraRideAbilityTuning* Ride = UVeyraAbilitiesTuningSubsystem::FindRide(Ability))
	{
		// A ride reaches as far as it carries its rider; its mounted actions, in the slots it holds, do
		// the rest.
		Profile.Targeting = EVeyraBotTargeting::Self;
		Profile.Reach = Ride->SetSpeed * Ride->DurationSeconds;
		Profile.LeadSeconds = Ride->Cast.WindupSeconds;
		Profile.CostByRank = Ride->Cast.ResourceCostByRank;
		return Profile;
	}
	return {};
}
}
