// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VeyraServerClock.generated.h"

struct FVeyraMotionTuning;

/**
 * A client's estimate of the server's world clock (ADR-079 §4), from timed exchanges with the server: each says when the
 * client asked, what the server's clock read as it answered, and when the answer arrived. The exchange with the shortest
 * round trip among the latest is trusted most, since it waited least on the way. The estimate moves toward a new answer
 * slowly, so the drawn world never jumps, unless it is far off.
 */
struct VEYRACOMBAT_API FVeyraClockEstimate
{
	/** Adds an exchange: asked and received on the client's world clock, answered on the server's. */
	void AddExchange(double AskedAt, double ServerTime, double ReceivedAt, const FVeyraMotionTuning& Tuning);

	/** Moves the offset in use toward the trusted one, at most the tuning's slew rate over DeltaSeconds. */
	void Advance(double DeltaSeconds, const FVeyraMotionTuning& Tuning);

	bool HasEstimate() const { return bHasEstimate; }

	/** Server world seconds less client world seconds, as now in use. */
	double GetOffset() const { return Offset; }

	/** The round trip of the trusted exchange, in seconds. */
	double GetRoundTrip() const { return TrustedRoundTrip; }

private:
	struct FExchange
	{
		double RoundTrip = 0.0;
		double Offset = 0.0;
	};
	TArray<FExchange> Latest;
	double Offset = 0.0;
	double TrustedOffset = 0.0;
	double TrustedRoundTrip = 0.0;
	bool bHasEstimate = false;
};

/**
 * The server's world clock, as every machine reads it (ADR-079 §4). The server reads its own; a client its estimate, fed
 * by its player controller's timed exchanges. A unit's motion is told in the server's seconds, so this is the clock every
 * machine draws bodies by.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraServerClock : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UVeyraServerClock* Get(const UWorld* World);

	/** The server's world seconds now, as this machine best knows them. */
	double Now() const;

	/** Whether this machine knows the server's clock: it is the server, or a client with an estimate. */
	bool IsKnown() const;

	/** The round trip to the server, in seconds, as the clock's trusted exchange found it: 0 on the server. */
	double GetRoundTrip() const;

	/** Client: adds a timed exchange (ADR-079 §4). AskedAt is the client's world seconds as it asked. */
	void AddExchange(double AskedAt, double ServerTime);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool IsServerClock() const;

	FVeyraClockEstimate Estimate;
};
