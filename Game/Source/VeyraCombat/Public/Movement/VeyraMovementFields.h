// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"

#include "VeyraMovementFields.generated.h"

/** A field that bends the forced moves of its side's enemies toward its centre (ADR-033 §5), as Relay's Magnetic Field. */
struct FVeyraMovementField
{
	FVector Centre = FVector::ZeroVector;
	double Radius = 0.0;

	/** Whose field it is: the moves of that side's enemies bend. */
	EVeyraTeam Side = EVeyraTeam::None;

	/** How far, at most, it moves a forced move's end toward its centre. */
	double Pull = 0.0;
};

/**
 * Combat's movement fields (ADR-003, world volumes: Combat owns movement fields; ADR-033 §5). A field
 * stands while what made it lasts; a dash or a displacement of one of its side's enemies that starts
 * inside it or crosses it ends nearer its centre, by up to its pull, as the move is planned, and terrain
 * then shortens the bent path as ever. Server only.
 */
UCLASS()
class VEYRACOMBAT_API UVeyraMovementFieldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Stands Field until Remove; its handle. */
	int32 Add(const FVeyraMovementField& Field);
	void Remove(int32 Handle);

	/** Where a forced move of a unit of UnitSide from Start to End ends, once the fields of its enemies it starts in or crosses bend it. */
	FVector Bend(EVeyraTeam UnitSide, const FVector& Start, const FVector& End) const;

	int32 GetFieldCount() const { return Fields.Num(); }

private:
	TMap<int32, FVeyraMovementField> Fields;
	int32 NextHandle = 1;
};
