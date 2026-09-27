// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraOffenceSet.h"

#include "Net/UnrealNetwork.h"
#include "VeyraCombatLog.h"

UVeyraOffenceSet::UVeyraOffenceSet()
{
	// Identities: no amplification and no penetration until an effect grants them. Power and Attack
	// Speed are 0 until a Vanguard's data sets them.
	InitOutgoingDamageMultiplier(1.0f);
	InitPhysicalPenetrationFlat(0.0f);
	InitPhysicalPenetrationRetained(1.0f);
	InitMagicPenetrationFlat(0.0f);
	InitMagicPenetrationRetained(1.0f);
	InitPhysicalPower(0.0f);
	InitMagicPower(0.0f);
	InitAttackSpeed(0.0f);
}

void UVeyraOffenceSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Offence shows on the owner's own HUD only.
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraOffenceSet, OutgoingDamageMultiplier, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraOffenceSet, PhysicalPenetrationFlat, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraOffenceSet, PhysicalPenetrationRetained, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraOffenceSet, MagicPenetrationFlat, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraOffenceSet, MagicPenetrationRetained, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraOffenceSet, PhysicalPower, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraOffenceSet, MagicPower, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraOffenceSet, AttackSpeed, Params);
}

void UVeyraOffenceSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	// Power and Attack Speed are never negative in data; a negative base would be a caller's bug.
	const FGameplayAttributeData* Data = Attribute == GetPhysicalPowerAttribute() ? &PhysicalPower
		: Attribute == GetMagicPowerAttribute()                                   ? &MagicPower
		: Attribute == GetAttackSpeedAttribute()                                  ? &AttackSpeed
																				  : nullptr;
	if (Data && NewValue < 0.0f)
	{
		UE_LOG(LogVeyraCombat, Error, TEXT("Base %s on %s would become %g; it cannot be negative, so it keeps %g."),
			*Attribute.GetName(), *GetNameSafe(GetOwningActor()), NewValue, Data->GetBaseValue());
		NewValue = Data->GetBaseValue();
	}
}

void UVeyraOffenceSet::OnRep_OutgoingDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraOffenceSet, OutgoingDamageMultiplier, OldValue);
}

void UVeyraOffenceSet::OnRep_PhysicalPenetrationFlat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraOffenceSet, PhysicalPenetrationFlat, OldValue);
}

void UVeyraOffenceSet::OnRep_PhysicalPenetrationRetained(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraOffenceSet, PhysicalPenetrationRetained, OldValue);
}

void UVeyraOffenceSet::OnRep_MagicPenetrationFlat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraOffenceSet, MagicPenetrationFlat, OldValue);
}

void UVeyraOffenceSet::OnRep_MagicPenetrationRetained(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraOffenceSet, MagicPenetrationRetained, OldValue);
}

void UVeyraOffenceSet::OnRep_PhysicalPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraOffenceSet, PhysicalPower, OldValue);
}

void UVeyraOffenceSet::OnRep_MagicPower(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraOffenceSet, MagicPower, OldValue);
}

void UVeyraOffenceSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraOffenceSet, AttackSpeed, OldValue);
}
