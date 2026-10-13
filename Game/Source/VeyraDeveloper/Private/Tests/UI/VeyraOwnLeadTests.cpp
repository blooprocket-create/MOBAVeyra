// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Greybox/VeyraOwnLead.h"

namespace VeyraOwnLeadTests
{
	// Veyra.UI.OwnLead.*: the player's own body walks off the moment its order is given and is drawn ahead along its path by
	// what the order will have done by now, while the server walks the true body behind it (ADR-077 §2).
	TEST_CLASS(OwnLead, "Veyra.UI")
	{
		// Fixture values: a frame, a body's speed, how long the server takes to start and how far the drawing trails it, and
		// the lead's shape as the shipped settings give it.
		static constexpr double Frame = 1.0 / 60.0;
		static constexpr double Speed = 350.0;
		static constexpr double Answer = 0.05;
		static constexpr double Trail = 1.0 / 30.0;
		static constexpr double Close = 6.0;

		static FVeyraOwnLeadShape Shape()
		{
			FVeyraOwnLeadShape Shape;
			Shape.CatchUpScale = 1.25;
			Shape.StallSeconds = 0.15;
			Shape.OffPathDistance = 80.0;
			Shape.BlendSeconds = 0.12;
			Shape.SwitchSeconds = 0.1;
			Shape.MaxDistance = 100.0;
			Shape.ArrivalTolerance = 10.0;
			Shape.StandingSpeedShare = 0.2;
			return Shape;
		}

		/**
		 * A server that walks its body along Path at Speed once Answer has passed since the order, and stands from StandsAt on;
		 * the player's machine draws it Trail later. It stops where its path following accepts it as arrived, short of the
		 * path's end by the arrival tolerance, or where an attack's reach stops it. Its path reaches the machine at PathAt, unless
		 * never; Push puts its body off the path.
		 */
		struct FServer
		{
			TArray<FVector2D> Path;
			TOptional<double> StandsAt;
			TOptional<double> PathAt;
			TOptional<double> StopsAt;
			FVector2D Push = FVector2D::ZeroVector;

			double Rest() const
			{
				return StopsAt.Get(FMath::Max(0.0, VeyraOwnLead::LengthOf(Path) - Shape().ArrivalTolerance));
			}

			double ArcAt(double Since) const
			{
				const double Walked = FMath::Max(0.0, (StandsAt ? FMath::Min(Since, *StandsAt) : Since) - Answer);
				return FMath::Min(Walked * Speed, Rest());
			}

			bool WalksAt(double Since) const
			{
				return Since > Answer && (!StandsAt || Since < *StandsAt) && ArcAt(Since) < Rest();
			}
		};

		/** Where the body is drawn, frame by frame, for an order to Goal given at time 0; Each sees every frame. */
		static TArray<FVector2D> Run(const FServer& Server, const FVector2D& Goal, double Seconds, TOptional<double> StopWithin = {},
			bool bMayLead = true, TFunction<void(double, const FVector2D&, const FVector2D&)> Each = nullptr)
		{
			FVeyraOwnLeadState State;
			TArray<FVector2D> Shown;
			for (double Now = Frame; Now <= Seconds; Now += Frame)
			{
				const double Seen = Now - Trail;
				FVeyraOwnLeadInputs In;
				In.Now = Now;
				In.GivenAt = 0.0;
				In.DeltaSeconds = Frame;
				In.Goal = Goal;
				In.StopWithin = StopWithin;
				if (Server.PathAt && Now >= *Server.PathAt)
				{
					In.ServerPath = Server.Path;
					In.ServerPathSerial = 1;
				}
				In.Drawn = VeyraOwnLead::PointAt(Server.Path, Server.ArcAt(Seen)) + Server.Push;
				In.ServerSpeed = Server.WalksAt(Seen) ? Speed : 0.0;
				In.Speed = Speed;
				In.LeadSeconds = Answer + Trail;
				In.bMayLead = bMayLead;
				const FVeyraOwnLead Lead = VeyraOwnLead::Step(State, In, Shape());
				Shown.Add(In.Drawn + Lead.Offset);
				if (Each)
				{
					Each(Now, In.Drawn, In.Drawn + Lead.Offset);
				}
			}
			return Shown;
		}

		static FServer Straight(double Length)
		{
			FServer Server;
			Server.Path = { FVector2D::ZeroVector, FVector2D(Length, 0.0) };
			return Server;
		}

		TEST_METHOD(TheBodyWalksOffAtOnceAndKeepsUpWithItsOrder)
		{
			// Where the order would have taken it by now, had the server answered at once.
			bool bChecked = false;
			Run(Straight(2000.0), FVector2D(2000.0, 0.0), 1.0, {}, true, [&](double Now, const FVector2D& Drawn, const FVector2D& Shown) {
				const double Ideal = Speed * Now;
				ASSERT_THAT(IsTrue(FMath::Abs(Shown.X - Ideal) <= Close, *FString::Printf(TEXT("at %.3f s it shows %.1f, not %.1f"), Now, Shown.X, Ideal)));
				if (Now < Answer)
				{
					ASSERT_THAT(IsTrue(Drawn.X == 0.0 && Shown.X > 0.0, TEXT("it walks before the server answers")));
					bChecked = true;
				}
			});
			ASSERT_THAT(IsTrue(bChecked));
		}

		TEST_METHOD(ItNeverStepsBackNorPassesItsPathsEnd)
		{
			const double Length = 300.0;
			const TArray<FVector2D> Shown = Run(Straight(Length), FVector2D(Length, 0.0), 2.0);
			for (int32 Index = 1; Index < Shown.Num(); ++Index)
			{
				ASSERT_THAT(IsTrue(Shown[Index].X >= Shown[Index - 1].X - 1e-6, *FString::Printf(TEXT("stepped back at frame %d"), Index)));
				ASSERT_THAT(IsTrue(Shown[Index].X <= Length + 1e-6, TEXT("past its end")));
			}
			ASSERT_THAT(IsTrue(Shown.Last().Equals(FVector2D(Length - Shape().ArrivalTolerance, 0.0), 1e-3), TEXT("it rests where the server does")));
		}

		TEST_METHOD(AServerBodyThatStandsTakesTheLeadBack)
		{
			// Stunned mid-walk: the body is soon drawn where the server holds it.
			FServer Server = Straight(2000.0);
			Server.StandsAt = 0.4;
			Run(Server, FVector2D(2000.0, 0.0), 1.2, {}, true, [&](double Now, const FVector2D& Drawn, const FVector2D& Shown) {
				if (Now > *Server.StandsAt + Trail + Answer + Shape().StallSeconds + Shape().BlendSeconds + Frame * 2.0)
				{
					ASSERT_THAT(IsTrue(Shown.Equals(Drawn, 1e-3), *FString::Printf(TEXT("at %.3f s still %.1f ahead"), Now, Shown.X - Drawn.X)));
				}
			});
		}

		TEST_METHOD(ABodyPushedOffItsPathIsDrawnWhereItIs)
		{
			// The server's path is known from the start, and the server's body walks well beside it.
			FServer Server = Straight(2000.0);
			Server.PathAt = 0.0;
			Server.Push = FVector2D(0.0, Shape().OffPathDistance * 2.0);
			Run(Server, FVector2D(2000.0, 0.0), 0.6, {}, true, [&](double Now, const FVector2D& Drawn, const FVector2D& Shown) {
				if (Now > Shape().BlendSeconds + Frame * 2.0)
				{
					ASSERT_THAT(IsTrue(Shown.Equals(Drawn, 1e-3)));
				}
			});
		}

		TEST_METHOD(TheServersPathIsTakenUpWithoutAJump)
		{
			// The first guess runs straight at the goal; the server's path bends round something in the way.
			FServer Server;
			Server.Path = { FVector2D::ZeroVector, FVector2D(60.0, 0.0), FVector2D(60.0, 400.0), FVector2D(400.0, 400.0) };
			Server.PathAt = 0.08;
			const double MostAFrame = Speed * Shape().CatchUpScale * Frame + Close;
			FVector2D Last = FVector2D::ZeroVector;
			Run(Server, FVector2D(400.0, 400.0), 3.0, {}, true, [&](double Now, const FVector2D& Drawn, const FVector2D& Shown) {
				ASSERT_THAT(IsTrue(FVector2D::Distance(Shown, Last) <= MostAFrame, *FString::Printf(TEXT("jumped %.1f at %.3f s"), FVector2D::Distance(Shown, Last), Now)));
				Last = Shown;
				// Once its switch time has passed, the body is drawn on the server's path, not pulled off it by the first guess.
				if (Now >= *Server.PathAt + Shape().SwitchSeconds + Frame)
				{
					double Off = 0.0;
					VeyraOwnLead::ArcNearest(Server.Path, Shown, &Off);
					ASSERT_THAT(IsTrue(Off <= 1e-3, *FString::Printf(TEXT("%.2f off the server's path at %.3f s"), Off, Now)));
				}
			});
			ASSERT_THAT(IsTrue(Last.Equals(VeyraOwnLead::PointAt(Server.Path, Server.Rest()), 1e-3), TEXT("it rests where the server does")));
		}

		TEST_METHOD(AnAttackStopsWithinReachOfItsTarget)
		{
			// The server stops it to attack as its target comes within its reach.
			const double Reach = 150.0;
			FServer Server = Straight(1000.0 - Reach);
			Server.StopsAt = 1000.0 - Reach;
			const TArray<FVector2D> Shown = Run(Server, FVector2D(1000.0, 0.0), 4.0, Reach);
			for (const FVector2D& Point : Shown)
			{
				ASSERT_THAT(IsTrue(Point.X <= 1000.0 - Reach + 1e-3, *FString::Printf(TEXT("drawn at %.1f, inside its reach"), Point.X)));
			}
		}

		TEST_METHOD(ABodyThatMayNotLeadIsDrawnWhereItIs)
		{
			Run(Straight(2000.0), FVector2D(2000.0, 0.0), 0.5, {}, /*bMayLead*/ false, [this](double, const FVector2D& Drawn, const FVector2D& Shown) {
				ASSERT_THAT(IsTrue(Shown.Equals(Drawn, 1e-6)));
			});
		}

		TEST_METHOD(ANewOrderWalksOffFromWhereTheBodyIsShown)
		{
			// Mid-walk, a new order turns it about: the drawn body turns where it stands, it does not jump.
			FVeyraOwnLeadState State;
			FVeyraOwnLeadInputs In;
			In.DeltaSeconds = Frame;
			In.Speed = Speed;
			In.ServerSpeed = Speed;
			In.LeadSeconds = Answer + Trail;
			In.bMayLead = true;
			In.Goal = FVector2D(2000.0, 0.0);
			FVector2D Before = FVector2D::ZeroVector;
			for (In.Now = Frame; In.Now < 0.5; In.Now += Frame)
			{
				In.Drawn = FVector2D(FMath::Max(0.0, In.Now - Answer - Trail) * Speed, 0.0);
				Before = In.Drawn + VeyraOwnLead::Step(State, In, Shape()).Offset;
			}
			In.GivenAt = In.Now;
			In.Goal = FVector2D(-2000.0, 0.0);
			In.Now += Frame;
			const FVector2D After = In.Drawn + VeyraOwnLead::Step(State, In, Shape()).Offset;
			ASSERT_THAT(IsTrue(FVector2D::Distance(Before, After) <= Speed * Shape().CatchUpScale * Frame + 1e-3,
				*FString::Printf(TEXT("jumped %.1f"), FVector2D::Distance(Before, After))));
		}

		TEST_METHOD(AFinishedOrderStaysFinishedThroughAPush)
		{
			// The body arrives and stands; later a knockback leaves it back on its old path, standing. The order is done: the
			// lead stays gone rather than waking for a moment toward the old path's end.
			const double Length = 300.0;
			FServer Server = Straight(Length);
			FVeyraOwnLeadState State;
			FVeyraOwnLeadInputs In;
			In.GivenAt = 0.0;
			In.DeltaSeconds = Frame;
			In.Goal = FVector2D(Length, 0.0);
			In.Speed = Speed;
			In.LeadSeconds = Answer + Trail;
			In.bMayLead = true;
			for (In.Now = Frame; In.Now < 2.0; In.Now += Frame)
			{
				In.Drawn = VeyraOwnLead::PointAt(Server.Path, Server.ArcAt(In.Now - Trail));
				In.ServerSpeed = Server.WalksAt(In.Now - Trail) ? Speed : 0.0;
				VeyraOwnLead::Step(State, In, Shape());
			}
			In.Drawn = FVector2D(Length / 2.0, 0.0);
			In.ServerSpeed = 0.0;
			for (int32 Frames = 0; Frames < 30; ++Frames, In.Now += Frame)
			{
				const FVeyraOwnLead Lead = VeyraOwnLead::Step(State, In, Shape());
				ASSERT_THAT(IsTrue(Lead.Offset.IsNearlyZero(1e-6), *FString::Printf(TEXT("woke %.1f ahead"), Lead.Offset.Size())));
			}
		}

		TEST_METHOD(AChaseLeadsAgainOnceItsTargetMovesOff)
		{
			// The body stops within reach and stands to attack; its target moves off, and the server plans a new path after it
			// and walks on. The order is not done: the body is drawn ahead again.
			const double Reach = 150.0;
			const double Stop = 1000.0 - Reach;
			FVeyraOwnLeadState State;
			FVeyraOwnLeadInputs In;
			In.GivenAt = 0.0;
			In.DeltaSeconds = Frame;
			In.Goal = FVector2D(1000.0, 0.0);
			In.StopWithin = Reach;
			In.Speed = Speed;
			In.LeadSeconds = Answer + Trail;
			In.bMayLead = true;
			In.ServerPath = { FVector2D::ZeroVector, FVector2D(1000.0, 0.0) };
			In.ServerPathSerial = 1;
			for (In.Now = Frame; In.Now < 4.0; In.Now += Frame)
			{
				const double Arc = FMath::Min(FMath::Max(0.0, In.Now - Trail - Answer) * Speed, Stop);
				In.Drawn = FVector2D(Arc, 0.0);
				In.ServerSpeed = Arc < Stop ? Speed : 0.0;
				VeyraOwnLead::Step(State, In, Shape());
			}
			In.Goal = FVector2D(2000.0, 0.0);
			In.ServerPath = { FVector2D(Stop, 0.0), FVector2D(2000.0, 0.0) };
			In.ServerPathSerial = 2;
			const double WalksOn = In.Now;
			double Most = 0.0;
			for (; In.Now < WalksOn + 0.5; In.Now += Frame)
			{
				In.Drawn = FVector2D(Stop + FMath::Max(0.0, In.Now - WalksOn - Trail) * Speed, 0.0);
				In.ServerSpeed = Speed;
				Most = FMath::Max(Most, VeyraOwnLead::Step(State, In, Shape()).Offset.X);
			}
			ASSERT_THAT(IsTrue(Most > Close, *FString::Printf(TEXT("led at most %.1f"), Most)));
		}

		TEST_METHOD(AnOrderToNoPointIsNeverLed)
		{
			// The server refuses an order to a point that is no point at all; the drawn body never heads for it.
			FVeyraOwnLeadState State;
			FVeyraOwnLeadInputs In;
			In.Now = Frame;
			In.DeltaSeconds = Frame;
			In.Goal = FVector2D(NAN, 0.0);
			In.Speed = Speed;
			In.LeadSeconds = Answer + Trail;
			In.bMayLead = true;
			for (int32 Frames = 0; Frames < 10; ++Frames, In.Now += Frame)
			{
				const FVeyraOwnLead Lead = VeyraOwnLead::Step(State, In, Shape());
				ASSERT_THAT(IsTrue(Lead.Offset.IsZero() && !Lead.Yaw.IsSet(), *FString::Printf(TEXT("drawn %s ahead"), *Lead.Offset.ToString())));
			}
		}

		TEST_METHOD(PathsAreMeasuredAlongTheirSegments)
		{
			const TArray<FVector2D> Path = { FVector2D(0.0, 0.0), FVector2D(100.0, 0.0), FVector2D(100.0, 50.0) };
			ASSERT_THAT(IsNear(VeyraOwnLead::LengthOf(Path), 150.0, 1e-9));
			ASSERT_THAT(IsTrue(VeyraOwnLead::PointAt(Path, 120.0).Equals(FVector2D(100.0, 20.0), 1e-9)));
			ASSERT_THAT(IsTrue(VeyraOwnLead::PointAt(Path, 999.0).Equals(FVector2D(100.0, 50.0), 1e-9), TEXT("clamped to its end")));
			ASSERT_THAT(IsNear(VeyraOwnLead::YawAt(Path, 40.0), 0.0, 1e-9));
			ASSERT_THAT(IsNear(VeyraOwnLead::YawAt(Path, 120.0), 90.0, 1e-9));
			double Off = 0.0;
			ASSERT_THAT(IsNear(VeyraOwnLead::ArcNearest(Path, FVector2D(40.0, 30.0), &Off), 40.0, 1e-9));
			ASSERT_THAT(IsNear(Off, 30.0, 1e-9));
			ASSERT_THAT(IsNear(VeyraOwnLead::ArcWithin(Path, FVector2D(100.0, 50.0), 20.0), 130.0, 1e-9));
			ASSERT_THAT(IsNear(VeyraOwnLead::ArcWithin(Path, FVector2D(0.0, 0.0), 20.0), 0.0, 1e-9, TEXT("already within")));
			ASSERT_THAT(IsNear(VeyraOwnLead::ArcWithin(Path, FVector2D(500.0, 500.0), 20.0), 150.0, 1e-9, TEXT("never within: its length")));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
