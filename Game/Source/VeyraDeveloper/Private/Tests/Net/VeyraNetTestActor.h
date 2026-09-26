// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "VeyraNetTestActor.generated.h"

// A replicated actor for the Veyra.Net tests. UHT forbids preprocessor guards around UCLASSes,
// so this header is unconditional.
UCLASS(NotBlueprintable, Transient)
class AVeyraNetTestActor : public AActor
{
	GENERATED_BODY()

public:
	AVeyraNetTestActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetValue(int32 NewValue);
	int32 GetValue() const { return Value; }

private:
	UPROPERTY(Replicated)
	int32 Value = 0;
};
