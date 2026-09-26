// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Attributes/VeyraOffenceSet.h"

#include "Net/UnrealNetwork.h"

UVeyraOffenceSet::UVeyraOffenceSet()
{
	// Identities: no amplification and no penetration until an effect grants them.
	InitOutgoingDamageMultiplier(1.0f);
	InitPhysicalPenetrationFlat(0.0f);
	InitPhysicalPenetrationRetained(1.0f);
	InitMagicPenetrationFlat(0.0f);
	InitMagicPenetrationRetained(1.0f);
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
