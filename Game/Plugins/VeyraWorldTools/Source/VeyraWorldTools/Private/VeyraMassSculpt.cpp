// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraMassSculpt.h"

#include "CompGeom/PolygonTriangulation.h"
#include "Dom/JsonObject.h"
#include "IndexTypes.h"
#include "Math/RandomStream.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraTerrainField.h"
#include "StaticMeshAttributes.h"
#include "Tuning/VeyraWorldTuning.h"

namespace
{
	// Resolution and geometry, not tuning: rings added between each pair of the profile; how finely a corner's room to
	// step back is searched, and the share of its step-back a corner must keep beneath it; how many times a top that
	// crosses itself is drawn in again before it stands on the outline; how far a box-projected UV runs over a unit; and
	// the height share below which the face keeps to the gameplay edge, at walking height.
	constexpr int32 RingsBetween = 2;
	constexpr int32 RoomSearchSteps = 10;
	constexpr double RoomShare = 0.55;
	constexpr int32 TopRetries = 4;
	constexpr double SculptUVSpan = 400.0;
	constexpr double FootShare = 0.08;

	// How finely the floor along an outline is sampled for the mass's base, in units a step. Resolution, not tuning.
	constexpr double SculptGroundStep = 50.0;

	double Smooth(double From, double To, double X)
	{
		const double T = FMath::Clamp((X - From) / (To - From), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	/** The profile's step-back share at height share H, linear between its pairs. */
	double ProfileAt(TConstArrayView<FVector2D> Profile, double H)
	{
		if (Profile.IsEmpty())
		{
			return 0.0;
		}
		if (H <= Profile[0].X)
		{
			return Profile[0].Y;
		}
		for (int32 Index = 1; Index < Profile.Num(); ++Index)
		{
			if (H <= Profile[Index].X)
			{
				const double T = (H - Profile[Index - 1].X) / FMath::Max(Profile[Index].X - Profile[Index - 1].X, UE_DOUBLE_SMALL_NUMBER);
				return FMath::Lerp(Profile[Index - 1].Y, Profile[Index].Y, T);
			}
		}
		return Profile.Last().Y;
	}

	/** Outline with corners added along each span so none is farther than Spacing from the next. */
	TArray<FVector2D> Resampled(TConstArrayView<FVector2D> Outline, double Spacing)
	{
		TArray<FVector2D> Out;
		for (int32 Corner = 0; Corner < Outline.Num(); ++Corner)
		{
			const FVector2D& From = Outline[Corner];
			const FVector2D& To = Outline[(Corner + 1) % Outline.Num()];
			const int32 Steps = FMath::Max(1, FMath::CeilToInt32(FVector2D::Distance(From, To) / Spacing));
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				Out.Add(FMath::Lerp(From, To, static_cast<double>(Step) / Steps));
			}
		}
		return Out;
	}

	/** One growing smooth mesh: shared corners, its normals summed from the faces round each. */
	struct FSmoothMesh
	{
		TArray<FVector> Positions;
		TArray<FVector> Normals;
		TArray<FVector4f> Colours;
		TArray<UE::Geometry::FIndex3i> Triangles;

		int32 Add(const FVector& Position, float Shade)
		{
			Normals.Add(FVector::ZeroVector);
			Colours.Add(FVector4f(Shade, Shade, Shade, 1.0f));
			return Positions.Add(Position);
		}

		/** A triangle of A, B and C facing Toward, in the order Unreal faces it by: (C - A) x (B - A). */
		void Face(int32 A, int32 B, int32 C, const FVector& Toward)
		{
			const FVector Facing = FVector::CrossProduct(Positions[C] - Positions[A], Positions[B] - Positions[A]);
			if (Facing.SizeSquared() <= UE_DOUBLE_SMALL_NUMBER)
			{
				return;
			}
			const bool bAsGiven = FVector::DotProduct(Facing, Toward) >= 0.0;
			const UE::Geometry::FIndex3i Triangle = bAsGiven ? UE::Geometry::FIndex3i(A, B, C) : UE::Geometry::FIndex3i(A, C, B);
			// Area-weighted, so a sliver does not bend a corner's normal.
			const FVector Weighted = bAsGiven ? Facing : -Facing;
			Normals[A] += Weighted;
			Normals[B] += Weighted;
			Normals[C] += Weighted;
			Triangles.Add(Triangle);
		}

		void Into(FMeshDescription& Description) const
		{
			FStaticMeshAttributes Attributes(Description);
			Attributes.Register();
			const FPolygonGroupID Group = Description.CreatePolygonGroup();
			Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Surface");
			TArray<FVertexInstanceID> Instances;
			for (int32 Index = 0; Index < Positions.Num(); ++Index)
			{
				const FVertexID Vertex = Description.CreateVertex();
				Attributes.GetVertexPositions()[Vertex] = FVector3f(Positions[Index]);
				const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
				const FVector Normal = Normals[Index].GetSafeNormal(UE_DOUBLE_SMALL_NUMBER, FVector::UpVector);
				Attributes.GetVertexInstanceNormals()[Instance] = FVector3f(Normal);
				const FVector Axis = Normal.GetAbs();
				const FVector& P = Positions[Index];
				const FVector2D UV = Axis.Z >= Axis.X && Axis.Z >= Axis.Y ? FVector2D(P.X, P.Y) : (Axis.X >= Axis.Y ? FVector2D(P.Y, P.Z) : FVector2D(P.X, P.Z));
				Attributes.GetVertexInstanceUVs().Set(Instance, 0, FVector2f(UV / SculptUVSpan));
				Attributes.GetVertexInstanceColors()[Instance] = Colours[Index];
				Instances.Add(Instance);
			}
			for (const UE::Geometry::FIndex3i& Triangle : Triangles)
			{
				Description.CreateTriangle(Group, TArray<FVertexInstanceID>{ Instances[Triangle.A], Instances[Triangle.B], Instances[Triangle.C] });
			}
		}
	};

	double Number(const FJsonObject& Kind, const TCHAR* Field, bool& bOk)
	{
		double Value = 0.0;
		bOk &= Kind.TryGetNumberField(Field, Value) && FMath::IsFinite(Value);
		return Value;
	}
}

bool FVeyraMassForm::Read(const FJsonObject& Kind, FVeyraMassForm& Out, FString& Error)
{
	bool bOk = true;
	Out.Height = Number(Kind, TEXT("height"), bOk);
	Out.InsetMax = Number(Kind, TEXT("insetMax"), bOk);
	Out.Bulge = Number(Kind, TEXT("bulge"), bOk);
	Out.BulgeScale = Number(Kind, TEXT("bulgeScale"), bOk);
	Out.Fissure = Number(Kind, TEXT("fissure"), bOk);
	Out.FissureScale = Number(Kind, TEXT("fissureScale"), bOk);
	Out.HeightVariation = Number(Kind, TEXT("heightVariation"), bOk);
	Kind.TryGetBoolField(TEXT("drape"), Out.bDrape);
	Out.FootShade = Number(Kind, TEXT("footShade"), bOk);
	Out.FissureShade = Number(Kind, TEXT("fissureShade"), bOk);
	const TArray<TSharedPtr<FJsonValue>>* Profile = nullptr;
	bOk &= Kind.TryGetArrayField(TEXT("profile"), Profile) && Profile->Num() >= 2;
	if (bOk)
	{
		for (const TSharedPtr<FJsonValue>& Pair : *Profile)
		{
			const TArray<TSharedPtr<FJsonValue>>& Values = Pair->AsArray();
			bOk &= Values.Num() == 2;
			if (bOk)
			{
				Out.Profile.Add(FVector2D(Values[0]->AsNumber(), Values[1]->AsNumber()));
				bOk &= Out.Profile.Num() == 1 || Out.Profile.Last().X > Out.Profile[Out.Profile.Num() - 2].X;
			}
		}
	}
	bOk &= Out.Height > 0.0 && Out.InsetMax >= 0.0 && Out.Bulge >= 0.0 && Out.BulgeScale > 0.0 && Out.Fissure >= 0.0 && Out.FissureScale > 0.0
		&& Out.HeightVariation >= 0.0 && Out.HeightVariation < 1.0 && Out.FootShade > 0.0 && Out.FissureShade > 0.0;
	if (!bOk)
	{
		Error = TEXT("massForms: each kind needs a height, insetMax, a rising profile of [height share, step-back share] pairs, bulge and fissure with their scales, heightVariation (under 1), and footShade and fissureShade above 0.");
	}
	return bOk;
}

bool FVeyraMassForms::Read(const FJsonObject& Style, FVeyraMassForms& Out, FString& Error)
{
	const TSharedPtr<FJsonObject>* Forms = nullptr;
	const TSharedPtr<FJsonObject>* Kinds = nullptr;
	if (!Style.TryGetObjectField(TEXT("massForms"), Forms) || !(*Forms)->TryGetObjectField(TEXT("kinds"), Kinds))
	{
		Error = TEXT("CrucibleStyle.json needs massForms with its kinds.");
		return false;
	}
	bool bOk = true;
	Out.Sink = Number(**Forms, TEXT("sink"), bOk);
	Out.SampleSpacing = Number(**Forms, TEXT("sampleSpacing"), bOk);
	Out.Seed = static_cast<int32>(Number(**Forms, TEXT("seed"), bOk));
	if (!bOk || Out.Sink < 0.0 || !(Out.SampleSpacing > 0.0))
	{
		Error = TEXT("massForms needs a seed, a sink of at least 0 and a sampleSpacing above 0.");
		return false;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Kind : (*Kinds)->Values)
	{
		const TSharedPtr<FJsonObject>* Form = nullptr;
		if (!Kind.Value->TryGetObject(Form) || !FVeyraMassForm::Read(**Form, Out.Kinds.Add(Kind.Key), Error))
		{
			Error = Kind.Key + TEXT(": ") + Error;
			return false;
		}
	}
	return true;
}

const FVeyraMassForm* FVeyraMassForms::Find(const FVeyraMassShape& Mass) const
{
	return Kinds.Find(StaticEnum<EVeyraMassKind>()->GetNameStringByValue(static_cast<int64>(Mass.Kind)));
}

int32 FVeyraMassForms::SeedOf(const FVeyraMassShape& Mass) const
{
	return Seed + Mass.Index;
}

namespace VeyraMassSculpt
{
	double TopAt(const FVeyraMassShape& Mass, const FVeyraMassForm& Form, const FVeyraTerrainField& Field, int32 Seed, const FVector2D& Point)
	{
		// Each mass its own height, so a run of cliffs steps rather than standing as one wall.
		FRandomStream Stream(Seed);
		const double Rise = Form.Height * (1.0 + Form.HeightVariation * Stream.FRandRange(-1.0, 1.0));
		if (Form.bDrape)
		{
			return Field.Height(Point) + Rise;
		}
		double Base = -TNumericLimits<double>::Max();
		for (const FVector2D& Corner : Resampled(Mass.Outline, SculptGroundStep))
		{
			Base = FMath::Max(Base, Field.Height(Corner));
		}
		return Base + Rise;
	}

	FVeyraSculptedMass Sculpt(const FVeyraMassShape& Mass, const FVeyraMassForm& Form, const FVeyraTerrainField& Field, double Sink, double SampleSpacing, int32 Seed)
	{
		FVeyraSculptedMass Out;
		TArray<FVector2D> Edge = Resampled(Mass.Outline, SampleSpacing);
		const int32 Count = Edge.Num();
		if (Count < 3)
		{
			return Out;
		}
		// Each corner's way in, and how far round the outline it lies, for the fissures that run up the face.
		TArray<FVector2D> Inward;
		TArray<double> Around;
		double Travelled = 0.0;
		for (int32 Corner = 0; Corner < Count; ++Corner)
		{
			const FVector2D& Before = Edge[(Corner + Count - 1) % Count];
			const FVector2D& Here = Edge[Corner];
			const FVector2D& After = Edge[(Corner + 1) % Count];
			// Counter-clockwise, so the mass lies to the left of each span.
			const FVector2D In = FVector2D(-(Here - Before).Y, (Here - Before).X).GetSafeNormal();
			const FVector2D Out2 = FVector2D(-(After - Here).Y, (After - Here).X).GetSafeNormal();
			const FVector2D Way = (In + Out2).GetSafeNormal();
			Inward.Add(Way / FMath::Max(FVector2D::DotProduct(Way, Out2), 0.35));
			Around.Add(Travelled);
			Travelled += FVector2D::Distance(Here, After);
		}

		// The floor under each corner, and the top above it: level over the highest of it, or following it.
		const FVector2D Offset(FMath::Fmod(Seed * 0.731, 97.0), FMath::Fmod(Seed * 1.379, 89.0));
		TArray<double> Ground;
		TArray<double> Tops;
		for (const FVector2D& Corner : Edge)
		{
			Ground.Add(Field.Height(Corner));
			Tops.Add(TopAt(Mass, Form, Field, Seed, Corner));
		}

		// How far each corner may step back: as far as the mass leaves room beneath it, so a neck's two faces never cross.
		const double Reach = Form.InsetMax + Form.Bulge + Form.Fissure;
		TArray<double> Room;
		for (int32 Corner = 0; Corner < Count; ++Corner)
		{
			double Allowed = 0.0;
			for (int32 Step = 0; Step < RoomSearchSteps && Reach > 0.0; ++Step)
			{
				const double Depth = Reach * (1.0 - static_cast<double>(Step) / RoomSearchSteps);
				if (Mass.DepthInside(Edge[Corner] + Inward[Corner] * Depth) >= RoomShare * Depth)
				{
					Allowed = Depth;
					break;
				}
			}
			Room.Add(Reach > 0.0 ? Allowed / Reach : 0.0);
		}

		// The rings, from below the floor up the profile to the lip, with rings added between its pairs.
		TArray<double> Shares = { 0.0 };
		for (int32 Pair = 0; Pair < Form.Profile.Num(); ++Pair)
		{
			const double From = Pair == 0 ? 0.0 : Form.Profile[Pair - 1].X;
			const double To = Form.Profile[Pair].X;
			for (int32 Between = 1; Between <= RingsBetween + 1 && To > From; ++Between)
			{
				Shares.Add(FMath::Lerp(From, To, static_cast<double>(Between) / (RingsBetween + 1)));
			}
		}
		if (Shares.Last() < 1.0)
		{
			Shares.Add(1.0);
		}

		const auto StepBack = [&](int32 Corner, double Share, double Height, double& OutFissure) {
			const FVector2D At = Edge[Corner] / Form.BulgeScale + Offset;
			const double Bulge = 0.5 + 0.5 * FMath::PerlinNoise3D(FVector(At, Height / Form.BulgeScale));
			OutFissure = FMath::Max(0.0, FMath::PerlinNoise2D(FVector2D(Around[Corner] / Form.FissureScale, Height / (Form.FissureScale * 5.0)) + Offset));
			// At walking height the face keeps to the gameplay edge; above it, it steps back and breaks.
			const double Above = Smooth(0.0, FootShare * 2.0, Share);
			return Room[Corner] * (ProfileAt(Form.Profile, Share) * Form.InsetMax + Above * (Form.Bulge * Bulge + Form.Fissure * OutFissure));
		};

		// The top's corners; drawn in again, each time less far, while it crosses itself.
		TArray<FVector2D> TopRing;
		for (int32 Attempt = 0; Attempt <= TopRetries; ++Attempt)
		{
			const double Scale = Attempt == TopRetries ? 0.0 : FMath::Pow(0.6, Attempt);
			TopRing.Reset();
			for (int32 Corner = 0; Corner < Count; ++Corner)
			{
				double Fissure = 0.0;
				TopRing.Add(Edge[Corner] + Inward[Corner] * (Scale * StepBack(Corner, 1.0, Tops[Corner] - Ground[Corner], Fissure)));
			}
			if (VeyraLayout::IsSimplePolygon(TopRing))
			{
				break;
			}
		}

		FSmoothMesh Mesh;
		TArray<TArray<int32>> Rings;
		for (int32 Ring = 0; Ring < Shares.Num(); ++Ring)
		{
			const double Share = Shares[Ring];
			TArray<int32>& Corners = Rings.AddDefaulted_GetRef();
			for (int32 Corner = 0; Corner < Count; ++Corner)
			{
				const double Wall = Tops[Corner] - Ground[Corner];
				const double Height = Ring == 0 ? -Sink : Share * Wall;
				double Fissure = 0.0;
				const FVector2D At = Ring + 1 == Shares.Num() ? TopRing[Corner] : Edge[Corner] + Inward[Corner] * (Ring == 0 ? 0.0 : StepBack(Corner, Share, Height, Fissure));
				const double Shade = FMath::Lerp(Form.FootShade, 1.0, Smooth(0.0, 0.35, Share)) * FMath::Lerp(1.0, Form.FissureShade, Fissure);
				Corners.Add(Mesh.Add(FVector(At, Ground[Corner] + Height), static_cast<float>(Shade)));
			}
		}
		for (int32 Ring = 0; Ring + 1 < Rings.Num(); ++Ring)
		{
			for (int32 Corner = 0; Corner < Count; ++Corner)
			{
				const int32 Next = (Corner + 1) % Count;
				const FVector2D Along = Edge[Next] - Edge[Corner];
				const FVector Outward(Along.Y, -Along.X, 0.0);
				Mesh.Face(Rings[Ring][Corner], Rings[Ring][Next], Rings[Ring + 1][Next], Outward);
				Mesh.Face(Rings[Ring][Corner], Rings[Ring + 1][Next], Rings[Ring + 1][Corner], Outward);
			}
		}
		// The top, its own corners so the lip keeps its crease.
		TArray<int32> Cap;
		for (int32 Corner = 0; Corner < Count; ++Corner)
		{
			Cap.Add(Mesh.Add(FVector(TopRing[Corner], Tops[Corner]), 1.0f));
		}
		TArray<UE::Geometry::FIndex3i> Triangles;
		PolygonTriangulation::TriangulateSimplePolygon<double>(TopRing, Triangles, /*bOrientAsHoleFill*/ false);
		for (const UE::Geometry::FIndex3i& Triangle : Triangles)
		{
			Mesh.Face(Cap[Triangle.A], Cap[Triangle.B], Cap[Triangle.C], FVector::UpVector);
		}
		Mesh.Into(Out.Mesh);
		return Out;
	}
}
