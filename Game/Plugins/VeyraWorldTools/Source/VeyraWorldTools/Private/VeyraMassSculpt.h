// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshDescription.h"

class FJsonObject;
class FVeyraTerrainField;
struct FVeyraMassShape;

/**
 * How a kind of mass rises from its outline (ADR-073 §5; the style profile's massForms): its height, and how far its face
 * steps back as it climbs, broken by broad bulges and vertical fissures, to a level top. Presentation only.
 */
struct FVeyraMassForm
{
	double Height = 0.0;

	/** The farthest the face steps back from the gameplay edge before the noise, in units. */
	double InsetMax = 0.0;

	/** (share of the height, share of InsetMax) pairs, rising: the face's profile from foot to lip. */
	TArray<FVector2D> Profile;

	double Bulge = 0.0;
	double BulgeScale = 1.0;
	double Fissure = 0.0;
	double FissureScale = 1.0;

	/** How far one mass's height may differ from another's of its kind, as a share of Height, each mass its own. */
	double HeightVariation = 0.0;

	/**
	 * Whether its top follows the floor at its height above it, as a low bank does, rather than standing level above the
	 * highest floor along its edge, as rock does: a level bank beside a sunken road would show a tall face there.
	 */
	bool bDrape = false;

	/** The occlusion baked into the foot and the fissures' depths: 1 is none. */
	double FootShade = 1.0;
	double FissureShade = 1.0;

	/** Reads one kind's form from massForms; false with Error when a field is missing or out of range. */
	static bool Read(const FJsonObject& Kind, FVeyraMassForm& Out, FString& Error);
};

/** The style profile's massForms: each kind's form by its name, how far every foot sinks, how finely outlines are sampled. */
struct FVeyraMassForms
{
	TMap<FString, FVeyraMassForm> Kinds;
	double Sink = 0.0;
	double SampleSpacing = 0.0;
	int32 Seed = 0;

	/** Reads massForms from Style; false with Error when it or any kind's form is missing or out of range. */
	static bool Read(const FJsonObject& Style, FVeyraMassForms& Out, FString& Error);

	/** Mass's kind's form, or null when massForms has none for it. */
	const FVeyraMassForm* Find(const FVeyraMassShape& Mass) const;

	/** Mass's own seed, so its sculpt and what stands on its top agree. */
	int32 SeedOf(const FVeyraMassShape& Mass) const;
};

/** One sculpted mass's mesh. */
struct FVeyraSculptedMass
{
	FMeshDescription Mesh;
};

namespace VeyraMassSculpt
{
	/**
	 * Where Mass's top stands over Point as Form over Field: its own height, seeded, above the floor there if it drapes,
	 * or above the highest floor along its edge if it stands level.
	 */
	double TopAt(const FVeyraMassShape& Mass, const FVeyraMassForm& Form, const FVeyraTerrainField& Field, int32 Seed, const FVector2D& Point);

	/**
	 * Sculpts Mass as Form over the floor Field gives (ADR-073 §5): rings lofted up its profile, each corner stepping back
	 * only inward, so the footprint at walking height is the gameplay edge and nothing reaches past it; its foot sunk Sink
	 * below the floor; smooth faces creased at the lip; occlusion in each vertex's colour. Seeded, so it builds the same
	 * every time. Corners are SampleSpacing apart at most.
	 */
	FVeyraSculptedMass Sculpt(const FVeyraMassShape& Mass, const FVeyraMassForm& Form, const FVeyraTerrainField& Field, double Sink, double SampleSpacing, int32 Seed);
}
