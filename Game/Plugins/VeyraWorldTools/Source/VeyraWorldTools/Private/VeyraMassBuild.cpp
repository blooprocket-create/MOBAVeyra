// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraWorldBuild.h"

#include "Algo/Reverse.h"
#include "CompGeom/PolygonTriangulation.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "IndexTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraTerrainField.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "Tuning/VeyraWorldTuning.h"

DEFINE_LOG_CATEGORY_STATIC(LogVeyraWorldMasses, Log, All);

namespace
{
	// How finely the floor under a mass's outline is sampled for its base, in units a step; how far a box-projected UV
	// runs over a unit of the world; and the sharpest bend an ink's mitre widens at before it is held. Resolution and
	// geometry, not tuning.
	constexpr double GroundStep = 100.0;
	constexpr double UVSpan = 400.0;
	constexpr double MaxInkMitre = 2.0;

	/** One growing mesh, every face its own corners, so each shades flat as the toon world's facets do. */
	struct FFacetedMesh
	{
		FMeshDescription Description;
		FStaticMeshAttributes Attributes{ Description };
		FPolygonGroupID Group;

		FFacetedMesh()
		{
			Attributes.Register();
			Group = Description.CreatePolygonGroup();
			Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Surface");
		}

		/** A triangle of A, B and C facing Toward: Unreal faces a triangle by (C - A) x (B - A), so they are put in that order. */
		void Face(const FVector& A, const FVector& B, const FVector& C, const FVector& Toward)
		{
			const FVector Facing = FVector::CrossProduct(C - A, B - A);
			if (Facing.SizeSquared() <= UE_DOUBLE_SMALL_NUMBER)
			{
				return;
			}
			const bool bAsGiven = FVector::DotProduct(Facing, Toward) >= 0.0;
			const FVector Normal = (bAsGiven ? Facing : -Facing).GetSafeNormal();
			TArray<FVertexInstanceID, TFixedAllocator<3>> Corners;
			for (const FVector& Corner : bAsGiven ? TArray<FVector>{ A, B, C } : TArray<FVector>{ A, C, B })
			{
				const FVertexID Vertex = Description.CreateVertex();
				Attributes.GetVertexPositions()[Vertex] = FVector3f(Corner);
				const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
				Attributes.GetVertexInstanceNormals()[Instance] = FVector3f(Normal);
				Attributes.GetVertexInstanceUVs().Set(Instance, 0, FVector2f(BoxProjected(Corner, Normal)));
				Attributes.GetVertexInstanceColors()[Instance] = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
				Corners.Add(Instance);
			}
			Description.CreateTriangle(Group, Corners);
		}

		void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Toward)
		{
			Face(A, B, C, Toward);
			Face(A, C, D, Toward);
		}

		bool IsEmpty() const { return Description.Triangles().Num() == 0; }

	private:
		/** The UV of a corner projected along its face's main axis (ADR-073 §5): the material is world-aligned besides. */
		static FVector2D BoxProjected(const FVector& Corner, const FVector& Normal)
		{
			const FVector Axis = Normal.GetAbs();
			if (Axis.Z >= Axis.X && Axis.Z >= Axis.Y)
			{
				return FVector2D(Corner.X, Corner.Y) / UVSpan;
			}
			return (Axis.X >= Axis.Y ? FVector2D(Corner.Y, Corner.Z) : FVector2D(Corner.X, Corner.Z)) / UVSpan;
		}
	};

	TArray<FVector2D> PointsOf(const FJsonObject& Owner, const TCHAR* Field)
	{
		TArray<FVector2D> Points;
		for (const TSharedPtr<FJsonValue>& Value : Owner.GetArrayField(Field))
		{
			const TArray<TSharedPtr<FJsonValue>>& Pair = Value->AsArray();
			Points.Add(FVector2D(Pair[0]->AsNumber(), Pair[1]->AsNumber()));
		}
		return Points;
	}

	/** Twice Polygon's signed area: positive when its corners run counter-clockwise in world X/Y. */
	double DoubleArea(TConstArrayView<FVector2D> Polygon)
	{
		double Sum = 0.0;
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			Sum += FVector2D::CrossProduct(Polygon[Index], Polygon[(Index + 1) % Polygon.Num()]);
		}
		return Sum;
	}

	/** A prism over Outline from Bottom to Top: its walls facing out and its top facing up. Its bottom is never seen. */
	void Prism(FFacetedMesh& Mesh, TArray<FVector2D> Outline, double Bottom, double Top)
	{
		if (Outline.Num() < 3 || Top <= Bottom)
		{
			return;
		}
		if (DoubleArea(Outline) < 0.0)
		{
			Algo::Reverse(Outline);
		}
		for (int32 Corner = 0; Corner < Outline.Num(); ++Corner)
		{
			const FVector2D& From = Outline[Corner];
			const FVector2D& To = Outline[(Corner + 1) % Outline.Num()];
			const FVector2D Along = (To - From).GetSafeNormal();
			// Counter-clockwise, so the prism lies to the left of each span and its wall faces right.
			const FVector Outward(Along.Y, -Along.X, 0.0);
			Mesh.Quad(FVector(From, Bottom), FVector(To, Bottom), FVector(To, Top), FVector(From, Top), Outward);
		}
		TArray<UE::Geometry::FIndex3i> Triangles;
		PolygonTriangulation::TriangulateSimplePolygon<double>(Outline, Triangles, /*bOrientAsHoleFill*/ false);
		for (const UE::Geometry::FIndex3i& Triangle : Triangles)
		{
			Mesh.Face(FVector(Outline[Triangle.A], Top), FVector(Outline[Triangle.B], Top), FVector(Outline[Triangle.C], Top), FVector::UpVector);
		}
	}

	/**
	 * A square tube Size across along the closed loop Points, its top and both sides facing out: an ink line. Each ring is
	 * mitred at its bend, so the line keeps its width round corners.
	 */
	void Tube(FFacetedMesh& Mesh, TConstArrayView<FVector> Points, double Size)
	{
		const int32 Count = Points.Num();
		if (Count < 3)
		{
			return;
		}
		const double Half = Size / 2.0;
		TArray<FVector> Left;
		TArray<FVector> Right;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector In = (Points[Index] - Points[(Index + Count - 1) % Count]).GetSafeNormal2D();
			const FVector Out = (Points[(Index + 1) % Count] - Points[Index]).GetSafeNormal2D();
			const FVector Side = FVector(-(In + Out).Y, (In + Out).X, 0.0).GetSafeNormal();
			const double Mitre = FMath::Min(1.0 / FMath::Max(FVector::DotProduct(Side, FVector(-Out.Y, Out.X, 0.0)), UE_DOUBLE_KINDA_SMALL_NUMBER), MaxInkMitre);
			Left.Add(Points[Index] + Side * (Half * Mitre));
			Right.Add(Points[Index] - Side * (Half * Mitre));
		}
		const FVector Up(0.0, 0.0, Half);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 Next = (Index + 1) % Count;
			const FVector Sideways = (Left[Index] + Left[Next] - Right[Index] - Right[Next]).GetSafeNormal();
			Mesh.Quad(Left[Index] + Up, Left[Next] + Up, Right[Next] + Up, Right[Index] + Up, FVector::UpVector);
			Mesh.Quad(Left[Index] - Up, Left[Next] - Up, Left[Next] + Up, Left[Index] + Up, Sideways);
			Mesh.Quad(Right[Index] - Up, Right[Next] - Up, Right[Next] + Up, Right[Index] + Up, -Sideways);
		}
	}

	/** A static mesh in Package from Mesh: one material, no collision (ADR-073 §5: the masses' terrain is World's). */
	UStaticMesh* Commit(UPackage& Package, const FString& Name, FFacetedMesh&& Mesh, UMaterialInterface* Material)
	{
		UStaticMesh* Asset = NewObject<UStaticMesh>(&Package, FName(*Name), RF_Public);
		Asset->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Surface"), TEXT("Surface")));
		FStaticMeshSourceModel& Source = Asset->AddSourceModel();
		Source.BuildSettings.bRecomputeNormals = false;
		Source.BuildSettings.bRecomputeTangents = true;
		Source.BuildSettings.bGenerateLightmapUVs = false;
		Asset->CreateMeshDescription(0, MoveTemp(Mesh.Description));
		Asset->CommitMeshDescription(0);
		Asset->CreateBodySetup();
		Asset->GetBodySetup()->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		Asset->Build(/*bInSilent*/ true);
		Asset->PostEditChange();
		return Asset;
	}

	/** Mesh placed at the centre, turned by Yaw degrees about it: Team A's as built, Team B's half a turn round. */
	void Place(UWorld& World, UStaticMesh& Mesh, const FString& Label, double Yaw)
	{
		AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator(0.0, Yaw, 0.0));
		Actor->SetActorLabel(Label);
		Actor->Tags.Add(TEXT("Veyra.GeneratedMass"));
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetStaticMesh(&Mesh);
		Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Component->SetCanEverAffectNavigation(false);
	}
}

namespace VeyraWorldBuild
{
	TSharedPtr<FJsonObject> LoadMassArt(const FVeyraWorldTuning& Tuning, FString& Error)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VeyraWorldTools"));
		FString Json;
		TSharedPtr<FJsonObject> Art;
		if (!Plugin || !FFileHelper::LoadFileToString(Json, *(Plugin->GetBaseDir() / TEXT("Config/CrucibleMassArt.json")))
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Art) || !Art.IsValid())
		{
			Error = TEXT("Cannot load CrucibleMassArt.json: run Game/Scripts/Crucible/BuildCrucibleMassArt.py.");
			return nullptr;
		}
		if (Art->GetStringField(TEXT("massesInputSha256")) != Tuning.Layout.Masses.InputSha256)
		{
			Error = TEXT("CrucibleMassArt.json was built from other masses: run Game/Scripts/Crucible/BuildCrucibleMassArt.py.");
			return nullptr;
		}
		return Art;
	}

	FMassGround MassGround(const FVeyraTerrainField& Field, TConstArrayView<FVector2D> Outline)
	{
		FMassGround Ground{ TNumericLimits<double>::Max(), -TNumericLimits<double>::Max() };
		for (int32 Corner = 0; Corner < Outline.Num(); ++Corner)
		{
			const FVector2D& From = Outline[Corner];
			const FVector2D& To = Outline[(Corner + 1) % Outline.Num()];
			const int32 Steps = FMath::Max(1, FMath::CeilToInt32(FVector2D::Distance(From, To) / GroundStep));
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const double Height = Field.Height(FMath::Lerp(From, To, static_cast<double>(Step) / Steps));
				Ground.Low = FMath::Min(Ground.Low, Height);
				Ground.Base = FMath::Max(Ground.Base, Height);
			}
		}
		return Ground;
	}

	bool Masses(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error)
	{
		const TSharedPtr<FJsonObject> Art = LoadMassArt(Tuning, Error);
		const TSharedPtr<FJsonObject>* Looks = nullptr;
		FString MaterialPath;
		FString InkPath;
		if (!Art || !Style.TryGetObjectField(TEXT("massLooks"), Looks) || !(*Looks)->TryGetStringField(TEXT("material"), MaterialPath)
			|| !(*Looks)->TryGetStringField(TEXT("inkMaterial"), InkPath))
		{
			Error = Error.IsEmpty() ? TEXT("CrucibleStyle.json needs massLooks: its material, ink material and kinds.") : Error;
			return false;
		}
		UMaterialInterface* Stone = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		UMaterialInterface* Ink = LoadObject<UMaterialInterface>(nullptr, *InkPath);
		if (!Stone || !Ink)
		{
			Error = TEXT("Build the environment art first (BuildEnvironmentArt.ps1 -Materials M_CrucibleRock,M_CrucibleInk): cannot load ") + (Stone ? InkPath : MaterialPath);
			return false;
		}

		// Each kind's surface: the kit's stone, its moss held to what a kind's top keeps.
		UPackage& Package = *World.GetPackage();
		TMap<FString, UMaterialInterface*> Surfaces;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Kind : (*Looks)->GetObjectField(TEXT("kinds"))->Values)
		{
			const FJsonObject& Look = *Kind.Value->AsObject();
			UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(&Package, *(TEXT("MI_CrucibleMass_") + Kind.Key), RF_Public | RF_Standalone);
			Instance->SetParentEditorOnly(Stone);
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("MossFrom")), static_cast<float>(Look.GetNumberField(TEXT("mossFrom"))));
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("MossTo")), static_cast<float>(Look.GetNumberField(TEXT("mossTo"))));
			const TArray<TSharedPtr<FJsonValue>>& Tint = Look.GetArrayField(TEXT("stoneTint"));
			Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("StoneTint")),
				FLinearColor(Tint[0]->AsNumber(), Tint[1]->AsNumber(), Tint[2]->AsNumber(), 1.0f));
			Instance->PostEditChange();
			Surfaces.Add(Kind.Key, Instance);
		}

		// Team A's masses, each built where it stands; Team B's are the same meshes turned half a turn about the centre.
		const FVeyraTerrainField Field(Tuning, ReliefOf(Style));
		const double Sink = Art->GetNumberField(TEXT("sink"));
		const double InkRise = (*Looks)->GetNumberField(TEXT("inkRise"));
		const double LipRise = (*Looks)->GetNumberField(TEXT("lipRise"));
		int32 Built = 0;
		for (const TSharedPtr<FJsonValue>& Value : Art->GetArrayField(TEXT("masses")))
		{
			const FJsonObject& Mass = *Value->AsObject();
			const FString Id = Mass.GetStringField(TEXT("id"));
			const FString Kind = Mass.GetStringField(TEXT("kind"));
			const FVeyraMassLayout* Layout = Tuning.Layout.Masses.Items.FindByPredicate([&Id](const FVeyraMassLayout& Each) { return Each.Id == Id; });
			UMaterialInterface* const* Surface = Surfaces.Find(Kind);
			if (!Layout || !Surface)
			{
				Error = FString::Printf(TEXT("CrucibleMassArt.json's mass %s has no mass or look of its kind %s."), *Id, *Kind);
				return false;
			}
			TArray<FVector2D> Outline;
			for (const FVeyraMapPoint& Point : Layout->Points)
			{
				Outline.Add(VeyraLayout::ToVector(Point));
			}
			const FMassGround Ground = MassGround(Field, Outline);
			TArray<double> Levels;
			for (const TSharedPtr<FJsonValue>& Level : Mass.GetArrayField(TEXT("levels")))
			{
				Levels.Add(Level->AsNumber());
			}

			// Its strata: the first from below the lowest floor under it, each later one from the top of the one before.
			FFacetedMesh Body;
			for (const TSharedPtr<FJsonValue>& StratumValue : Mass.GetArrayField(TEXT("strata")))
			{
				const FJsonObject& Stratum = *StratumValue->AsObject();
				const int32 Level = static_cast<int32>(Stratum.GetNumberField(TEXT("level")));
				if (!Levels.IsValidIndex(Level))
				{
					continue;
				}
				const double Bottom = Level == 0 ? Ground.Low - Sink : Ground.Base + Levels[Level - 1];
				Prism(Body, PointsOf(Stratum, TEXT("points")), Bottom, Ground.Base + Levels[Level]);
			}

			// Its ink: round its foot on the floor, and round the lip of each top step.
			FFacetedMesh Lines;
			for (const TSharedPtr<FJsonValue>& InkValue : Mass.GetArrayField(TEXT("inks")))
			{
				const FJsonObject& Line = *InkValue->AsObject();
				const bool bFoot = Line.GetStringField(TEXT("at")) == TEXT("Foot");
				const TArray<FVector2D> Loop = PointsOf(Line, TEXT("points"));
				TArray<FVector> Points;
				for (int32 Corner = 0; Corner < Loop.Num(); ++Corner)
				{
					// The foot's line follows the floor, so it is walked along it a ground step at a time.
					const FVector2D& From = Loop[Corner];
					const FVector2D& To = Loop[(Corner + 1) % Loop.Num()];
					const int32 Steps = bFoot ? FMath::Max(1, FMath::CeilToInt32(FVector2D::Distance(From, To) / GroundStep)) : 1;
					for (int32 Step = 0; Step < Steps; ++Step)
					{
						const FVector2D At = FMath::Lerp(From, To, static_cast<double>(Step) / Steps);
						Points.Add(FVector(At, bFoot ? Field.Height(At) + InkRise : Ground.Base + Levels.Last() + LipRise));
					}
				}
				Tube(Lines, Points, Line.GetNumberField(TEXT("size")));
			}

			if (Body.IsEmpty())
			{
				Error = FString::Printf(TEXT("Mass %s builds no geometry."), *Id);
				return false;
			}
			UStaticMesh* BodyMesh = Commit(Package, TEXT("SM_CrucibleMass_") + Id, MoveTemp(Body), *Surface);
			const FString Other = TEXT("b") + Id.RightChop(1);
			Place(World, *BodyMesh, TEXT("Mass_") + Id, 0.0);
			Place(World, *BodyMesh, TEXT("Mass_") + Other, 180.0);
			if (!Lines.IsEmpty())
			{
				UStaticMesh* InkMesh = Commit(Package, TEXT("SM_CrucibleInk_") + Id, MoveTemp(Lines), Ink);
				Place(World, *InkMesh, TEXT("Ink_") + Id, 0.0);
				Place(World, *InkMesh, TEXT("Ink_") + Other, 180.0);
			}
			++Built;
		}
		UE_LOG(LogVeyraWorldMasses, Display, TEXT("Built %d of Team A's masses and placed both teams'."), Built);
		return true;
	}

	bool Keeps(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error)
	{
		const TSharedPtr<FJsonObject>* Look = nullptr;
		FString MaterialPath;
		if (!Style.TryGetObjectField(TEXT("keepLook"), Look) || !(*Look)->TryGetStringField(TEXT("material"), MaterialPath))
		{
			Error = TEXT("CrucibleStyle.json needs keepLook: its material and proportions.");
			return false;
		}
		UMaterialInterface* Kit = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!Kit)
		{
			Error = TEXT("Build the environment art first (BuildEnvironmentArt.ps1): cannot load ") + MaterialPath;
			return false;
		}
		// The kit's masonry, its moss held off the walls' tops so they read as stone, not as hedges.
		UMaterialInstanceConstant* Masonry = NewObject<UMaterialInstanceConstant>(World.GetPackage(), TEXT("MI_CrucibleKeep"), RF_Public | RF_Standalone);
		Masonry->SetParentEditorOnly(Kit);
		Masonry->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("MossFrom")), static_cast<float>((*Look)->GetNumberField(TEXT("mossFrom"))));
		Masonry->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("MossTo")), static_cast<float>((*Look)->GetNumberField(TEXT("mossTo"))));
		Masonry->PostEditChange();
		const double Sink = (*Look)->GetNumberField(TEXT("sink"));
		const double MerlonWidth = (*Look)->GetNumberField(TEXT("merlonWidth"));
		const double MerlonGap = (*Look)->GetNumberField(TEXT("merlonGap"));
		const double MerlonHeight = (*Look)->GetNumberField(TEXT("merlonHeight"));
		const double PillarSize = (*Look)->GetNumberField(TEXT("pillarSize"));
		const double PillarRise = (*Look)->GetNumberField(TEXT("pillarRise"));
		const FVeyraTerrainField Field(Tuning, ReliefOf(Style));
		const FVeyraBattlegroundLayout& Layout = Tuning.Layout;

		// A footprint Length long along Along and Deep across it, about Centre.
		const auto Footprint = [](const FVector2D& Centre, const FVector2D& Along, double Length, double Deep) {
			const FVector2D Side(-Along.Y, Along.X);
			return TArray<FVector2D>{ Centre - Along * (Length / 2.0) - Side * (Deep / 2.0), Centre + Along * (Length / 2.0) - Side * (Deep / 2.0),
				Centre + Along * (Length / 2.0) + Side * (Deep / 2.0), Centre - Along * (Length / 2.0) + Side * (Deep / 2.0) };
		};

		// Team A's keep: each wall over exactly the terrain that blocks there, crenellated along its top.
		FFacetedMesh Keep;
		for (const FVeyraKeepWallLayout& Run : Layout.Keep.Walls)
		{
			for (const FVeyraTerrainBox& Box : VeyraLayout::KeepWallRun(Run, EVeyraTeam::A))
			{
				const FVector2D Along(-Box.Facing.Y, Box.Facing.X);
				const TArray<FVector2D> Body = Footprint(Box.Centre, Along, Box.Length, Box.Thickness);
				const FMassGround Ground = MassGround(Field, Body);
				const double Top = Ground.Base + Run.Height;
				Prism(Keep, Body, Ground.Low - Sink, Top);
				const int32 Merlons = FMath::FloorToInt32((Box.Length + MerlonGap) / (MerlonWidth + MerlonGap));
				const double Used = Merlons * MerlonWidth + FMath::Max(Merlons - 1, 0) * MerlonGap;
				for (int32 Merlon = 0; Merlon < Merlons; ++Merlon)
				{
					const FVector2D At = Box.Centre + Along * (-Used / 2.0 + MerlonWidth / 2.0 + Merlon * (MerlonWidth + MerlonGap));
					Prism(Keep, Footprint(At, Along, MerlonWidth, Box.Thickness), Top, Top + MerlonHeight);
				}
			}
		}
		// A pillar at each wall's end beside a gate, flush with that end, so the opening seen is the opening walked.
		for (const FVeyraKeepGateLayout& Gate : Layout.Keep.Gates)
		{
			const FVector2D Centre = VeyraLayout::ToVector(Gate.Center);
			for (const FVeyraKeepWallLayout& Run : Layout.Keep.Walls)
			{
				for (const int32 End : { 0, Run.Points.Num() - 1 })
				{
					const FVector2D Here = VeyraLayout::ToVector(Run.Points[End]);
					if (Run.Points.Num() < 2 || FVector2D::Distance(Here, Centre) > Gate.Width / 2.0 + PillarSize)
					{
						continue;
					}
					const FVector2D Next = VeyraLayout::ToVector(Run.Points[End == 0 ? 1 : End - 1]);
					const FVector2D Into = (Next - Here).GetSafeNormal();
					const TArray<FVector2D> Base = Footprint(Here + Into * (PillarSize / 2.0), Into, PillarSize, PillarSize);
					const FMassGround Ground = MassGround(Field, Base);
					Prism(Keep, Base, Ground.Low - Sink, Ground.Base + Run.Height + PillarRise);
				}
			}
		}
		if (Keep.IsEmpty())
		{
			return true;
		}
		UStaticMesh* Mesh = Commit(*World.GetPackage(), TEXT("SM_CrucibleKeep"), MoveTemp(Keep), Masonry);
		Place(World, *Mesh, TEXT("Keep_A"), 0.0);
		Place(World, *Mesh, TEXT("Keep_B"), 180.0);
		return true;
	}
}
