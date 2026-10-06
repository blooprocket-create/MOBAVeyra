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
#include "Greybox/VeyraFountainShop.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Hud/VeyraCombatTextModel.h"
#include "Hud/VeyraHudDeck.h"
#include "Hud/VeyraHudLayout.h"
#include "Hud/VeyraHudModel.h"
#include "Ending/VeyraMatchEnding.h"
#include "Hud/VeyraMinimapModel.h"
#include "Structures/VeyraStructure.h"
#include "Input/VeyraInputSettings.h"
#include "Teams/VeyraTeam.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraUIInputSettings.h"
#include "Slots/VeyraAbilitySlot.h"
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

	void DrawHudText(UCanvas& Canvas, float Scale, const FVector2D& TopLeft, const FString& Text, const FLinearColor& Color)
	{
		FCanvasTextItem Item(TopLeft, FText::FromString(Text), HudFont(), Color);
		Item.Scale = FVector2D(Scale);
		Item.EnableShadow(FLinearColor::Black);
		Canvas.DrawItem(Item);
	}

	/**
	 * Health with shields after it, the resource under them, and the unit's statuses above; a Fluxborn's or a
	 * jungle creature's only when the player's bar settings show it (ADR-052 §2). Targeted are the units the
	 * player targets now.
	 */
	void DrawOverheadBars(UCanvas& Canvas, const UVeyraGreyboxSubsystem& Greybox, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences,
		TConstArrayView<const AActor*> Targeted, const APawn& Unit, double Now)
	{
		const TOptional<FVeyraHudVitals> Vitals = VeyraHud::VitalsOf(Unit, Greybox.GetViewerTeam());
		if (!Vitals || Vitals->MaxHealth <= 0.0 || Unit.IsHidden())
		{
			return;
		}
		FVeyraBarFacts Facts;
		Facts.Kind = VeyraUnits::KindOf(&Unit).Get(EVeyraUnitKind::Vanguard);
		Facts.bAllied = VeyraTeams::TeamOf(&Unit) == Greybox.GetViewerTeam();
		Facts.bDamaged = Vitals->Health < Vitals->MaxHealth;
		Facts.bTargeted = Targeted.Contains(&Unit);
		Facts.bFighting = VeyraHud::IsFighting(Unit);
		if (!VeyraInterfacePreferences::ShowsBar(Preferences, Facts))
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

		// At the player's Overhead Bar Size (ADR-059 §1).
		const float Scale = Preferences.HudScales.OverheadBars;
		const float BarWidth = Settings.BarWidth * Scale;
		const float BarHeight = Settings.BarHeight * Scale;
		const float ResourceBarHeight = Settings.ResourceBarHeight * Scale;
		const float LineHeight = HudLineHeight() * Scale;
		const FVector2D TopLeft(OnScreen.X - BarWidth / 2.0f, OnScreen.Y - BarHeight);
		// Health and shields share the bar; when together they pass Max Health, the bar holds their total.
		const double Total = FMath::Max(Vitals->MaxHealth, Vitals->Health + Vitals->Shield);
		const float HealthWidth = BarWidth * Vitals->Health / Total;
		DrawHudRect(Canvas, TopLeft, FVector2D(BarWidth, BarHeight), Settings.BarBackgroundColor);
		DrawHudRect(Canvas, TopLeft, FVector2D(HealthWidth, BarHeight), Greybox.SideColorOf(Unit));
		DrawHudRect(Canvas, TopLeft + FVector2D(HealthWidth, 0.0f), FVector2D(BarWidth * Vitals->Shield / Total, BarHeight), Settings.ShieldColor);
		// Ticks every HealthPerTick along a Vanguard's bar, a longer one every tenth, so its Health reads at
		// a glance; never so close that they blur.
		constexpr double HealthPerTick = 100.0;
		constexpr int32 TicksPerLongTick = 10;
		constexpr float LeastTickGap = 3.0f;
		const double TickGap = BarWidth * HealthPerTick / Total;
		const TOptional<EVeyraUnitKind> Kind = VeyraUnits::KindOf(&Unit);
		const bool bVanguardBar = (Kind.IsSet() && Kind.GetValue() == EVeyraUnitKind::Vanguard) || &VeyraHud::PresentedUnitOf(Unit, Greybox.GetViewerTeam()) != &Unit;
		if (bVanguardBar && TickGap >= LeastTickGap)
		{
			for (int32 Tick = 1; Tick * HealthPerTick < Total; ++Tick)
			{
				const bool bLong = Tick % TicksPerLongTick == 0;
				const float Height = bLong ? BarHeight : BarHeight * 0.55f;
				DrawHudRect(Canvas, FVector2D(TopLeft.X + TickGap * Tick, TopLeft.Y), FVector2D(1.0f, Height), FLinearColor(0.0f, 0.0f, 0.0f, bLong ? 0.8f : 0.5f));
			}
		}
		{
			FCanvasBoxItem Edge(TopLeft, FVector2D(BarWidth, BarHeight));
			Edge.SetColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
			Edge.BlendMode = SE_BLEND_Translucent;
			Canvas.DrawItem(Edge);
		}
		if (Vitals->MaxResource > 0.0)
		{
			const FVector2D ResourceTopLeft = TopLeft + FVector2D(0.0f, BarHeight);
			DrawHudRect(Canvas, ResourceTopLeft, FVector2D(BarWidth, ResourceBarHeight), Settings.BarBackgroundColor);
			DrawHudRect(Canvas, ResourceTopLeft, FVector2D(BarWidth * Vitals->Resource / Vitals->MaxResource, ResourceBarHeight),
				Settings.ResourceColorOf(Vitals->Family));
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
			Y -= LineHeight;
			DrawHudText(Canvas, Scale, FVector2D(TopLeft.X, Y), Label, Settings.TextColor);
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
			Y -= LineHeight;
			DrawHudText(Canvas, Scale, FVector2D(TopLeft.X, Y), Label, Settings.TextColor);
		}
		// The mastery emote: the player's Mastery Level in its tier's colour, nearest the bar (ADR-045 §9).
		if (const TOptional<FVeyraHudMasteryEmote> Emote = VeyraHud::MasteryEmoteOf(Unit, Now); Emote && !Settings.MasteryEmoteTierColors.IsEmpty())
		{
			const int32 Index = FMath::Clamp(Emote->Tier - 1, 0, Settings.MasteryEmoteTierColors.Num() - 1);
			Y -= LineHeight;
			DrawHudText(Canvas, Scale, FVector2D(TopLeft.X, Y), FString::Printf(TEXT("Mastery %d"), Emote->Level), Settings.MasteryEmoteTierColors[Index]);
		}
		if (const TOptional<FVeyraContentId> Species = VeyraHud::SpeciesOf(Unit))
		{
			Y -= LineHeight;
			DrawHudText(Canvas, Scale, FVector2D(TopLeft.X, Y), Species->ToString(), Settings.TextColor);
		}
		for (const FVeyraHudStatus& Status : VeyraHud::StatusesOf(Unit, Now, Greybox.GetViewerTeam()))
		{
			Y -= LineHeight;
			const FString Count = Status.Stacks > 1 ? FString::Printf(TEXT(" x%d"), Status.Stacks) : FString();
			// A mark has no effect of its own, so its name says what it is, as Doom's or a Hex's.
			const FString Name = Status.Kind == EVeyraStatusKind::Counter ? Status.Id.ToString() : HudEnumName(Status.Kind);
			DrawHudText(Canvas, Scale, FVector2D(TopLeft.X, Y), FString::Printf(TEXT("%s%s %.1f s"), *Name, *Count, Status.RemainingSeconds), Settings.TextColor);
		}
	}

	/** A number's colour: damage coded by type, or Uniform's two (Proposal 52); healing and shielding their own. */
	FLinearColor CombatTextColor(const UVeyraGreyboxSettings& Settings, bool bUniform, const FVeyraCombatTextShown& Number)
	{
		switch (Number.Kind)
		{
		case EVeyraCombatTextKind::Healing:
			return Settings.CombatTextHealingColor;
		case EVeyraCombatTextKind::Shielding:
			return Settings.CombatTextShieldingColor;
		case EVeyraCombatTextKind::DamageDealt:
		case EVeyraCombatTextKind::DamageReceived:
			break;
		}
		if (bUniform)
		{
			return Number.Kind == EVeyraCombatTextKind::DamageReceived ? Settings.CombatTextReceivedColor : Settings.CombatTextUniformColor;
		}
		switch (Number.DamageType)
		{
		case EVeyraDamageType::Magic:
			return Settings.CombatTextMagicColor;
		case EVeyraDamageType::TrueDamage:
			return Settings.CombatTextTrueColor;
		case EVeyraDamageType::Physical:
			break;
		}
		return Settings.CombatTextPhysicalColor;
	}

	/** The player's combat text, each number rising from its unit's bars and fading as it goes (ADR-052 §1). */
	void DrawCombatText(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, TConstArrayView<FVeyraCombatTextShown> Numbers)
	{
		for (const FVeyraCombatTextShown& Number : Numbers)
		{
			const AActor* Unit = Number.Unit.Get();
			if (!Unit || Unit->IsHidden())
			{
				continue;
			}
			float Radius = 0.0f;
			float HalfHeight = 0.0f;
			Unit->GetSimpleCollisionCylinder(Radius, HalfHeight);
			const double Lift = HalfHeight + Settings.BarLift + Settings.CombatTextRise * Number.Progress;
			const FVector OnScreen = Canvas.Project(Unit->GetActorLocation() + FVector::UpVector * Lift);
			if (OnScreen.Z <= 0.0)
			{
				continue;
			}
			const bool bGiven = Number.Kind == EVeyraCombatTextKind::Healing || Number.Kind == EVeyraCombatTextKind::Shielding;
			const FString Text = FString::Printf(TEXT("%s%d"), bGiven ? TEXT("+") : TEXT(""), FMath::RoundToInt(Number.Amount));
			FCanvasTextItem Item(FVector2D(OnScreen.X, OnScreen.Y), FText::FromString(Text), HudFont(),
				CombatTextColor(Settings, Preferences.bUniformDamageColors, Number).CopyWithNewOpacity(static_cast<float>(1.0 - Number.Progress)));
			Item.bCentreX = true;
			Item.Scale = FVector2D(Settings.CombatTextScale * Preferences.HudScales.CombatText * (Number.bCritical ? Settings.CombatTextCritScale : 1.0f));
			Item.EnableShadow(FLinearColor::Black);
			Canvas.DrawItem(Item);
		}
	}

	/** The HUD on a Viewport-sized screen as the player set it: the deck, the minimap and the chat inside the safe area (ADR-059 §1-§2). */
	FVeyraHudArrangement Arrangement(const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, const FVector2D& Viewport)
	{
		return VeyraHudLayout::Arrange(Viewport, Settings, Preferences, UE_ARRAY_COUNT(VeyraAbilitySlots::All), UVeyraWorldTuningSubsystem::Get().Layout.HalfExtent);
	}

	/** A minimap side's colour, as the player's colour vision gives it (ADR-055 §1). */
	FLinearColor MinimapColor(const FVeyraInterfacePreferences& Preferences, EVeyraMinimapSide Side)
	{
		switch (Side)
		{
		case EVeyraMinimapSide::Own:
			return Preferences.SideColors.Own;
		case EVeyraMinimapSide::Ally:
			return Preferences.SideColors.Ally;
		case EVeyraMinimapSide::Enemy:
			return Preferences.SideColors.Enemy;
		case EVeyraMinimapSide::Neutral:
			break;
		}
		return Preferences.SideColors.Neutral;
	}

	/** A square outline of Side pixels around Centre. */
	void DrawHudOutline(UCanvas& Canvas, const FVector2D& Centre, double Side, const FLinearColor& Color)
	{
		FCanvasBoxItem Box(Centre - FVector2D(Side / 2.0), FVector2D(Side));
		Box.SetColor(Color);
		Canvas.DrawItem(Box);
	}

	/**
	 * The minimap, bottom-right (Settings Bible §3.2; ADR-020 §2): the river, the walls (ADR-043 §4), the
	 * lanes, every unit this client has (so only what its side sees), its side's presence pings, and where
	 * the camera looks.
	 */
	void DrawMinimap(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, const FVeyraMinimapView& View)
	{
		const FVeyraMinimapFrame& Frame = View.Frame;
		DrawHudRect(Canvas, Frame.Origin, FVector2D(Frame.Size), Settings.MinimapBackgroundColor);
		FCanvasBoxItem Edge(Frame.Origin, FVector2D(Frame.Size));
		Edge.SetColor(Settings.HudHairlineColor);
		Edge.BlendMode = SE_BLEND_Translucent;
		Canvas.DrawItem(Edge);
		// The authored river is a triangle list, including both sampled branches.
		for (int32 Index = 2; Index < View.River.Num(); Index += 3)
		{
			FCanvasTriangleItem Piece(View.River[Index - 2], View.River[Index - 1], View.River[Index], GWhiteTexture);
			Piece.SetColor(Settings.RiverColor);
			Canvas.DrawItem(Piece);
		}
		for (const FVeyraMinimapWall& Wall : View.Walls)
		{
			// A box is two triangles.
			if (Wall.Corners.Num() == 4)
			{
				for (const int32 Third : { 2, 3 })
				{
					FCanvasTriangleItem Half(Wall.Corners[0], Wall.Corners[Third - 1], Wall.Corners[Third], GWhiteTexture);
					Half.SetColor(Settings.MinimapWallColor);
					Canvas.DrawItem(Half);
				}
			}
		}
		for (const FVeyraMinimapLane& Lane : View.Lanes)
		{
			for (int32 Index = 1; Index < Lane.Points.Num(); ++Index)
			{
				FCanvasLineItem Line(Lane.Points[Index - 1], Lane.Points[Index]);
				Line.SetColor(Settings.LaneColor);
				Canvas.DrawItem(Line);
			}
		}
		// The fog over the ground, under every mark and unit on it (ADR-054 §3).
		for (const FBox2D& Fog : View.Fog)
		{
			DrawHudRect(Canvas, Fog.Min, Fog.GetSize(), Settings.MinimapFogColor);
		}
		for (const FVeyraMinimapPing& Ping : View.Pings)
		{
			DrawHudOutline(Canvas, Ping.Centre, 2.0 * Ping.Radius, Settings.PresencePingColor.CopyWithNewOpacity(Ping.Fade));
		}
		for (const FVeyraMinimapDot& Dot : View.Dots)
		{
			const double Side = Dot.Kind == EVeyraMinimapDot::Vanguard ? Preferences.MinimapVanguardIcon
				: Dot.Kind == EVeyraMinimapDot::Structure					  ? Preferences.MinimapStructureIcon
																			  : Preferences.MinimapUnitIcon;
			DrawHudRect(Canvas, Dot.Position - FVector2D(Side / 2.0), FVector2D(Side), MinimapColor(Preferences, Dot.Side));
		}
		for (const FVeyraMinimapTeamPing& Ping : View.TeamPings)
		{
			const FLinearColor Color = Ping.Kind == EVeyraPingKind::Danger ? Settings.DangerPingColor : Settings.LookPingColor;
			DrawHudOutline(Canvas, Ping.Centre, Preferences.MinimapVanguardIcon * 2.0, Color.CopyWithNewOpacity(Ping.Fade));
		}
		if (View.Focus.IsSet())
		{
			DrawHudOutline(Canvas, View.Focus.GetValue(), Preferences.MinimapVanguardIcon * 2.0, Settings.TextColor);
		}
	}

	/** Its side's pings where they point on the ground, fading as they age; labelled when the player asks (SET-68). */
	void DrawWorldPings(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, double PingSeconds, TConstArrayView<FVeyraReceivedPing> Pings, double Now)
	{
		for (const FVeyraReceivedPing& Held : Pings)
		{
			const double Age = Now - Held.ReceivedAt;
			if (Age >= PingSeconds)
			{
				continue;
			}
			const FVector OnScreen = Canvas.Project(Held.Ping.Point);
			if (OnScreen.Z <= 0.0)
			{
				continue;
			}
			const FLinearColor Color = (Held.Ping.Kind == EVeyraPingKind::Danger ? Settings.DangerPingColor : Settings.LookPingColor)
				.CopyWithNewOpacity(1.0 - FMath::Max(0.0, Age) / PingSeconds);
			const FVector2D Centre(OnScreen.X, OnScreen.Y);
			DrawHudOutline(Canvas, Centre, Settings.WorldPingSize, Color);
			DrawHudOutline(Canvas, Centre, Settings.WorldPingSize / 2.0, Color);
			if (Settings.bPingTextLabels)
			{
				DrawHudText(Canvas, 1.0f, Centre + FVector2D(Settings.WorldPingSize, -Settings.WorldPingSize / 2.0), HudEnumName(Held.Ping.Kind), Color);
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
			const AVeyraPlayerController* Owner = WeakController.Get();
			const FVeyraInterfacePreferences Preferences = VeyraInterfacePreferences::Resolve(Settings, VeyraInterfacePreferences::StoreOf(Owner));
			const bool bOn = Purpose == AVeyraPlayerController::EMinimapClick::Camera ? Preferences.bMinimapClickMovesCamera
				: Purpose == AVeyraPlayerController::EMinimapClick::Move			  ? Preferences.bMinimapRightClickMoves
																					  : true;
			const UGameViewportClient* Viewport = Owner && Owner->GetLocalPlayer() ? Owner->GetLocalPlayer()->ViewportClient : nullptr;
			if (!bOn || !Viewport)
			{
				return {};
			}
			FVector2D Size;
			Viewport->GetViewportSize(Size);
			return VeyraMinimap::ToWorld(Arrangement(Settings, Preferences, Size).Minimap, Screen);
		});
	}

}

void VeyraGreyboxHud::Draw(UCanvas& Canvas, const UVeyraGreyboxSubsystem& Greybox, const APlayerController* Viewer)
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FVeyraInterfacePreferences Preferences = VeyraInterfacePreferences::Resolve(Settings, VeyraInterfacePreferences::StoreOf(Greybox.GetWorld()));
	const double Now = Greybox.GetServerNow();
	const FVeyraHudArrangement Layout = Arrangement(Settings, Preferences, FVector2D(Canvas.ClipX, Canvas.ClipY));
	const AVeyraPlayerController* Targeting = Cast<AVeyraPlayerController>(Viewer);
	const TArray<const AActor*> Targeted = Targeting ? Targeting->GetTargetedUnits() : TArray<const AActor*>();
	for (TActorIterator<APawn> It(Greybox.GetWorld()); It; ++It)
	{
		if (VeyraUnits::KindOf(*It).IsSet())
		{
			DrawOverheadBars(Canvas, Greybox, Settings, Preferences, Targeted, **It, Now);
		}
	}
	// The player's own fountain shop says what it is, so nobody needs telling there is one (ADR-063 §6).
	for (const AVeyraFountainShop* Shop : Greybox.GetShops())
	{
		const FVector OnScreen = Shop->GetTeam() == Greybox.GetViewerTeam()
			? Canvas.Project(Shop->GetActorLocation() + FVector::UpVector * (Shop->GetHeight() + Settings.BarLift))
			: FVector::ZeroVector;
		if (OnScreen.Z > 0.0)
		{
			const FString Label = TEXT("Shop");
			const float Scale = Preferences.HudScales.OverheadBars;
			float Width = 0.0f;
			float Height = 0.0f;
			Canvas.TextSize(HudFont(), Label, Width, Height, Scale, Scale);
			DrawHudText(Canvas, Scale, FVector2D(OnScreen.X - Width / 2.0f, OnScreen.Y - Height), Label, Settings.ShopColor);
		}
	}
	DrawCombatText(Canvas, Settings, Preferences, VeyraCombatTextView::Describe(Greybox.GetCombatText(), FPlatformTime::Seconds(), Preferences.CombatText));
	if (const AVeyraGameState* GameState = Greybox.GetWorld()->GetGameState<AVeyraGameState>())
	{
		const AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(Viewer);
		const AVeyraPlayerState* Own = Viewer ? Viewer->GetPlayerState<AVeyraPlayerState>() : nullptr;
		if (Player && Own)
		{
			BindMinimapClicks(*const_cast<AVeyraPlayerController*>(Player));
			const AVeyraCameraRig* Rig = Player->GetCameraRig();
			const TOptional<FVector> Focus = Rig ? TOptional<FVector>(Rig->GetFocus()) : TOptional<FVector>();
			const FVeyraMinimapFrame& Frame = Layout.Minimap;
			FVeyraMinimapView View = VeyraMinimap::Describe(*Greybox.GetWorld(), Frame, Own->GetVeyraTeam(), Own->GetPawn(), Focus, Now);
			const double RealNow = FPlatformTime::Seconds();
			View.TeamPings = VeyraMinimap::DescribeTeamPings(Frame, Player->GetPings(), RealNow, Preferences.PingSeconds);
			DrawWorldPings(Canvas, Settings, Preferences.PingSeconds, Player->GetPings(), RealNow);
			DrawMinimap(Canvas, Settings, Preferences, View);
		}
		// The deck, the top strip and Team Flux (VeyraHudDeck).
		// The warnings the player allows, while they show (ADR-055 §5).
		TArray<FString> Warnings;
		if (Greybox.IsShowingConnectionWarning())
		{
			Warnings.Add(TEXT("Connection unstable"));
		}
		if (Greybox.IsShowingPerformanceWarning())
		{
			Warnings.Add(TEXT("Low frame rate: Graphics settings can help"));
		}
		VeyraHudDeck::Draw(Canvas, Settings, Preferences, Greybox.GetHudFont(), *Greybox.GetWorld(), *GameState, Viewer, Own, Now, Warnings, Layout);
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
