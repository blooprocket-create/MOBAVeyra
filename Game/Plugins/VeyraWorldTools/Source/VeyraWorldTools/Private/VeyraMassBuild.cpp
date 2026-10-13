// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraWorldBuild.h"
#include "VeyraMassSculpt.h"

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
	// How finely the floor under an outline is sampled for its base, in units a step, and how far a box-projected UV runs
	// over a unit of the world. Resolution, not tuning.
	constexpr double GroundStep = 100.0;
	constexpr double UVSpan = 400.0;

	/** One growing mesh, every face its own corners, so each shades flat as cut masonry does. */
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

	/** A static mesh in Package from Mesh: one material, no collision (ADR-073 §5: the masses' terrain is World's). */
	UStaticMesh* Commit(UPackage& Package, const FString& Name, FMeshDescription&& Mesh, UMaterialInterface* Material)
	{
		UStaticMesh* Asset = NewObject<UStaticMesh>(&Package, FName(*Name), RF_Public);
		Asset->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Surface"), TEXT("Surface")));
		FStaticMeshSourceModel& Source = Asset->AddSourceModel();
		Source.BuildSettings.bRecomputeNormals = false;
		Source.BuildSettings.bRecomputeTangents = true;
		Source.BuildSettings.bGenerateLightmapUVs = false;
		Asset->CreateMeshDescription(0, MoveTemp(Mesh));
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
		const TSharedPtr<FJsonObject>* Looks = nullptr;
		FString MaterialPath;
		if (!Style.TryGetObjectField(TEXT("massLooks"), Looks) || !(*Looks)->TryGetStringField(TEXT("material"), MaterialPath))
		{
			Error = TEXT("CrucibleStyle.json needs massLooks: its material and kinds.");
			return false;
		}
		FVeyraMassForms Forms;
		if (!FVeyraMassForms::Read(Style, Forms, Error))
		{
			return false;
		}
		UMaterialInterface* Stone = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!Stone)
		{
			Error = TEXT("Build the environment art first (BuildEnvironmentArt.ps1 -Materials M_CrucibleRock): cannot load ") + MaterialPath;
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
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("MossPatchFrom")), static_cast<float>(Look.GetNumberField(TEXT("mossPatchFrom"))));
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("MossPatchTo")), static_cast<float>(Look.GetNumberField(TEXT("mossPatchTo"))));
			const TArray<TSharedPtr<FJsonValue>>& Tint = Look.GetArrayField(TEXT("stoneTint"));
			Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("StoneTint")),
				FLinearColor(Tint[0]->AsNumber(), Tint[1]->AsNumber(), Tint[2]->AsNumber(), 1.0f));
			Instance->PostEditChange();
			Surfaces.Add(Kind.Key, Instance);
		}

		// Team A's masses, each sculpted where it stands; Team B's are the same meshes turned half a turn about the centre.
		const FVeyraTerrainField Field(Tuning, ReliefOf(Style));
		int32 Built = 0;
		for (const FVeyraMassShape& Mass : VeyraLayout::Masses(Tuning.Layout))
		{
			const FVeyraMassLayout& Entry = Tuning.Layout.Masses.Items[Mass.Index];
			if (!Entry.Id.StartsWith(TEXT("a")))
			{
				continue;
			}
			const FString Kind = StaticEnum<EVeyraMassKind>()->GetNameStringByValue(static_cast<int64>(Entry.Kind));
			const FVeyraMassForm* Form = Forms.Find(Mass);
			UMaterialInterface* const* Surface = Surfaces.Find(Kind);
			if (!Form || !Surface)
			{
				Error = FString::Printf(TEXT("Mass %s's kind %s has no form in massForms or no look in massLooks."), *Entry.Id, *Kind);
				return false;
			}
			FVeyraSculptedMass Sculpted = VeyraMassSculpt::Sculpt(Mass, *Form, Field, Forms.Sink, Forms.SampleSpacing, Forms.SeedOf(Mass));
			if (Sculpted.Mesh.Triangles().Num() == 0)
			{
				Error = FString::Printf(TEXT("Mass %s sculpts no geometry."), *Entry.Id);
				return false;
			}
			UStaticMesh* Body = Commit(Package, TEXT("SM_CrucibleMass_") + Entry.Id, MoveTemp(Sculpted.Mesh), *Surface);
			Place(World, *Body, TEXT("Mass_") + Entry.Id, 0.0);
			Place(World, *Body, TEXT("Mass_b") + Entry.Id.RightChop(1), 180.0);
			++Built;
		}
		UE_LOG(LogVeyraWorldMasses, Display, TEXT("Sculpted %d of Team A's masses and placed both teams'."), Built);
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
		// The kit's masonry, its moss only in sparse patches along the walls' tops, so they read as old stone and never as hedges.
		UMaterialInstanceConstant* Masonry = NewObject<UMaterialInstanceConstant>(World.GetPackage(), TEXT("MI_CrucibleKeep"), RF_Public | RF_Standalone);
		Masonry->SetParentEditorOnly(Kit);
		Masonry->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("MossFrom")), static_cast<float>((*Look)->GetNumberField(TEXT("mossFrom"))));
		Masonry->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("MossTo")), static_cast<float>((*Look)->GetNumberField(TEXT("mossTo"))));
		Masonry->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("MossPatchFrom")), static_cast<float>((*Look)->GetNumberField(TEXT("mossPatchFrom"))));
		Masonry->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("MossPatchTo")), static_cast<float>((*Look)->GetNumberField(TEXT("mossPatchTo"))));
		Masonry->PostEditChange();
		const double Sink = (*Look)->GetNumberField(TEXT("sink"));
		const double MerlonWidth = (*Look)->GetNumberField(TEXT("merlonWidth"));
		const double MerlonGap = (*Look)->GetNumberField(TEXT("merlonGap"));
		const double MerlonHeight = (*Look)->GetNumberField(TEXT("merlonHeight"));
		const double PillarSize = (*Look)->GetNumberField(TEXT("pillarSize"));
		const double PillarRise = (*Look)->GetNumberField(TEXT("pillarRise"));
		const double PlinthHeight = (*Look)->GetNumberField(TEXT("plinthHeight"));
		const double Batter = (*Look)->GetNumberField(TEXT("batter"));
		const double CopingHeight = (*Look)->GetNumberField(TEXT("copingHeight"));
		const double CopingOverhang = (*Look)->GetNumberField(TEXT("copingOverhang"));
		const double PierRise = (*Look)->GetNumberField(TEXT("pierRise"));
		if (!(PlinthHeight > 0.0) || Batter < 0.0 || !(CopingHeight > 0.0) || CopingOverhang < 0.0 || PierRise < 0.0)
		{
			Error = TEXT("keepLook needs a plinthHeight and copingHeight above 0, and a batter, copingOverhang and pierRise of at least 0.");
			return false;
		}
		const FVeyraTerrainField Field(Tuning, ReliefOf(Style));
		const FVeyraBattlegroundLayout& Layout = Tuning.Layout;

		// A footprint Length long along Along and Deep across it, about Centre.
		const auto Footprint = [](const FVector2D& Centre, const FVector2D& Along, double Length, double Deep) {
			const FVector2D Side(-Along.Y, Along.X);
			return TArray<FVector2D>{ Centre - Along * (Length / 2.0) - Side * (Deep / 2.0), Centre + Along * (Length / 2.0) - Side * (Deep / 2.0),
				Centre + Along * (Length / 2.0) + Side * (Deep / 2.0), Centre - Along * (Length / 2.0) + Side * (Deep / 2.0) };
		};

		// Team A's keep: each wall over exactly the terrain that blocks there, built as masonry is: a base course its full
		// thickness, the wall above stepping in by the batter on each face, and a coping course standing out from it at
		// the top, above anyone's head, crenellated along it.
		FFacetedMesh Keep;
		for (const FVeyraKeepWallLayout& Run : Layout.Keep.Walls)
		{
			for (const FVeyraTerrainBox& Box : VeyraLayout::KeepWallRun(Run, EVeyraTeam::A))
			{
				const FVector2D Along(-Box.Facing.Y, Box.Facing.X);
				const TArray<FVector2D> Body = Footprint(Box.Centre, Along, Box.Length, Box.Thickness);
				const FMassGround Ground = MassGround(Field, Body);
				const double Top = Ground.Base + Run.Height;
				const double Plinth = FMath::Min(Ground.Base + PlinthHeight, Top - CopingHeight);
				Prism(Keep, Body, Ground.Low - Sink, Plinth);
				Prism(Keep, Footprint(Box.Centre, Along, Box.Length, FMath::Max(Box.Thickness - 2.0 * Batter, 1.0)), Plinth, Top - CopingHeight);
				Prism(Keep, Footprint(Box.Centre, Along, Box.Length, Box.Thickness + 2.0 * CopingOverhang), Top - CopingHeight, Top);
				const int32 Merlons = FMath::FloorToInt32((Box.Length + MerlonGap) / (MerlonWidth + MerlonGap));
				const double Used = Merlons * MerlonWidth + FMath::Max(Merlons - 1, 0) * MerlonGap;
				for (int32 Merlon = 0; Merlon < Merlons; ++Merlon)
				{
					const FVector2D At = Box.Centre + Along * (-Used / 2.0 + MerlonWidth / 2.0 + Merlon * (MerlonWidth + MerlonGap));
					Prism(Keep, Footprint(At, Along, MerlonWidth, Box.Thickness), Top, Top + MerlonHeight);
				}
			}
		}
		// A capped pier: a base course Side across, its shaft stepping in by the batter, a cap standing out over it at Crown.
		const auto Pier = [&](const FVector2D& Centre, const FVector2D& Along, double Side, double Crown, const FMassGround& Ground) {
			const double Plinth = FMath::Min(Ground.Base + PlinthHeight, Crown - CopingHeight);
			Prism(Keep, Footprint(Centre, Along, Side, Side), Ground.Low - Sink, Plinth);
			const double Shaft = FMath::Max(Side - 2.0 * Batter, 1.0);
			Prism(Keep, Footprint(Centre, Along, Shaft, Shaft), Plinth, Crown - CopingHeight);
			const double Cap = Side + 2.0 * CopingOverhang;
			Prism(Keep, Footprint(Centre, Along, Cap, Cap), Crown - CopingHeight, Crown);
		};
		// A pier at each bend of a wall, wide enough to close the wedge its two straight runs leave outside the bend, so the
		// wall turns as one built thing.
		for (const FVeyraKeepWallLayout& Run : Layout.Keep.Walls)
		{
			for (int32 Bend = 1; Bend + 1 < Run.Points.Num(); ++Bend)
			{
				const FVector2D Here = VeyraLayout::ToVector(Run.Points[Bend]);
				const FVector2D In = (Here - VeyraLayout::ToVector(Run.Points[Bend - 1])).GetSafeNormal();
				const FVector2D Out = (VeyraLayout::ToVector(Run.Points[Bend + 1]) - Here).GetSafeNormal();
				// The outside corner lies half the thickness over the cosine of half the turn out from the bend.
				const double HalfTurnCosine = FMath::Sqrt(FMath::Max((1.0 + FVector2D::DotProduct(In, Out)) / 2.0, 0.25));
				const double Side = Run.Thickness / HalfTurnCosine;
				const FVector2D Along = (In + Out).IsNearlyZero() ? In : (In + Out).GetSafeNormal();
				const FMassGround Ground = MassGround(Field, Footprint(Here, Along, Side, Side));
				Pier(Here, Along, Side, Ground.Base + Run.Height + PierRise, Ground);
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
					const FVector2D PillarCentre = Here + Into * (PillarSize / 2.0);
					const FMassGround Ground = MassGround(Field, Footprint(PillarCentre, Into, PillarSize, PillarSize));
					Pier(PillarCentre, Into, PillarSize, Ground.Base + Run.Height + PillarRise, Ground);
				}
			}
		}
		if (Keep.IsEmpty())
		{
			return true;
		}
		UStaticMesh* Mesh = Commit(*World.GetPackage(), TEXT("SM_CrucibleKeep"), MoveTemp(Keep.Description), Masonry);
		Place(World, *Mesh, TEXT("Keep_A"), 0.0);
		Place(World, *Mesh, TEXT("Keep_B"), 180.0);
		return true;
	}
}
