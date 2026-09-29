// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraGreyboxHud.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Hud/VeyraHudDeck.h"
#include "Hud/VeyraHudModel.h"
#include "Ending/VeyraMatchEnding.h"
#include "Hud/VeyraMinimapModel.h"
#include "Structures/VeyraStructure.h"
#include "Input/VeyraInputSettings.h"
#include "Shell/VeyraUIInputSettings.h"
#include "Text/VeyraContentText.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Tuning/VeyraWorldTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "Camera/VeyraCameraRig.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"

namespace
{
	template <typename EnumType>
	FString HudEnumName(EnumType Value)
	{
		return StaticEnum<EnumType>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	UFont* HudFont()
	{
		return GEngine->GetSmallFont();
	}

	float HudLineHeight()
	{
		return HudFont()->GetMaxCharHeight();
	}

	void DrawHudRect(UCanvas& Canvas, const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color)
	{
		FCanvasTileItem Tile(TopLeft, GWhiteTexture, Size, Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas.DrawItem(Tile);
	}

	void DrawHudText(UCanvas& Canvas, const FVector2D& TopLeft, const FString& Text, const FLinearColor& Color)
	{
		FCanvasTextItem Item(TopLeft, FText::FromString(Text), HudFont(), Color);
		Item.EnableShadow(FLinearColor::Black);
		Canvas.DrawItem(Item);
	}

	/** Health with shields after it, the resource under them, and the unit's statuses above. */
	void DrawOverheadBars(UCanvas& Canvas, const UVeyraGreyboxSubsystem& Greybox, const UVeyraGreyboxSettings& Settings, const APawn& Unit, double Now)
	{
		const TOptional<FVeyraHudVitals> Vitals = VeyraHud::VitalsOf(Unit);
		if (!Vitals || Vitals->MaxHealth <= 0.0 || Unit.IsHidden())
		{
			return;
		}
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Unit.GetSimpleCollisionCylinder(Radius, HalfHeight);
		const FVector OnScreen = Canvas.Project(Unit.GetActorLocation() + FVector::UpVector * (HalfHeight + Settings.BarLift));
		if (OnScreen.Z <= 0.0)
		{
			return; // Behind the camera.
		}

		const FVector2D TopLeft(OnScreen.X - Settings.BarWidth / 2.0f, OnScreen.Y - Settings.BarHeight);
		// Health and shields share the bar; when together they pass Max Health, the bar holds their total.
		const double Total = FMath::Max(Vitals->MaxHealth, Vitals->Health + Vitals->Shield);
		const float HealthWidth = Settings.BarWidth * Vitals->Health / Total;
		DrawHudRect(Canvas, TopLeft, FVector2D(Settings.BarWidth, Settings.BarHeight), Settings.BarBackgroundColor);
		DrawHudRect(Canvas, TopLeft, FVector2D(HealthWidth, Settings.BarHeight), Greybox.SideColorOf(Unit));
		DrawHudRect(Canvas, TopLeft + FVector2D(HealthWidth, 0.0f), FVector2D(Settings.BarWidth * Vitals->Shield / Total, Settings.BarHeight), Settings.ShieldColor);
		if (Vitals->MaxResource > 0.0)
		{
			const FVector2D ResourceTopLeft = TopLeft + FVector2D(0.0f, Settings.BarHeight);
			DrawHudRect(Canvas, ResourceTopLeft, FVector2D(Settings.BarWidth, Settings.ResourceBarHeight), Settings.BarBackgroundColor);
			DrawHudRect(Canvas, ResourceTopLeft, FVector2D(Settings.BarWidth * Vitals->Resource / Vitals->MaxResource, Settings.ResourceBarHeight),
				Settings.ResourceColor);
		}

		float Y = TopLeft.Y;
		// A structure says what it is, and whether the structures before it still protect it.
		if (const TOptional<FVeyraHudStructure> Structure = VeyraHud::StructureOf(Unit, Now))
		{
			FString Label = HudEnumName(Structure->Kind);
			if (Structure->bInvulnerable)
			{
				Label += TEXT("  invulnerable");
			}
			if (Structure->RebuildSeconds > 0.0)
			{
				Label += FString::Printf(TEXT("  rebuilds in %d s"), FMath::CeilToInt32(Structure->RebuildSeconds));
			}
			Y -= HudLineHeight();
			DrawHudText(Canvas, FVector2D(TopLeft.X, Y), Label, Settings.TextColor);
		}
		// A Flux Well says where it stands in its cycle; a creature, what it is (ADR-014).
		if (const TOptional<FVeyraHudFluxWell> Well = VeyraHud::FluxWellOf(Unit, Now))
		{
			FString Label = TEXT("Flux Well");
			switch (Well->State)
			{
			case EVeyraFluxWellState::Closed:
				Label += FString::Printf(TEXT("  opens in %d s"), FMath::CeilToInt32(Well->OpensInSeconds));
				break;
			case EVeyraFluxWellState::Open:
				Label += TEXT("  open");
				break;
			case EVeyraFluxWellState::Respawning:
				Label += FString::Printf(TEXT("  returns in %d s"), FMath::CeilToInt32(Well->OpensInSeconds));
				break;
			}
			Y -= HudLineHeight();
			DrawHudText(Canvas, FVector2D(TopLeft.X, Y), Label, Settings.TextColor);
		}
		if (const TOptional<FVeyraContentId> Species = VeyraHud::SpeciesOf(Unit))
		{
			Y -= HudLineHeight();
			DrawHudText(Canvas, FVector2D(TopLeft.X, Y), Species->ToString(), Settings.TextColor);
		}
		for (const FVeyraHudStatus& Status : VeyraHud::StatusesOf(Unit, Now))
		{
			Y -= HudLineHeight();
			const FString Count = Status.Stacks > 1 ? FString::Printf(TEXT(" x%d"), Status.Stacks) : FString();
			DrawHudText(Canvas, FVector2D(TopLeft.X, Y), FString::Printf(TEXT("%s%s %.1f s"), *HudEnumName(Status.Kind), *Count, Status.RemainingSeconds), Settings.TextColor);
		}
	}

	/** The minimap's frame on a Viewport-sized screen. */
	FVeyraMinimapFrame MinimapFrame(const UVeyraGreyboxSettings& Settings, const FVector2D& Viewport)
	{
		return VeyraMinimap::FrameFor(Viewport, Settings.MinimapSize, Settings.HudMargin, UVeyraWorldTuningSubsystem::Get().Layout.HalfExtent);
	}

	FLinearColor MinimapColor(const UVeyraGreyboxSettings& Settings, EVeyraMinimapSide Side)
	{
		switch (Side)
		{
		case EVeyraMinimapSide::Own:
			return Settings.OwnColor;
		case EVeyraMinimapSide::Ally:
			return Settings.AllyColor;
		case EVeyraMinimapSide::Enemy:
			return Settings.EnemyColor;
		case EVeyraMinimapSide::Neutral:
			break;
		}
		return Settings.NeutralColor;
	}

	/** A square outline of Side pixels around Centre. */
	void DrawHudOutline(UCanvas& Canvas, const FVector2D& Centre, double Side, const FLinearColor& Color)
	{
		FCanvasBoxItem Box(Centre - FVector2D(Side / 2.0), FVector2D(Side));
		Box.SetColor(Color);
		Canvas.DrawItem(Box);
	}

	/**
	 * The minimap, bottom-right (Settings Bible §3.2; ADR-020 §2): the lanes, every unit this client has
	 * (so only what its side sees), its side's presence pings, and where the camera looks.
	 */
	void DrawMinimap(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const FVeyraMinimapView& View)
	{
		const FVeyraMinimapFrame& Frame = View.Frame;
		DrawHudRect(Canvas, Frame.Origin, FVector2D(Frame.Size), Settings.MinimapBackgroundColor);
		FCanvasBoxItem Edge(Frame.Origin, FVector2D(Frame.Size));
		Edge.SetColor(Settings.HudHairlineColor);
		Edge.BlendMode = SE_BLEND_Translucent;
		Canvas.DrawItem(Edge);
		for (const FVeyraMinimapLane& Lane : View.Lanes)
		{
			for (int32 Index = 1; Index < Lane.Points.Num(); ++Index)
			{
				FCanvasLineItem Line(Lane.Points[Index - 1], Lane.Points[Index]);
				Line.SetColor(Settings.LaneColor);
				Canvas.DrawItem(Line);
			}
		}
		for (const FVeyraMinimapPing& Ping : View.Pings)
		{
			DrawHudOutline(Canvas, Ping.Centre, 2.0 * Ping.Radius, Settings.PresencePingColor.CopyWithNewOpacity(Ping.Fade));
		}
		for (const FVeyraMinimapDot& Dot : View.Dots)
		{
			const double Side = Dot.Kind == EVeyraMinimapDot::Vanguard ? Settings.MinimapVanguardIcon
				: Dot.Kind == EVeyraMinimapDot::Structure					  ? Settings.MinimapStructureIcon
																			  : Settings.MinimapUnitIcon;
			DrawHudRect(Canvas, Dot.Position - FVector2D(Side / 2.0), FVector2D(Side), MinimapColor(Settings, Dot.Side));
		}
		for (const FVeyraMinimapTeamPing& Ping : View.TeamPings)
		{
			const FLinearColor Color = Ping.Kind == EVeyraPingKind::Danger ? Settings.DangerPingColor : Settings.LookPingColor;
			DrawHudOutline(Canvas, Ping.Centre, Settings.MinimapVanguardIcon * 2.0, Color.CopyWithNewOpacity(Ping.Fade));
		}
		if (View.Focus.IsSet())
		{
			DrawHudOutline(Canvas, View.Focus.GetValue(), Settings.MinimapVanguardIcon * 2.0, Settings.TextColor);
		}
	}

	/** Its side's pings where they point on the ground, fading as they age; labelled when the player asks (SET-68). */
	void DrawWorldPings(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, TConstArrayView<FVeyraReceivedPing> Pings, double Now)
	{
		for (const FVeyraReceivedPing& Held : Pings)
		{
			const double Age = Now - Held.ReceivedAt;
			if (Age >= Settings.PingSeconds)
			{
				continue;
			}
			const FVector OnScreen = Canvas.Project(Held.Ping.Point);
			if (OnScreen.Z <= 0.0)
			{
				continue;
			}
			const FLinearColor Color = (Held.Ping.Kind == EVeyraPingKind::Danger ? Settings.DangerPingColor : Settings.LookPingColor)
				.CopyWithNewOpacity(1.0 - FMath::Max(0.0, Age) / Settings.PingSeconds);
			const FVector2D Centre(OnScreen.X, OnScreen.Y);
			DrawHudOutline(Canvas, Centre, Settings.WorldPingSize, Color);
			DrawHudOutline(Canvas, Centre, Settings.WorldPingSize / 2.0, Color);
			if (Settings.bPingTextLabels)
			{
				DrawHudText(Canvas, Centre + FVector2D(Settings.WorldPingSize, -Settings.WorldPingSize / 2.0), HudEnumName(Held.Ping.Kind), Color);
			}
		}
	}

	/**
	 * Gives the local controller the minimap's hit test (ADR-020 §2): the controller, below the UI, asks
	 * it where a click on the minimap points, and the player's toggles decide whether each click counts.
	 */
	void BindMinimapClicks(AVeyraPlayerController& Controller)
	{
		if (Controller.HasMinimapHitTest())
		{
			return;
		}
		Controller.SetMinimapHitTest([WeakController = TWeakObjectPtr<AVeyraPlayerController>(&Controller)](
			const FVector2D& Screen, AVeyraPlayerController::EMinimapClick Purpose) -> TOptional<FVector> {
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			const bool bOn = Purpose == AVeyraPlayerController::EMinimapClick::Camera ? Settings.bMinimapClickMovesCamera
				: Purpose == AVeyraPlayerController::EMinimapClick::Move			  ? Settings.bMinimapRightClickMoves
																					  : true;
			const AVeyraPlayerController* Owner = WeakController.Get();
			const UGameViewportClient* Viewport = Owner && Owner->GetLocalPlayer() ? Owner->GetLocalPlayer()->ViewportClient : nullptr;
			if (!bOn || !Viewport)
			{
				return {};
			}
			FVector2D Size;
			Viewport->GetViewportSize(Size);
			return VeyraMinimap::ToWorld(MinimapFrame(Settings, Size), Screen);
		});
	}

}

void VeyraGreyboxHud::Draw(UCanvas& Canvas, const UVeyraGreyboxSubsystem& Greybox, const APlayerController* Viewer)
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const double Now = Greybox.GetServerNow();
	for (TActorIterator<APawn> It(Greybox.GetWorld()); It; ++It)
	{
		if (VeyraUnits::KindOf(*It).IsSet())
		{
			DrawOverheadBars(Canvas, Greybox, Settings, **It, Now);
		}
	}
	if (const AVeyraGameState* GameState = Greybox.GetWorld()->GetGameState<AVeyraGameState>())
	{
		const AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(Viewer);
		const AVeyraPlayerState* Own = Viewer ? Viewer->GetPlayerState<AVeyraPlayerState>() : nullptr;
		if (Player && Own)
		{
			BindMinimapClicks(*const_cast<AVeyraPlayerController*>(Player));
			const AVeyraCameraRig* Rig = Player->GetCameraRig();
			const TOptional<FVector> Focus = Rig ? TOptional<FVector>(Rig->GetFocus()) : TOptional<FVector>();
			const FVeyraMinimapFrame Frame = MinimapFrame(Settings, FVector2D(Canvas.ClipX, Canvas.ClipY));
			FVeyraMinimapView View = VeyraMinimap::Describe(*Greybox.GetWorld(), Frame, Own->GetVeyraTeam(), Own->GetPawn(), Focus, Now);
			const double RealNow = FPlatformTime::Seconds();
			View.TeamPings = VeyraMinimap::DescribeTeamPings(Frame, Player->GetPings(), RealNow, Settings.PingSeconds);
			DrawWorldPings(Canvas, Settings, Player->GetPings(), RealNow);
			DrawMinimap(Canvas, Settings, View);
		}
		// The deck, the top strip and Team Flux (VeyraHudDeck).
		VeyraHudDeck::Draw(Canvas, Settings, Greybox.GetHudFont(), *Greybox.GetWorld(), *GameState, Viewer, Own, Now);
		// How the match ended, while its players watch the end (ADR-020 §1).
		if (GameState->GetPhase() == EVeyraMatchPhase::Ended)
		{
			const AVeyraStructure* Fallen = VeyraMatchEnding::FindFallenPrimeWell(*Greybox.GetWorld());
			const EVeyraEndingHeadline Headline = VeyraMatchEnding::Headline(Fallen ? TOptional<EVeyraTeam>(Fallen->GetVeyraTeam()) : TOptional<EVeyraTeam>(),
				Own ? Own->GetVeyraTeam() : EVeyraTeam::None);
			VeyraHudDeck::DrawHeadline(Canvas, Settings, Greybox.GetHudFont(), HudEnumName(Headline), Headline == EVeyraEndingHeadline::Defeat ? Settings.WarningColor : Settings.GoldColor);
		}
	}
}
