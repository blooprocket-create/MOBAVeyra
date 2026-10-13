// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraWorldBuild.h"
#include "VeyraMassSculpt.h"
#include "Vision/VeyraDenseFogVisuals.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Elements/PCGCreatePoints.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Layout/VeyraDressingRules.h"
#include "Layout/VeyraLayout.h"
#include "Layout/VeyraRiver.h"
#include "Layout/VeyraTerrainField.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "Terrain/VeyraGround.h"
#include "Tuning/VeyraWorldTuning.h"

namespace
{
	const TCHAR* MeshFolder = TEXT("/Game/Veyra/World/Environment/Meshes/SM_Crucible_");

	/** Every variant of Family the environment kit imported, in order. */
	TArray<UStaticMesh*> Variants(const FString& Family)
	{
		TArray<UStaticMesh*> Meshes;
		for (int32 Variant = 0;; ++Variant)
		{
			const FString Path = FString::Printf(TEXT("%s%s_%02d"), MeshFolder, *Family, Variant);
			if (!FPackageName::DoesPackageExist(Path))
			{
				break;
			}
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path))
			{
				Meshes.Add(Mesh);
			}
		}
		return Meshes;
	}

	/** One piece of dressing: where, which way, how large, and which of its family's variants. */
	struct FPiece
	{
		FTransform Transform;
		int32 Variant = 0;
	};

	/** A region's pieces of one family, which a single PCG graph spawns and bakes. */
	struct FRegion
	{
		FString Name;
		FString Family;
		TArray<FPiece> Pieces;
	};

	/** The battleground's dressing: placements decided by Veyra's rules, then turned half a turn for Team B. */
	class FDressing
	{
	public:
		FDressing(const UWorld& InWorld, const FVeyraWorldTuning& InTuning, const FJsonObject& InRoot, const FJsonObject& InProfile)
			: World(InWorld)
			, Tuning(InTuning)
			, Layout(InTuning.Layout)
			, Root(InRoot)
			, Style(InProfile)
			, River(VeyraRiver::ShapeOf(InTuning.Layout))
			, Field(InTuning, VeyraWorldBuild::ReliefOf(InRoot))
		{
		}

		double Number(const TCHAR* Key) const { return Style.GetNumberField(Key); }

		FVector2D Range(const TCHAR* Key) const
		{
			const TArray<TSharedPtr<FJsonValue>>& Values = Style.GetArrayField(Key);
			return FVector2D(Values[0]->AsNumber(), Values[1]->AsNumber());
		}

		/** The ground's height at Point: the Landscape itself, so dressing stands on what was built. */
		TOptional<double> Ground(const FVector2D& Point) const
		{
			FHitResult Hit;
			if (!VeyraGround::Find(World, Point, Layout.Surface.MaxZ, Layout.Surface.MinZ, Hit))
			{
				return {};
			}
			return Hit.ImpactPoint.Z;
		}

		/** Adds a piece at Point, Yaw degrees, Scale, on the ground less Sink; and its rotation for Team B. */
		void Place(FRegion& Region, FRandomStream& Random, int32 Variants, const FVector2D& Point, double Yaw, const FVector& Scale, double Sink,
			TOptional<double> Base = {})
		{
			const int32 Variant = Random.RandHelper(FMath::Max(Variants, 1));
			for (const bool bRotated : { false, true })
			{
				const FVector2D Where = bRotated ? VeyraLayout::Rotate(Point) : Point;
				const TOptional<double> Height = Base.IsSet() ? Base : Ground(Where);
				if (!Height.IsSet())
				{
					continue;
				}
				FPiece& Piece = Region.Pieces.AddDefaulted_GetRef();
				Piece.Transform = FTransform(FRotator(0.0, bRotated ? Yaw + 180.0 : Yaw, 0.0), FVector(Where, Height.GetValue() - Sink), Scale);
				Piece.Variant = Variant;
			}
		}

		/** Jittered grid points over Team A's half of the floor and Reach beyond, Spacing apart. */
		template <typename FVisit>
		void ScatterTeamA(FRandomStream& Random, double Spacing, double Reach, FVisit&& Visit) const
		{
			const double Extent = Layout.HalfExtent + Reach;
			const double Jitter = Number(TEXT("pointJitterFraction"));
			for (double X = -Extent; X <= Extent; X += Spacing)
			{
				for (double Y = -Extent; Y <= Extent; Y += Spacing)
				{
					const FVector2D Point(X + Random.FRandRange(-Jitter, Jitter) * Spacing, Y + Random.FRandRange(-Jitter, Jitter) * Spacing);
					// Team A's half only, clear of the dividing line, so a piece and its rotation never meet.
					if (VeyraLayout::DepthInTeamAHalf(Layout, Point) > Spacing * 0.5)
					{
						Visit(Point);
					}
				}
			}
		}

		const UWorld& World;
		const FVeyraWorldTuning& Tuning;
		const FVeyraBattlegroundLayout& Layout;
		/** The whole style profile, and its dressing profile. */
		const FJsonObject& Root;
		const FJsonObject& Style;
		const FVeyraRiverShape& River;
		FVeyraTerrainField Field;
	};

	bool Bake(UWorld& World, const FRegion& Region, const TArray<UStaticMesh*>& Meshes, int32 Seed, double Timeout, FString& Error)
	{
		if (Region.Pieces.IsEmpty())
		{
			return true;
		}
		AActor* Source = World.SpawnActor<AActor>();
		FBox Bounds(ForceInit);
		for (const FPiece& Piece : Region.Pieces)
		{
			Bounds += Piece.Transform.GetLocation();
		}
		Bounds = Bounds.ExpandBy(1000.0);
		UBoxComponent* Root = NewObject<UBoxComponent>(Source);
		Root->SetBoxExtent(Bounds.GetExtent());
		Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Root->SetCanEverAffectNavigation(false);
		Source->SetRootComponent(Root);
		Source->AddInstanceComponent(Root);
		Root->RegisterComponent();
		Source->SetActorLocation(Bounds.GetCenter());
		UPCGComponent* PCG = NewObject<UPCGComponent>(Source);
		Source->AddInstanceComponent(PCG);
		PCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
		PCG->Seed = Seed;
		PCG->RegisterComponent();

		// One spawner a variant: each takes the points Veyra's rules assigned it, so the bake matches the placement
		// exactly; PCG instances and bakes them.
		UPCGGraph* Graph = NewObject<UPCGGraph>(Source);
		for (int32 Variant = 0; Variant < Meshes.Num(); ++Variant)
		{
			UPCGCreatePointsSettings* Points = nullptr;
			UPCGNode* Input = Graph->AddNodeOfType(Points);
			Points->CoordinateSpace = EPCGCoordinateSpace::World;
			Points->PointsToCreate.Reset();
			for (int32 Index = 0; Index < Region.Pieces.Num(); ++Index)
			{
				if (Region.Pieces[Index].Variant != Variant)
				{
					continue;
				}
				FPCGPoint& Point = Points->PointsToCreate.AddDefaulted_GetRef();
				Point.Transform = Region.Pieces[Index].Transform;
				Point.Seed = Seed + Index;
				Point.Density = 1.0f;
			}
			if (Points->PointsToCreate.IsEmpty())
			{
				continue;
			}
			UPCGStaticMeshSpawnerSettings* Spawner = nullptr;
			UPCGNode* Output = Graph->AddNodeOfType(Spawner);
			Spawner->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
			Spawner->bSynchronousLoad = true;
			UPCGMeshSelectorWeighted* Selector = CastChecked<UPCGMeshSelectorWeighted>(Spawner->MeshSelectorParameters);
			FPCGMeshSelectorWeightedEntry& Entry = Selector->MeshEntries.Emplace_GetRef(TSoftObjectPtr<UStaticMesh>(Meshes[Variant]), 1);
			Entry.Descriptor.BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Entry.Descriptor.bCanEverAffectNavigation = false;
			Graph->AddEdge(Input, PCGPinConstants::DefaultOutputLabel, Output, PCGPinConstants::DefaultInputLabel);
		}
		PCG->SetGraph(Graph);
		UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(&World);
		if (!Subsystem)
		{
			Error = TEXT("PCG subsystem unavailable in the authoring world.");
			return false;
		}
		PCG->GenerateLocal(true);
		const double Deadline = FPlatformTime::Seconds() + Timeout;
		while (PCG->IsGenerating() && FPlatformTime::Seconds() < Deadline)
		{
			Subsystem->Tick(0.0f);
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FPlatformProcess::SleepNoStats(0.001f);
		}
		if (PCG->IsGenerating())
		{
			Error = TEXT("PCG generation timed out for ") + Region.Name;
			return false;
		}
		AActor* Baked = PCG->ClearPCGLink();
		if (!Baked)
		{
			Error = TEXT("PCG generated no baked resources for ") + Region.Name;
			return false;
		}
		Baked->SetActorLabel(TEXT("Crucible_") + Region.Name);
		Baked->Tags.Add(TEXT("Veyra.GeneratedDressing"));
		TArray<UInstancedStaticMeshComponent*> Instances;
		Baked->GetComponents(Instances);
		int32 Count = 0;
		for (UInstancedStaticMeshComponent* Component : Instances)
		{
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetCanEverAffectNavigation(false);
			Count += Component->GetInstanceCount();
		}
		World.DestroyActor(Source);
		if (Count != Region.Pieces.Num())
		{
			Error = FString::Printf(TEXT("PCG %s baked %d of %d pieces."), *Region.Name, Count, Region.Pieces.Num());
			return false;
		}

		// The region's record: what made it, from what, and a digest of every placement, so a regeneration that
		// changes nothing is seen to change nothing.
		FString Canonical;
		for (const FPiece& Piece : Region.Pieces)
		{
			const FVector P = Piece.Transform.GetLocation();
			const FQuat Q = Piece.Transform.GetRotation();
			const FVector S = Piece.Transform.GetScale3D();
			Canonical += FString::Printf(TEXT("%d;%.3f,%.3f,%.3f;%.6f,%.6f,%.6f,%.6f;%.4f,%.4f,%.4f\n"), Piece.Variant, P.X, P.Y, P.Z, Q.X, Q.Y, Q.Z, Q.W, S.X, S.Y, S.Z);
		}
		TSharedRef<FJsonObject> Manifest = MakeShared<FJsonObject>();
		Manifest->SetStringField(TEXT("region"), Region.Name);
		Manifest->SetStringField(TEXT("family"), Region.Family);
		Manifest->SetStringField(TEXT("generator"), TEXT("VeyraWorldTools.Dressing.v2"));
		Manifest->SetNumberField(TEXT("seed"), Seed);
		Manifest->SetNumberField(TEXT("instanceCount"), Count);
		Manifest->SetNumberField(TEXT("variants"), Meshes.Num());
		Manifest->SetStringField(TEXT("placementDigest"), FMD5::HashAnsiString(*Canonical));
		Manifest->SetStringField(TEXT("collision"), TEXT("None; World owns what blocks"));
		FString Serialized;
		FJsonSerializer::Serialize(Manifest, TJsonWriterFactory<>::Create(&Serialized));
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("WorldGeneration/Regions");
		IFileManager::Get().MakeDirectory(*Directory, true);
		if (!FFileHelper::SaveStringToFile(Serialized, *(Directory / (Region.Name + TEXT(".json"))), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Error = TEXT("Cannot write the region manifest for ") + Region.Name;
			return false;
		}
		return true;
	}

	double YawOf(const FVector2D& Direction)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	}
}

namespace VeyraWorldBuild
{
	bool Dressing(UWorld& World, const FVeyraWorldTuning& Tuning, const FJsonObject& Style, FString& Error)
	{
		const TSharedPtr<FJsonObject>* Settings = nullptr;
		if (!Style.TryGetObjectField(TEXT("dressingProfile"), Settings))
		{
			Error = TEXT("Dressing requires the style profile's dressingProfile.");
			return false;
		}
		FDressing Dress(World, Tuning, Style, **Settings);
		const FJsonObject& Profile = **Settings;
		const FVeyraBattlegroundLayout& Layout = Tuning.Layout;
		const int32 Seed = static_cast<int32>(Style.GetNumberField(TEXT("seed")));
		const double Timeout = Style.GetNumberField(TEXT("pcgTimeoutSeconds"));
		TMap<FString, TArray<UStaticMesh*>> Kit;
		for (const TCHAR* Family : { TEXT("Cliff"), TEXT("Boulder"), TEXT("Pillar"), TEXT("Block"), TEXT("Stele"), TEXT("Tree"), TEXT("Shrub"), TEXT("Fern"), TEXT("Reeds"), TEXT("Grass") })
		{
			TArray<UStaticMesh*> Meshes = Variants(Family);
			if (Meshes.IsEmpty())
			{
				Error = FString::Printf(TEXT("Import the environment kit first (BuildEnvironmentArt.ps1): no %s meshes."), Family);
				return false;
			}
			Kit.Add(Family, MoveTemp(Meshes));
		}
		const auto Size = [&Kit](const TCHAR* Family) { return Kit[Family][0]->GetBoundingBox().GetSize(); };
		// Each region its own allocation, so a reference to one holds however many are added after it.
		TIndirectArray<FRegion> Regions;
		const auto Region = [&Regions](const TCHAR* Name, const TCHAR* Family) -> FRegion& {
			FRegion& Made = Regions[Regions.Add(new FRegion())];
			Made.Name = Name;
			Made.Family = Family;
			return Made;
		};
		// Each of Team A's masses (ADR-073 §5): what stands on its top, from the masses' art (a forest's canopy, a cliff's
		// shrubs and boulders), and along every mass's foot, clumps of grass and fern where rock meets the floor. Its cliffs are its own mesh (Masses). Team B's
		// are the rotation (Place).
		{
			const TSharedPtr<FJsonObject> Art = LoadMassArt(Tuning, Error);
			const TSharedPtr<FJsonObject>* Looks = nullptr;
			FVeyraMassForms Forms;
			if (!Art || !Style.TryGetObjectField(TEXT("massLooks"), Looks) || !FVeyraMassForms::Read(Style, Forms, Error))
			{
				Error = Error.IsEmpty() ? TEXT("Dressing the masses requires the style profile's massLooks.") : Error;
				return false;
			}
			const TArray<FVeyraMassShape> Shapes = VeyraLayout::Masses(Layout);
			FRandomStream Random(Seed + 1);
			// Each piece the art names: its region, the kit family it draws from, and the scale that stands it at its height.
			struct FTopPiece
			{
				FRegion* Region;
				const TCHAR* Family;
				double Scale;
			};
			TMap<FString, FTopPiece> TopPieces;
			for (const TPair<const TCHAR*, const TCHAR*>& Kind : { TPair<const TCHAR*, const TCHAR*>(TEXT("Tree"), TEXT("treeHeight")),
					 TPair<const TCHAR*, const TCHAR*>(TEXT("Shrub"), TEXT("shrubHeight")), TPair<const TCHAR*, const TCHAR*>(TEXT("Boulder"), TEXT("boulderHeight")) })
			{
				FRegion& Made = Region(*(FString(TEXT("Mass_")) + Kind.Key + TEXT("s")), Kind.Key);
				TopPieces.Add(Kind.Key, FTopPiece{ &Made, Kind.Key, (*Looks)->GetNumberField(Kind.Value) / FMath::Max(Size(Kind.Key).Z, 1.0) });
			}
			FRegion& Grass = Region(TEXT("Mass_FootGrass"), TEXT("Grass"));
			FRegion& Ferns = Region(TEXT("Mass_FootFerns"), TEXT("Fern"));
			const FVector2D Reach = Dress.Range(TEXT("massFootReach"));
			const FVector2D Spacing = Dress.Range(TEXT("massFootSpacing"));
			const FVector2D FootScale = Dress.Range(TEXT("massFootScale"));
			const double FernShare = Dress.Number(TEXT("massFootFernShare"));
			for (const TSharedPtr<FJsonValue>& Value : Art->GetArrayField(TEXT("masses")))
			{
				const FJsonObject& Mass = *Value->AsObject();
				const FString Id = Mass.GetStringField(TEXT("id"));
				const int32 EntryIndex = Layout.Masses.Items.IndexOfByPredicate([&Id](const FVeyraMassLayout& Each) { return Each.Id == Id; });
				const FVeyraMassShape* Shape = Shapes.FindByPredicate([EntryIndex](const FVeyraMassShape& Each) { return Each.Index == EntryIndex; });
				const FVeyraMassForm* Form = Shape ? Forms.Find(*Shape) : nullptr;
				if (!Form)
				{
					Error = TEXT("CrucibleMassArt.json names a mass the layout lacks, or one whose kind has no form: ") + Id;
					return false;
				}
				const TArray<FVector2D>& Outline = Shape->Outline;
				for (const TSharedPtr<FJsonValue>& PieceValue : Mass.GetArrayField(TEXT("canopy")))
				{
					const FJsonObject& Piece = *PieceValue->AsObject();
					const TArray<TSharedPtr<FJsonValue>>& At = Piece.GetArrayField(TEXT("at"));
					const FTopPiece* Kind = TopPieces.Find(Piece.GetStringField(TEXT("kind")));
					if (!Kind)
					{
						Error = TEXT("CrucibleMassArt.json names a piece the dressing cannot place: ") + Piece.GetStringField(TEXT("kind"));
						return false;
					}
					const FVector2D Spot(At[0]->AsNumber(), At[1]->AsNumber());
					Dress.Place(*Kind->Region, Random, Kit[Kind->Family].Num(), Spot, Piece.GetNumberField(TEXT("yaw")),
						FVector(Kind->Scale * Piece.GetNumberField(TEXT("scale"))), Dress.Number(TEXT("treeSink")),
						VeyraMassSculpt::TopAt(*Shape, *Form, Dress.Field, Forms.SeedOf(*Shape), Spot));
				}
				// The foot's clumps, a little way out from its edge on open ground, never in play's way.
				const double Out = Random.FRandRange(Reach.X, Reach.Y);
				for (int32 Corner = 0; Corner < Outline.Num(); ++Corner)
				{
					const FVector2D& From = Outline[Corner];
					const FVector2D& To = Outline[(Corner + 1) % Outline.Num()];
					const double Length = FVector2D::Distance(From, To);
					const FVector2D Along = (To - From).GetSafeNormal();
					// Counter-clockwise, so open ground lies to the right of each span.
					const FVector2D Outward(Along.Y, -Along.X);
					for (double Distance = Random.FRandRange(0.0, Spacing.X); Distance < Length; Distance += Random.FRandRange(Spacing.X, Spacing.Y))
					{
						const FVector2D Spot = From + Along * Distance + Outward * Out;
						const FVeyraTerrainSample Sample = Dress.Field.Sample(Spot);
						if (Sample.Mass > 0.0 || Sample.Keep > 0.0 || Sample.Road > 0.0 || !VeyraDressing::Allows(Tuning, Spot, 0.0))
						{
							continue;
						}
						const bool bFern = Random.FRand() < FernShare;
						Dress.Place(bFern ? Ferns : Grass, Random, Kit[bFern ? TEXT("Fern") : TEXT("Grass")].Num(), Spot, Random.FRandRange(0.0, 360.0),
							FVector(Random.FRandRange(FootScale.X, FootScale.Y)), 0.0);
					}
				}
			}
		}
		// The playable square's edge: a cliff line up the rim, open where the river leaves, and forest across the vista.
		{
			FRandomStream Random(Seed + 2);
			FRegion& Cliffs = Region(TEXT("Rim_Cliffs"), TEXT("Cliff"));
			FRegion& Forest = Region(TEXT("Vista_Forest"), TEXT("Tree"));
			const FVector Cliff = Size(TEXT("Cliff"));
			const double Span = Dress.Number(TEXT("rimCliffSpan"));
			const double Edge = VeyraLayout::PlayableHalfExtent(Layout) + Dress.Number(TEXT("rimInset"));
			const FVector2D Height = Dress.Range(TEXT("cliffHeightScale"));
			const double Gorge = Layout.Terrain.BankWidth * Dress.Number(TEXT("gorgeClearance"));
			// The two sides nearest Team A's base, walked once; Team B's are their rotation.
			for (const FVector2D& Side : { FVector2D(-1.0, 0.0), FVector2D(0.0, -1.0) })
			{
				const FVector2D Along(-Side.Y, Side.X);
				for (double Distance = -Edge; Distance <= Edge; Distance += Span)
				{
					const FVector2D Point = Side * Edge + Along * (Distance + Random.FRandRange(-0.15, 0.15) * Span);
					if (Dress.River.SignedDistance(Point) < Gorge)
					{
						continue;
					}
					Dress.Place(Cliffs, Random, Kit[TEXT("Cliff")].Num(), Point, YawOf(Along) + Random.FRandRange(-8.0, 8.0),
						FVector(Dress.Number(TEXT("cliffOverlap")) * Span / Cliff.X, Random.FRandRange(1.0, 1.4), Random.FRandRange(Height.X, Height.Y) * 1.3),
						Dress.Number(TEXT("cliffSink")));
				}
			}
			const FVector2D TreeScale = Dress.Range(TEXT("treeScale"));
			const double Start = VeyraLayout::PlayableHalfExtent(Layout) + Layout.Terrain.BoundaryWidth * Dress.Number(TEXT("vistaForestStart"));
			Dress.ScatterTeamA(Random, Dress.Number(TEXT("vistaTreeSpacing")), Layout.Terrain.BoundaryWidth + Dress.Root.GetNumberField(TEXT("vistaWidth")), [&](const FVector2D& Point) {
				if (FMath::Max(FMath::Abs(Point.X), FMath::Abs(Point.Y)) < Start || Dress.River.SignedDistance(Point) < Gorge)
				{
					return;
				}
				Dress.Place(Forest, Random, Kit[TEXT("Tree")].Num(), Point, Random.FRandRange(0.0, 360.0), FVector(Random.FRandRange(TreeScale.X, TreeScale.Y) * 1.15),
					Dress.Number(TEXT("treeSink")));
			});
		}

		// The jungle's floor: shrubs, ferns, grass and boulders, only where dressing may stand and the ground is jungle.
		{
			FRandomStream Random(Seed + 3);
			for (const TSharedPtr<FJsonValue>& Value : Profile.GetArrayField(TEXT("jungle")))
			{
				const FJsonObject& Family = *Value->AsObject();
				const FString Name = Family.GetStringField(TEXT("family"));
				FRegion& Scatter = Region(*(TEXT("Jungle_") + Name), *Name);
				const double Chance = Family.GetNumberField(TEXT("chance"));
				const double Radius = Family.GetNumberField(TEXT("radius"));
				const TArray<TSharedPtr<FJsonValue>>& ScaleRange = Family.GetArrayField(TEXT("scale"));
				Dress.ScatterTeamA(Random, Family.GetNumberField(TEXT("spacing")), 0.0, [&](const FVector2D& Point) {
					if (Random.FRand() > Chance || !VeyraDressing::Allows(Tuning, Point, Radius))
					{
						return;
					}
					const FVeyraTerrainSample Sample = Dress.Field.Sample(Point);
					if (Sample.Road > 0.0 || Sample.Keep > 0.0 || Sample.Mass > 0.0 || Sample.WaterDistance < Dress.Number(TEXT("shoreClearance")))
					{
						return;
					}
					Dress.Place(Scatter, Random, Kit[Name].Num(), Point, Random.FRandRange(0.0, 360.0),
						FVector(Random.FRandRange(ScaleRange[0]->AsNumber(), ScaleRange[1]->AsNumber())), Family.GetNumberField(TEXT("sink")));
				});
			}
		}

		// The visual consumes exactly the layout circles that Vision joins into volumes.
		AActor* FogOwner = World.SpawnActor<AActor>();
		FogOwner->SetActorLabel(TEXT("DenseFog_Visuals"));
		FogOwner->Tags.Add(VeyraDenseFogVisuals::AuthoredTag());
		for (const FVeyraFogPlacement& Fog : VeyraLayout::DenseFog(Layout))
		{
			if (!VeyraDenseFogVisuals::Add(*FogOwner, Fog.Center, Fog.Radius, Layout.Surface))
			{
				Error = TEXT("Could not place Dense Fog presentation on the playable surface.");
				return false;
			}
		}

		// The river's banks: boulders along the shore, never in the water units wade.
		{
			FRandomStream Random(Seed + 5);
			FRegion& Boulders = Region(TEXT("Shore_Boulders"), TEXT("Boulder"));
			const double Chance = Dress.Number(TEXT("shoreBoulderChance"));
			const FVector2D Band = Dress.Range(TEXT("shoreBoulderBand"));
			const FVector2D Scale = Dress.Range(TEXT("shoreBoulderScale"));
			Dress.ScatterTeamA(Random, Dress.Number(TEXT("shoreBoulderSpacing")), Layout.Terrain.BoundaryWidth, [&](const FVector2D& Point) {
				const double Water = Dress.River.SignedDistance(Point);
				if (Water < Band.X || Water > Band.Y || Random.FRand() > Chance || !VeyraDressing::Allows(Tuning, Point, 0.0))
				{
					return;
				}
				Dress.Place(Boulders, Random, Kit[TEXT("Boulder")].Num(), Point, Random.FRandRange(0.0, 360.0), FVector(Random.FRandRange(Scale.X, Scale.Y)), 20.0);
			});
		}

		// The old Fluxways' ruins: pillars and blocks beside the lanes, glyph steles round each Flux Well.
		{
			FRandomStream Random(Seed + 6);
			FRegion& Pillars = Region(TEXT("Lane_Pillars"), TEXT("Pillar"));
			FRegion& Blocks = Region(TEXT("Lane_Blocks"), TEXT("Block"));
			FRegion& Steles = Region(TEXT("Well_Steles"), TEXT("Stele"));
			const double Chance = Dress.Number(TEXT("ruinChance"));
			for (const FVeyraLaneLayout& Lane : Layout.Lanes)
			{
				const double Length = VeyraLayout::Length(Lane.Points);
				for (double Distance = 0.0; Distance < Length; Distance += Dress.Number(TEXT("ruinSpacing")))
				{
					const FVector2D Point = VeyraLayout::PointAlong(Lane.Points, Distance);
					const FVector2D Ahead = (VeyraLayout::PointAlong(Lane.Points, Distance + 10.0) - Point).GetSafeNormal();
					const FVector2D Side(-Ahead.Y, Ahead.X);
					for (const double Sign : { -1.0, 1.0 })
					{
						const FVector2D Spot = Point + Side * Sign * (Lane.Width / 2.0 + Dress.Number(TEXT("ruinOffset")) + Random.FRandRange(0.0, 120.0));
						if (VeyraLayout::DepthInTeamAHalf(Layout, Spot) < 600.0 || Random.FRand() > Chance || !VeyraDressing::Allows(Tuning, Spot, 120.0)
							|| Dress.Field.Sample(Spot).Mass > 0.0)
						{
							continue;
						}
						FRegion& Into = Random.FRand() < 0.4 ? Pillars : Blocks;
						Dress.Place(Into, Random, Kit[Into.Family].Num(), Spot, YawOf(Ahead) + Random.FRandRange(-12.0, 12.0), FVector(Random.FRandRange(0.85, 1.15)), 0.0);
					}
				}
			}
			// Each keep's floor lined inside its edge by the old Fluxway's ruins, clear of its lanes, towers, Well and fountain.
			const TArray<FVector2D> Floor = VeyraLayout::KeepFloor(Layout, EVeyraTeam::A);
			const double PillarInset = Dress.Number(TEXT("basePillarInset"));
			for (int32 Corner = 0; Corner < Floor.Num(); ++Corner)
			{
				const FVector2D From = Floor[Corner];
				const FVector2D To = Floor[(Corner + 1) % Floor.Num()];
				const double Length = FVector2D::Distance(From, To);
				const FVector2D Along = (To - From).GetSafeNormal();
				const int32 Count = FMath::FloorToInt(Length / Dress.Number(TEXT("basePillarSpacing")));
				for (int32 Index = 1; Index < Count; ++Index)
				{
					const FVector2D OnEdge = From + Along * (Length * (Index + Random.FRandRange(-0.2, 0.2)) / Count);
					// Inward, whichever way round the floor runs.
					const FVector2D Left(-Along.Y, Along.X);
					const FVector2D Spot = OnEdge + (VeyraLayout::IsInPolygon(Floor, OnEdge + Left * PillarInset) ? Left : -Left) * PillarInset;
					if (!VeyraDressing::Allows(Tuning, Spot, 150.0) || !VeyraLayout::IsInPolygon(Floor, Spot))
					{
						continue;
					}
					FRegion& Into = Random.FRand() < 0.65 ? Pillars : Blocks;
					Dress.Place(Into, Random, Kit[Into.Family].Num(), Spot, YawOf(Along), FVector(Random.FRandRange(0.95, 1.25)), 0.0);
				}
			}
			// Steles round each Well: one of each pair of sites the half turn swaps, since Place sets the other's. The
			// sites may stand on the line between the halves, so the pair, not the half, decides which.
			const double Ring = Tuning.FluxWells.Radius + Dress.Number(TEXT("steleRing"));
			const int32 SteleCount = static_cast<int32>(Dress.Number(TEXT("steleCount")));
			const double SteleJitter = Dress.Number(TEXT("steleJitterRadians"));
			TArray<FVector2D> Dressed;
			for (const FVeyraMapPoint& Site : Tuning.FluxWells.Sites)
			{
				const FVector2D Well = VeyraLayout::ToVector(Site);
				if (Dressed.ContainsByPredicate([&Well](const FVector2D& Other) { return Other.Equals(VeyraLayout::Rotate(Well), 1.0); }))
				{
					continue;
				}
				Dressed.Add(Well);
				for (int32 Index = 0; Index < SteleCount; ++Index)
				{
					const double Angle = UE_TWO_PI * Index / SteleCount + Random.FRandRange(-SteleJitter, SteleJitter);
					const FVector2D Spot = Well + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Ring;
					if (Dress.River.SignedDistance(Spot) < Dress.Number(TEXT("shoreClearance")))
					{
						continue;
					}
					Dress.Place(Steles, Random, Kit[TEXT("Stele")].Num(), Spot, FMath::RadiansToDegrees(Angle) + 90.0, FVector(1.0), 0.0);
				}
			}
		}

		// Each lane's edging (ADR-073 §5): kerbstones flush with the road along both its edges, some fallen away, so the
		// causeway reads as built and old. Team A's half of each lane; Team B's are its rotation (Place).
		{
			const TSharedPtr<FJsonObject>* Edging = nullptr;
			if (!Profile.TryGetObjectField(TEXT("laneEdging"), Edging))
			{
				Error = TEXT("Dressing requires the dressing profile's laneEdging.");
				return false;
			}
			const auto Setting = [&Edging](const TCHAR* Key) { return (*Edging)->GetNumberField(Key); };
			const TArray<TSharedPtr<FJsonValue>>& SizeValues = (*Edging)->GetArrayField(TEXT("size"));
			const FVector Stone(SizeValues[0]->AsNumber(), SizeValues[1]->AsNumber(), SizeValues[2]->AsNumber());
			const double Spacing = Setting(TEXT("spacing"));
			if (!(Spacing > 0.0) || !(Stone.GetMin() > 0.0))
			{
				Error = TEXT("laneEdging needs a spacing and a stone size above 0.");
				return false;
			}
			const FVector Block = Size(TEXT("Block"));
			const FVector Scale(Stone.X / FMath::Max(Block.X, 1.0), Stone.Y / FMath::Max(Block.Y, 1.0), Stone.Z / FMath::Max(Block.Z, 1.0));
			FRandomStream Random(Seed + 7);
			FRegion& Kerbs = Region(TEXT("Lane_Edging"), TEXT("Block"));
			for (int32 LaneIndex = 0; LaneIndex < Layout.Lanes.Num(); ++LaneIndex)
			{
				const FVeyraLaneLayout& Lane = Layout.Lanes[LaneIndex];
				const double Length = VeyraLayout::Length(Lane.Points);
				for (double Along = Random.FRandRange(0.0, Spacing); Along < Length; Along += Spacing)
				{
					const FVector2D Here = VeyraLayout::PointAlong(Lane.Points, Along);
					// Clear of the dividing line, so a stone and its rotation never meet.
					if (VeyraLayout::DepthInTeamAHalf(Layout, Here) < Spacing)
					{
						continue;
					}
					const FVector2D Ahead = (VeyraLayout::PointAlong(Lane.Points, Along + Spacing * 0.5) - Here).GetSafeNormal();
					const FVector2D Side(-Ahead.Y, Ahead.X);
					for (const double Sign : { -1.0, 1.0 })
					{
						const double Out = Lane.Width / 2.0 + Setting(TEXT("outset")) + Random.FRandRange(-1.0, 1.0) * Setting(TEXT("offsetJitter"));
						const FVector2D Spot = Here + Side * Sign * Out;
						const double Grown = 1.0 + Random.FRandRange(-1.0, 1.0) * Setting(TEXT("sizeJitter"));
						const double Yaw = YawOf(Ahead) + Random.FRandRange(-1.0, 1.0) * Setting(TEXT("yawJitter"));
						if (Random.FRand() < Setting(TEXT("missingShare")) || !VeyraDressing::AllowsEdging(Tuning, Spot, Stone.X / 2.0, LaneIndex)
							|| Dress.Field.Sample(Spot).Mass > 0.0)
						{
							continue;
						}
						Dress.Place(Kerbs, Random, Kit[TEXT("Block")].Num(), Spot, Yaw, Scale * Grown, Setting(TEXT("sink")));
					}
				}
			}
		}

		for (int32 Index = 0; Index < Regions.Num(); ++Index)
		{
			if (!Bake(World, Regions[Index], Kit[Regions[Index].Family], Seed + 100 * (Index + 1), Timeout, Error))
			{
				return false;
			}
		}
		return true;
	}
}
