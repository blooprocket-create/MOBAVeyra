// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraMinimapModel.h"

#include "Entities/VeyraPlacedMarker.h"

#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Hud/VeyraHudModel.h"
#include "Targeting/VeyraTargeting.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "Units/VeyraUnit.h"

namespace VeyraMinimap
{
FVeyraMinimapFrame FrameFor(const FVector2D& Viewport, double Size, double Margin, double HalfExtent)
{
	FVeyraMinimapFrame Frame;
	Frame.Size = Size;
	Frame.HalfExtent = HalfExtent;
	Frame.Origin = FVector2D(Viewport.X - Margin - Size, Viewport.Y - Margin - Size);
	return Frame;
}

FVector2D ToMap(const FVeyraMinimapFrame& Frame, const FVector& World)
{
	const double Span = 2.0 * Frame.HalfExtent;
	if (Span <= 0.0)
	{
		return Frame.Origin;
	}
	// +Y is right, +X is up; screen Y grows down.
	return Frame.Origin + FVector2D((World.Y + Frame.HalfExtent) / Span, (Frame.HalfExtent - World.X) / Span) * Frame.Size;
}

TOptional<FVector> ToWorld(const FVeyraMinimapFrame& Frame, const FVector2D& Screen)
{
	const FVector2D Local = (Screen - Frame.Origin) / Frame.Size;
	if (Frame.Size <= 0.0 || Local.X < 0.0 || Local.Y < 0.0 || Local.X > 1.0 || Local.Y > 1.0)
	{
		return {};
	}
	const double Span = 2.0 * Frame.HalfExtent;
	return FVector(Frame.HalfExtent - Local.Y * Span, Local.X * Span - Frame.HalfExtent, 0.0);
}

TArray<FVeyraMinimapTeamPing> DescribeTeamPings(const FVeyraMinimapFrame& Frame, TConstArrayView<FVeyraReceivedPing> Pings, double Now, double Seconds)
{
	TArray<FVeyraMinimapTeamPing> Drawn;
	for (const FVeyraReceivedPing& Held : Pings)
	{
		const double Age = Now - Held.ReceivedAt;
		if (Seconds <= 0.0 || Age >= Seconds)
		{
			continue;
		}
		Drawn.Add({ ToMap(Frame, Held.Ping.Point), Held.Ping.Kind, 1.0 - FMath::Max(0.0, Age) / Seconds });
	}
	return Drawn;
}

EVeyraMinimapSide SideOf(EVeyraTeam Viewer, EVeyraTeam Team, bool bOwn)
{
	if (bOwn)
	{
		return EVeyraMinimapSide::Own;
	}
	if (Team == EVeyraTeam::None)
	{
		return EVeyraMinimapSide::Neutral;
	}
	return Team == Viewer ? EVeyraMinimapSide::Ally : EVeyraMinimapSide::Enemy;
}

FVeyraMinimapView Describe(const UWorld& World, const FVeyraMinimapFrame& Frame, EVeyraTeam Viewer, const AActor* Own, const TOptional<FVector>& Focus,
	double ServerNow)
{
	FVeyraMinimapView View;
	View.Frame = Frame;
	for (const FVeyraLaneLayout& Lane : UVeyraWorldTuningSubsystem::Get().Layout.Lanes)
	{
		FVeyraMinimapLane& Drawn = View.Lanes.AddDefaulted_GetRef();
		for (const FVeyraMapPoint& Point : Lane.Points)
		{
			Drawn.Points.Add(ToMap(Frame, FVector(Point.X, Point.Y, 0.0)));
		}
	}
	// What this client has is what its side may see: the fog gate sends nothing else (ADR-016).
	for (TActorIterator<APawn> It(const_cast<UWorld*>(&World)); It; ++It)
	{
		const APawn* Unit = *It;
		const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(Unit);
		if (!Kind.IsSet() || !VeyraTargeting::IsAlive(Unit))
		{
			continue;
		}
		FVeyraMinimapDot Dot;
		Dot.Position = ToMap(Frame, Unit->GetActorLocation());
		Dot.Side = SideOf(Viewer, VeyraTeams::TeamOf(Unit), Unit == Own);
		switch (Kind.GetValue())
		{
		case EVeyraUnitKind::Vanguard:
			Dot.Kind = EVeyraMinimapDot::Vanguard;
			break;
		case EVeyraUnitKind::Fluxborn:
		// A companion is a unit's dot, as a Fluxborn's (ADR-034 §4).
		case EVeyraUnitKind::Companion:
			Dot.Kind = EVeyraMinimapDot::Fluxborn;
			break;
		case EVeyraUnitKind::Structure:
			Dot.Kind = EVeyraMinimapDot::Structure;
			break;
		case EVeyraUnitKind::Wildlife:
		case EVeyraUnitKind::Objective:
			Dot.Kind = EVeyraMinimapDot::Wildlife;
			break;
		case EVeyraUnitKind::Ward:
			Dot.Kind = EVeyraMinimapDot::Ward;
			break;
		case EVeyraUnitKind::Marker:
		{
			const AVeyraPlacedMarker* Marker = Cast<AVeyraPlacedMarker>(Unit);
			// A decoy is a Vanguard to its owner's enemies, and the illusion it is to its owner's side (ADR-030 §5).
			const bool bDeceives = Marker && Marker->GetPresentedAs() && Viewer != EVeyraTeam::None && Viewer != Marker->GetVeyraTeam();
			Dot.Kind = bDeceives ? EVeyraMinimapDot::Vanguard : EVeyraMinimapDot::Ward;
			break;
		}
		}
		View.Dots.Add(Dot);
	}
	// Vanguards on top of everything else.
	View.Dots.StableSort([](const FVeyraMinimapDot& A, const FVeyraMinimapDot& B) {
		return (A.Kind == EVeyraMinimapDot::Vanguard) < (B.Kind == EVeyraMinimapDot::Vanguard);
	});
	const double Scale = Frame.HalfExtent > 0.0 ? Frame.Size / (2.0 * Frame.HalfExtent) : 0.0;
	for (const FVeyraHudPing& Ping : VeyraHud::DescribeVision(&World, Viewer, ServerNow).Pings)
	{
		View.Pings.Add({ ToMap(Frame, FVector(Ping.Centre, 0.0)), Ping.Radius * Scale, Ping.Fade });
	}
	if (Focus.IsSet())
	{
		View.Focus = ToMap(Frame, Focus.GetValue());
	}
	return View;
}
}
