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
#include "Hud/VeyraHudModel.h"
#include "Hud/VeyraMinimapModel.h"
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
	constexpr int32 SecondsPerMinute = 60;

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
		if (View.Focus.IsSet())
		{
			DrawHudOutline(Canvas, View.Focus.GetValue(), Settings.MinimapVanguardIcon * 2.0, Settings.TextColor);
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
			const bool bOn = Purpose == AVeyraPlayerController::EMinimapClick::Camera ? Settings.bMinimapClickMovesCamera : Settings.bMinimapRightClickMoves;
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

	/** One line centred at the top, Row lines under the first. */
	void DrawCentredLine(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const FString& Text, int32 Row, const FLinearColor& Color)
	{
		float Width = 0.0f;
		float Height = 0.0f;
		Canvas.TextSize(HudFont(), Text, Width, Height);
		DrawHudText(Canvas, FVector2D((Canvas.ClipX - Width) / 2.0f, Settings.HudMargin + Row * HudLineHeight()), Text, Color);
	}

	/**
	 * The phase and match clock, top centre; "Paused" while a pause holds the match, with the time left
	 * of a voted intermission. Under it, the open vote the viewer may answer, and the viewer's AFK
	 * warning (ADR-019 §7).
	 */
	void DrawMatchClock(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const AVeyraGameState& GameState, const APlayerController* Viewer)
	{
		const int32 Seconds = FMath::FloorToInt32(GameState.GetMatchClockSeconds());
		FString Text = FString::Printf(TEXT("%s  %02d:%02d"), *HudEnumName(GameState.GetPhase()), Seconds / SecondsPerMinute, Seconds % SecondsPerMinute);
		if (GameState.IsMatchPaused())
		{
			const int32 Left = GameState.GetIntermissionSecondsLeft();
			Text += Left > 0 ? FString::Printf(TEXT("  Paused, resumes in %d:%02d"), Left / SecondsPerMinute, Left % SecondsPerMinute) : FString(TEXT("  Paused"));
		}
		DrawCentredLine(Canvas, Settings, Text, 0, Settings.TextColor);
		int32 Row = 1;
		const AVeyraPlayerState* Own = Viewer ? Viewer->GetPlayerState<AVeyraPlayerState>() : nullptr;
		const AVeyraPlayerController* Voter = Cast<AVeyraPlayerController>(Viewer);
		const FVeyraVoteState& Vote = Voter ? Voter->GetOpenVote() : GameState.GetVote();
		// A team's vote reaches only that team, as League shows a surrender.
		if (Vote.bOpen && Own)
		{
			const UVeyraInputSettings& Input = *GetDefault<UVeyraInputSettings>();
			const bool bVoted = Vote.Voted.Contains(Own->GetPlayerId());
			const FString Answer = bVoted ? FString(TEXT("you voted"))
				: FString::Printf(TEXT("%s Yes, %s No"), *Input.VoteYesKey.GetDisplayName().ToString(), *Input.VoteNoKey.GetDisplayName().ToString());
			DrawCentredLine(Canvas, Settings, FString::Printf(TEXT("%s vote: %d of %d yes, %d no, %.0f s   %s"), *HudEnumName(Vote.Kind), Vote.Yes, Vote.Needed, Vote.No,
				Vote.SecondsLeft, *Answer), Row++, Settings.TextColor);
		}
		const AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(Viewer);
		if (Player && Player->IsWarnedAfk())
		{
			DrawCentredLine(Canvas, Settings, TEXT("You are AFK: your Vanguard walks to safety. Give an order to take control back."), Row++, Settings.WarningColor);
		}
	}

	/** One line of the player's panel, in its colour. */
	struct FHudLine
	{
		FString Text;
		FLinearColor Color;
	};

	/** The indent that puts a description under its ability's name. */
	const TCHAR* const DescriptionIndent = TEXT("      ");

	/**
	 * The player's Vanguard, level, XP, skill points and vitals; its passive; and each slot's ability,
	 * rank and cooldown, each with a line saying what it does. Bottom left.
	 */
	void DrawPlayerPanel(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const AVeyraPlayerState& Participant, double Now)
	{
		const FVeyraHudPlayer Player = VeyraHud::DescribePlayer(Participant, Now);
		const FVeyraVanguardDefinition* Definition = UVeyraVanguardsTuningSubsystem::FindVanguard(Player.Vanguard);
		const FString Resource = Definition ? HudEnumName(Definition->Resource) : FString(TEXT("Resource"));
		const UVeyraInputSettings& Input = *GetDefault<UVeyraInputSettings>();
		const auto AddDescription = [&Settings](TArray<FHudLine>& Lines, const FText& Description) {
			if (!Description.IsEmpty())
			{
				Lines.Add({ DescriptionIndent + Description.ToString(), Settings.DescriptionColor });
			}
		};

		TArray<FHudLine> Lines;
		FString Name = VeyraContentText::VanguardName(Player.Vanguard).ToString();
		if (const FText Title = VeyraContentText::VanguardTitle(Player.Vanguard); !Title.IsEmpty())
		{
			Name += TEXT(", ") + Title.ToString();
		}
		Lines.Add({ FString::Printf(TEXT("%s   Level %d   XP %d / %d   Skill points %d   Gold %d"), *Name, Player.Level, Player.Experience,
			Player.ExperienceToNextLevel, Player.UnspentSkillPoints, Player.Gold), Settings.TextColor });
		Lines.Add({ FString::Printf(TEXT("Health %.0f / %.0f   Shield %.0f   %s %.0f / %.0f"), Player.Vitals.Health, Player.Vitals.MaxHealth, Player.Vitals.Shield,
			*Resource, Player.Vitals.Resource, Player.Vitals.MaxResource), Settings.TextColor });
		if (Player.Passive.IsValid())
		{
			Lines.Add({ FString::Printf(TEXT("Passive: %s"), *VeyraContentText::PassiveName(Player.Passive).ToString()), Settings.TextColor });
			AddDescription(Lines, VeyraContentText::PassiveDescription(Player.Passive));
		}
		for (const FVeyraHudSlot& Slot : Player.Slots)
		{
			const FString Key = Input.GetAbilityKey(Slot.Slot).GetDisplayName(false).ToString();
			if (!Slot.Ability.IsValid())
			{
				Lines.Add({ FString::Printf(TEXT("[%s] no ability"), *Key), Settings.TextColor });
				continue;
			}
			FString State;
			if (Slot.Rank == 0)
			{
				State = TEXT("not learned");
			}
			else if (Slot.CooldownSeconds > 0.0)
			{
				State = FString::Printf(TEXT("%.1f s"), Slot.CooldownSeconds);
			}
			else
			{
				State = TEXT("ready");
			}
			FString Line = FString::Printf(TEXT("[%s] %s   rank %d / %d   %s"), *Key, *VeyraContentText::AbilityName(Slot.Ability).ToString(), Slot.Rank,
				Slot.MaxRank, *State);
			if (Slot.bCanRankUp)
			{
				Line += FString::Printf(TEXT("   %s+%s to rank up"), *Input.RankUpModifierKey.GetDisplayName(false).ToString(), *Key);
			}
			const bool bEmpowered = Slot.EmpoweredSeconds > 0.0;
			if (bEmpowered)
			{
				Line += FString::Printf(TEXT("   next attack empowered: %.1f s"), Slot.EmpoweredSeconds);
			}
			Lines.Add({ MoveTemp(Line), bEmpowered ? Settings.EmpoweredColor : Settings.TextColor });
			AddDescription(Lines, VeyraContentText::AbilityDescription(Slot.Ability));
		}

		// The item bar: each inventory slot by its key, what it holds and its Active's cooldown (ADR-012 §1).
		if (!Player.Items.IsEmpty())
		{
			FString Bar;
			for (const FVeyraHudItemSlot& Item : Player.Items)
			{
				FString Held = TEXT("-");
				if (Item.Item.IsValid())
				{
					Held = VeyraContentText::ItemName(Item.Item).ToString();
					if (Item.Count > 1)
					{
						Held += FString::Printf(TEXT(" x%d"), Item.Count);
					}
					if (Item.CooldownSeconds > 0.0)
					{
						Held += FString::Printf(TEXT(" %.1f s"), Item.CooldownSeconds);
					}
				}
				Bar += FString::Printf(TEXT("[%s] %s   "), *Input.GetAbilityKey(Item.Slot).GetDisplayName(false).ToString(), *Held);
			}
			const FString ShopKey = GetDefault<UVeyraUIInputSettings>()->ShopKey.GetDisplayName(false).ToString();
			Bar += Player.PendingPurchases > 0 ? FString::Printf(TEXT("Shop [%s]: %d waiting for the fountain"), *ShopKey, Player.PendingPurchases)
											   : FString::Printf(TEXT("Shop [%s]"), *ShopKey);
			Lines.Add({ MoveTemp(Bar), Settings.TextColor });
		}

		// The Flux Spells, by their keys: locked with the permanent Flux each needs, ready, or cooling (ADR-015 §7).
		if (!Player.Spells.IsEmpty())
		{
			FString Bar;
			for (const FVeyraHudSpellSlot& Spell : Player.Spells)
			{
				FString State = TEXT("-");
				if (Spell.Spell.IsValid())
				{
					State = VeyraContentText::AbilityName(Spell.Spell).ToString();
					if (Spell.bLocked)
					{
						State += FString::Printf(TEXT(" (locked until %.0f permanent Team Flux)"), Spell.UnlockFlux);
					}
					else if (Spell.CooldownSeconds > 0.0)
					{
						State += FString::Printf(TEXT(" %.0f s"), FMath::CeilToDouble(Spell.CooldownSeconds));
					}
					else
					{
						State += TEXT(" ready");
					}
				}
				Bar += FString::Printf(TEXT("[%s] %s   "), *Input.GetAbilityKey(Spell.Slot).GetDisplayName(false).ToString(), *State);
			}
			Lines.Add({ MoveTemp(Bar), Settings.TextColor });
		}

		// The vision tool, by its key: the ward charges carried and when the next comes back, or the
		// tool's cooldown (ADR-016 §8).
		if (Player.VisionTool.bPresent)
		{
			const FString Key = Input.GetAbilityKey(EVeyraAbilitySlot::VisionTool).GetDisplayName(false).ToString();
			FString Tool;
			if (Player.VisionTool.Tool == EVeyraVisionTool::PersistentWard)
			{
				Tool = FString::Printf(TEXT("[%s] Ward %d/%d"), *Key, Player.VisionTool.WardCharges, Player.VisionTool.MaxWardCharges);
				if (Player.VisionTool.NextChargeSeconds > 0.0)
				{
					Tool += FString::Printf(TEXT(", next in %.0f s"), FMath::CeilToDouble(Player.VisionTool.NextChargeSeconds));
				}
			}
			else
			{
				const TCHAR* ToolName = Player.VisionTool.Tool == EVeyraVisionTool::Sweeper ? TEXT("Sweeper") : TEXT("Quick Sight");
				Tool = Player.VisionTool.CooldownSeconds > 0.0
					? FString::Printf(TEXT("[%s] %s %.0f s"), *Key, ToolName, FMath::CeilToDouble(Player.VisionTool.CooldownSeconds))
					: FString::Printf(TEXT("[%s] %s ready"), *Key, ToolName);
			}
			Lines.Add({ MoveTemp(Tool), Settings.TextColor });
		}

		float Y = Canvas.ClipY - Settings.HudMargin - Lines.Num() * HudLineHeight();
		for (const FHudLine& Line : Lines)
		{
			DrawHudText(Canvas, FVector2D(Settings.HudMargin, Y), Line.Text, Line.Color);
			Y += HudLineHeight();
		}

		// While dead, the wait for the fountain, under the match clock (Economy & Progression Bible §14).
		if (Player.bDead)
		{
			const FString Respawn = FString::Printf(TEXT("Respawning in %d s"), FMath::CeilToInt32(Player.RespawnSeconds));
			float Width = 0.0f;
			float Height = 0.0f;
			Canvas.TextSize(HudFont(), Respawn, Width, Height);
			DrawHudText(Canvas, FVector2D((Canvas.ClipX - Width) / 2.0f, Settings.HudMargin + HudLineHeight()), Respawn, Settings.TextColor);
		}

		// While recalling, the channel's bar filling toward home, with the time it has left (ADR-012 §8).
		if (Player.bRecalling)
		{
			const FVector2D TopLeft((Canvas.ClipX - Settings.ChannelBarWidth) / 2.0f, Canvas.ClipY - Settings.ChannelBarLift);
			DrawHudRect(Canvas, TopLeft, FVector2D(Settings.ChannelBarWidth, Settings.ChannelBarHeight), Settings.BarBackgroundColor);
			DrawHudRect(Canvas, TopLeft, FVector2D(Settings.ChannelBarWidth * Player.RecallProgress, Settings.ChannelBarHeight), Settings.ChannelColor);
			const FString Recall = FString::Printf(TEXT("Recall   %.1f s"), Player.RecallSeconds);
			float Width = 0.0f;
			float Height = 0.0f;
			Canvas.TextSize(HudFont(), Recall, Width, Height);
			DrawHudText(Canvas, FVector2D((Canvas.ClipX - Width) / 2.0f, TopLeft.Y - HudLineHeight()), Recall, Settings.TextColor);
		}
	}

	/**
	 * Each team's Team Flux, the viewer's own first: active and permanent Flux, what it gives their
	 * Fluxborn, and the time left on each temporary grant. Top left (ADR-011 §10).
	 */
	void DrawTeamFlux(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const UWorld* World, EVeyraTeam Viewer, double Now)
	{
		TArray<FVeyraHudTeamFlux> Teams = VeyraHud::DescribeTeamFlux(World, Now);
		Teams.StableSort([Viewer](const FVeyraHudTeamFlux& A, const FVeyraHudTeamFlux& B) { return A.Team == Viewer && B.Team != Viewer; });
		constexpr double Percent = 100.0;
		float Y = Settings.HudMargin;
		for (const FVeyraHudTeamFlux& Team : Teams)
		{
			FString Line = FString::Printf(TEXT("%s Team Flux %.0f (%.0f permanent)   Fluxborn +%.0f%%"), Team.Team == Viewer ? TEXT("Your") : TEXT("Enemy"),
				Team.Active, Team.Permanent, Team.FluxbornBonus * Percent);
			for (const double Seconds : Team.TemporarySeconds)
			{
				Line += FString::Printf(TEXT("   %d s"), FMath::CeilToInt32(Seconds));
			}
			DrawHudText(Canvas, FVector2D(Settings.HudMargin, Y), Line, Settings.TextColor);
			Y += HudLineHeight();
		}
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
		DrawMatchClock(Canvas, Settings, *GameState, Viewer);
		const AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(Viewer);
		const AVeyraPlayerState* Own = Viewer ? Viewer->GetPlayerState<AVeyraPlayerState>() : nullptr;
		if (Player && Own)
		{
			BindMinimapClicks(*const_cast<AVeyraPlayerController*>(Player));
			const AVeyraCameraRig* Rig = Player->GetCameraRig();
			const TOptional<FVector> Focus = Rig ? TOptional<FVector>(Rig->GetFocus()) : TOptional<FVector>();
			DrawMinimap(Canvas, Settings, VeyraMinimap::Describe(*Greybox.GetWorld(), MinimapFrame(Settings, FVector2D(Canvas.ClipX, Canvas.ClipY)),
				Own->GetVeyraTeam(), Own->GetPawn(), Focus, Now));
		}
	}
	if (const AVeyraPlayerState* Participant = Viewer ? Viewer->GetPlayerState<AVeyraPlayerState>() : nullptr)
	{
		DrawPlayerPanel(Canvas, Settings, *Participant, Now);
		DrawTeamFlux(Canvas, Settings, Greybox.GetWorld(), Participant->GetVeyraTeam(), Now);
	}
}
