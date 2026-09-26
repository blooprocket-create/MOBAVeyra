// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Tests/Net/VeyraNetTestActor.h"

#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

AVeyraNetTestActor::AVeyraNetTestActor()
{
	bReplicates = true;
	bAlwaysRelevant = true;
}

void AVeyraNetTestActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(AVeyraNetTestActor, Value, Params);
}

void AVeyraNetTestActor::SetValue(int32 NewValue)
{
	Value = NewValue;
	MARK_PROPERTY_DIRTY_FROM_NAME(AVeyraNetTestActor, Value, this);
}
