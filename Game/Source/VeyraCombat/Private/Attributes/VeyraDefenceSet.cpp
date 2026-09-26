// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraDefenceSet.h"

#include "Net/UnrealNetwork.h"

UVeyraDefenceSet::UVeyraDefenceSet()
{
	// Identities: no resistance, reduction or damage reduction until data or an effect sets them.
	InitArmor(0.0f);
	InitMagicResist(0.0f);
	InitArmorReductionFlat(0.0f);
	InitArmorReductionRetained(1.0f);
	InitMagicResistReductionFlat(0.0f);
	InitMagicResistReductionRetained(1.0f);
	InitIncomingDamageMultiplier(1.0f);
}

void UVeyraDefenceSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.RepNotifyCondition = REPNOTIFY_Always;
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraDefenceSet, Armor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraDefenceSet, MagicResist, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraDefenceSet, ArmorReductionFlat, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraDefenceSet, ArmorReductionRetained, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraDefenceSet, MagicResistReductionFlat, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraDefenceSet, MagicResistReductionRetained, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UVeyraDefenceSet, IncomingDamageMultiplier, Params);
}

void UVeyraDefenceSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraDefenceSet, Armor, OldValue);
}

void UVeyraDefenceSet::OnRep_MagicResist(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraDefenceSet, MagicResist, OldValue);
}

void UVeyraDefenceSet::OnRep_ArmorReductionFlat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraDefenceSet, ArmorReductionFlat, OldValue);
}

void UVeyraDefenceSet::OnRep_ArmorReductionRetained(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraDefenceSet, ArmorReductionRetained, OldValue);
}

void UVeyraDefenceSet::OnRep_MagicResistReductionFlat(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraDefenceSet, MagicResistReductionFlat, OldValue);
}

void UVeyraDefenceSet::OnRep_MagicResistReductionRetained(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraDefenceSet, MagicResistReductionRetained, OldValue);
}

void UVeyraDefenceSet::OnRep_IncomingDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVeyraDefenceSet, IncomingDamageMultiplier, OldValue);
}
