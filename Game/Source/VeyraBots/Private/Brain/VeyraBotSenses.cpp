// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Brain/VeyraBotSenses.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Brain/VeyraBotAbilities.h"
#include "Brain/VeyraBotLane.h"
#include "Brain/VeyraBotRules.h"
#include "Gold/VeyraGoldComponent.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Cooldowns/VeyraCooldownComponent.h"
#include "Damage/VeyraDamageResolver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fluxborn/VeyraFluxborn.h"
#include "GameFramework/GameStateBase.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Layout/VeyraLayout.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Progression/VeyraProgressionComponent.h"
#include "Recall/VeyraRecallComponent.h"
#include "Structures/VeyraStructure.h"
#include "Structures/VeyraStructureAttackComponent.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraCombatTuningSubsystem.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "VeyraBattlegroundSubsystem.h"
#include "VeyraCombatVerbs.h"
#include "VeyraPlayerState.h"
#include "VeyraTeamStart.h"
#include "Wells/VeyraFluxWell.h"
#include "Wells/VeyraFluxWellSubsystem.h"
#include "Wildlife/VeyraJungleSubsystem.h"
#include "Wildlife/VeyraWildlife.h"
#include "Tools/VeyraVisionToolComponent.h"
#include "VeyraVisionSubsystem.h"
#include "Wards/VeyraWard.h"

namespace VeyraBotSenses
{
namespace
{
	double AttributeOf(const UAbilitySystemComponent* AbilitySystem, const FGameplayAttribute& Attribute)
	{
		return AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(Attribute) ? AbilitySystem->GetNumericAttribute(Attribute) : 0.0;
	}

	FVeyraBotUnit UnitOf(const AActor& Actor)
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Actor);
		FVeyraBotUnit Unit;
		Unit.Actor = const_cast<AActor*>(&Actor);
		Unit.Location = Actor.GetActorLocation();
		Unit.Velocity = Actor.GetVelocity();
		Unit.Health = AttributeOf(AbilitySystem, UVeyraVitalsSet::GetHealthAttribute());
		Unit.MaxHealth = AttributeOf(AbilitySystem, UVeyraVitalsSet::GetMaxHealthAttribute());
		Unit.Radius = Actor.GetSimpleCollisionRadius();
		return Unit;
	}

	const AActor* FindStart(const UWorld& World, EVeyraTeam Team)
	{
		for (TActorIterator<AVeyraTeamStart> It(&World); It; ++It)
		{
			if (It->GetVeyraTeam() == Team)
			{
				return *It;
			}
		}
		return nullptr;
	}

	FVector2D Flat(const FVector& Location)
	{
		return FVector2D(Location.X, Location.Y);
	}

	/**
	 * Its slots as it can use them now: learned, off cooldown, affordable, and not held by another cast;
	 * a Flux Spell also unlocked by its team's permanent Flux (ADR-015 §8).
	 */
	void SenseSlots(const AVeyraPlayerState& Bot, const FVeyraBotVanguardTuning* Behaviour, const FVeyraBotsTuning& Tuning, FVeyraBotView& View)
	{
		const UVeyraAbilityLoadoutComponent* Loadout = Bot.FindComponentByClass<UVeyraAbilityLoadoutComponent>();
		const UVeyraCooldownComponent* Cooldowns = Bot.FindComponentByClass<UVeyraCooldownComponent>();
		const UVeyraProgressionComponent* Progression = Bot.FindComponentByClass<UVeyraProgressionComponent>();
		const UVeyraCastStateComponent* CastState = Bot.FindComponentByClass<UVeyraCastStateComponent>();
		const UAbilitySystemComponent* AbilitySystem = Bot.GetAbilitySystemComponent();
		if (!Behaviour || !Loadout || !Cooldowns || !Progression || !AbilitySystem)
		{
			return;
		}
		const bool bBusy = CastState && CastState->IsBusy();
		for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::All)
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Slot);
			const EVeyraBotAbilityUse* Use = Entry ? Behaviour->Abilities.Find(Entry->Ability) : nullptr;
			const TOptional<FVeyraBotAbilityProfile> Profile = Entry ? VeyraBotAbilities::ProfileOf(Entry->Ability, View.AttackRange) : TOptional<FVeyraBotAbilityProfile>();
			if (!Use || !Profile.IsSet())
			{
				continue;
			}
			FVeyraBotSlot& Seen = View.Slots.AddDefaulted_GetRef();
			Seen.Slot = Slot;
			Seen.Ability = Entry->Ability;
			Seen.Use = *Use;
			Seen.Profile = Profile.GetValue();
			const int32 Rank = Progression->IsInitialized() ? Progression->GetRank(Slot) : 0;
			Seen.Cost = Rank >= 1 ? VeyraAbilityRules::ValueAtRank(Seen.Profile.CostByRank, Rank) : 0.0;
			Seen.bReady = Rank >= 1 && !bBusy && Cooldowns->GetRemainingSeconds(Loadout->CooldownIdOf(Entry->Ability), View.Now) <= 0.0
				&& VeyraCombat::CanAffordResource(*AbilitySystem, Seen.Cost);
		}
		// Its Flux Spells: no ranks, what each is for from its seat's data.
		for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Spells)
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Slot);
			const EVeyraBotAbilityUse* Use = Entry ? Tuning.FluxSpells.Find(Entry->Ability) : nullptr;
			const TOptional<FVeyraBotAbilityProfile> Profile = Entry ? VeyraBotAbilities::ProfileOf(Entry->Ability, View.AttackRange) : TOptional<FVeyraBotAbilityProfile>();
			if (!Use || !Profile.IsSet())
			{
				continue;
			}
			FVeyraBotSlot& Seen = View.Slots.AddDefaulted_GetRef();
			Seen.Slot = Slot;
			Seen.Ability = Entry->Ability;
			Seen.Use = *Use;
			Seen.Profile = Profile.GetValue();
			constexpr int32 SpellRank = 1;
			Seen.Cost = VeyraAbilityRules::ValueAtRank(Seen.Profile.CostByRank, SpellRank);
			Seen.bReady = !Loadout->IsLocked(Slot) && !bBusy && Cooldowns->GetRemainingSeconds(Entry->Ability, View.Now) <= 0.0
				&& VeyraCombat::CanAffordResource(*AbilitySystem, Seen.Cost);
		}
		// Its items' Actives: no ranks, cooling down under their own IDs, what each is for from the data (ADR-051 §6).
		for (const EVeyraAbilitySlot Slot : VeyraAbilitySlots::Items)
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(Slot);
			const EVeyraBotAbilityUse* Use = Entry ? Tuning.ItemActives.Find(Entry->Ability) : nullptr;
			const TOptional<FVeyraBotAbilityProfile> Profile = Entry ? VeyraBotAbilities::ProfileOf(Entry->Ability, View.AttackRange) : TOptional<FVeyraBotAbilityProfile>();
			if (!Use || *Use == EVeyraBotAbilityUse::Never || !Profile.IsSet())
			{
				continue;
			}
			FVeyraBotSlot& Seen = View.Slots.AddDefaulted_GetRef();
			Seen.Slot = Slot;
			Seen.Ability = Entry->Ability;
			Seen.Use = *Use;
			Seen.Profile = Profile.GetValue();
			constexpr int32 ActiveRank = 1;
			Seen.Cost = VeyraAbilityRules::ValueAtRank(Seen.Profile.CostByRank, ActiveRank);
			Seen.bReady = !bBusy && Cooldowns->GetRemainingSeconds(Entry->Ability, View.Now) <= 0.0 && VeyraCombat::CanAffordResource(*AbilitySystem, Seen.Cost);
		}
	}
}

FVeyraBotView Sense(const AVeyraPlayerState& Bot, EVeyraBotRole Role, bool bWards, const FVeyraBotsTuning& Tuning)
{
	const EVeyraLane Lane = VeyraBots::LaneOf(Role);
	FVeyraBotView View;
	View.bJungle = Role == EVeyraBotRole::Jungle;
	View.bWards = bWards;
	const UWorld* World = Bot.GetWorld();
	const APawn* Body = Bot.GetPawn();
	if (!World)
	{
		return View;
	}
	View.Now = World->GetTimeSeconds();
	View.bAlive = Body && VeyraTargeting::IsAlive(&Bot);
	if (const UVeyraRecallComponent* Recall = Bot.FindComponentByClass<UVeyraRecallComponent>())
	{
		View.bRecalling = Recall->IsRecalling();
	}
	const UVeyraInventoryComponent* Inventory = Bot.FindComponentByClass<UVeyraInventoryComponent>();
	const UVeyraGoldComponent* Gold = Bot.FindComponentByClass<UVeyraGoldComponent>();
	const FVeyraBotVanguardTuning* Behaviour = Tuning.Vanguards.Find(Bot.GetVanguardId());
	if (Inventory)
	{
		View.bAtFountain = Inventory->IsAtFountain();
	}
	if (Inventory && Gold && Behaviour)
	{
		View.Gold = Gold->GetGold();
		View.bPurchaseWaiting =
			VeyraBotRules::NextPurchase(UVeyraItemsTuningSubsystem::Get(), Behaviour->Build, Inventory->GetSlots(), Inventory->GetQueue(), Inventory->GetMythical(), View.Gold).IsSet();
	}
	const EVeyraTeam Team = Bot.GetVeyraTeam();
	const AActor* Start = FindStart(*World, Team);
	View.Home = Start ? Start->GetActorLocation() : FVector::ZeroVector;
	if (!View.bAlive)
	{
		return View;
	}
	View.Self = UnitOf(*Body);

	// Its basic attack: reach, and what one does before the target's resistance.
	const UAbilitySystemComponent* AbilitySystem = Bot.GetAbilitySystemComponent();
	View.MaxResource = AttributeOf(AbilitySystem, UVeyraResourceSet::GetMaxResourceAttribute());
	View.Resource = AttributeOf(AbilitySystem, UVeyraResourceSet::GetResourceAttribute());
	const UVeyraBasicAttackComponent* Attacks = Bot.FindComponentByClass<UVeyraBasicAttackComponent>();
	const bool bPhysicalAttack = !Attacks || Attacks->GetProfile().DamageType != EVeyraDamageType::Magic;
	if (Attacks && Attacks->HasProfile())
	{
		const FVeyraBasicAttackProfile& Profile = Attacks->GetProfile();
		View.AttackRange = Attacks->GetRange(nullptr);
		View.AttackDamage = AttributeOf(AbilitySystem, UVeyraOffenceSet::GetPhysicalPowerAttribute()) * Profile.PhysicalPowerRatio
			+ AttributeOf(AbilitySystem, UVeyraOffenceSet::GetMagicPowerAttribute()) * Profile.MagicPowerRatio;
	}

	// Vanguards in sight, by side.
	const double Sight = Tuning.Senses.SightRadius;
	const auto InSight = [&View, Sight](const FVector& Location) { return FVector::Dist2D(View.Self.Location, Location) <= Sight; };
	for (const APlayerState* Member : World->GetGameState() ? World->GetGameState()->PlayerArray : TArray<TObjectPtr<APlayerState>>())
	{
		const APawn* Other = Member && Member != &Bot ? Member->GetPawn() : nullptr;
		if (!Other || !VeyraTargeting::IsAlive(Member) || !InSight(Other->GetActorLocation()))
		{
			continue;
		}
		// A bot knows only what its side sees, as a player does (ADR-016 §7).
		const bool bEnemy = VeyraTargeting::AreHostile(&Bot, Member);
		if (bEnemy && !VeyraTargeting::CanAcquire(&Bot, *Other))
		{
			continue;
		}
		(bEnemy ? View.EnemyVanguards : View.AllyVanguards).Add(UnitOf(*Other));
	}

	// Fluxborn in sight, each with how much of its attack it takes; and where the bot's wave stands.
	UVeyraBattlegroundSubsystem* Battleground = World->GetSubsystem<UVeyraBattlegroundSubsystem>();
	const FVeyraBattlegroundLayout* Layout = Battleground ? Battleground->GetLayout() : nullptr;
	const FVeyraLaneLayout* LaneLayout = Layout ? Layout->Lanes.FindByPredicate([Lane](const FVeyraLaneLayout& Candidate) { return Candidate.Lane == Lane; }) : nullptr;
	const TArray<FVector2D> Path = LaneLayout ? VeyraLayout::Waypoints(*LaneLayout, Team) : TArray<FVector2D>();
	const double MitigationConstant = UVeyraCombatTuningSubsystem::Get().Resistance.MitigationConstant;
	const double AnswerRange = UVeyraWorldTuningSubsystem::Get().Fluxborn.Ai.AggressionResponseRange;
	TOptional<double> WaveFront;
	TArray<const AVeyraFluxborn*> Allies;
	const TArray<AVeyraFluxborn*> Everyone = Battleground ? Battleground->GetFluxborn() : TArray<AVeyraFluxborn*>();
	for (const AVeyraFluxborn* Fluxborn : Everyone)
	{
		if (!Fluxborn || !Fluxborn->IsAlive())
		{
			continue;
		}
		if (Fluxborn->GetVeyraTeam() == Team)
		{
			Allies.Add(Fluxborn);
			if (!Path.IsEmpty() && Fluxborn->GetLane() == Lane)
			{
				const double Along = VeyraBotLane::DistanceAlong(Path, Flat(Fluxborn->GetActorLocation()));
				WaveFront = FMath::Max(WaveFront.Get(Along), Along);
			}
		}
		else if (InSight(Fluxborn->GetActorLocation()) && VeyraTargeting::CanAcquire(&Bot, *Fluxborn))
		{
			FVeyraBotUnit& Seen = View.EnemyFluxborn.Add_GetRef(UnitOf(*Fluxborn));
			const UAbilitySystemComponent* Defender = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Fluxborn);
			const double Resistance = AttributeOf(Defender, bPhysicalAttack ? UVeyraDefenceSet::GetArmorAttribute() : UVeyraDefenceSet::GetMagicResistAttribute());
			Seen.DamageTaken = VeyraDamage::ResistanceDamageMultiplier(Resistance, MitigationConstant);
		}
	}

	// Each enemy Vanguard's Fluxborn that would answer a hit on it: World's own predicate, edge to edge
	// from the defender, within their aggression response range (Battleground Bible §19).
	for (FVeyraBotUnit& Enemy : View.EnemyVanguards)
	{
		const AActor* Defender = Enemy.Actor.Get();
		for (const AVeyraFluxborn* Fluxborn : Everyone)
		{
			const bool bDefends = Defender && Fluxborn && Fluxborn->IsAlive() && Fluxborn->GetVeyraTeam() != Team
				&& VeyraTargeting::EdgeToEdgeDistance(*Fluxborn, *Defender) <= AnswerRange;
			Enemy.Defenders += bDefends ? 1 : 0;
		}
	}

	// The next enemy structure along its lane, or the nearest of the enemy base's when the lane is down;
	// and its own outermost standing structure in the lane.
	double OwnStructure = 0.0;
	const AVeyraStructure* Enemy = nullptr;
	double EnemyAlong = TNumericLimits<double>::Max();
	static const TArray<TObjectPtr<AVeyraStructure>> NoStructures;
	for (const AVeyraStructure* Structure : Battleground ? Battleground->GetStructures() : NoStructures)
	{
		if (!Structure || Structure->IsDestroyed())
		{
			continue;
		}
		const TOptional<EVeyraLane> StructureLane = Structure->GetLane();
		const bool bInLane = !StructureLane.IsSet() || StructureLane.GetValue() == Lane;
		const double Along = Path.IsEmpty() ? FVector::Dist2D(View.Self.Location, Structure->GetActorLocation())
											: VeyraBotLane::DistanceAlong(Path, Flat(Structure->GetActorLocation()));
		if (Structure->GetVeyraTeam() == Team)
		{
			OwnStructure = bInLane && StructureLane.IsSet() ? FMath::Max(OwnStructure, Along) : OwnStructure;
		}
		else if (bInLane && Along < EnemyAlong)
		{
			Enemy = Structure;
			EnemyAlong = Along;
		}
	}
	TOptional<double> EnemyReach;
	bool bWaveHoldsTower = false;
	if (Enemy)
	{
		FVeyraBotStructure& Seen = View.EnemyStructure.Emplace();
		Seen.Unit = UnitOf(*Enemy);
		Seen.bVulnerable = !Enemy->IsInvulnerable();
		if (const UVeyraStructureAttackComponent* Attack = Enemy->GetAttack())
		{
			Seen.AttackRange = UVeyraWorldTuningSubsystem::Get().TowerAttack.Range;
			Seen.bTargetsBot = Attack->GetTarget() == Body;
			Seen.bAlliesInRange = Allies.ContainsByPredicate([Attack](const AVeyraFluxborn* Ally) { return Attack->IsInRange(*Ally); });
			bWaveHoldsTower = Seen.bAlliesInRange;
			EnemyReach = EnemyAlong - Seen.AttackRange - Tuning.Senses.TowerMargin - Seen.Unit.Radius - View.Self.Radius;
		}
	}

	// Where it holds: behind its wave, at its outermost structure without one, and out of the enemy
	// tower's reach unless its wave holds the tower. Without lanes, halfway between the starts.
	if (!Path.IsEmpty())
	{
		const double Hold = VeyraBotLane::HoldDistance(WaveFront, OwnStructure, EnemyReach, bWaveHoldsTower, Tuning.Positioning.FollowDistance);
		const FVector2D Point = VeyraBotLane::PointAt(Path, Hold);
		View.LaneHold = FVector(Point.X, Point.Y, View.Self.Location.Z);
	}
	else
	{
		const AActor* EnemyStart = FindStart(*World, VeyraTeams::Opposing(Team));
		View.LaneHold = EnemyStart ? (View.Home + EnemyStart->GetActorLocation()) / 2.0 : View.Self.Location;
		View.LaneHold.Z = View.Self.Location.Z;
	}

	// Its side's camps and the open Flux Wells (ADR-014 §7); and for a jungler, the hurt enemies it might gank.
	if (const UVeyraJungleSubsystem* Jungle = World->GetSubsystem<UVeyraJungleSubsystem>())
	{
		for (const FVeyraCampState& Camp : Jungle->GetCamps())
		{
			if (Camp.Half != Team)
			{
				continue;
			}
			FVeyraBotCamp& Seen = View.Camps.AddDefaulted_GetRef();
			Seen.Center = FVector(Camp.Center, View.Self.Location.Z);
			Seen.SpawnsAt = Camp.SpawnsAt;
			for (const AVeyraWildlife* Creature : Jungle->GetCreatures(Camp.Index))
			{
				++Seen.Standing;
				if (VeyraTargeting::CanAcquire(&Bot, *Creature))
				{
					Seen.Creatures.Add(UnitOf(*Creature));
				}
			}
		}
	}
	// What a warding seat needs (ADR-016 §7): its charges, the fog patches, and where its side's wards stand.
	if (View.bWards)
	{
		const UVeyraVisionToolComponent* Tool = Bot.FindComponentByClass<UVeyraVisionToolComponent>();
		View.WardCharges = Tool && Tool->GetEquipped() == EVeyraVisionTool::PersistentWard ? Tool->GetWardCharges() : 0;
		if (const UVeyraVisionSubsystem* Vision = World->GetSubsystem<UVeyraVisionSubsystem>())
		{
			for (const FVeyraFogCircle& Patch : Vision->GetDenseFog())
			{
				View.WardSpots.Add(FVector(Patch.Center, View.Self.Location.Z));
			}
		}
		for (TActorIterator<AVeyraWard> It(World); It; ++It)
		{
			if (It->IsAlive() && It->GetVeyraTeam() == Team)
			{
				View.AlliedWards.Add(It->GetActorLocation());
			}
		}
	}
	if (const UVeyraFluxWellSubsystem* Wells = World->GetSubsystem<UVeyraFluxWellSubsystem>())
	{
		for (const AVeyraFluxWell* Well : Wells->GetWells())
		{
			if (Well && Well->GetState() == EVeyraFluxWellState::Open && Well->IsStanding())
			{
				View.Wells.Add(UnitOf(*Well));
			}
		}
	}
	if (View.bJungle)
	{
		for (const APlayerState* Member : World->GetGameState() ? World->GetGameState()->PlayerArray : TArray<TObjectPtr<APlayerState>>())
		{
			const APawn* Other = Member ? Member->GetPawn() : nullptr;
			if (Other && VeyraTargeting::IsAlive(Member) && VeyraTargeting::AreHostile(&Bot, Member)
				&& FVector::Dist2D(View.Self.Location, Other->GetActorLocation()) <= Tuning.Jungle.GankRange && VeyraTargeting::CanAcquire(&Bot, *Other))
			{
				View.GankTargets.Add(UnitOf(*Other));
			}
		}
	}

	SenseSlots(Bot, Behaviour, Tuning, View);
	return View;
}
}
