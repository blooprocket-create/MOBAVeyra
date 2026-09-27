// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraCombatVerbs.h"

#include "AbilitySystemComponent.h"
#include "Absorption/VeyraDamageAbsorptionComponent.h"
#include "Attributes/VeyraAttributePolicy.h"
#include "Attributes/VeyraDefenceSet.h"
#include "Attributes/VeyraMobilitySet.h"
#include "Attributes/VeyraOffenceSet.h"
#include "Attributes/VeyraResourceSet.h"
#include "Attributes/VeyraVitalsSet.h"
#include "Effects/VeyraCombatEffects.h"
#include "Effects/VeyraResourceSpendExecution.h"
#include "Life/VeyraLifeComponent.h"
#include "Movement/VeyraMovementComponent.h"
#include "Records/VeyraCombatRecords.h"
#include "Statuses/VeyraStatusComponent.h"
#include "Tags/VeyraHealthTags.h"
#include "VeyraCombatLog.h"
#include "VeyraCombatTagMapping.h"

namespace VeyraCombat
{
namespace
{
	// Veyra effects take every magnitude from SetByCaller data, never from Gameplay Ability System
	// level curves, so each spec uses the system's default effect level.
	constexpr float UnscaledEffectLevel = 1.0f;

	bool IsPositiveFinite(double Value)
	{
		return FMath::IsFinite(Value) && Value > 0.0;
	}

	bool IsNonNegativeFinite(double Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0;
	}

	/** Every base stat a stat block sets, paired with its value in the block. */
	struct FStatEntry
	{
		FGameplayAttribute Attribute;
		double Value;
	};

	TArray<FStatEntry, TInlineAllocator<9>> StatEntries(const FVeyraStatBlock& Stats)
	{
		return {
			{ UVeyraVitalsSet::GetMaxHealthAttribute(), Stats.MaxHealth },
			{ UVeyraResourceSet::GetMaxResourceAttribute(), Stats.MaxResource },
			{ UVeyraResourceSet::GetResourceRegenAttribute(), Stats.ResourceRegen },
			{ UVeyraDefenceSet::GetArmorAttribute(), Stats.Armor },
			{ UVeyraDefenceSet::GetMagicResistAttribute(), Stats.MagicResist },
			{ UVeyraOffenceSet::GetPhysicalPowerAttribute(), Stats.PhysicalPower },
			{ UVeyraOffenceSet::GetMagicPowerAttribute(), Stats.MagicPower },
			{ UVeyraOffenceSet::GetAttackSpeedAttribute(), Stats.AttackSpeed },
			{ UVeyraMobilitySet::GetMoveSpeedAttribute(), Stats.MoveSpeed },
		};
	}

	bool HasEveryStatSet(const UAbilitySystemComponent& AbilitySystem)
	{
		return AbilitySystem.GetSet<UVeyraVitalsSet>() && AbilitySystem.GetSet<UVeyraResourceSet>() && AbilitySystem.GetSet<UVeyraDefenceSet>()
			&& AbilitySystem.GetSet<UVeyraOffenceSet>() && AbilitySystem.GetSet<UVeyraMobilitySet>();
	}

	bool IsDeadUnit(const UAbilitySystemComponent& AbilitySystem)
	{
		const AActor* Owner = AbilitySystem.GetOwner();
		const UVeyraLifeComponent* Life = Owner ? Owner->FindComponentByClass<UVeyraLifeComponent>() : nullptr;
		return Life && !Life->IsAlive();
	}

	/** The movement of the unit's body, its avatar; none before it has one. */
	UVeyraMovementComponent* FindMovement(const UAbilitySystemComponent& AbilitySystem)
	{
		const AActor* Body = AbilitySystem.GetAvatarActor();
		return Body ? Body->FindComponentByClass<UVeyraMovementComponent>() : nullptr;
	}

	FActiveGameplayEffectHandle GrantAbsorption(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target,
		TSubclassOf<UGameplayEffect> EffectClass, const FGameplayTag& AmountTag, double Amount, double DurationSeconds, const TCHAR* What)
	{
		if (!IsPositiveFinite(Amount) || !IsPositiveFinite(DurationSeconds))
		{
			UE_LOG(LogVeyraCombat, Error, TEXT("Refused %s of %g for %g s on %s: both must be finite and above 0."),
				What, Amount, DurationSeconds, *GetNameSafe(Target.GetOwner()));
			return FActiveGameplayEffectHandle();
		}
		const FGameplayEffectSpecHandle Spec = Source.MakeOutgoingSpec(EffectClass, UnscaledEffectLevel, Source.MakeEffectContext());
		if (!Spec.IsValid())
		{
			UE_LOG(LogVeyraCombat, Error, TEXT("Could not create %s for %s."), What, *GetNameSafe(Target.GetOwner()));
			return FActiveGameplayEffectHandle();
		}
		Spec.Data->SetSetByCallerMagnitude(AmountTag, static_cast<float>(Amount));
		Spec.Data->SetDuration(static_cast<float>(DurationSeconds), /*bLockDuration*/ true);
		return Source.ApplyGameplayEffectSpecToTarget(*Spec.Data, &Target);
	}
}

void ConfigureCombatant(UAbilitySystemComponent& AbilitySystem, UVeyraDamageAbsorptionComponent& Absorption, UVeyraStatusComponent& Statuses)
{
	AbilitySystem.GameplayEffectApplicationQueries.Add(FGameplayEffectApplicationQuery::CreateStatic(&VeyraAttributePolicy::AllowsSpec));
	Absorption.BindTo(AbilitySystem);
	Statuses.BindTo(AbilitySystem);
}

bool InitializeVitals(UAbilitySystemComponent& AbilitySystem, double MaxHealth)
{
	if (!AbilitySystem.GetSet<UVeyraVitalsSet>() || !IsPositiveFinite(MaxHealth))
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused to initialize vitals on %s with Max Health %g: it needs a UVeyraVitalsSet and a finite Max Health above 0."),
			*GetNameSafe(AbilitySystem.GetOwner()), MaxHealth);
		return false;
	}
	AbilitySystem.SetNumericAttributeBase(UVeyraVitalsSet::GetMaxHealthAttribute(), static_cast<float>(MaxHealth));
	AbilitySystem.SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()));
	return true;
}

bool InitializeMoveSpeed(UAbilitySystemComponent& AbilitySystem, double MoveSpeed)
{
	if (!AbilitySystem.GetSet<UVeyraMobilitySet>() || !IsPositiveFinite(MoveSpeed))
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused to initialize Move Speed on %s with %g: it needs a UVeyraMobilitySet and a finite speed above 0."),
			*GetNameSafe(AbilitySystem.GetOwner()), MoveSpeed);
		return false;
	}
	AbilitySystem.SetNumericAttributeBase(UVeyraMobilitySet::GetMoveSpeedAttribute(), static_cast<float>(MoveSpeed));
	return true;
}

bool InitializeResource(UAbilitySystemComponent& AbilitySystem, double MaxResource)
{
	if (!AbilitySystem.GetSet<UVeyraResourceSet>() || !FMath::IsFinite(MaxResource) || MaxResource < 0.0)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused to initialize the resource on %s with Max Resource %g: it needs a UVeyraResourceSet and a finite Max Resource of at least 0."),
			*GetNameSafe(AbilitySystem.GetOwner()), MaxResource);
		return false;
	}
	AbilitySystem.SetNumericAttributeBase(UVeyraResourceSet::GetMaxResourceAttribute(), static_cast<float>(MaxResource));
	AbilitySystem.SetNumericAttributeBase(UVeyraResourceSet::GetResourceAttribute(), AbilitySystem.GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute()));
	return true;
}

bool InitializeStats(UAbilitySystemComponent& AbilitySystem, const FVeyraStatBlock& Stats)
{
	bool bValid = HasEveryStatSet(AbilitySystem) && IsPositiveFinite(Stats.MaxHealth) && IsPositiveFinite(Stats.MoveSpeed) && IsPositiveFinite(Stats.AttackSpeed);
	for (const FStatEntry& Entry : StatEntries(Stats))
	{
		bValid &= IsNonNegativeFinite(Entry.Value);
	}
	if (!bValid)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused to initialize the stats of %s: it needs every Veyra attribute set, Max Health, Move Speed and Attack Speed above 0, and every other stat finite and at least 0."),
			*GetNameSafe(AbilitySystem.GetOwner()));
		return false;
	}
	for (const FStatEntry& Entry : StatEntries(Stats))
	{
		AbilitySystem.SetNumericAttributeBase(Entry.Attribute, static_cast<float>(Entry.Value));
	}
	AbilitySystem.SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()));
	AbilitySystem.SetNumericAttributeBase(UVeyraResourceSet::GetResourceAttribute(), AbilitySystem.GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute()));
	return true;
}

bool GrowBaseStats(UAbilitySystemComponent& AbilitySystem, const FVeyraStatBlock& Growth)
{
	bool bValid = HasEveryStatSet(AbilitySystem);
	for (const FStatEntry& Entry : StatEntries(Growth))
	{
		bValid &= IsNonNegativeFinite(Entry.Value);
	}
	if (!bValid)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused to grow the stats of %s: it needs every Veyra attribute set, and every growth finite and at least 0."),
			*GetNameSafe(AbilitySystem.GetOwner()));
		return false;
	}

	const FGameplayAttribute Health = UVeyraVitalsSet::GetHealthAttribute();
	const FGameplayAttribute MaxHealth = UVeyraVitalsSet::GetMaxHealthAttribute();
	const FGameplayAttribute Resource = UVeyraResourceSet::GetResourceAttribute();
	const FGameplayAttribute MaxResource = UVeyraResourceSet::GetMaxResourceAttribute();
	const float MissingHealth = AbilitySystem.GetNumericAttribute(MaxHealth) - AbilitySystem.GetNumericAttribute(Health);
	const float MissingResource = AbilitySystem.GetNumericAttribute(MaxResource) - AbilitySystem.GetNumericAttribute(Resource);

	for (const FStatEntry& Entry : StatEntries(Growth))
	{
		if (Entry.Value > 0.0)
		{
			AbilitySystem.SetNumericAttributeBase(Entry.Attribute, AbilitySystem.GetNumericAttributeBase(Entry.Attribute) + static_cast<float>(Entry.Value));
		}
	}

	// A dead unit stays at 0 Health; it is revived with full Health anyway (Combat Bible §18).
	if (!IsDeadUnit(AbilitySystem))
	{
		AbilitySystem.SetNumericAttributeBase(Health, AbilitySystem.GetNumericAttribute(MaxHealth) - MissingHealth);
	}
	AbilitySystem.SetNumericAttributeBase(Resource, AbilitySystem.GetNumericAttribute(MaxResource) - MissingResource);
	return true;
}

bool RestoreResource(UAbilitySystemComponent& AbilitySystem, double Amount)
{
	if (!AbilitySystem.GetSet<UVeyraResourceSet>() || !IsNonNegativeFinite(Amount))
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused to restore %g resource on %s: it needs a UVeyraResourceSet and an amount that is finite and at least 0."),
			Amount, *GetNameSafe(AbilitySystem.GetOwner()));
		return false;
	}
	// The resource set keeps Resource within [0, Max Resource].
	const FGameplayAttribute Resource = UVeyraResourceSet::GetResourceAttribute();
	AbilitySystem.SetNumericAttributeBase(Resource, AbilitySystem.GetNumericAttributeBase(Resource) + static_cast<float>(Amount));
	return true;
}

bool CanAffordResource(const UAbilitySystemComponent& AbilitySystem, double Amount)
{
	return Amount <= 0.0 || (AbilitySystem.GetSet<UVeyraResourceSet>() && AbilitySystem.GetNumericAttribute(UVeyraResourceSet::GetResourceAttribute()) >= Amount);
}

bool SpendResource(UAbilitySystemComponent& AbilitySystem, double Amount)
{
	if (!FMath::IsFinite(Amount) || Amount < 0.0)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused a resource cost of %g on %s: it must be finite and at least 0."), Amount, *GetNameSafe(AbilitySystem.GetOwner()));
		return false;
	}
	if (!CanAffordResource(AbilitySystem, Amount))
	{
		return false;
	}
	if (Amount == 0.0)
	{
		return true;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem.MakeOutgoingSpec(UVeyraResourceSpendEffect::StaticClass(), UnscaledEffectLevel, AbilitySystem.MakeEffectContext());
	if (!Spec.IsValid())
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Could not create a resource cost for %s."), *GetNameSafe(AbilitySystem.GetOwner()));
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(UVeyraResourceSpendExecution::ResourceCostName, static_cast<float>(Amount));
	AbilitySystem.ApplyGameplayEffectSpecToSelf(*Spec.Data);
	return true;
}

bool Revive(UAbilitySystemComponent& AbilitySystem)
{
	AActor* Owner = AbilitySystem.GetOwner();
	UVeyraLifeComponent* Life = Owner ? Owner->FindComponentByClass<UVeyraLifeComponent>() : nullptr;
	if (!Life || !Life->SetState(EVeyraLifeState::Alive))
	{
		return false;
	}
	AbilitySystem.SetNumericAttributeBase(UVeyraVitalsSet::GetHealthAttribute(), AbilitySystem.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()));
	if (AbilitySystem.GetSet<UVeyraResourceSet>())
	{
		AbilitySystem.SetNumericAttributeBase(UVeyraResourceSet::GetResourceAttribute(), AbilitySystem.GetNumericAttribute(UVeyraResourceSet::GetMaxResourceAttribute()));
	}
	return true;
}

FVeyraPreparedDamage PrepareDamage(UAbilitySystemComponent& Source, const FVeyraRawDamageEvent& Damage)
{
	TArray<EVeyraDamageType, TInlineAllocator<3>> Types;
	bool bValid = !Damage.Components.IsEmpty();
	for (const FVeyraDamageComponent& Component : Damage.Components)
	{
		bValid &= !Types.Contains(Component.Type) && IsNonNegativeFinite(Component.Amount);
		Types.Add(Component.Type);
	}
	for (const FVeyraPenetration* Penetration : { &Damage.PhysicalPenetration, &Damage.MagicPenetration })
	{
		bValid &= IsNonNegativeFinite(Penetration->Flat) && IsNonNegativeFinite(Penetration->Retained) && Penetration->Retained <= 1.0;
	}
	if (!bValid)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused damage from %s: it needs at least one component, each type at most once, with a finite amount of at least 0, and penetration with Flat at least 0 and Retained within [0, 1]."),
			*GetNameSafe(Source.GetOwner()));
		return FVeyraPreparedDamage();
	}

	// Making the spec captures the source's offence now (UVeyraDamageExecution snapshots it).
	FVeyraPreparedDamage Prepared;
	Prepared.Spec = Source.MakeOutgoingSpec(UVeyraDamageEffect::StaticClass(), UnscaledEffectLevel, Source.MakeEffectContext());
	if (!Prepared.IsValid())
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Could not create a damage effect from %s."), *GetNameSafe(Source.GetOwner()));
		return FVeyraPreparedDamage();
	}
	for (const FVeyraDamageComponent& Component : Damage.Components)
	{
		Prepared.Spec.Data->SetSetByCallerMagnitude(VeyraCombatTagMapping::DamageTypeTag(Component.Type), static_cast<float>(Component.Amount));
	}
	const TPair<FName, double> EventPenetration[] = {
		{ UVeyraDamageEffect::PhysicalPenetrationFlatName, Damage.PhysicalPenetration.Flat },
		{ UVeyraDamageEffect::PhysicalPenetrationRetainedName, Damage.PhysicalPenetration.Retained },
		{ UVeyraDamageEffect::MagicPenetrationFlatName, Damage.MagicPenetration.Flat },
		{ UVeyraDamageEffect::MagicPenetrationRetainedName, Damage.MagicPenetration.Retained },
	};
	for (const TPair<FName, double>& Value : EventPenetration)
	{
		Prepared.Spec.Data->SetSetByCallerMagnitude(Value.Key, static_cast<float>(Value.Value));
	}
	return Prepared;
}

bool DealPreparedDamage(const FVeyraPreparedDamage& Damage, UAbilitySystemComponent& Target)
{
	UAbilitySystemComponent* Source = Damage.IsValid() ? Damage.Spec.Data->GetContext().GetInstigatorAbilitySystemComponent() : nullptr;
	if (!Source)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused damage to %s: it was not prepared, or its source is gone."), *GetNameSafe(Target.GetOwner()));
		return false;
	}
	if (IsDeadUnit(Target))
	{
		UE_LOG(LogVeyraCombat, Verbose, TEXT("Ignored damage to %s: its death is final."), *GetNameSafe(Target.GetOwner()));
		return false;
	}
	return Source->ApplyGameplayEffectSpecToTarget(*Damage.Spec.Data, &Target).WasSuccessfullyApplied();
}

bool DealPreparedDamage(const FVeyraPreparedDamage& Damage, UAbilitySystemComponent& Target, TConstArrayView<FVeyraDamageComponent> AddedAtImpact)
{
	if (AddedAtImpact.IsEmpty() || !Damage.IsValid())
	{
		return DealPreparedDamage(Damage, Target);
	}
	for (const FVeyraDamageComponent& Added : AddedAtImpact)
	{
		if (!IsNonNegativeFinite(Added.Amount))
		{
			UE_LOG(LogVeyraCombat, Error, TEXT("Refused damage to %s: an amount added at impact must be finite and at least 0."), *GetNameSafe(Target.GetOwner()));
			return false;
		}
	}
	// A copy, so the preparation stays the same for every other target it reaches.
	FVeyraPreparedDamage Landing;
	Landing.Spec = FGameplayEffectSpecHandle(new FGameplayEffectSpec(*Damage.Spec.Data));
	for (const FVeyraDamageComponent& Added : AddedAtImpact)
	{
		const FGameplayTag Tag = VeyraCombatTagMapping::DamageTypeTag(Added.Type);
		const float Prepared = Landing.Spec.Data->GetSetByCallerMagnitude(Tag, /*WarnIfNotFound*/ false, 0.0f);
		Landing.Spec.Data->SetSetByCallerMagnitude(Tag, Prepared + static_cast<float>(Added.Amount));
	}
	return DealPreparedDamage(Landing, Target);
}

double GetMissingHealth(const UAbilitySystemComponent& Unit)
{
	return FMath::Max(0.0, Unit.GetNumericAttribute(UVeyraVitalsSet::GetMaxHealthAttribute()) - Unit.GetNumericAttribute(UVeyraVitalsSet::GetHealthAttribute()));
}

bool DealDamage(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraRawDamageEvent& Damage)
{
	if (IsDeadUnit(Target))
	{
		UE_LOG(LogVeyraCombat, Verbose, TEXT("Ignored damage to %s: its death is final."), *GetNameSafe(Target.GetOwner()));
		return false;
	}
	const FVeyraPreparedDamage Prepared = PrepareDamage(Source, Damage);
	return Prepared.IsValid() && DealPreparedDamage(Prepared, Target);
}

FActiveGameplayEffectHandle GrantShield(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraShieldGrant& Grant)
{
	AActor* TargetOwner = Target.GetOwner();
	UVeyraDamageAbsorptionComponent* Absorption = TargetOwner ? TargetOwner->FindComponentByClass<UVeyraDamageAbsorptionComponent>() : nullptr;
	if (!Absorption)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused shield %s on %s: it has no UVeyraDamageAbsorptionComponent."), *Grant.Id.ToString(), *GetNameSafe(TargetOwner));
		return FActiveGameplayEffectHandle();
	}
	return Absorption->GrantShield(Source, Grant);
}

FActiveGameplayEffectHandle GrantShield(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, EVeyraShieldCategory Category,
	double Amount, double DurationSeconds)
{
	FVeyraShieldGrant Grant;
	Grant.Category = Category;
	Grant.Amount = Amount;
	Grant.MaxAmount = Amount;
	Grant.DurationSeconds = DurationSeconds;
	return GrantShield(Source, Target, Grant);
}

FActiveGameplayEffectHandle GrantTemporaryHealth(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, double Amount, double DurationSeconds)
{
	return GrantAbsorption(Source, Target, UVeyraTemporaryHealthEffect::StaticClass(), VeyraTags::TemporaryHealth, Amount, DurationSeconds,
		TEXT("Temporary Health"));
}

bool ApplyStatus(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraStatusSpec& Status)
{
	AActor* TargetOwner = Target.GetOwner();
	UVeyraStatusComponent* Statuses = TargetOwner ? TargetOwner->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	if (!Statuses || IsDeadUnit(Target))
	{
		UE_LOG(LogVeyraCombat, Verbose, TEXT("Ignored status %s on %s: it has no status ledger, or its death is final."),
			*Status.Id.ToString(), *GetNameSafe(TargetOwner));
		return false;
	}
	return Statuses->Apply(Source, Status);
}

bool RemoveStatus(UAbilitySystemComponent& Target, const FVeyraContentId& Id)
{
	AActor* TargetOwner = Target.GetOwner();
	UVeyraStatusComponent* Statuses = TargetOwner ? TargetOwner->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	return Statuses && Statuses->Remove(Id);
}

EVeyraActionBlocks GetActionBlocks(const UAbilitySystemComponent& Unit)
{
	const AActor* Owner = Unit.GetOwner();
	const UVeyraStatusComponent* Statuses = Owner ? Owner->FindComponentByClass<UVeyraStatusComponent>() : nullptr;
	return Statuses ? Statuses->GetActionBlocks() : EVeyraActionBlocks::None;
}

bool Displace(UAbilitySystemComponent& Source, UAbilitySystemComponent& Target, const FVeyraDisplacement& Displacement)
{
	UVeyraMovementComponent* Movement = FindMovement(Target);
	if (!Movement || IsDeadUnit(Target))
	{
		UE_LOG(LogVeyraCombat, Verbose, TEXT("Ignored a displacement of %s: it has no body to move, or its death is final."), *GetNameSafe(Target.GetOwner()));
		return false;
	}
	// §9: Displacement Resistance shortens the path; each source keeps less than all of it, so some remains.
	const double Retained = Target.GetSet<UVeyraDefenceSet>() ? Target.GetNumericAttribute(UVeyraDefenceSet::GetDisplacementRetainedAttribute()) : 1.0;
	if (!Movement->StartDisplacement(Displacement.Direction, Displacement.Distance * Retained, Displacement.Speed))
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Refused a displacement of %s by %g at %g: it needs a horizontal direction and a finite distance and speed above 0."),
			*GetNameSafe(Target.GetOwner()), Displacement.Distance, Displacement.Speed);
		return false;
	}
	VeyraCombatRecords::NoteHostileAction(&Source, Target);
	const AActor* Owner = Target.GetOwner();
	if (UVeyraStatusComponent* Statuses = Owner ? Owner->FindComponentByClass<UVeyraStatusComponent>() : nullptr)
	{
		Statuses->NotifyInterrupted();
	}
	return true;
}

bool Dash(UAbilitySystemComponent& Unit, const FVeyraDash& Dash)
{
	UVeyraMovementComponent* Movement = FindMovement(Unit);
	if (!Movement || IsDeadUnit(Unit))
	{
		return false;
	}
	const bool bLocked = Movement->IsMovementLocked();
	const bool bStarted = Movement->StartDash(Dash);
	UE_CLOG(!bStarted && !bLocked, LogVeyraCombat, Error, TEXT("Refused a dash by %s of %g at %g: it needs a horizontal direction and a finite distance and speed above 0."),
		*GetNameSafe(Unit.GetOwner()), Dash.Distance, Dash.Speed);
	return bStarted;
}

void SetCastLocksMovement(UAbilitySystemComponent& Unit, bool bLocks)
{
	if (UVeyraMovementComponent* Movement = FindMovement(Unit))
	{
		Movement->SetCastLocksMovement(bLocks);
	}
}
}
