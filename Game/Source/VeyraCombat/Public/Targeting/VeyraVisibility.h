// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"

#include "VeyraVisibility.generated.h"

class AActor;
class UWorld;
struct FVeyraPlacedShape;

/** The shape of Dense Fog an ability lays (ADR-036 §1). */
enum class EVeyraFogShapeKind : uint8
{
	/** A circle of Radius around Origin. */
	Circle,
	/** A band Width wide from Origin along Direction for Length. */
	Corridor,
};

/** Dense Fog an ability lays, where its cast puts it (ADR-036 §1). */
struct FVeyraFogShape
{
	EVeyraFogShapeKind Kind = EVeyraFogShapeKind::Circle;
	FVector Origin = FVector::ZeroVector;
	/** A corridor's way, flat; a circle has none. */
	FVector Direction = FVector::ForwardVector;
	/** A circle's; above 0. */
	double Radius = 0.0;
	/** A corridor's; above 0. */
	double Length = 0.0;
	double Width = 0.0;
};

/**
 * What a world's vision allows (ADR-016 §2): the contract Vision implements and Combat's targeting,
 * orders and bots read, so none of them sees more than a player may. Vision is its only writer.
 */
class IVeyraVisibility
{
public:
	virtual ~IVeyraVisibility() = default;

	/**
	 * Whether Observer may see Target now, and so acquire it as a target: Observer's team sees it,
	 * and when Target is an enemy Vanguard inside Dense Fog, Observer is inside the same fog
	 * (Vision Bible §2). Observer is a unit, or anything that has a side.
	 */
	virtual bool CanSee(const UObject& Observer, const AActor& Target) const = 0;

	/** Whether Team's shared vision holds Target now; Dense Fog sightings are not shared (Vision Bible §2). */
	virtual bool IsVisibleToTeam(EVeyraTeam Team, const AActor& Target) const = 0;

	/**
	 * Lights Radius around Centre for Team for DurationSeconds (ADR-016 §5): ordinary vision, which over
	 * Dense Fog senses presence instead of showing what is inside (Vision Bible §6). An ability that
	 * lights an area, as Bryn's Sounding Flare does, asks for it here.
	 */
	virtual void RevealArea(EVeyraTeam Team, const FVector& Centre, double Radius, double DurationSeconds) = 0;

	/**
	 * Lights Placed for Team for DurationSeconds (ADR-018 §5): ordinary vision inside the shape, never
	 * True Sight, and over Dense Fog nothing at all. A lingering area, as Kade's Sightline, asks for it.
	 */
	virtual void RevealShape(EVeyraTeam Team, const FVeyraPlacedShape& Placed, double DurationSeconds) = 0;

	/**
	 * Lays Dense Fog of Shape for DurationSeconds (ADR-036 §1): the same construct as the map's fog, hiding
	 * either side's Vanguards from those outside it, and joining any fog it touches into one volume while
	 * it lasts (Vision Bible §2). An ability that makes fog, as Lay the Mist, asks for it here.
	 */
	virtual void AddDenseFog(const FVeyraFogShape& Shape, double DurationSeconds) = 0;

	/** The Dense Fog volume holding Point now, or INDEX_NONE outside all fog (ADR-036 §1). */
	virtual int32 FogVolumeAt(const FVector& Point) const = 0;
};

/**
 * Holds a world's vision, if it has one. Vision registers itself here on the server; a world without
 * vision (unit tests, development maps) sees everything.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraVisibilityRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** InVisibility governs this world until Unregister; it must outlive that. */
	void Register(IVeyraVisibility& InVisibility);
	void Unregister(const IVeyraVisibility& InVisibility);

	const IVeyraVisibility* Get() const { return Visibility; }
	IVeyraVisibility* GetMutable() const { return Visibility; }

private:
	IVeyraVisibility* Visibility = nullptr;
};

/** Reading a world's vision; everything is visible where no vision is registered. */
namespace VeyraVisibility
{
	/** World's vision, or null when it has none. */
	VEYRACOMBAT_API const IVeyraVisibility* Find(const UWorld* World);

	/** Whether Observer may see Target now: its world's vision says so, or no vision governs it. */
	VEYRACOMBAT_API bool CanSee(const UObject& Observer, const AActor& Target);

	/** Whether Team sees Target now: its world's vision says so, or no vision governs it. */
	VEYRACOMBAT_API bool IsVisibleToTeam(EVeyraTeam Team, const AActor& Target);

	/** Server: lights an area for Team in World's vision; a world without vision already sees everything. */
	VEYRACOMBAT_API void RevealArea(const UWorld& World, EVeyraTeam Team, const FVector& Centre, double Radius, double DurationSeconds);

	/** Server: lights a shape for Team in World's vision; a world without vision already sees everything. */
	VEYRACOMBAT_API void RevealShape(const UWorld& World, EVeyraTeam Team, const FVeyraPlacedShape& Placed, double DurationSeconds);

	/** Server: lays Dense Fog in World's vision; a world without vision has no fog to lay. */
	VEYRACOMBAT_API void AddDenseFog(const UWorld& World, const FVeyraFogShape& Shape, double DurationSeconds);

	/** The fog volume holding Point in World's vision, or INDEX_NONE: always so in a world without vision. */
	VEYRACOMBAT_API int32 FogVolumeAt(const UWorld* World, const FVector& Point);
}
