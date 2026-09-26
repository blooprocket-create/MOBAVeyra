// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"

#include "VeyraLoadTestSubsystem.generated.h"

class AVeyraPlayerState;

/**
 * The load test for the ADR-006 §5 bandwidth spike. On a dedicated server whose map URL carries
 * VeyraLoadBots=<n> and VeyraLoadStandIns=<n>, it adds n bot participants and n lane stand-ins when
 * preparation begins. The stand-ins walk three lanes between the sides' starts, and once the match
 * is live the bots wander to random points on the lanes. VeyraLoadStandInHz=<n> sets the
 * stand-ins' network update rate; without it they keep the engine's default for characters. These
 * counts and rates are load-test parameters, not tuning: canon gives no wave counts yet, so the
 * spike measures a sweep.
 */
UCLASS()
class UVeyraLoadTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return BotCount > 0 || StandInCount > 0; }
	virtual TStatId GetStatId() const override;

private:
	bool Populate(UWorld& World);
	void Wander(UWorld& World);

	int32 BotCount = 0;
	int32 StandInCount = 0;
	int32 StandInUpdateHz = 0;
	bool bPopulated = false;
	FVector StartA = FVector::ZeroVector;
	FVector StartB = FVector::ZeroVector;
	double NextWanderTime = 0.0;
	FRandomStream Random;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AVeyraPlayerState>> Bots;
};
