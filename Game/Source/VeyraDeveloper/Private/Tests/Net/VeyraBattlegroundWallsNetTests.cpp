// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Algo/NoneOf.h"
#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#if ENABLE_PIE_NETWORK_TEST

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Layout/VeyraLayout.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Terrain/VeyraTerrainMass.h"
#include "Tests/Net/VeyraBattlegroundNetTestHelpers.h"
#include "Tests/Net/VeyraNetTestHelpers.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"

namespace VeyraNetTests
{
	// Veyra.Net.BattlegroundTerrain.*: the committed battleground's terrain stands solid on every machine, each building its
	// own, and the server's paths go round it to every lane, camp and Flux Well, never into a mass (ADR-073 §3).
	NETWORK_TEST_CLASS(BattlegroundTerrain, "Veyra.Net")
	{
		struct FState : public FBasePIENetworkComponentState
		{
		};

		FPIENetworkComponent<FState> Network{ TestRunner, TestCommandBuilder, bInitializing };
		TUniquePtr<FScopedExpectedPlayers> ExpectedPlayers;
		FVeyraGreyboxLayout Greybox;

		// Fixture values: how long the server may take to build the whole battleground's navigation, how
		// far a probed point may be moved onto it, how far beyond a wall's faces its sides are probed, and
		// how far from every mass a probed side must lie to count as open ground.
		static constexpr double NavigationBuildSeconds = 120.0;
		static constexpr double ProjectionReach = 300.0;
		static constexpr double BesideWall = 200.0;
		static constexpr double BodyClearance = 60.0;

		BEFORE_EACH()
		{
			IgnoreKnownIrisWarnings(*TestRunner);
			ASSERT_THAT(IsTrue(VeyraGreybox::LoadLayout(Greybox).IsEmpty()));
			ExpectedPlayers = MakeUnique<FScopedExpectedPlayers>(MatchClientCount);
			BuildMatchNetwork(Network);
		}

		AFTER_EACH()
		{
			ExpectedPlayers.Reset();
		}

		static const FVeyraBattlegroundLayout& Layout()
		{
			return UVeyraWorldTuningSubsystem::Get().Layout;
		}

		/** A complete path between two points of the floor, and its length; unset where none joins them. */
		static TOptional<double> PathLength(UWorld* World, const FVector2D& From, const FVector2D& To)
		{
			UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			FNavLocation Start;
			FNavLocation End;
			const FVector Reach(ProjectionReach);
			if (!Navigation || !Navigation->ProjectPointToNavigation(FVector(From, 0.0), Start, Reach) || !Navigation->ProjectPointToNavigation(FVector(To, 0.0), End, Reach))
			{
				return {};
			}
			const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Start.Location, End.Location);
			if (!Path || !Path->IsValid() || Path->IsPartial())
			{
				return {};
			}
			return Path->GetPathLength();
		}

		/** How many of the world's terrain boxes block. */
		static int32 SolidBoxes(UWorld* World)
		{
			int32 Solid = 0;
			for (TActorIterator<AVeyraTerrainMass> It(World); It; ++It)
			{
				for (const UBoxComponent* Box : It->GetBoxes())
				{
					Solid += Box && Box->GetCollisionEnabled() != ECollisionEnabled::NoCollision ? 1 : 0;
				}
			}
			return Solid;
		}

		TEST_METHOD(TheTerrainStandsForEveryoneAndPathsGoRoundIt)
		{
			const int32 BoxCount = VeyraLayout::Walls(Layout()).Num();
			ASSERT_THAT(IsTrue(BoxCount > 0));
			Network
				.ThenServer(TEXT("Build the committed battleground on the server"), [this](FState& State) {
					VeyraBattlegroundBuilder::SpawnRuntimeBattleground(*State.World, Layout(), Greybox, /*bServer*/ true);
					State.World->GetSubsystem<UVeyraBattlegroundSubsystem>()->SpawnStructures(Layout());
				})
				.ThenClients(TEXT("Build the floor on each client, which raises its own terrain as the map's marker would"), [this](FState& State) {
					VeyraBattlegroundBuilder::SpawnRuntimeBattleground(*State.World, Layout(), Greybox, /*bServer*/ false);
					State.World->GetSubsystem<UVeyraBattlegroundSubsystem>()->RaiseTerrain(Layout());
				})
				.ThenServer(TEXT("The server has every box, solid"), [this, BoxCount](FState& State) {
					ASSERT_THAT(AreEqual(BoxCount, SolidBoxes(State.World)));
				})
				.ThenClients(TEXT("Every client has every box, solid, none of it replicated"), [this, BoxCount](FState& State) {
					ASSERT_THAT(AreEqual(BoxCount, SolidBoxes(State.World)));
					for (TActorIterator<AVeyraTerrainMass> It(State.World); It; ++It)
					{
						ASSERT_THAT(IsFalse(It->GetIsReplicated()));
					}
				})
				.UntilServer(TEXT("The server's navigation is built round the terrain"), [](FState& State) {
					UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(State.World);
					return Navigation && Navigation->GetDefaultNavDataInstance() && !Navigation->IsNavigationBuildInProgress()
						&& PathLength(State.World, VeyraLayout::Fountain(Layout(), EVeyraTeam::A), VeyraLayout::Fountain(Layout(), EVeyraTeam::B)).IsSet();
				}, FTimespan::FromSeconds(NavigationBuildSeconds))
				.ThenServer(TEXT("Paths reach every lane's ends, camp and Flux Well from Team A's fountain, never into a mass, and go round a keep wall"), [this](FState& State) {
					const FVector2D Fountain = VeyraLayout::Fountain(Layout(), EVeyraTeam::A);
					TArray<TPair<FString, FVector2D>> Targets;
					for (const FVeyraLaneLayout& Lane : Layout().Lanes)
					{
						Targets.Add({ UEnum::GetValueAsString(Lane.Lane) + TEXT(" start"), VeyraLayout::ToVector(Lane.Points[0]) });
						Targets.Add({ UEnum::GetValueAsString(Lane.Lane) + TEXT(" end"), VeyraLayout::ToVector(Lane.Points.Last()) });
					}
					for (const FVeyraCampTuning& Camp : UVeyraWorldTuningSubsystem::Get().Wildlife.Camps)
					{
						for (const EVeyraTeam Team : { EVeyraTeam::A, EVeyraTeam::B })
						{
							Targets.Add({ Camp.Id.ToString(), VeyraLayout::ForTeam(VeyraLayout::ToVector(Camp.Center), Team) });
						}
					}
					for (const FVeyraMapPoint& Site : UVeyraWorldTuningSubsystem::Get().FluxWells.Sites)
					{
						Targets.Add({ TEXT("a Flux Well"), VeyraLayout::ToVector(Site) });
					}
					for (const TPair<FString, FVector2D>& Target : Targets)
					{
						ASSERT_THAT(IsTrue(PathLength(State.World, Fountain, Target.Value).IsSet(), *FString::Printf(TEXT("a path to %s"), *Target.Key)));
					}

					// Deep inside each mass, the ground its edge ring closes off: no path reaches its deepest probed point.
					const TArray<FVeyraMassShape> Masses = VeyraLayout::Masses(Layout());
					const double Deep = Layout().MassRules.EdgeThickness + ProjectionReach;
					for (const FVeyraMassShape& Mass : Masses)
					{
						const FBox2D Bounds(Mass.Outline);
						constexpr int32 Steps = 16;
						FVector2D Deepest = Bounds.GetCenter();
						for (int32 X = 1; X < Steps; ++X)
						{
							for (int32 Y = 1; Y < Steps; ++Y)
							{
								const FVector2D Inside = Bounds.Min + (Bounds.Max - Bounds.Min) * FVector2D(X, Y) / Steps;
								Deepest = Mass.DepthInside(Inside) > Mass.DepthInside(Deepest) ? Inside : Deepest;
							}
						}
						if (Mass.DepthInside(Deepest) > Deep)
						{
							ASSERT_THAT(IsFalse(PathLength(State.World, Fountain, Deepest).IsSet(), *FString::Printf(TEXT("no path into a mass at %s"), *Deepest.ToString())));
						}
					}

					// From outside a keep's wall to inside it is farther by path than in a straight line: round through a gate. Masses
					// press against the keep's walls, so the wall chosen is one with open ground a body's width clear on both sides.
					const auto Open = [&Masses](const FVector2D& Point) { return Algo::NoneOf(Masses, [&Point](const FVeyraMassShape& Mass) { return Mass.DepthInside(Point) > -BodyClearance; }); };
					TOptional<FVeyraTerrainBox> Across;
					for (const FVeyraTerrainBox& Box : VeyraLayout::KeepWalls(Layout()))
					{
						const double Out = Box.Thickness / 2.0 + BesideWall;
						if ((!Across.IsSet() || Box.Length > Across->Length) && Open(Box.Centre + Box.Facing * Out) && Open(Box.Centre - Box.Facing * Out))
						{
							Across = Box;
						}
					}
					ASSERT_THAT(IsTrue(Across.IsSet(), TEXT("a keep wall with open ground on both sides")));
					const FVeyraTerrainBox Wall = Across.GetValue();
					const FVector2D Front = Wall.Centre + Wall.Facing * (Wall.Thickness / 2.0 + BesideWall);
					const FVector2D Back = Wall.Centre - Wall.Facing * (Wall.Thickness / 2.0 + BesideWall);
					// The straight way across it is cut, and a path still joins its two sides, round it.
					UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(State.World);
					FNavLocation From;
					FNavLocation To;
					ASSERT_THAT(IsTrue(Navigation->ProjectPointToNavigation(FVector(Front, 0.0), From, FVector(ProjectionReach))
						&& Navigation->ProjectPointToNavigation(FVector(Back, 0.0), To, FVector(ProjectionReach))));
					FVector Hit;
					ASSERT_THAT(IsTrue(UNavigationSystemV1::NavigationRaycast(State.World, From.Location, To.Location, Hit), TEXT("no straight way through the wall")));
					const TOptional<double> Round = PathLength(State.World, Front, Back);
					ASSERT_THAT(IsTrue(Round.IsSet() && Round.GetValue() > FVector2D::Distance(Front, Back), *FString::Printf(TEXT("round the wall: %g"), Round.Get(0.0))));
				});
		}
	};
}

#endif // ENABLE_PIE_NETWORK_TEST
