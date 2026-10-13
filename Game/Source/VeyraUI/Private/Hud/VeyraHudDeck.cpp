// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudDeck.h"

#include "CanvasItem.h"
#include "HAL/IConsoleManager.h"
#include "Client/VeyraClientFlowSubsystem.h"
#include "Client/VeyraClientIntents.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Hud/VeyraChatLogModel.h"
#include "Hud/VeyraHudLayout.h"
#include "Hud/VeyraHudModel.h"
#include "Hud/VeyraHudMotion.h"
#include "Hud/VeyraHudPainter.h"
#include "Hud/VeyraStatGlyphs.h"
#include "Hud/VeyraKillFeedModel.h"
#include "Hud/VeyraTargetFrameModel.h"
#include "Input/VeyraInputSettings.h"
#include "Rendering/SlateRenderer.h"
#include "Engine/GameInstance.h"
#include "Match/VeyraMatchMenuSubsystem.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellArt.h"
#include "Shell/VeyraShellBrushes.h"
#include "Shell/VeyraShellStyleSettings.h"
#include "Shop/VeyraShopModel.h"
#include "Shell/VeyraUIInputSettings.h"
#include "Statistics/VeyraScoreComponent.h"
#include "Styling/CoreStyle.h"
#include "Text/VeyraContentText.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"
#include "VeyraVanguardCharacter.h"

// The engine keeps a smoothed frame rate, declared only where it is defined.
extern ENGINE_API float GAverageFPS;

#if !UE_BUILD_SHIPPING
// For captures of the HUD (Smoke.ps1 -ScreenshotCommands): a deck tile's tooltip shown as if the cursor rested on it, and
// the stats panel held open. Not in a shipping build.
static TAutoConsoleVariable<FString> CVarDevHudHover(TEXT("Veyra.Dev.HudHover"), TEXT(""),
	TEXT("Shows the tooltip of a deck tile as if hovered: Passive, Q, W, E, R or Item1 to Item6; empty for the cursor's own."));
static TAutoConsoleVariable<bool> CVarDevStatsPanelOpen(TEXT("Veyra.Dev.StatsPanelOpen"), false, TEXT("Holds the HUD's stats panel open."));
#endif

namespace
{
	constexpr int32 SecondsPerMinute = 60;

	/** The HUD's painter (ADR-082 §2), by its short name here. */
	using FPainter = FVeyraHudPainter;

	/** What the cursor rests on, for its tooltip (ADR-080 §2). */
	struct FHover
	{
		FString Title;
		FString Detail;
		FString Description;
		/** The header's icon, and the key it is cast with, in a chip beside the title. */
		UTexture2D* Icon = nullptr;
		FString Key;
		/** The header's right side: the cooldown the caster would wait, and the cost or the price. */
		FString Cooldown;
		FString Cost;
		/** An ability's facts, each labelled, coloured by kind and with how it is made beside it. */
		TArray<FVeyraAbilityFact> Facts;
		/** An item's stats, a line each. */
		TArray<FString> Stats;
		/** The footer: each damage part's amount at the next rank. */
		FString NextRank;
		/** The hovered tile's top edge: its centre across, and its top. The tooltip stands over it. */
		FVector2D Anchor = FVector2D::ZeroVector;
	};

	FString KeyName(const FKey& Key)
	{
		return Key.GetDisplayName(false).ToString();
	}

	/** Draws Icon whole in the square at At, Size across, its corners cut Cut across, in Tint; false when there is none to draw. */
	bool DrawIcon(const FPainter& Paint, UTexture2D* Icon, const FVector2D& At, float Size, float Cut = 0.0f, const FLinearColor& Tint = FLinearColor::White)
	{
		if (!Icon || !Icon->GetResource())
		{
			return false;
		}
		if (Cut > 0.0f)
		{
			Paint.ImageIn(Icon, FPainter::Chamfered(At, FVector2D(Size), Cut), FBox2D(At, At + FVector2D(Size)), Tint);
		}
		else
		{
			Paint.Image(Icon, At, FVector2D(Size), Tint);
		}
		return true;
	}

	FString Monogram(const FString& Name);

	/** Vanguard's hero art and the part of it that is its portrait, or none. */
	UTexture2D* PortraitOf(const FVeyraContentId& Vanguard, FVector2D& UV0, FVector2D& UV1)
	{
		const FString VanguardId = Vanguard.ToString();
		UTexture2D* Hero = VeyraShellArt::HeroOf(VanguardId);
		if (!Hero || !Hero->GetResource())
		{
			return nullptr;
		}
		const FBox2f Crop = VeyraShellArt::Crop(VanguardId, Hero->GetSizeX(), Hero->GetSizeY(), 1.0f, true);
		UV0 = FVector2D(Crop.Min);
		UV1 = FVector2D(Crop.Max);
		return Hero;
	}

	/** Vanguard's face in the square at At, Size across: its hero art cropped to a portrait, or its monogram at MonogramSize. */
	void DrawFace(const FPainter& Paint, const FVeyraContentId& Vanguard, const FVector2D& At, float Size, int32 MonogramSize, const FLinearColor& Tint = FLinearColor::White)
	{
		FVector2D UV0;
		FVector2D UV1;
		if (UTexture2D* Hero = PortraitOf(Vanguard, UV0, UV1))
		{
			Paint.Image(Hero, At, FVector2D(Size), Tint, UV0, UV1);
			return;
		}
		Paint.Rect(At, FVector2D(Size), Paint.Settings.BarBackgroundColor);
		Paint.TextCentred(At + FVector2D(Size / 2.0f), Monogram(VeyraContentText::VanguardName(Vanguard).ToString()), Paint.Font(TEXT("Black"), MonogramSize),
			Paint.Settings.TextColor);
	}

	/** Vanguard's face in a circle of Radius at Centre (ADR-082 §4), or its monogram on a dark disc. */
	void DrawRoundFace(const FPainter& Paint, const FVeyraContentId& Vanguard, const FVector2D& Centre, float Radius, int32 MonogramSize)
	{
		const TArray<FVector2D> Round = FPainter::Circle(Centre, Radius);
		FVector2D UV0;
		FVector2D UV1;
		if (UTexture2D* Hero = PortraitOf(Vanguard, UV0, UV1))
		{
			Paint.ImageIn(Hero, Round, FBox2D(Centre - FVector2D(Radius), Centre + FVector2D(Radius)), FLinearColor::White, UV0, UV1);
			return;
		}
		Paint.Polygon(Round, Paint.Settings.BarBackgroundColor);
		Paint.TextCentred(Centre, Monogram(VeyraContentText::VanguardName(Vanguard).ToString()), Paint.Font(TEXT("Black"), MonogramSize), Paint.Settings.TextColor);
	}

	/** Two letters that stand for Name on a tile without an icon: its words' initials. */
	FString Monogram(const FString& Name)
	{
		TArray<FString> Words;
		Name.ParseIntoArrayWS(Words);
		FString Letters;
		for (const FString& Word : Words)
		{
			if (Letters.Len() < 2 && !Word.IsEmpty() && FChar::IsAlpha(Word[0]))
			{
				Letters.AppendChar(FChar::ToUpper(Word[0]));
			}
		}
		return Letters.IsEmpty() ? Name.Left(2).ToUpper() : Letters;
	}

	bool Contains(const FVector2D& TopLeft, const FVector2D& Size, const TOptional<FVector2D>& Point)
	{
		return Point.IsSet() && Point->X >= TopLeft.X && Point->Y >= TopLeft.Y && Point->X < TopLeft.X + Size.X && Point->Y < TopLeft.Y + Size.Y;
	}

	/** The side of the ability tile the tile's look was drawn for, in units: smaller tiles keep their cut and bursts in proportion. */
	constexpr float DrawnTileSide = 52.0f;

	/** A tile's cut corners at the painter's scale (ADR-082 §4). */
	float TileCut(const FPainter& Paint, float Side)
	{
		return Paint.S(Paint.Settings.TileChamfer) * FMath::Min(1.0f, Side / Paint.S(DrawnTileSide));
	}

	/**
	 * A tile's frame (ADR-082 §4): its chamfered edge, shading brass from head to foot while it is ready to use (so ready always
	 * shows, whatever the cooldown options; ADR-059 §3), dim while it is not, or in Edge when given.
	 */
	void DrawTileFrame(const FPainter& Paint, const FVector2D& At, float Side, bool bReady, const TOptional<FLinearColor>& Edge = {})
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const TArray<FVector2D> Points = FPainter::Chamfered(At, FVector2D(Side), TileCut(Paint, Side));
		if (Edge.IsSet())
		{
			Paint.Polygon(Points, Edge.GetValue());
			return;
		}
		if (!bReady)
		{
			Paint.Polygon(Points, Settings.TileIdleColor);
			return;
		}
		TArray<FLinearColor> Colors;
		for (const FVector2D& Point : Points)
		{
			Colors.Add(FMath::Lerp(Settings.TileReadyTopColor, Settings.TileReadyBottomColor, static_cast<float>((Point.Y - At.Y) / Side)));
		}
		Paint.Polygon(Points, FLinearColor::White, Colors);
	}

	/** How far inside its frame a tile's icon sits, so the frame's edge shows around it. */
	float IconInset(const FPainter& Paint)
	{
		return FMath::Max(1.0f, Paint.S(1.5f));
	}

	/** A tile's icon inside its frame, in Tint: false when it has none. */
	bool DrawTileIcon(const FPainter& Paint, UTexture2D* Icon, const FVector2D& At, float Side, const FLinearColor& Tint = FLinearColor::White)
	{
		const float Inset = IconInset(Paint);
		return DrawIcon(Paint, Icon, At + FVector2D(Inset), Side - Inset * 2.0f, TileCut(Paint, Side) - Inset * 0.4f, Tint);
	}

	/**
	 * A cooling slot (ADR-059 §3): a sweep over the part still to wait, its leading edge lit, or with the sweep off the whole tile
	 * shaded, and the seconds left over it as the player chose. Total is what the cooldown started with; without one the whole
	 * tile shades. The sweep keeps inside the tile's cut corners.
	 */
	void DrawCooldown(const FPainter& Paint, const FVeyraCooldownDisplay& Display, const FVector2D& TopLeft, float Side, double SecondsLeft, double Total)
	{
		if (SecondsLeft <= 0.0)
		{
			return;
		}
		const float Cut = TileCut(Paint, Side);
		const FVector2D Centre = TopLeft + FVector2D(Side / 2.0f);
		if (Display.bSweep && Total > 0.0)
		{
			const TArray<FVector2D> Fan = VeyraHud::SweepOutline(TopLeft, Side, 1.0 - SecondsLeft / Total);
			const FVector2D BottomRight = TopLeft + FVector2D(Side);
			TArray<FVector2D> Shaded;
			for (int32 Index = 0; Index < Fan.Num(); ++Index)
			{
				const FVector2D& Point = Fan[Index];
				// A square corner that is cut becomes the cut's two ends, clockwise; any other edge point keeps inside the cut.
				if (Index > 1 && Point.Equals(TopLeft))
				{
					Shaded.Add(TopLeft + FVector2D(0.0f, Cut));
					Shaded.Add(TopLeft + FVector2D(Cut, 0.0f));
				}
				else if (Index > 1 && Point.Equals(BottomRight))
				{
					Shaded.Add(BottomRight - FVector2D(0.0f, Cut));
					Shaded.Add(BottomRight - FVector2D(Cut, 0.0f));
				}
				else
				{
					Shaded.Add(Index == 0 ? Point : FPainter::IntoChamfer(Point, Centre, TopLeft, FVector2D(Side), Cut));
				}
			}
			Paint.Polygon(Shaded, Paint.Settings.ShadeColor);
			if (Shaded.Num() > 1)
			{
				Paint.Line(Centre, Shaded[1], Paint.Settings.MotionHotColor.CopyWithNewOpacity(0.55f), FMath::Max(1.0f, Paint.S(1.5f)));
			}
		}
		else
		{
			Paint.Polygon(FPainter::Chamfered(TopLeft, FVector2D(Side), Cut), Paint.Settings.ShadeColor);
		}
		if (Display.bNumbers)
		{
			Paint.TextCentred(Centre, VeyraHud::CooldownLabel(SecondsLeft, Display.bTenths), Paint.Font(TEXT("Bold"), Paint.Settings.HudHeadingFontSize), Paint.Settings.TextColor,
				true);
		}
	}

	/**
	 * A tile's moments (ADR-082 §3): a white flash, a glint across it and a brass burst as it comes ready, SinceReady seconds ago;
	 * a smaller burst in the accent as it is used, SinceUsed seconds ago.
	 */
	void DrawTileMoments(const FPainter& Paint, const FVector2D& At, float Side, double SinceReady, double SinceUsed, uint32 Seed)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const FVector2D Centre = At + FVector2D(Side / 2.0f);
		const float Reach = Paint.S(Settings.MotionBurstRadius) * Side / Paint.S(DrawnTileSide);
		if (SinceReady < Settings.MotionFlashSeconds)
		{
			const float Flash = 1.0f - static_cast<float>(SinceReady / Settings.MotionFlashSeconds);
			Paint.Polygon(FPainter::Chamfered(At, FVector2D(Side), TileCut(Paint, Side)), Settings.MotionHotColor.CopyWithNewOpacity(0.55f * Flash * Flash));
		}
		// The glint crosses after the flash, as the website's sheen crosses a button.
		const double GlintSeconds = Settings.MotionBurstSeconds * 1.4;
		if (SinceReady < GlintSeconds)
		{
			Paint.Sweep(At, FVector2D(Side), FPainter::Eased(Settings.MotionSmear, SinceReady, GlintSeconds), Side * 0.7f, Settings.MotionHotColor.CopyWithNewOpacity(0.6f));
		}
		Paint.Burst(Centre, SinceReady, Reach, Settings.TileReadyTopColor, Seed);
		Paint.Burst(Centre, SinceUsed, Reach * 0.7f, Settings.HudAccentColor, Seed * 7u + 3u);
	}

	/** Kills each side has made, from its players' public scores. */
	void CountKills(const AVeyraGameState& GameState, EVeyraTeam Viewer, int32& OwnKills, int32& EnemyKills)
	{
		OwnKills = 0;
		EnemyKills = 0;
		for (const APlayerState* Member : GameState.PlayerArray)
		{
			const UVeyraScoreComponent* Score = Member ? Member->FindComponentByClass<UVeyraScoreComponent>() : nullptr;
			const AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(Member);
			if (!Score || !Participant || Participant->GetVeyraTeam() == EVeyraTeam::None)
			{
				continue;
			}
			(Participant->GetVeyraTeam() == Viewer ? OwnKills : EnemyKills) += Score->GetScore().Kills;
		}
	}

	/**
	 * The top strip: each side's kills in the display face around the match clock, under the safe area's top edge (Inset); a
	 * side's count glows as it rises (ADR-082 §3).
	 */
	void DrawTopStrip(const FPainter& Paint, const AVeyraGameState& GameState, EVeyraTeam Viewer, const FVeyraSideColors& Sides, const FVector2D& Inset)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const int32 Clock = FMath::FloorToInt32(GameState.GetMatchClockSeconds());
		const FString ClockText = FString::Printf(TEXT("%02d:%02d"), Clock / SecondsPerMinute, Clock % SecondsPerMinute);
		int32 OwnKills = 0;
		int32 EnemyKills = 0;
		CountKills(GameState, Viewer, OwnKills, EnemyKills);
		FSlateFontInfo ClockFont = Paint.Font(TEXT("SemiBold"), Settings.HudClockFontSize);
		ClockFont.LetterSpacing = 40;
		const FSlateFontInfo KillFont = Paint.Font(TEXT("DisplayBold"), Settings.HudClockFontSize);
		const float Width = Paint.S(220.0f);
		const float Height = Paint.S(40.0f);
		const FVector2D TopLeft((Paint.Canvas.ClipX - Width) / 2.0f, Inset.Y + Paint.S(Settings.DeckGap));
		Paint.Surface(TopLeft, FVector2D(Width, Height));
		const float Middle = TopLeft.Y + Height / 2.0f;
		Paint.TextCentred(FVector2D(Paint.Canvas.ClipX / 2.0f, Middle), ClockText, ClockFont, Settings.TextColor);
		const auto Kills = [&](const TCHAR* Key, int32 Count, float Across, const FLinearColor& Color) {
			const FVector2D Centre(TopLeft.X + Width * Across, Middle);
			if (Paint.Motion)
			{
				const FVeyraHudMotion::FCount Changed = Paint.Motion->Count(Key, Count, Settings.MotionCountSeconds, Settings.MotionSmear);
				if (Changed.bRose && Changed.SinceChange < Settings.MotionBurstSeconds * 2.0)
				{
					Paint.Glow(Centre, Height, Color.CopyWithNewOpacity(0.5f * static_cast<float>(1.0 - Changed.SinceChange / (Settings.MotionBurstSeconds * 2.0))));
				}
			}
			Paint.TextCentred(Centre, FString::FromInt(Count), KillFont, Color);
		};
		Kills(TEXT("OwnKills"), OwnKills, 0.14f, Sides.Ally);
		Kills(TEXT("EnemyKills"), EnemyKills, 0.86f, Sides.Enemy);
	}

	/**
	 * The frame rate and ping the player asked to see, top right (Settings Bible §3.6), and under them the warnings that
	 * show: steady and silent, never flashing (SET-21, Proposal 110; ADR-055 §5).
	 */
	void DrawReadouts(const FPainter& Paint, const FVeyraInterfacePreferences& Preferences, const APlayerController* Viewer, TConstArrayView<FString> Warnings,
		const FVector2D& Inset)
	{
		const APlayerState* Participant = Viewer ? Viewer->PlayerState.Get() : nullptr;
		// A server's own player has no ping to show.
		const float Ping = Participant ? Participant->GetPingInMilliseconds() : 0.0f;
		const FString Text = VeyraInterfacePreferences::DescribeReadouts(Preferences, GAverageFPS, Ping > 0.0f ? TOptional<float>(Ping) : TOptional<float>());
		const FSlateFontInfo Font = Paint.Font(TEXT("Bold"), Paint.Settings.HudSmallFontSize);
		const float Gap = Paint.S(Paint.Settings.DeckGap);
		const float Right = Paint.Canvas.ClipX - Inset.X - Gap;
		float Y = Inset.Y + Gap;
		if (!Text.IsEmpty())
		{
			const FVector2D Size = Paint.Measure(Text, Font);
			Paint.Text(FVector2D(Right - Size.X, Y), Text, Font, Paint.Settings.TextColor, true);
			Y += Size.Y + Gap / 2.0f;
		}
		for (const FString& Warning : Warnings)
		{
			const FVector2D Size = Paint.Measure(Warning, Font);
			Paint.Text(FVector2D(Right - Size.X, Y), Warning, Font, Paint.Settings.WarningColor, true);
			Y += Size.Y + Gap / 2.0f;
		}
	}

	/**
	 * Each side's Team Flux, the viewer's first, top left (ADR-011 §10), as the client draws a panel (ADR-082 §4): its label in
	 * the accent, then a row a side with its side's bar, its active Flux large, its permanent Flux, and its Fluxborn bonus in its
	 * side's colour at the right; temporary Flux's seconds follow small.
	 */
	void DrawTeamFlux(const FPainter& Paint, const UWorld& World, EVeyraTeam Viewer, double Now, const FVeyraSideColors& Sides, const FVector2D& Inset)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		TArray<FVeyraHudTeamFlux> Teams = VeyraHud::DescribeTeamFlux(&World, Now);
		if (Teams.IsEmpty())
		{
			return;
		}
		Teams.StableSort([Viewer](const FVeyraHudTeamFlux& A, const FVeyraHudTeamFlux& B) { return A.Team == Viewer && B.Team != Viewer; });
		constexpr double Percent = 100.0;
		FSlateFontInfo Label = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		Label.LetterSpacing = 200;
		const FSlateFontInfo Value = Paint.Font(TEXT("Bold"), Settings.HudHeadingFontSize);
		const FSlateFontInfo Note = Paint.Font(TEXT("Regular"), Settings.HudSmallFontSize);
		const float Pad = Paint.S(Settings.DeckPadding);
		const float Row = Paint.S(26.0f);
		const float Head = Paint.S(20.0f);
		// As wide as its widest row needs, and never narrower than its designed width.
		const auto PermanentOf = [](const FVeyraHudTeamFlux& Team) {
			FString Permanent = FString::Printf(TEXT("%.0f permanent"), Team.Permanent);
			for (const double Left : Team.TemporarySeconds)
			{
				Permanent += FString::Printf(TEXT("   %ds"), FMath::CeilToInt32(Left));
			}
			return Permanent;
		};
		const auto BonusOf = [](const FVeyraHudTeamFlux& Team) { return FString::Printf(TEXT("Fluxborn +%.0f%%"), Team.FluxbornBonus * Percent); };
		const float NoteAt = Paint.S(58.0f);
		float Needed = Paint.S(260.0f);
		for (const FVeyraHudTeamFlux& Team : Teams)
		{
			Needed = FMath::Max(Needed, Pad * 2.0f + NoteAt + static_cast<float>(Paint.Measure(PermanentOf(Team), Note).X + Paint.Measure(BonusOf(Team), Note).X) + Paint.S(12.0f));
		}
		const FVector2D TopLeft(Inset.X + Paint.S(Settings.DeckGap), Inset.Y + Paint.S(Settings.DeckGap));
		const FVector2D Size(Needed, Pad + Head + Row * Teams.Num());
		Paint.Surface(TopLeft, Size);
		Paint.Text(TopLeft + FVector2D(Pad, Paint.S(7.0f)), TEXT("TEAM FLUX"), Label, Settings.HudAccentColor);
		float Y = TopLeft.Y + Head + Paint.S(2.0f);
		for (const FVeyraHudTeamFlux& Team : Teams)
		{
			const FLinearColor Side = Team.Team == Viewer ? Sides.Ally : Sides.Enemy;
			const float X = TopLeft.X + Pad;
			const float ValueHeight = Paint.Measure(TEXT("0"), Value).Y;
			const float NoteY = Y + (ValueHeight - Paint.Measure(TEXT("Ag"), Note).Y) / 2.0f + Paint.S(1.0f);
			Paint.Rect(FVector2D(X, Y + Paint.S(3.0f)), FVector2D(Paint.S(3.0f), ValueHeight - Paint.S(6.0f)), Side);
			// Its active Flux runs to a new value (ADR-082 §3).
			const double Active = Paint.Motion
				? Paint.Motion->Count(FName(TEXT("TeamFlux"), static_cast<int32>(Team.Team) + 1), Team.Active, Settings.MotionCountSeconds, Settings.MotionSmear).Shown
				: Team.Active;
			Paint.Text(FVector2D(X + Paint.S(10.0f), Y), FString::Printf(TEXT("%.0f"), Active), Value, Settings.TextColor);
			Paint.Text(FVector2D(X + NoteAt, NoteY), PermanentOf(Team), Note, Settings.DescriptionColor);
			const FString Bonus = BonusOf(Team);
			Paint.Text(FVector2D(TopLeft.X + Size.X - Pad - Paint.Measure(Bonus, Note).X, NoteY), Bonus, Note, Side);
			Y += Row;
		}
	}

	/** Pause, the open vote and the AFK warning, each in a pill under the strip. */
	void DrawNotices(const FPainter& Paint, const AVeyraGameState& GameState, const APlayerController* Viewer, const FVector2D& Inset)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		TArray<TPair<FString, FLinearColor>> Notices;
		if (GameState.IsMatchPaused())
		{
			const int32 Left = GameState.GetIntermissionSecondsLeft();
			Notices.Add({ Left > 0 ? FString::Printf(TEXT("Paused   resumes in %d:%02d"), Left / SecondsPerMinute, Left % SecondsPerMinute) : FString(TEXT("Paused")),
				Settings.HudAccentColor });
		}
		const AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(Viewer);
		const AVeyraPlayerState* Own = Viewer ? Viewer->GetPlayerState<AVeyraPlayerState>() : nullptr;
		const FVeyraVoteState& Vote = Player ? Player->GetOpenVote() : GameState.GetVote();
		// A team's vote reaches only that team.
		if (Vote.bOpen && Own)
		{
			const UVeyraInputSettings& Input = Player ? Player->GetKeys() : *GetDefault<UVeyraInputSettings>();
			const bool bVoted = Vote.Voted.Contains(Own->GetPlayerId());
			const FString Answer = bVoted ? FString(TEXT("you voted")) : FString::Printf(TEXT("%s yes   %s no"), *KeyName(Input.VoteYesKey), *KeyName(Input.VoteNoKey));
			Notices.Add({ FString::Printf(TEXT("%s vote   %d of %d yes, %d no   %.0f s   %s"), *StaticEnum<EVeyraVoteKind>()->GetNameStringByValue(static_cast<int64>(Vote.Kind)),
				Vote.Yes, Vote.Needed, Vote.No, Vote.SecondsLeft, *Answer), Settings.TextColor });
		}
		if (Player && Player->IsWarnedAfk())
		{
			Notices.Add({ TEXT("You are AFK: your Vanguard walks to safety. Give an order to take control back."), Settings.WarningColor });
		}
		const FSlateFontInfo Font = Paint.Font(TEXT("Bold"), Settings.HudBodyFontSize);
		float Y = Inset.Y + Paint.S(Settings.DeckGap) + Paint.S(40.0f) + Paint.S(Settings.DeckGap);
		for (const TPair<FString, FLinearColor>& Notice : Notices)
		{
			const FVector2D Size = Paint.Measure(Notice.Key, Font) + FVector2D(Paint.S(24.0f), Paint.S(10.0f));
			const FVector2D TopLeft((Paint.Canvas.ClipX - Size.X) / 2.0f, Y);
			Paint.Surface(TopLeft, Size);
			Paint.Outline(TopLeft, Size, Notice.Value.CopyWithNewOpacity(0.6f));
			Paint.TextCentred(TopLeft + Size / 2.0, Notice.Key, Font, Notice.Value);
			Y += Size.Y + Paint.S(Settings.DeckGap / 2.0f);
		}
	}

	/** The hovered slot's name, state and what it does, above the deck. */
	/** Text in rows no wider than Width, the first FirstWidth, breaking between words. */
	TArray<FString> WrapAfter(const FPainter& Paint, const FString& Text, const FSlateFontInfo& Font, float FirstWidth, float Width)
	{
		TArray<FString> Words;
		Text.ParseIntoArrayWS(Words);
		TArray<FString> Rows;
		FString Row;
		for (const FString& Word : Words)
		{
			const FString Tried = Row.IsEmpty() ? Word : Row + TEXT(" ") + Word;
			const float Room = Rows.IsEmpty() ? FirstWidth : Width;
			if (Paint.Measure(Tried, Font).X > Room && (!Row.IsEmpty() || !Rows.IsEmpty() || FirstWidth < Width))
			{
				// Row is full; a first row too narrow for even one word stays empty, and the word starts the next.
				Rows.Add(Row);
				Row = Word;
			}
			else
			{
				Row = Tried;
			}
		}
		Rows.Add(Row);
		return Rows;
	}

	/** The chat log at the bottom left, above the composer in Frame (ADR-029 §5): each line's channel and sender in their side's colour. */
	void DrawChat(const FPainter& Paint, const FVeyraInterfacePreferences& Preferences, const FVeyraChatFrame& Frame, const AVeyraPlayerController& Player,
		const AGameStateBase& Match, EVeyraTeam Side, bool bComposing)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		FVeyraChatLogPreferences Log;
		Log.Lines = Settings.ChatLines;
		Log.FadeSeconds = Preferences.ChatFadeSeconds;
		Log.FadeOutSeconds = Settings.ChatFadeOutSeconds;
		Log.bTimestamps = Preferences.bChatTimestamps;
		// The party's and friends' lines the client flow holds, mixed in without popping up or taking focus (ADR-046 §6).
		TArray<FVeyraOutsideChat> Outside;
		const UGameInstance* Game = Player.GetGameInstance();
		if (const UVeyraClientFlowSubsystem* Flow = Game ? Game->GetSubsystem<UVeyraClientFlowSubsystem>() : nullptr)
		{
			Outside = VeyraChatLog::OutsideOf(Flow->GetClient().GetSnapshot());
		}
		const TArray<FVeyraChatLine> Lines = VeyraChatLog::Describe(Player.GetChat(), Outside, FPlatformTime::Seconds(), bComposing, Log, Side, [&Match](int32 PlayerId) {
			for (const APlayerState* Each : Match.PlayerArray)
			{
				const AVeyraPlayerState* Participant = Cast<AVeyraPlayerState>(Each);
				if (Participant && Participant->GetPlayerId() == PlayerId && Participant->GetVanguardId().IsValid())
				{
					return VeyraContentText::VanguardName(Participant->GetVanguardId()).ToString();
				}
			}
			return FString();
		});
		if (Lines.IsEmpty())
		{
			return;
		}
		const FSlateFontInfo Body = Paint.Font(TEXT("Regular"), Preferences.ChatFontSize);
		const FSlateFontInfo Head = Paint.Font(TEXT("Bold"), Preferences.ChatFontSize);
		// Before Slate measures (a server, a test), a row is its type's size and a little.
		constexpr float RowOverType = 1.35f;
		const float RowHeight = FMath::Max(static_cast<float>(Paint.Measure(TEXT("Ag"), Body).Y), Body.Size * RowOverType);
		const float Inset = Paint.S(Settings.DeckGap) / 2.0f;
		const float TextWidth = Frame.Width - Inset * 2.0f;

		// Each line's rows, top to bottom, then all of them upward from the log's bottom.
		struct FRow
		{
			FString Head;
			FString Text;
			FLinearColor HeadColor;
			FLinearColor TextColor;
			double Opacity = 1.0;
		};
		TArray<FRow> Rows;
		for (const FVeyraChatLine& Line : Lines)
		{
			const FLinearColor SideColor = Line.Side == EVeyraChatLineSide::Ally ? Preferences.SideColors.Ally
				: Line.Side == EVeyraChatLineSide::Enemy						 ? Preferences.SideColors.Enemy
				: Line.Side == EVeyraChatLineSide::Party						 ? Settings.PartyChatColor
				: Line.Side == EVeyraChatLineSide::Direct						 ? Settings.DirectChatColor
																				 : Settings.DescriptionColor;
			const FString LineHead = Line.Prefix + Line.Sender;
			const float HeadWidth = static_cast<float>(Paint.Measure(LineHead, Head).X);
			const TArray<FString> Wrapped = WrapAfter(Paint, Line.Text, Body, TextWidth - HeadWidth, TextWidth);
			for (int32 Index = 0; Index < Wrapped.Num(); ++Index)
			{
				Rows.Add({ Index == 0 ? LineHead : FString(), Wrapped[Index], SideColor,
					Line.Side == EVeyraChatLineSide::Notice ? Settings.DescriptionColor : Settings.TextColor, Line.Opacity });
			}
		}
		float Y = Frame.LogBottomLeft.Y - Rows.Num() * RowHeight - Inset;
		for (const FRow& Row : Rows)
		{
			const auto Faded = [&Row](const FLinearColor& Color) { return Color.CopyWithNewOpacity(Color.A * Row.Opacity); };
			if (Preferences.ChatBackdrop.A > 0.0f)
			{
				Paint.Rect(FVector2D(Frame.LogBottomLeft.X, Y), FVector2D(Frame.Width, RowHeight), Faded(Preferences.ChatBackdrop));
			}
			const float X = Frame.LogBottomLeft.X + Inset;
			float HeadWidth = 0.0f;
			if (!Row.Head.IsEmpty())
			{
				Paint.Text(FVector2D(X, Y), Row.Head, Head, Faded(Row.HeadColor), true);
				HeadWidth = static_cast<float>(Paint.Measure(Row.Head, Head).X);
			}
			Paint.Text(FVector2D(X + HeadWidth, Y), Row.Text, Body, Faded(Row.TextColor), true);
			Y += RowHeight;
		}
	}

	/**
	 * The hovered tile's tooltip over it (ADR-080 §2): a header with the icon, the title and its key, the detail, and the cooldown
	 * and cost; then the description; each fact labelled, coloured by kind, with how it is made beside it; an item's stats; and
	 * the next rank. Painted at the player's Interface Text Size.
	 */
	void DrawTooltip(const FPainter& Paint, const FHover& Hover)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const float Pad = Paint.S(Settings.DeckPadding);
		const float Width = Paint.S(Settings.TooltipWidth);
		const float Inner = Width - Pad * 2.0f;
		const float Gap = Paint.S(6.0f);
		const float Icon = Hover.Icon ? Paint.S(Settings.TooltipIconSize) : 0.0f;
		const float LabelWidth = Paint.S(Settings.TooltipLabelWidth);
		const FSlateFontInfo TitleFont = Paint.Font(TEXT("Bold"), Settings.HudHeadingFontSize);
		const FSlateFontInfo SmallFont = Paint.Font(TEXT("Regular"), Settings.HudSmallFontSize);
		const FSlateFontInfo LabelFont = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		const FSlateFontInfo BodyFont = Paint.Font(TEXT("Regular"), Settings.HudBodyFontSize);
		const FSlateFontInfo ValueFont = Paint.Font(TEXT("Bold"), Settings.HudBodyFontSize);
		const float SmallHeight = Paint.Measure(TEXT("Ag"), SmallFont).Y;
		const float BodyHeight = Paint.Measure(TEXT("Ag"), BodyFont).Y;
		const FLinearColor Muted = Settings.DescriptionColor;
		const auto ToneColor = [&Settings](EVeyraFactTone Tone) {
			switch (Tone)
			{
			case EVeyraFactTone::Physical:
				return Settings.CombatTextPhysicalColor;
			case EVeyraFactTone::Magic:
				return Settings.CombatTextMagicColor;
			case EVeyraFactTone::TrueDamage:
				return Settings.CombatTextTrueColor;
			case EVeyraFactTone::Harm:
				return Settings.TooltipHarmColor;
			case EVeyraFactTone::Help:
				return Settings.CombatTextHealingColor;
			default:
				return Settings.TextColor;
			}
		};

		// The header: the icon, the title with its key in a chip and the detail under it, and on the right the cooldown and cost.
		TArray<FString> Right;
		if (!Hover.Cooldown.IsEmpty())
		{
			Right.Add(Hover.Cooldown);
		}
		if (!Hover.Cost.IsEmpty())
		{
			Right.Add(Hover.Cost);
		}
		float RightWidth = 0.0f;
		for (const FString& Line : Right)
		{
			RightWidth = FMath::Max(RightWidth, static_cast<float>(Paint.Measure(Line, ValueFont).X));
		}
		const float TitleHeight = Paint.Measure(TEXT("Ag"), TitleFont).Y;
		const float HeaderText = TitleHeight + (Hover.Detail.IsEmpty() ? 0.0f : SmallHeight);
		const float HeaderHeight = FMath::Max3(Icon, HeaderText, Right.Num() * BodyHeight);

		// The description, then each fact: its label in a column, its value, and how it is made after it, wrapping under the value.
		const TArray<FString> Lines = Paint.Wrap(Hover.Description, BodyFont, Inner);
		struct FFactRows
		{
			FString Value;
			TArray<FString> Note;
		};
		TArray<FFactRows> FactRows;
		float FactsHeight = 0.0f;
		const float ValueRoom = Inner - LabelWidth;
		for (const FVeyraAbilityFact& Fact : Hover.Facts)
		{
			FFactRows& Rows = FactRows.AddDefaulted_GetRef();
			Rows.Value = Fact.Value;
			const float ValueWidth = Paint.Measure(Fact.Value + TEXT("  "), ValueFont).X;
			if (!Fact.Note.IsEmpty())
			{
				Rows.Note = WrapAfter(Paint, Fact.Note, SmallFont, ValueRoom - ValueWidth, ValueRoom);
			}
			FactsHeight += BodyHeight + FMath::Max(0, Rows.Note.Num() - 1) * SmallHeight;
		}
		// An item's stats in two columns. The header and the facts carry every number an ability's cast has, so none repeats.
		const int32 StatRows = (Hover.Stats.Num() + 1) / 2;
		const float RuleSpace = Gap * 2.0f;
		float Height = Pad * 2.0f + HeaderHeight + RuleSpace;
		Height += Lines.IsEmpty() ? 0.0f : BodyHeight * Lines.Num() + Gap;
		Height += FactsHeight > 0.0f ? FactsHeight + Gap : 0.0f;
		Height += StatRows > 0 ? BodyHeight * StatRows + Gap : 0.0f;
		Height += Hover.NextRank.IsEmpty() ? 0.0f : SmallHeight + Gap;
		Height -= Gap;

		// Over the hovered tile, kept on the screen.
		const float Left = FMath::Clamp(static_cast<float>(Hover.Anchor.X) - Width / 2.0f, 0.0f, FMath::Max(0.0f, Paint.Canvas.ClipX - Width));
		const float Top = FMath::Max(0.0f, static_cast<float>(Hover.Anchor.Y) - Height - Paint.S(Settings.DeckGap));
		const FVector2D TopLeft(Left, Top);
		Paint.Surface(TopLeft, FVector2D(Width, Height));

		const float X = Left + Pad;
		float Y = Top + Pad;
		if (Hover.Icon && Hover.Icon->GetResource())
		{
			// In its brass-edged tile, as on the deck (ADR-082 §4).
			DrawTileFrame(Paint, FVector2D(X, Y), Icon, true);
			DrawTileIcon(Paint, Hover.Icon, FVector2D(X, Y), Icon);
		}
		const float TextX = X + (Icon > 0.0f ? Icon + Gap * 2.0f : 0.0f);
		const float TextTop = Y + (HeaderHeight - HeaderText) / 2.0f;
		Paint.Text(FVector2D(TextX, TextTop), Hover.Title, TitleFont, Settings.TextColor);
		if (!Hover.Key.IsEmpty())
		{
			// The key in a chip beside the title, edged in the accent.
			const FVector2D KeySize = Paint.Measure(Hover.Key, LabelFont) + FVector2D(Paint.S(8.0f), Paint.S(2.0f));
			const FVector2D KeyAt(TextX + Paint.Measure(Hover.Title, TitleFont).X + Gap, TextTop + (TitleHeight - KeySize.Y) / 2.0f);
			Paint.Outline(KeyAt, KeySize, Settings.HudAccentColor);
			Paint.Text(KeyAt + FVector2D(Paint.S(4.0f), Paint.S(1.0f)), Hover.Key, LabelFont, Settings.HudAccentColor);
		}
		if (!Hover.Detail.IsEmpty())
		{
			Paint.Text(FVector2D(TextX, TextTop + TitleHeight), Hover.Detail, SmallFont, Muted);
		}
		float RightY = Y + (HeaderHeight - Right.Num() * BodyHeight) / 2.0f;
		for (const FString& Line : Right)
		{
			Paint.Text(FVector2D(Left + Width - Pad - Paint.Measure(Line, ValueFont).X, RightY), Line, ValueFont, Settings.TextColor);
			RightY += BodyHeight;
		}
		Y += HeaderHeight + Gap;
		Paint.Rect(FVector2D(X, Y), FVector2D(Inner, FMath::Max(1.0f, Paint.S(1.0f))), Settings.HudHairlineColor);
		Y += Gap;

		for (const FString& Line : Lines)
		{
			Paint.Text(FVector2D(X, Y), Line, BodyFont, Settings.TextColor.CopyWithNewOpacity(0.85f));
			Y += BodyHeight;
		}
		if (!Lines.IsEmpty())
		{
			Y += Gap;
		}
		for (int32 Index = 0; Index < Hover.Facts.Num(); ++Index)
		{
			const FVeyraAbilityFact& Fact = Hover.Facts[Index];
			const FFactRows& Rows = FactRows[Index];
			Paint.Text(FVector2D(X, Y + (BodyHeight - SmallHeight) / 2.0f), Fact.Label.ToString().ToUpper(), LabelFont, Muted);
			const float ValueX = X + LabelWidth;
			Paint.Text(FVector2D(ValueX, Y), Rows.Value, ValueFont, ToneColor(Fact.Tone));
			const float NoteX = ValueX + Paint.Measure(Rows.Value + TEXT("  "), ValueFont).X;
			for (int32 Row = 0; Row < Rows.Note.Num(); ++Row)
			{
				const float RowY = Row == 0 ? Y + (BodyHeight - SmallHeight) / 2.0f : Y + BodyHeight + (Row - 1) * SmallHeight;
				Paint.Text(FVector2D(Row == 0 ? NoteX : ValueX, RowY), Rows.Note[Row], SmallFont, Muted);
			}
			Y += BodyHeight + FMath::Max(0, Rows.Note.Num() - 1) * SmallHeight;
		}
		if (FactsHeight > 0.0f)
		{
			Y += Gap;
		}
		for (int32 Index = 0; Index < Hover.Stats.Num(); ++Index)
		{
			Paint.Text(FVector2D(X + (Index % 2) * Inner / 2.0f, Y + (Index / 2) * BodyHeight), Hover.Stats[Index], ValueFont, Settings.TextColor);
		}
		if (StatRows > 0)
		{
			Y += BodyHeight * StatRows + Gap;
		}
		if (!Hover.NextRank.IsEmpty())
		{
			Paint.Text(FVector2D(X, Y), Hover.NextRank, SmallFont, Muted);
		}
	}

	/** Color lit toward white by Share, its opacity kept: a fill's head. */
	FLinearColor Lit(const FLinearColor& Color, float Share)
	{
		return FMath::Lerp(Color, FLinearColor::White, Share).CopyWithNewOpacity(Color.A);
	}

	/** Color darkened to Share of itself, its opacity kept: a fill's foot. */
	FLinearColor Darkened(const FLinearColor& Color, float Share)
	{
		return FLinearColor(Color.R * Share, Color.G * Share, Color.B * Share, Color.A);
	}

	/**
	 * A bar filled Fill of the way in Color, lit at its head and deeper at its foot, with Label centred over it. Its lost part
	 * shows to Trail, held then draining (ADR-082 §3); shields follow the fill, and ticks mark each TickEvery of its length.
	 */
	void DrawBar(const FPainter& Paint, const FVector2D& TopLeft, const FVector2D& Size, double Fill, const FLinearColor& Color, const FString& Label,
		double ShieldFill = 0.0, double TickEvery = 0.0, double Trail = 0.0)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		Paint.Rect(TopLeft, Size, Settings.BarBackgroundColor);
		const double Shown = FMath::Clamp(Fill, 0.0, 1.0);
		const float Filled = Size.X * Shown;
		if (Trail > Shown)
		{
			Paint.Rect(TopLeft + FVector2D(Filled, 0.0f), FVector2D(Size.X * (FMath::Min(Trail, 1.0) - Shown), Size.Y), Settings.MotionTrailColor);
		}
		Paint.Gradient(TopLeft, FVector2D(Filled, Size.Y), Lit(Color, 0.22f), Darkened(Color, 0.72f));
		if (ShieldFill > 0.0)
		{
			Paint.Rect(TopLeft + FVector2D(Filled, 0.0f), FVector2D(Size.X * FMath::Clamp(ShieldFill, 0.0, 1.0 - Shown), Size.Y), Settings.ShieldColor);
		}
		// A light edge along the top gives the bar depth.
		Paint.Rect(TopLeft, FVector2D(Filled, FMath::Max(1.0f, Size.Y * 0.14f)), FLinearColor(1.0f, 1.0f, 1.0f, 0.22f));
		// A tick at each TickEvery of the bar's length, so its length reads as an amount (ADR-080 §4); none when they would crowd.
		constexpr float LeastTickSpacing = 3.0f;
		if (TickEvery > 0.0 && Size.X * TickEvery >= LeastTickSpacing)
		{
			const FVector2D TickSize(FMath::Max(1.0f, Paint.S(1.0f)), Size.Y * 0.6f);
			for (double At = TickEvery; At < 1.0; At += TickEvery)
			{
				Paint.Rect(TopLeft + FVector2D(Size.X * At, 0.0f), TickSize, FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
			}
		}
		Paint.Outline(TopLeft, Size, FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		if (!Label.IsEmpty())
		{
			Paint.TextCentred(TopLeft + Size / 2.0, Label, Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor, true);
		}
	}

	/**
	 * The player's own statuses in a row just above the deck at DeckTopLeft (Proposals 38, 43, 53; ADR-059 §4): the beneficial,
	 * then the harmful, as Display orders them, each chip marked so its group reads without colour. Those that do not fit
	 * across the deck are counted at the row's end. Standing in Dense Fog, which no status says, leads the row (ADR-068 §6).
	 */
	void DrawStatusRow(const FPainter& Paint, const FVeyraStatusDisplay& Display, const AVeyraPlayerState& Participant, double Now, const FVector2D& DeckTopLeft,
		float DeckWidth, bool bInDenseFog)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const TArray<FVeyraHudStatus> Statuses = VeyraHud::OrderOwnStatuses(VeyraHud::StatusesOf(Participant, Now, Participant.GetVeyraTeam()), Display.Sort);
		if (Statuses.IsEmpty() && !bInDenseFog)
		{
			return;
		}
		const FSlateFontInfo Font = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		const FVector2D Pad(Paint.S(6.0f), Paint.S(3.0f));
		const float Gap = Paint.S(Settings.DeckGap) / 2.0f;
		// Before Slate measures (a server, a test), a row is its type's size and a little.
		constexpr float RowOverType = 1.35f;
		const float TextHeight = FMath::Max(static_cast<float>(Paint.Measure(TEXT("Ag"), Font).Y), Font.Size * RowOverType);
		const float Height = TextHeight + Pad.Y * 2.0f;
		const float Y = DeckTopLeft.Y - Height - Gap;
		const float Right = DeckTopLeft.X + DeckWidth;
		float X = DeckTopLeft.X;
		int32 Unshown = 0;
		bool bHarmfulStarted = false;
		// Hidden from enemies outside the fog, as the body's veil shows; the player's to use, as a beneficial status is.
		if (bInDenseFog)
		{
			const FString Text = TEXT("In Dense Fog");
			const FVector2D Size(static_cast<float>(Paint.Measure(Text, Font).X) + Pad.X * 2.0f, Height);
			const FVector2D At(X, Y);
			if (Display.bHighContrast)
			{
				Paint.Rect(At, Size, Settings.HudSurfaceColor.CopyWithNewOpacity(1.0f));
				Paint.Outline(At, Size, Settings.FogVeilColor, Paint.S(2.0f));
			}
			else
			{
				Paint.Surface(At, Size);
				Paint.Outline(At, Size, Settings.FogVeilColor);
			}
			Paint.Text(At + Pad, Text, Font, Settings.TextColor, !Display.bHighContrast);
			X += Size.X + Gap;
		}
		for (const FVeyraHudStatus& Status : Statuses)
		{
			const bool bHarmful = Status.Group != EVeyraStatusGroup::Beneficial;
			// A wider gap where the harmful begin.
			if (bHarmful && !bHarmfulStarted && X > DeckTopLeft.X)
			{
				X += Gap * 2.0f;
			}
			bHarmfulStarted |= bHarmful;
			const FString Text = VeyraHud::StatusChipText(Status, Display.bDurations);
			const FVector2D Size(static_cast<float>(Paint.Measure(Text, Font).X) + Pad.X * 2.0f, Height);
			if (X + Size.X > Right)
			{
				++Unshown;
				continue;
			}
			const FVector2D At(X, Y);
			const FLinearColor Edge = bHarmful ? Settings.WarningColor : Settings.HudAccentColor;
			if (Display.bHighContrast)
			{
				Paint.Rect(At, Size, Settings.HudSurfaceColor.CopyWithNewOpacity(1.0f));
				Paint.Outline(At, Size, Edge, Paint.S(2.0f));
			}
			else
			{
				Paint.Surface(At, Size);
				Paint.Outline(At, Size, Edge.CopyWithNewOpacity(0.6f));
			}
			Paint.Text(At + Pad, Text, Font, Status.Group == EVeyraStatusGroup::CrowdControl ? Settings.WarningColor : Settings.TextColor, !Display.bHighContrast);
			X += Size.X + Gap;
		}
		if (Unshown > 0)
		{
			Paint.Text(FVector2D(X, Y + Pad.Y), FString::Printf(TEXT("+%d"), Unshown), Font, Settings.DescriptionColor, true);
		}
	}

	/** Stat's icon (ADR-082 §4) in a square Side across at At: its strokes, in its colour. */
	void DrawStatGlyph(const FPainter& Paint, const FString& Stat, const FVector2D& At, float Side)
	{
		const FLinearColor* Color = Paint.Settings.StatColors.Find(Stat);
		const float Units = Side / VeyraStatGlyphs::Size;
		const float Thickness = FMath::Max(1.0f, Paint.S(1.4f));
		for (const TArray<FVector2D>& Stroke : VeyraStatGlyphs::Of(Stat))
		{
			for (int32 Index = 1; Index < Stroke.Num(); ++Index)
			{
				Paint.Line(At + Stroke[Index - 1] * Units, At + Stroke[Index] * Units, Color ? *Color : Paint.Settings.DescriptionColor, Thickness);
			}
		}
	}

	/**
	 * The stats panel beside the portrait (ADR-080 §3): the core stats in two columns in Core, and with the rest in More above it
	 * while it is open, cutting in as it opens (ADR-082 §2), MoreOpened seconds ago. Each stat has its icon; one raised above
	 * the Vanguard's own value reads in brass; a chevron in the corner says the panel opens or closes on a click.
	 */
	void DrawStatsPanel(const FPainter& Paint, const FVeyraHudStats& Stats, const FBox2D& Core, const FBox2D& More, double MoreOpened)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const float Pad = Paint.S(Settings.DeckPadding);
		const float Row = Paint.S(Settings.StatsRowHeight);
		const FSlateFontInfo NameFont = Paint.Font(TEXT("Regular"), Settings.HudSmallFontSize);
		const FSlateFontInfo ValueFont = Paint.Font(TEXT("Bold"), Settings.HudBodyFontSize);
		const float NameHeight = Paint.Measure(TEXT("Ag"), NameFont).Y;
		const float ValueHeight = Paint.Measure(TEXT("Ag"), ValueFont).Y;
		const float Icon = Paint.S(14.0f);
		const auto Grid = [&](const FPainter& Painter, const FBox2D& Box, TConstArrayView<FVeyraHudStat> Shown, float Top) {
			const float ColumnWidth = (static_cast<float>(Box.GetSize().X) - Pad * 3.0f) / 2.0f;
			for (int32 Index = 0; Index < Shown.Num(); ++Index)
			{
				const FVeyraHudStat& Stat = Shown[Index];
				const float X = static_cast<float>(Box.Min.X) + Pad + (Index % 2) * (ColumnWidth + Pad);
				const float Y = Top + (Index / 2) * Row;
				DrawStatGlyph(Painter, Stat.Key, FVector2D(X, Y + (Row - Icon) / 2.0f), Icon);
				const float NameX = X + Icon + Painter.S(6.0f);
				Painter.Text(FVector2D(NameX, Y + (Row - NameHeight) / 2.0f), Stat.Name.ToString(), NameFont, Settings.DescriptionColor);
				const float ValueWidth = Painter.Measure(Stat.Value, ValueFont).X;
				Painter.Text(FVector2D(X + ColumnWidth - ValueWidth, Y + (Row - ValueHeight) / 2.0f), Stat.Value, ValueFont, Stat.bRaised ? Settings.GoldColor : Settings.TextColor);
			}
		};
		Paint.Surface(Core.Min, Core.GetSize());
		Grid(Paint, Core, Stats.Core, static_cast<float>(Core.Min.Y) + Pad);
		// The chevron: up to open the more stats above, down to close them.
		const bool bOpen = MoreOpened >= 0.0;
		const float Chevron = Paint.S(5.0f);
		const FVector2D Tip(static_cast<float>(Core.Max.X) - Pad - Chevron, static_cast<float>(Core.Min.Y) + Paint.S(7.0f) + (bOpen ? Chevron / 2.0f : 0.0f));
		const float Rise = bOpen ? -Chevron / 2.0f : Chevron / 2.0f;
		const float Thickness = FMath::Max(1.0f, Paint.S(1.5f));
		Paint.Line(Tip + FVector2D(-Chevron, Rise), Tip, Settings.HudAccentColor, Thickness);
		Paint.Line(Tip, Tip + FVector2D(Chevron, Rise), Settings.HudAccentColor, Thickness);
		if (!bOpen)
		{
			return;
		}
		const FPainter Opening = Paint.CuttingIn(MoreOpened);
		Opening.Surface(More.Min, More.GetSize());
		const FSlateFontInfo LabelFont = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		Opening.Text(FVector2D(static_cast<float>(More.Min.X) + Pad, static_cast<float>(More.Min.Y) + Pad + (Row - Paint.Measure(TEXT("Ag"), LabelFont).Y) / 2.0f),
			TEXT("MORE"), LabelFont, Settings.DescriptionColor);
		Grid(Opening, More, Stats.More, static_cast<float>(More.Min.Y) + Pad + Row);
	}

	/** A small chamfered plate at TopLeft, filled in Fill and edged in Edge: a level's badge, a key's chip. */
	void DrawPlate(const FPainter& Paint, const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Fill, const FLinearColor& Edge)
	{
		const float Cut = FMath::Min(Paint.S(4.0f), static_cast<float>(FMath::Min(Size.X, Size.Y)) / 3.0f);
		const TArray<FVector2D> Points = FPainter::Chamfered(TopLeft, Size, Cut);
		Paint.Polygon(Points, Fill);
		const float Thickness = FMath::Max(1.0f, Paint.S(1.0f));
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			Paint.Line(Points[Index], Points[(Index + 1) % Points.Num()], Edge, Thickness);
		}
	}

	/** A Gold coin of Radius at Centre: a brass disc with its inner ring. */
	void DrawCoin(const FPainter& Paint, const FVector2D& Centre, float Radius)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		Paint.Polygon(FPainter::Circle(Centre, Radius), Lit(Settings.GoldColor, 0.25f));
		Paint.Ring(Centre, Radius * 0.62f, 0.3f, Darkened(Settings.GoldColor, 0.45f));
	}

	/** Key, a motion's name for one of a row's parts: Ready1, Ready2 and so on. */
	FName MotionKey(const TCHAR* Kind, int32 Index)
	{
		return FName(Kind, Index + 1);
	}

	/**
	 * The deck along the bottom, measured as Deck and placed at TopLeft, each section at its own scale (ADR-059 §1), in the
	 * client's look and moving (ADR-082 §3-§4); returns its top edge, and what the cursor rests on.
	 */
	float DrawDeck(const FPainter& Paint, const FVeyraDeckGeometry& Deck, const FVector2D& TopLeft, const FVeyraCooldownDisplay& Cooldowns,
		const FVeyraStatusDisplay& Statuses, const AVeyraPlayerState& Participant, const UVeyraInputSettings& Input, const FString& ShopKey,
		const TOptional<FVector2D>& Cursor, double Now, float RankMarkOpacity, bool bInDenseFog, TOptional<FHover>& Hover, FVeyraHudStats& OutStats)
	{
		TOptional<FVector2D> Mouse = Cursor;
#if !UE_BUILD_SHIPPING
		// A tile pinned for a capture is hovered at its centre.
		if (const FString Pinned = CVarDevHudHover.GetValueOnGameThread(); !Pinned.IsEmpty())
		{
			const FVector2D Row = TopLeft + Deck.AbilitiesAt;
			const TCHAR* Keys[] = { TEXT("Q"), TEXT("W"), TEXT("E"), TEXT("R") };
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
			{
				if (Pinned.Equals(Keys[Index], ESearchCase::IgnoreCase))
				{
					Mouse = Row + FVector2D(Deck.Passive + Deck.AbilityGap + Index * (Deck.Ability + Deck.AbilityGap) + Deck.Ability / 2.0f, Deck.Ability / 2.0f);
				}
			}
			if (Pinned.Equals(TEXT("Passive"), ESearchCase::IgnoreCase))
			{
				Mouse = Row + FVector2D(Deck.Passive / 2.0f, Deck.Ability / 2.0f);
			}
			int32 ItemNumber = 0;
			if (Pinned.StartsWith(TEXT("Item"), ESearchCase::IgnoreCase) && LexTryParseString(ItemNumber, *Pinned.RightChop(4)) && ItemNumber >= 1)
			{
				const int32 Index = ItemNumber - 1;
				Mouse = TopLeft + Deck.ItemsAt + FVector2D((Index % 3) * (Deck.Item + Deck.ItemGap) + Deck.Item / 2.0f, (Index / 3) * (Deck.Item + Deck.ItemGap) + Deck.Item / 2.0f);
			}
		}
#endif
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		FVeyraHudMotion* Motion = Paint.Motion;
		// What the HUD remembers between frames (ADR-082 §2); still where there is no memory, as a test's canvas.
		const auto OnRise = [Motion](FName Key, bool bOn) { return Motion ? Motion->OnRise(Key, bOn) : FVeyraHudMotion::LongAgo; };
		const auto Count = [Motion, &Settings](FName Key, double Value) {
			return Motion ? Motion->Count(Key, Value, Settings.MotionCountSeconds, Settings.MotionSmear) : FVeyraHudMotion::FCount{ Value, FVeyraHudMotion::LongAgo, false };
		};
		const FVeyraHudPlayer Player = VeyraHud::DescribePlayer(Participant, Now);
		OutStats = Player.Stats;
		const FPainter Frame = Paint.At(Deck.Scales.Base);
		const FPainter Bar = Paint.At(Deck.Scales.AbilityBar);
		const FPainter Vitals = Paint.At(Deck.Scales.Vitals);
		const FPainter Spells = Paint.At(Deck.Scales.Spells);
		const FPainter Items = Paint.At(Deck.Scales.Items);
		Frame.Surface(TopLeft, Deck.Size);

		// The portrait, round, in a ring that fills with XP, and the level on a brass plate at its foot (ADR-082 §4).
		const float Portrait = Deck.Portrait;
		const FVector2D PortraitAt = TopLeft + Deck.PortraitAt;
		const FVector2D PortraitCentre = PortraitAt + FVector2D(Portrait / 2.0f);
		const float Radius = Portrait / 2.0f;
		const float RingWidth = Bar.S(4.0f);
		const double Xp = Player.ExperienceToNextLevel > 0 ? static_cast<double>(Player.Experience) / Player.ExperienceToNextLevel : 1.0;
		const FVeyraHudMotion::FCount Level = Count(TEXT("Level"), Player.Level);
		// The ring runs to its new share, and empties into the next level rather than running back.
		const double XpShown = Level.SinceChange < Settings.MotionCountSeconds ? Xp : Count(TEXT("Xp"), Xp).Shown;
		Bar.Polygon(FPainter::Circle(PortraitCentre, Radius), Settings.BarBackgroundColor);
		Bar.Arc(PortraitCentre, Radius - RingWidth, Radius, 0.0, 1.0, FLinearColor(1.0f, 1.0f, 1.0f, 0.1f));
		Bar.Arc(PortraitCentre, Radius - RingWidth, Radius, 0.0, XpShown, Settings.HudAccentColor);
		DrawRoundFace(Bar, Player.Vanguard, PortraitCentre, Radius - RingWidth - Bar.S(1.5f), Settings.HudClockFontSize);
		const FSlateFontInfo LevelFont = Bar.Font(TEXT("Bold"), Settings.HudBodyFontSize);
		const FVector2D LevelSize(Bar.S(28.0f), Bar.S(20.0f));
		const FVector2D LevelAt(PortraitCentre.X - LevelSize.X / 2.0f, PortraitAt.Y + Portrait - LevelSize.Y / 2.0f - Bar.S(2.0f));
		const bool bLevelling = Level.bRose && Level.SinceChange < Settings.MotionBurstSeconds;
		DrawPlate(Bar, LevelAt, LevelSize, Settings.HudSurfaceColor.CopyWithNewOpacity(0.96f), bLevelling ? Settings.MotionHotColor : Settings.TileReadyTopColor);
		Bar.TextCentred(LevelAt + LevelSize / 2.0, FString::FromInt(Player.Level), LevelFont, Settings.TextColor);
		// A level comes: the ring flashes outward and the plate bursts.
		if (Level.bRose && Level.SinceChange < Settings.MotionBurstSeconds)
		{
			const double Out = FPainter::Eased(Settings.MotionSnap, Level.SinceChange, Settings.MotionBurstSeconds);
			Bar.Ring(PortraitCentre, Radius * static_cast<float>(1.0 + 0.35 * Out), 0.12f, Settings.GoldColor.CopyWithNewOpacity(static_cast<float>(1.0 - Out)), true);
			Bar.Burst(LevelAt + LevelSize / 2.0, Level.SinceChange, Bar.S(Settings.MotionBurstRadius), Settings.GoldColor, static_cast<uint32>(Player.Level));
		}
		if (Contains(PortraitAt, FVector2D(Portrait), Mouse))
		{
			FString Title = VeyraContentText::VanguardName(Player.Vanguard).ToString();
			Hover = FHover{ Title, FString::Printf(TEXT("Level %d   XP %d / %d"), Player.Level, Player.Experience, Player.ExperienceToNextLevel),
				VeyraContentText::VanguardTitle(Player.Vanguard).ToString() };
			Hover->Anchor = FVector2D(PortraitAt.X + Portrait / 2.0f, PortraitAt.Y);
		}

		// The passive, then Q W E R.
		const float Ability = Deck.Ability;
		const float Small = Deck.Passive;
		const float Gap = Deck.AbilityGap;
		const float Pip = Deck.Pip;
		float X = TopLeft.X + Deck.AbilitiesAt.X;
		const float RowTop = TopLeft.Y + Deck.AbilitiesAt.Y;
		const FVector2D PassiveAt(X, RowTop + (Ability - Small) / 2.0f);
		const FString PassiveName = Player.Passive.IsValid() ? VeyraContentText::PassiveName(Player.Passive).ToString() : FString();
		DrawTileFrame(Bar, PassiveAt, Small, false);
		if (!DrawTileIcon(Bar, VeyraShellArt::AbilityIconOf(Player.Passive.ToString()), PassiveAt, Small))
		{
			Bar.TextCentred(PassiveAt + FVector2D(Small / 2.0f), Monogram(PassiveName), Bar.Font(TEXT("Bold"), Settings.HudBodyFontSize), Settings.DescriptionColor);
		}
		if (Player.Passive.IsValid() && Contains(PassiveAt, FVector2D(Small), Mouse))
		{
			Hover = FHover{ PassiveName, TEXT("Passive"), VeyraContentText::PassiveDescription(Player.Passive).ToString() };
			Hover->Icon = VeyraShellArt::AbilityIconOf(Player.Passive.ToString());
			Hover->Anchor = FVector2D(PassiveAt.X + Small / 2.0f, PassiveAt.Y);
		}
		X += Small + Gap;
		const FSlateFontInfo NameFont = Bar.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		const FSlateFontInfo CostFont = Bar.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		for (int32 SlotIndex = 0; SlotIndex < Player.Slots.Num(); ++SlotIndex)
		{
			const FVeyraHudSlot& Slot = Player.Slots[SlotIndex];
			const FVector2D At(X, RowTop);
			const bool bLearned = Slot.Rank > 0;
			const FString Name = Slot.Ability.IsValid() ? VeyraContentText::AbilityName(Slot.Ability).ToString() : FString();
			const bool bCooled = bLearned && Slot.CooldownSeconds <= 0.0;
			const bool bReady = bCooled && Slot.bAffordable;
			const bool bEmpowered = Slot.EmpoweredSeconds > 0.0;
			const bool bHovered = Slot.Ability.IsValid() && Contains(At, FVector2D(Ability), Mouse);
			// The frame: brass while ready, dim while not, the empowered colour while its next attack is empowered.
			DrawTileFrame(Bar, At, Ability, bReady, bEmpowered ? TOptional<FLinearColor>(Settings.EmpoweredColor) : TOptional<FLinearColor>());
			if (bEmpowered)
			{
				Bar.Glow(At + FVector2D(Ability / 2.0f), Ability * 0.85f, Settings.EmpoweredColor.CopyWithNewOpacity(0.35f * RankMarkOpacity));
			}
			// Its icon, dimmed while it waits or is not learned.
			const FLinearColor IconTint = bReady || (bCooled && !Slot.bAffordable) ? FLinearColor::White : Settings.TileDimTint;
			if (!DrawTileIcon(Bar, VeyraShellArt::SlotIconOf(Slot.Ability.ToString(), Slot.OwnAbility.ToString()), At, Ability, IconTint))
			{
				// Until it has an icon, its name.
				float LineY = At.Y + Ability / 2.0f;
				const TArray<FString> Lines = Bar.Wrap(Name, NameFont, Ability - Bar.S(8.0f), 2);
				const float LineHeight = Bar.Measure(TEXT("Ag"), NameFont).Y;
				LineY -= LineHeight * Lines.Num() / 2.0f - LineHeight / 2.0f;
				for (const FString& Line : Lines)
				{
					Bar.TextCentred(FVector2D(At.X + Ability / 2.0f, LineY), Line, NameFont, bLearned ? Settings.TextColor : Settings.DescriptionColor);
					LineY += LineHeight;
				}
			}
			const float Inset = IconInset(Bar);
			DrawCooldown(Bar, Cooldowns, At + FVector2D(Inset), Ability - Inset * 2.0f, bLearned ? Slot.CooldownSeconds : 0.0, Slot.CooldownTotal);
			// Off cooldown but more than its owner holds: darkened in its resource's colour with its cost over it, so colour is
			// never the only sign (ADR-066 §1).
			if (bCooled && !Slot.bAffordable)
			{
				const FLinearColor Resource = Settings.ResourceColorOf(Player.Vitals.Family);
				const TArray<FVector2D> Inside = FPainter::Chamfered(At + FVector2D(Inset), FVector2D(Ability - Inset * 2.0f), TileCut(Bar, Ability));
				Bar.Polygon(Inside, Settings.ShadeColor);
				Bar.Polygon(Inside, Resource.CopyWithNewOpacity(Settings.UnaffordableTintOpacity));
				Bar.TextCentred(At + FVector2D(Ability / 2.0f), FString::FromInt(FMath::CeilToInt(Slot.Cost)), Bar.Font(TEXT("Bold"), Settings.HudHeadingFontSize), Settings.TextColor, true);
			}
			else if (bLearned && Slot.Cost > 0.0)
			{
				// Its cost in the corner, in the resource's light.
				const FString Cost = FString::FromInt(FMath::CeilToInt(Slot.Cost));
				const FVector2D CostSize = Bar.Measure(Cost, CostFont);
				Bar.Text(At + FVector2D(Ability - CostSize.X - Bar.S(4.0f), Ability - CostSize.Y - Bar.S(1.0f)), Cost, CostFont, Settings.TileCostColor, true);
			}
			// Its moments: ready, and used (ADR-082 §3).
			DrawTileMoments(Bar, At, Ability, OnRise(MotionKey(TEXT("Ready"), SlotIndex), bReady), OnRise(MotionKey(TEXT("Used"), SlotIndex), bLearned && Slot.CooldownSeconds > 0.0),
				static_cast<uint32>(SlotIndex * 37 + 11));
			if (bHovered)
			{
				const TArray<FVector2D> Edge = FPainter::Chamfered(At, FVector2D(Ability), TileCut(Bar, Ability));
				for (int32 Index = 0; Index < Edge.Num(); ++Index)
				{
					Bar.Line(Edge[Index], Edge[(Index + 1) % Edge.Num()], Settings.MotionHotColor.CopyWithNewOpacity(0.7f), FMath::Max(1.0f, Bar.S(1.0f)));
				}
			}
			Bar.KeyCap(At, KeyName(Input.GetAbilityKey(Slot.Slot)));
			// Its ranks as brass pips under it; a new rank flashes.
			const FVeyraHudMotion::FCount Ranked = Count(MotionKey(TEXT("Rank"), SlotIndex), Slot.Rank);
			const float PipGap = Bar.S(2.0f);
			const float PipWidth = Slot.MaxRank > 0 ? (Ability - (Slot.MaxRank - 1) * PipGap) / Slot.MaxRank : 0.0f;
			for (int32 Rank = 0; Rank < Slot.MaxRank; ++Rank)
			{
				const FVector2D PipAt(At.X + Rank * (PipWidth + PipGap), At.Y + Ability + Gap / 2.0f);
				Bar.Rect(PipAt, FVector2D(PipWidth, Pip), Rank < Slot.Rank ? Settings.TileReadyTopColor : FLinearColor(1.0f, 1.0f, 1.0f, 0.15f));
				if (Rank == Slot.Rank - 1 && Ranked.bRose && Ranked.SinceChange < Settings.MotionBurstSeconds)
				{
					const FVector2D PipCentre = PipAt + FVector2D(PipWidth, Pip) / 2.0;
					Bar.Glow(PipCentre, PipWidth, Settings.MotionHotColor.CopyWithNewOpacity(static_cast<float>(1.0 - Ranked.SinceChange / Settings.MotionBurstSeconds)));
					Bar.Burst(PipCentre, Ranked.SinceChange, Bar.S(Settings.MotionBurstRadius) * 0.6f, Settings.GoldColor, static_cast<uint32>(SlotIndex * 13 + Slot.Rank));
				}
			}
			// A skill point to spend on it: a lit brass plate above the tile, pulsing while it waits (ADR-065 §5).
			if (Slot.bCanRankUp)
			{
				const FVector2D MarkSize(Bar.S(22.0f), Bar.S(16.0f));
				const FVector2D MarkAt(At.X + (Ability - MarkSize.X) / 2.0f, At.Y - MarkSize.Y - Bar.S(4.0f));
				Bar.Glow(MarkAt + MarkSize / 2.0, MarkSize.X, Settings.TileReadyTopColor.CopyWithNewOpacity(0.35f * RankMarkOpacity));
				DrawPlate(Bar, MarkAt, MarkSize, Settings.TileReadyTopColor.CopyWithNewOpacity(RankMarkOpacity), Settings.MotionHotColor.CopyWithNewOpacity(RankMarkOpacity));
				Bar.TextCentred(MarkAt + MarkSize / 2.0, TEXT("+"), Bar.Font(TEXT("Bold"), Settings.HudBodyFontSize), Settings.HudSurfaceColor.CopyWithNewOpacity(1.0f));
			}
			if (bHovered)
			{
				// An unlearned ability shows its first rank's numbers, so its detail says it is not yet learned.
				FString Detail = Slot.Rank > 0 ? FString::Printf(TEXT("Rank %d of %d"), Slot.Rank, Slot.MaxRank) : FString(TEXT("Not learned"));
				if (Slot.bCanRankUp)
				{
					Detail += FString::Printf(TEXT("   %s+%s to rank up"), *KeyName(Input.RankUpModifierKey), *KeyName(Input.GetAbilityKey(Slot.Slot)));
				}
				if (bEmpowered)
				{
					Detail += FString::Printf(TEXT("   next attack empowered %.1f s"), Slot.EmpoweredSeconds);
				}
				Hover = FHover{ Name, Detail, VeyraContentText::AbilityDescription(Slot.Ability).ToString() };
				// Its facts at its rank with the player's power now, and its wait after Ability Haste (ADR-080 §2).
				Hover->Icon = VeyraShellArt::SlotIconOf(Slot.Ability.ToString(), Slot.OwnAbility.ToString());
				Hover->Key = KeyName(Input.GetAbilityKey(Slot.Slot));
				Hover->Cooldown = Slot.Summary.Cooldown.IsEmpty() ? FString() : Slot.Summary.Cooldown + TEXT(" cooldown");
				Hover->Cost = Slot.Summary.Cost;
				Hover->Facts = Slot.Summary.Facts;
				Hover->NextRank = Slot.Summary.NextRank.IsEmpty() ? FString() : FString::Printf(TEXT("Next rank: %s"), *Slot.Summary.NextRank);
				Hover->Anchor = FVector2D(At.X + Ability / 2.0f, At.Y);
			}
			X += Ability + Gap;
		}

		// Health and the resource under the abilities, as wide as they are: health's lost part trails behind it (ADR-082 §3).
		const FVector2D BarsAt = TopLeft + Deck.VitalsAt;
		const float BarsWidth = Deck.AbilityRow;
		const double Total = FMath::Max(Player.Vitals.MaxHealth, Player.Vitals.Health + Player.Vitals.Shield);
		const double HealthShare = Total > 0.0 ? Player.Vitals.Health / Total : 0.0;
		const double Trail = Motion ? Motion->Trail(TEXT("Health"), HealthShare, Settings.MotionTrailHoldSeconds, Settings.MotionTrailDrainSeconds, Settings.MotionSmear) : HealthShare;
		const FString HealthLabel = Player.Vitals.Shield > 0.0
			? FString::Printf(TEXT("%.0f / %.0f  +%.0f"), Player.Vitals.Health, Player.Vitals.MaxHealth, Player.Vitals.Shield)
			: FString::Printf(TEXT("%.0f / %.0f"), Player.Vitals.Health, Player.Vitals.MaxHealth);
		DrawBar(Vitals, BarsAt, FVector2D(BarsWidth, Deck.Health), HealthShare, Settings.HealthColor, HealthLabel, Total > 0.0 ? Player.Vitals.Shield / Total : 0.0,
			Total > 0.0 ? Settings.HealthTickAmount / Total : 0.0, Trail);
		if (Player.Vitals.MaxResource > 0.0)
		{
			const double ResourceShare = Player.Vitals.Resource / Player.Vitals.MaxResource;
			const double ResourceTrail = Motion ? Motion->Trail(TEXT("Resource"), ResourceShare, Settings.MotionTrailHoldSeconds, Settings.MotionTrailDrainSeconds, Settings.MotionSmear) : ResourceShare;
			DrawBar(Vitals, BarsAt + FVector2D(0.0f, Deck.Health + Deck.BarSpacing), FVector2D(BarsWidth, Deck.Resource), ResourceShare, Settings.ResourceColorOf(Player.Vitals.Family),
				FString::Printf(TEXT("%.0f / %.0f"), FMath::FloorToDouble(Player.Vitals.Resource), Player.Vitals.MaxResource), 0.0, 0.0, ResourceTrail);
		}

		// A brass rule between the abilities and the Flux Spells.
		const float RuleX = TopLeft.X + Deck.SpellsAt.X - Deck.Gap / 2.0f;
		Frame.Rect(FVector2D(RuleX, TopLeft.Y + Deck.Pad), FVector2D(FMath::Max(1.0f, Frame.S(1.0f)), Deck.Size.Y - Deck.Pad * 2.0f), Settings.TileReadyTopColor.CopyWithNewOpacity(0.2f));

		// The Flux Spells and, under them, the vision tool.
		X = TopLeft.X + Deck.SpellsAt.X;
		const float SpellTop = TopLeft.Y + Deck.SpellsAt.Y;
		for (int32 SpellIndex = 0; SpellIndex < Player.Spells.Num(); ++SpellIndex)
		{
			const FVeyraHudSpellSlot& Spell = Player.Spells[SpellIndex];
			const FVector2D At(X, SpellTop);
			const FString Name = Spell.Spell.IsValid() ? VeyraContentText::AbilityName(Spell.Spell).ToString() : FString();
			const bool bReady = !Spell.bLocked && Spell.Spell.IsValid() && Spell.CooldownSeconds <= 0.0;
			DrawTileFrame(Spells, At, Deck.Spell, bReady);
			if (!DrawTileIcon(Spells, VeyraShellArt::AbilityIconOf(Spell.Spell.ToString()), At, Deck.Spell, bReady ? FLinearColor::White : Settings.TileDimTint))
			{
				Spells.TextCentred(At + FVector2D(Deck.Spell / 2.0f), Monogram(Name), Spells.Font(TEXT("Bold"), Settings.HudBodyFontSize), Settings.TextColor);
			}
			const float Inset = IconInset(Spells);
			if (Spell.bLocked)
			{
				Spells.Polygon(FPainter::Chamfered(At + FVector2D(Inset), FVector2D(Deck.Spell - Inset * 2.0f), TileCut(Spells, Deck.Spell)), Settings.ShadeColor);
			}
			DrawCooldown(Spells, Cooldowns, At + FVector2D(Inset), Deck.Spell - Inset * 2.0f, Spell.bLocked ? 0.0 : Spell.CooldownSeconds, Spell.CooldownTotal);
			DrawTileMoments(Spells, At, Deck.Spell, OnRise(MotionKey(TEXT("SpellReady"), SpellIndex), bReady),
				OnRise(MotionKey(TEXT("SpellUsed"), SpellIndex), !Spell.bLocked && Spell.CooldownSeconds > 0.0), static_cast<uint32>(SpellIndex * 53 + 29));
			Spells.KeyCap(At, KeyName(Input.GetAbilityKey(Spell.Slot)));
			if (Spell.Spell.IsValid() && Contains(At, FVector2D(Deck.Spell), Mouse))
			{
				const FString Detail = Spell.bLocked ? FString::Printf(TEXT("Flux Spell   locked until %.0f permanent Team Flux"), Spell.UnlockFlux) : FString(TEXT("Flux Spell"));
				Hover = FHover{ Name, Detail, VeyraContentText::AbilityDescription(Spell.Spell).ToString() };
				Hover->Icon = VeyraShellArt::AbilityIconOf(Spell.Spell.ToString());
				Hover->Key = KeyName(Input.GetAbilityKey(Spell.Slot));
				Hover->Anchor = FVector2D(At.X + Deck.Spell / 2.0f, At.Y);
			}
			X += Deck.Spell + Deck.SpellGap;
		}
		if (Player.VisionTool.bPresent)
		{
			// The vision tool: its key on a chip, a diamond in the accent, and its charges or its wait.
			const bool bWard = Player.VisionTool.Tool == EVeyraVisionTool::PersistentWard;
			const FString Label = bWard ? FString::Printf(TEXT("WARD %d/%d"), Player.VisionTool.WardCharges, Player.VisionTool.MaxWardCharges)
										: FString(Player.VisionTool.Tool == EVeyraVisionTool::Sweeper ? TEXT("SWEEPER") : TEXT("QUICK SIGHT"));
			const double Waiting = bWard ? Player.VisionTool.NextChargeSeconds : Player.VisionTool.CooldownSeconds;
			const FString Shown = Waiting > 0.0 ? FString::Printf(TEXT("%s  %ss"), *Label, *VeyraHud::CooldownLabel(Waiting, Cooldowns.bTenths)) : Label;
			const FSlateFontInfo ToolFont = Spells.Font(TEXT("Bold"), Settings.HudSmallFontSize);
			const FString Key = KeyName(Input.GetAbilityKey(EVeyraAbilitySlot::VisionTool));
			const FVector2D KeySize = Spells.Measure(Key, ToolFont) + FVector2D(Spells.S(8.0f), Spells.S(2.0f));
			const float Diamond = Spells.S(5.0f);
			const float Width = KeySize.X + Spells.S(6.0f) + Diamond * 2.0f + Spells.S(5.0f) + Spells.Measure(Shown, ToolFont).X;
			const FVector2D LineAt = TopLeft + Deck.ToolAt + FVector2D((Deck.SpellsWidth - Width) / 2.0f, (Deck.ToolLine - KeySize.Y) / 2.0f);
			DrawPlate(Spells, LineAt, KeySize, Settings.HudSurfaceColor.CopyWithNewOpacity(0.9f), FLinearColor(1.0f, 1.0f, 1.0f, 0.25f));
			Spells.TextCentred(LineAt + KeySize / 2.0, Key, ToolFont, Settings.TextColor);
			const FVector2D DiamondCentre = LineAt + FVector2D(KeySize.X + Spells.S(6.0f) + Diamond, KeySize.Y / 2.0f);
			const TArray<FVector2D> Gem{ DiamondCentre + FVector2D(0.0f, -Diamond * 1.3f), DiamondCentre + FVector2D(Diamond, 0.0f), DiamondCentre + FVector2D(0.0f, Diamond * 1.3f),
				DiamondCentre + FVector2D(-Diamond, 0.0f) };
			Spells.Polygon(Gem, Waiting > 0.0 ? Settings.TileDimTint : Settings.HudAccentColor);
			Spells.Text(FVector2D(DiamondCentre.X + Diamond + Spells.S(5.0f), LineAt.Y + (KeySize.Y - Spells.Measure(TEXT("Ag"), ToolFont).Y) / 2.0f), Shown, ToolFont,
				Settings.DescriptionColor);
		}

		// The items, two rows of three, with Gold and the shop under them.
		const float ItemsLeft = TopLeft.X + Deck.ItemsAt.X;
		const float ItemsTop = TopLeft.Y + Deck.ItemsAt.Y;
		const float Item = Deck.Item;
		for (int32 Index = 0; Index < Player.Items.Num(); ++Index)
		{
			const FVeyraHudItemSlot& Held = Player.Items[Index];
			const FVector2D At(ItemsLeft + (Index % 3) * (Item + Deck.ItemGap), ItemsTop + (Index / 3) * (Item + Deck.ItemGap));
			// An item whose Active is ready shows ready, as an ability does.
			const bool bReady = Held.Item.IsValid() && Held.bActive && Held.CooldownSeconds <= 0.0;
			DrawTileFrame(Items, At, Item, bReady);
			const float Inset = IconInset(Items);
			if (!Held.Item.IsValid())
			{
				Items.Polygon(FPainter::Chamfered(At + FVector2D(Inset), FVector2D(Item - Inset * 2.0f), TileCut(Items, Item)), Settings.BarBackgroundColor);
			}
			else
			{
				const FString Name = VeyraContentText::ItemName(Held.Item).ToString();
				if (!DrawTileIcon(Items, VeyraShellArt::ItemIconOf(Held.Item.ToString()), At, Item))
				{
					Items.TextCentred(At + FVector2D(Item / 2.0f), Monogram(Name), Items.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor);
				}
				// A stack shows how many; a refillable consumable its charges, even none (ADR-023 §6).
				if (Held.Charges.IsSet() || Held.Count > 1)
				{
					const FString Stack = FString::FromInt(Held.Charges.Get(Held.Count));
					const FSlateFontInfo StackFont = Items.Font(TEXT("Bold"), Settings.HudSmallFontSize);
					const FVector2D StackSize = Items.Measure(Stack, StackFont);
					Items.Text(At + FVector2D(Item - StackSize.X - Items.S(3.0f), Item - StackSize.Y - Items.S(1.0f)), Stack, StackFont, Settings.TextColor, true);
				}
				// A Quest Item shows how far its quest has come (ADR-025 §3); an item that keeps Current or
				// Reserve shows them, Current above and Reserve below (ADR-025 §7).
				if (Held.Quest.IsSet())
				{
					Items.Text(At + FVector2D(Items.S(2.0f), Items.S(1.0f)), FString::Printf(TEXT("%d/%d"), Held.Quest->X, Held.Quest->Y),
						Items.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor, true);
				}
				else if (Held.Current.IsSet())
				{
					Items.Text(At + FVector2D(Items.S(2.0f), Items.S(1.0f)), FString::Printf(TEXT("C%d"), Held.Current.GetValue()),
						Items.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor, true);
				}
				if (Held.Reserve.IsSet())
				{
					Items.Text(At + FVector2D(Items.S(2.0f), Item - Items.S(16.0f)), FString::Printf(TEXT("R%d"), Held.Reserve.GetValue()),
						Items.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor, true);
				}
				DrawCooldown(Items, Cooldowns, At + FVector2D(Inset), Item - Inset * 2.0f, Held.CooldownSeconds, Held.CooldownTotal);
				if (Contains(At, FVector2D(Item), Mouse))
				{
					Hover = FHover{ Name, TEXT("Item"), VeyraContentText::ItemDescription(Held.Item).ToString() };
					// Its stats as the shop words them, then what it does (ADR-080 §2; Settings Bible 97: stats before effects).
					Hover->Icon = VeyraShellArt::ItemIconOf(Held.Item.ToString());
					Hover->Key = KeyName(Input.GetAbilityKey(Held.Slot));
					if (const FVeyraItemDefinition* Definition = UVeyraItemsTuningSubsystem::Get().Items.Find(Held.Item))
					{
						for (const FText& Line : VeyraShopModel::StatLines(Definition->Stats))
						{
							Hover->Stats.Add(Line.ToString());
						}
					}
					Hover->Anchor = FVector2D(At.X + Item / 2.0f, At.Y);
				}
			}
			// A new item lands in its slot with a burst.
			DrawTileMoments(Items, At, Item, OnRise(MotionKey(TEXT("ItemReady"), Index), bReady),
				Count(MotionKey(TEXT("ItemHeld"), Index), Held.Item.IsValid() ? GetTypeHash(Held.Item.ToString()) % 100000 : 0).SinceChange, static_cast<uint32>(Index * 71 + 5));
			Items.KeyCap(At, KeyName(Input.GetAbilityKey(Held.Slot)));
		}
		// Gold: a coin, then the amount running to its new value, lit as it rises (ADR-082 §3); the shop's key on a chip at the right.
		const FVeyraHudMotion::FCount Gold = Count(TEXT("Gold"), Player.Gold);
		const float GoldY = ItemsTop + 2.0f * Item + Deck.ItemGap * 1.5f;
		const FSlateFontInfo GoldFont = Items.Font(TEXT("Bold"), Settings.HudBodyFontSize);
		const float GoldHeight = Items.Measure(TEXT("0"), GoldFont).Y;
		const float CoinRadius = Items.S(6.0f);
		const FVector2D CoinCentre(ItemsLeft + CoinRadius, GoldY + GoldHeight / 2.0f);
		const bool bEarning = Gold.bRose && Gold.SinceChange < Settings.MotionCountSeconds;
		if (bEarning)
		{
			Items.Glow(CoinCentre, CoinRadius * 3.0f, Settings.GoldColor.CopyWithNewOpacity(0.6f * static_cast<float>(1.0 - Gold.SinceChange / Settings.MotionCountSeconds)));
		}
		DrawCoin(Items, CoinCentre, CoinRadius);
		const FLinearColor GoldColor = bEarning ? FMath::Lerp(Lit(Settings.GoldColor, 0.4f), Settings.GoldColor, static_cast<float>(Gold.SinceChange / Settings.MotionCountSeconds))
												: Lit(Settings.GoldColor, 0.25f);
		Items.Text(FVector2D(ItemsLeft + CoinRadius * 2.0f + Items.S(6.0f), GoldY), FText::AsNumber(FMath::RoundToInt32(Gold.Shown)).ToString(), GoldFont, GoldColor);
		const FSlateFontInfo ShopFont = Items.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		const FString ShopWord = Player.PendingPurchases > 0 ? FString::Printf(TEXT("SHOP  %d AT THE FOUNTAIN"), Player.PendingPurchases) : FString(TEXT("SHOP"));
		const FVector2D ShopKeySize = Items.Measure(ShopKey, ShopFont) + FVector2D(Items.S(8.0f), Items.S(2.0f));
		const float ShopWordWidth = Items.Measure(ShopWord, ShopFont).X;
		const float ShopRight = ItemsLeft + Deck.ItemsWidth;
		const FVector2D ShopKeyAt(ShopRight - ShopWordWidth - Items.S(5.0f) - ShopKeySize.X, GoldY + (GoldHeight - ShopKeySize.Y) / 2.0f);
		DrawPlate(Items, ShopKeyAt, ShopKeySize, Settings.HudSurfaceColor.CopyWithNewOpacity(0.9f), FLinearColor(1.0f, 1.0f, 1.0f, 0.25f));
		Items.TextCentred(ShopKeyAt + ShopKeySize / 2.0, ShopKey, ShopFont, Settings.TextColor);
		Items.Text(FVector2D(ShopRight - ShopWordWidth, GoldY + (GoldHeight - Items.Measure(TEXT("Ag"), ShopFont).Y) / 2.0f), ShopWord, ShopFont, Settings.DescriptionColor);

		// The player's own statuses just above the deck (ADR-059 §4).
		DrawStatusRow(Frame, Statuses, Participant, Now, TopLeft, Deck.Size.X, bInDenseFog);

		// Recalling: the channel over the deck, filling toward home (ADR-012 §8).
		const float DeckCentre = TopLeft.X + Deck.Size.X / 2.0f;
		if (Player.bRecalling)
		{
			const FVector2D Size(Frame.S(320.0f), Frame.S(12.0f));
			const FVector2D At(DeckCentre - Size.X / 2.0f, TopLeft.Y - Size.Y - Frame.S(34.0f));
			DrawBar(Frame, At, Size, Player.RecallProgress, Settings.ChannelColor, FString());
			Frame.Glow(At + FVector2D(Size.X * static_cast<float>(FMath::Clamp(Player.RecallProgress, 0.0, 1.0)), Size.Y / 2.0f), Size.Y * 1.5f, Settings.ChannelColor.CopyWithNewOpacity(0.7f));
			Frame.TextCentred(At + FVector2D(Size.X / 2.0f, -Frame.S(12.0f)), FString::Printf(TEXT("RECALL   %.1f"), Player.RecallSeconds),
				Frame.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor, true);
		}

		// Commanding its Echo: its Integrity over the deck, strained as it runs low, and the keys it may still cast with
		// (ADR-050 §7). Its Vanguard waits in Stasis, so no Recall runs beside it.
		if (Player.Echo.IsSet())
		{
			const FVeyraHudEcho& Echo = Player.Echo.GetValue();
			const FVector2D Size(Frame.S(320.0f), Frame.S(12.0f));
			const FVector2D At(DeckCentre - Size.X / 2.0f, TopLeft.Y - Size.Y - Frame.S(34.0f));
			const bool bStrained = Echo.IntegrityShare < Settings.EchoStrainShare;
			DrawBar(Frame, At, Size, Echo.IntegrityShare, bStrained ? Settings.EchoStrainColor : Settings.ChannelColor, FString());
			FString Keys;
			for (const EVeyraAbilitySlot Slot : Echo.Slots)
			{
				Keys += KeyName(Input.GetAbilityKey(Slot)) + TEXT(" ");
			}
			const TCHAR* Phase = Echo.FormingSeconds > 0.0 ? TEXT("FORMING") : Echo.ImmuneSeconds > 0.0 ? TEXT("ECHO   PROTECTED") : TEXT("ECHO");
			const FString Line = Echo.RepeatsLeft > 0 ? FString::Printf(TEXT("%s   %s x%d"), Phase, *Keys.TrimEnd(), Echo.RepeatsLeft) : FString(Phase);
			Frame.TextCentred(At + FVector2D(Size.X / 2.0f, -Frame.S(12.0f)), Line, Frame.Font(TEXT("Bold"), Settings.HudSmallFontSize),
				bStrained ? Settings.EchoStrainColor : Settings.TextColor, true);
		}

		// Dead: the world dims, and the wait for the fountain counts down in the display face (Economy & Progression Bible §14).
		if (Player.bDead)
		{
			const double Dying = OnRise(TEXT("Dead"), true);
			const FPainter Fading = Frame.Moved(static_cast<float>(VeyraHudEase::Progress(Dying, Settings.MotionBannerSeconds)), FVector2D::ZeroVector);
			Fading.Rect(FVector2D::ZeroVector, FVector2D(Frame.Canvas.ClipX, TopLeft.Y), Settings.ShadeColor.CopyWithNewOpacity(Settings.ShadeColor.A * 0.6f));
			const FVector2D Centre(Frame.Canvas.ClipX / 2.0f, Frame.Canvas.ClipY * 0.4f);
			FSlateFontInfo Caption = Frame.Font(TEXT("Bold"), Settings.HudBodyFontSize);
			Caption.LetterSpacing = 240;
			Fading.TextCentred(Centre - FVector2D(0.0f, Frame.S(52.0f)), TEXT("RESPAWNING IN"), Caption, Settings.DescriptionColor);
			Fading.TextCentred(Centre, FString::FromInt(FMath::CeilToInt32(Player.RespawnSeconds)), Frame.Font(TEXT("DisplayBold"), Settings.HudHeadlineFontSize),
				Settings.TextColor, true);
		}
		else
		{
			OnRise(TEXT("Dead"), false);
		}
		return TopLeft.Y;
	}
}

namespace VeyraHudDeck
{
void Draw(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, const UFont* Font, const UWorld& World,
	const AVeyraGameState& GameState, const APlayerController* Viewer, const AVeyraPlayerState* Own, double ServerNow, TConstArrayView<FString> Warnings,
	const FVeyraHudArrangement& Layout)
{
	// What the HUD remembers between frames to move (ADR-082 §2), which the presentation keeps.
	UVeyraGreyboxSubsystem* Presentation = World.GetSubsystem<UVeyraGreyboxSubsystem>();
	FVeyraHudMotion* Motion = Presentation ? &Presentation->GetHudMotion() : nullptr;
	const FPainter Paint(Canvas, Settings, Font, Preferences.HudScale, Motion);
	// The team panels at their own scale, every edge-anchored part inside the safe area (ADR-059 §1-§2).
	const FPainter Panels = Paint.At(Layout.TeamPanels);
	const EVeyraTeam Side = Own ? Own->GetVeyraTeam() : EVeyraTeam::None;
	// Sides in the player's colour vision (ADR-055 §1).
	DrawTopStrip(Panels, GameState, Side, Preferences.SideColors, Layout.Inset);
	DrawReadouts(Paint, Preferences, Viewer, Warnings, Layout.Inset);
	DrawNotices(Panels, GameState, Viewer, Layout.Inset);
	DrawTeamFlux(Panels, World, Side, ServerNow, Preferences.SideColors, Layout.Inset);
	if (!Own)
	{
		return;
	}
	TOptional<FVector2D> Mouse;
	FVector2D At;
	if (Viewer && Viewer->GetMousePosition(At.X, At.Y))
	{
		Mouse = At;
	}
	TOptional<FHover> Hover;
	// The player's own keys on the key caps (ADR-024 §6).
	const AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(Viewer);
	const UVeyraMatchMenuSubsystem* Screens = World.GetGameInstance() ? World.GetGameInstance()->GetSubsystem<UVeyraMatchMenuSubsystem>() : nullptr;
	const FString ShopKey = KeyName(Screens ? Screens->GetKeys().ShopKey : GetDefault<UVeyraUIInputSettings>()->ShopKey);
	// The rank-up marks pulse while a point waits, unless the player reduced UI animation (ADR-065 §5; ADR-055 §3).
	const double Phase = FMath::Fmod(FPlatformTime::Seconds(), static_cast<double>(Settings.RankUpPulseSeconds)) / Settings.RankUpPulseSeconds;
	const float RankMarkOpacity = Preferences.bReduceUiAnimation ? 1.0f
		: FMath::Lerp(Settings.RankUpPulseFloor, 1.0f, static_cast<float>(0.5 + 0.5 * FMath::Cos(UE_DOUBLE_TWO_PI * Phase)));
	// Whether the player's own Vanguard stands in Dense Fog, as the presentation knows it (ADR-068 §6).
	const APawn* Vanguard = Player ? Player->GetVanguard() : nullptr;
	const bool bInDenseFog = Presentation && Vanguard && Presentation->HiddenKindOf(*Vanguard) == EVeyraHiddenKind::DenseFog;
	FVeyraHudStats Stats;
	DrawDeck(Paint, Layout.Deck, Layout.DeckTopLeft, Preferences.Cooldowns, Preferences.Statuses, *Own,
		Player ? Player->GetKeys() : *GetDefault<UVeyraInputSettings>(), ShopKey, Mouse, ServerNow, RankMarkOpacity, bInDenseFog, Hover, Stats);
	// The stats panel beside the portrait, which a click on it opens or closes (ADR-080 §3).
	if (Presentation)
	{
		if (Player && Mouse.IsSet() && Layout.StatsCore.IsInsideOrOn(Mouse.GetValue()) && Player->WasInputKeyJustPressed(Player->GetKeys().SelectKey))
		{
			Presentation->ToggleStatsPanel();
		}
		bool bStatsOpen = Presentation->IsStatsPanelOpen();
#if !UE_BUILD_SHIPPING
		bStatsOpen |= CVarDevStatsPanelOpen.GetValueOnGameThread();
#endif
		// Its more stats cut in as they open (ADR-082 §2).
		const double MoreOpened = Motion ? Motion->Held(TEXT("StatsMore"), bStatsOpen) : (bStatsOpen ? FVeyraHudMotion::LongAgo : -1.0);
		DrawStatsPanel(Paint.At(Layout.Stats), Stats, Layout.StatsCore, Layout.StatsMore, MoreOpened);
	}
	if (Player)
	{
		DrawChat(Paint, Preferences, Layout.Chat, *Player, GameState, Side, Screens && Screens->IsChatOpen());
	}
	// A tooltip cuts in over its tile, anew for each tile it moves to (ADR-082 §2).
	const uint32 HoverIdentity = Hover.IsSet() ? GetTypeHash(Hover->Title) ^ GetTypeHash(Hover->Anchor) : 0u;
	const double HoverOpened = Motion ? Motion->Held(TEXT("Tooltip"), Hover.IsSet(), HoverIdentity) : FVeyraHudMotion::LongAgo;
	if (Hover.IsSet())
	{
		// At the player's Interface Text Size, as every informational tooltip (SET-62).
		const float* TextScale = GetDefault<UVeyraShellStyleSettings>()->TextSizeScales.Find(Preferences.TextSize);
		DrawTooltip(Paint.At(Paint.Scale * (TextScale ? *TextScale : 1.0f)).CuttingIn(HoverOpened), Hover.GetValue());
	}
}

namespace
{
	/**
	 * A banner across the screen at CentreY (ADR-082 §3), Age seconds after it came and with Seconds to show: a smoked band that
	 * opens from its middle with hairlines in Color, Title in the display face rising into it, a glint crossing the title once
	 * it stands, Subtitle under it, and the whole fading through the last FadeShare of its time. Title is drawn in capitals.
	 */
	void DrawBanner(const FPainter& Paint, float CentreY, const FString& Title, const FString& Subtitle, const FLinearColor& Color, int32 TitleSize, double Age,
		double Seconds, double FadeShare)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		if (Age < 0.0 || Age >= Seconds)
		{
			return;
		}
		const float Leaving = static_cast<float>(FMath::Clamp((Seconds - Age) / FMath::Max(Seconds * FadeShare, UE_SMALL_NUMBER), 0.0, 1.0));
		const FPainter Shown = Paint.Moved(Leaving, FVector2D(0.0f, -Paint.S(Settings.MotionPanelRise) * (1.0f - Leaving)));
		FSlateFontInfo TitleFont = Paint.Font(TEXT("DisplayBold"), TitleSize);
		TitleFont.LetterSpacing = 140;
		const FString Upper = Title.ToUpper();
		const FVector2D TitleSize2 = Paint.Measure(Upper, TitleFont);
		const FSlateFontInfo SubFont = Paint.Font(TEXT("SemiBold"), Settings.HudBodyFontSize);
		const FVector2D SubSize = Subtitle.IsEmpty() ? FVector2D::ZeroVector : Paint.Measure(Subtitle, SubFont);
		const float Pad = Paint.S(14.0f);
		const float Height = TitleSize2.Y + (Subtitle.IsEmpty() ? 0.0f : SubSize.Y + Paint.S(4.0f)) + Pad * 2.0f;
		const float FullWidth = FMath::Max(static_cast<float>(TitleSize2.X), static_cast<float>(SubSize.X)) + Paint.S(260.0f);
		// The band opens from its middle along the snap.
		const float Open = static_cast<float>(FPainter::Eased(Settings.MotionSnap, Age, Settings.MotionBannerSeconds));
		const float Width = FullWidth * Open;
		const float Middle = Paint.Canvas.ClipX / 2.0f;
		const float Top = CentreY - Height / 2.0f;
		const FLinearColor Band = Settings.HudSurfaceColor.CopyWithNewOpacity(FMath::Min(1.0f, Settings.HudSurfaceColor.A + 0.08f));
		const FLinearColor Clear = Band.CopyWithNewOpacity(0.0f);
		const auto Faded = [&](float Left, float Right, float Y0, float Y1, const FLinearColor& Inner) {
			const FLinearColor Edge = Inner.CopyWithNewOpacity(0.0f);
			Shown.Polygon({ FVector2D(Left, Y0), FVector2D(Middle, Y0), FVector2D(Middle, Y1), FVector2D(Left, Y1) }, FLinearColor::White, { Edge, Inner, Inner, Edge });
			Shown.Polygon({ FVector2D(Middle, Y0), FVector2D(Right, Y0), FVector2D(Right, Y1), FVector2D(Middle, Y1) }, FLinearColor::White, { Inner, Edge, Edge, Inner });
		};
		Faded(Middle - Width / 2.0f, Middle + Width / 2.0f, Top, Top + Height, Band);
		const float Hair = FMath::Max(1.0f, Paint.S(1.0f));
		Faded(Middle - Width / 2.0f, Middle + Width / 2.0f, Top, Top + Hair, Color.CopyWithNewOpacity(0.85f));
		Faded(Middle - Width / 2.0f, Middle + Width / 2.0f, Top + Height - Hair, Top + Height, Color.CopyWithNewOpacity(0.85f));
		(void)Clear;
		// A brass diamond on each hairline's middle.
		const float Gem = Paint.S(4.0f);
		for (const float Y : { Top, Top + Height })
		{
			Shown.Polygon({ FVector2D(Middle, Y - Gem), FVector2D(Middle + Gem, Y), FVector2D(Middle, Y + Gem), FVector2D(Middle - Gem, Y) },
				Settings.TileReadyTopColor.CopyWithNewOpacity(Open));
		}
		// The title rises in once the band is half open, and a glint crosses it after.
		const double TitleAge = Age - Settings.MotionBannerSeconds * 0.3;
		const float TitleIn = static_cast<float>(VeyraHudEase::Progress(TitleAge, Settings.MotionBannerSeconds * 0.5));
		const float TitleRise = Paint.S(10.0f) * (1.0f - static_cast<float>(FPainter::Eased(Settings.MotionSettle, TitleAge, Settings.MotionBannerSeconds * 0.6)));
		const FPainter Words = Shown.Moved(TitleIn, FVector2D(0.0f, TitleRise));
		const FVector2D TitleAt(Middle - TitleSize2.X / 2.0f, Top + Pad);
		Words.Glow(FVector2D(Middle, TitleAt.Y + TitleSize2.Y / 2.0f), TitleSize2.X * 0.45f, Color.CopyWithNewOpacity(0.18f));
		Words.Text(TitleAt, Upper, TitleFont, Color, true);
		const double GlintAge = Age - Settings.MotionBannerSeconds;
		if (GlintAge > 0.0 && GlintAge < Settings.MotionGlintSeconds)
		{
			Words.Sweep(TitleAt - FVector2D(0.0f, Paint.S(2.0f)), TitleSize2 + FVector2D(0.0f, Paint.S(4.0f)), FPainter::Eased(Settings.MotionSmear, GlintAge, Settings.MotionGlintSeconds),
				TitleSize2.Y * 2.5f, Settings.MotionHotColor.CopyWithNewOpacity(0.55f));
		}
		if (!Subtitle.IsEmpty())
		{
			Words.Text(FVector2D(Middle - SubSize.X / 2.0f, TitleAt.Y + TitleSize2.Y + Paint.S(4.0f)), Subtitle, SubFont, Settings.TextColor, true);
		}
	}
}

void DrawHeadline(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const UFont* FontAsset, const FString& Text, const FLinearColor& Color, FVeyraHudMotion* Motion)
{
	const FPainter Paint(Canvas, Settings, FontAsset, 1.0f, Motion);
	// The match's end stands until the players leave: its banner opens once and stays.
	const double Age = Motion ? FMath::Min(Motion->OnRise(TEXT("Ending"), true), Settings.MotionBannerSeconds + Settings.MotionGlintSeconds) : FVeyraHudMotion::LongAgo;
	const double Shown = FMath::Min(Age, Settings.MotionBannerSeconds + Settings.MotionGlintSeconds);
	// A shade over the battleground behind it, so it reads over the fallen Well.
	Paint.Moved(static_cast<float>(VeyraHudEase::Progress(Shown, Settings.MotionBannerSeconds)), FVector2D::ZeroVector)
		.Rect(FVector2D::ZeroVector, FVector2D(Canvas.ClipX, Canvas.ClipY), Settings.ShadeColor.CopyWithNewOpacity(Settings.ShadeColor.A * 0.5f));
	constexpr double Stays = 1.0e6;
	DrawBanner(Paint, Canvas.ClipY / 3.0f, Text, FString(), Color, Settings.HudHeadlineFontSize, Shown, Stays, 0.0);
}

void DrawLevelUp(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const UFont* FontAsset, int32 Level, int32 UnspentPoints, double Shown)
{
	const FPainter Paint(Canvas, Settings, FontAsset);
	const FString Note = UnspentPoints > 1 ? FString::Printf(TEXT("%d skill points to spend"), UnspentPoints) : UnspentPoints > 0 ? FString(TEXT("A skill point to spend")) : FString();
	DrawBanner(Paint, Canvas.ClipY * Settings.LevelUpHeightShare, FString::Printf(TEXT("Level %d"), Level), Note, Settings.GoldColor, Settings.LevelUpFontSize,
		Shown * Settings.LevelUpBannerSeconds, Settings.LevelUpBannerSeconds, Settings.LevelUpFadeShare);
}

void DrawKillFeed(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, const UFont* FontAsset,
	TConstArrayView<FVeyraKillFeedRow> Rows, EVeyraTeam OwnSide, const FVector2D& Inset)
{
	const FPainter Paint(Canvas, Settings, FontAsset, Preferences.HudScale);
	const FSlateFontInfo NameFont = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
	const float Face = Paint.S(Settings.KillFeedFaceSize);
	const float Gap = Paint.S(6.0f);
	const float Right = Canvas.ClipX - Inset.X - Paint.S(Settings.DeckGap);
	// Under the top strip's readouts on the right.
	float Y = Inset.Y + Paint.S(Settings.DeckGap) + Paint.S(64.0f);
	const auto SideColor = [&Preferences, OwnSide](EVeyraTeam Side) {
		return Side == EVeyraTeam::None ? Preferences.SideColors.Neutral : Side == OwnSide ? Preferences.SideColors.Ally : Preferences.SideColors.Enemy;
	};
	constexpr double FadeShare = 0.2;
	for (const FVeyraKillFeedRow& Row : Rows)
	{
		const FVeyraKillFeedLine& Line = Row.Line;
		// Sliding in from the right as it comes (ADR-082 §3); fading through its last part.
		const double Age = Preferences.bReduceUiAnimation ? FVeyraHudMotion::LongAgo : Row.Progress * Settings.KillFeedSeconds;
		const float Entering = static_cast<float>(VeyraHudEase::Progress(Age, Settings.MotionFeedSeconds * 0.4));
		const float Slide = Paint.S(Settings.MotionFeedSlide) * (1.0f - static_cast<float>(FPainter::Eased(Settings.MotionSnap, Age, Settings.MotionFeedSeconds)));
		const float Leaving = static_cast<float>(FMath::Clamp((1.0 - Row.Progress) / FadeShare, 0.0, 1.0));
		const FPainter Shown = Paint.Moved(Entering * Leaving, FVector2D(Slide, 0.0f));
		// The killer's side: a takedown's killer, else the side that took the fallen.
		const bool bKiller = Line.KillerPlayerId != INDEX_NONE;
		const FString KillerText = bKiller ? Line.KillerName : Line.Kind == EVeyraKillFeedKind::Execution ? FString(TEXT("Executed")) : FString(TEXT("Fluxborn"));
		const EVeyraTeam KillerSide = bKiller ? Line.KillerSide : Line.Kind == EVeyraKillFeedKind::Execution ? EVeyraTeam::None : VeyraTeams::Opposing(Line.VictimSide);
		const FLinearColor KillerColor = SideColor(KillerSide);
		const bool bStructure = Line.Kind == EVeyraKillFeedKind::Structure;
		const FString VictimText = bStructure ? VeyraKillFeedView::StructureName(Line) : Line.VictimName;
		const FString Assists = Line.Assists > 0 ? FString::Printf(TEXT("+%d"), Line.Assists) : FString();
		const float Arrow = Paint.S(14.0f);
		const float KillerWidth = (bKiller ? Face + Gap : 0.0f) + Paint.Measure(KillerText, NameFont).X;
		const float AssistWidth = Assists.IsEmpty() ? 0.0f : Paint.Measure(Assists, NameFont).X + Gap;
		const float VictimWidth = (bStructure ? 0.0f : Face + Gap) + Paint.Measure(VictimText, NameFont).X;
		const float Width = Gap * 6.0f + KillerWidth + AssistWidth + Arrow + VictimWidth;
		const float Height = Face + Gap;
		const FVector2D TopLeft(Right - Width, Y);
		// A smoked chip, edged in the killer's side; a first blood in gold.
		const TArray<FVector2D> Chip = FPainter::Chamfered(TopLeft, FVector2D(Width, Height), Paint.S(5.0f));
		Shown.Polygon(Chip, Settings.HudSurfaceColor.CopyWithNewOpacity(FMath::Min(1.0f, Settings.HudSurfaceColor.A + 0.06f)));
		const FLinearColor Edge = Line.bFirstBlood ? Settings.GoldColor : KillerColor.CopyWithNewOpacity(0.45f);
		for (int32 Index = 0; Index < Chip.Num(); ++Index)
		{
			Shown.Line(Chip[Index], Chip[(Index + 1) % Chip.Num()], Edge, FMath::Max(1.0f, Paint.S(1.0f)));
		}
		const float TextY = Y + (Height - Paint.Measure(TEXT("Ag"), NameFont).Y) / 2.0f;
		float X = TopLeft.X + Gap * 2.0f;
		if (bKiller)
		{
			DrawFace(Shown, Line.KillerVanguard, FVector2D(X, Y + Gap / 2.0f), Face, Settings.HudSmallFontSize);
			Shown.Outline(FVector2D(X, Y + Gap / 2.0f), FVector2D(Face), KillerColor);
			X += Face + Gap;
		}
		Shown.Text(FVector2D(X, TextY), KillerText, NameFont, KillerColor, true);
		X += Paint.Measure(KillerText, NameFont).X + Gap;
		if (!Assists.IsEmpty())
		{
			Shown.Text(FVector2D(X, TextY), Assists, NameFont, Settings.DescriptionColor, true);
			X += AssistWidth;
		}
		// The arrow: a shaft and its head.
		const float ArrowY = Y + Height / 2.0f;
		const float Thickness = FMath::Max(1.0f, Paint.S(1.4f));
		Shown.Line(FVector2D(X, ArrowY), FVector2D(X + Arrow, ArrowY), Settings.DescriptionColor, Thickness);
		Shown.Line(FVector2D(X + Arrow - Paint.S(4.0f), ArrowY - Paint.S(3.5f)), FVector2D(X + Arrow, ArrowY), Settings.DescriptionColor, Thickness);
		Shown.Line(FVector2D(X + Arrow - Paint.S(4.0f), ArrowY + Paint.S(3.5f)), FVector2D(X + Arrow, ArrowY), Settings.DescriptionColor, Thickness);
		X += Arrow + Gap;
		if (!bStructure)
		{
			// The fallen, dimmed.
			DrawFace(Shown, Line.VictimVanguard, FVector2D(X, Y + Gap / 2.0f), Face, Settings.HudSmallFontSize, Settings.TileDimTint);
			X += Face + Gap;
		}
		Shown.Text(FVector2D(X, TextY), VictimText, NameFont, SideColor(Line.VictimSide), true);
		Y += Height + Gap / 2.0f;
	}
}

void DrawAnnouncement(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, const UFont* FontAsset,
	const FVeyraAnnouncement& Announcement, const FVector2D& Inset)
{
	const FPainter Paint(Canvas, Settings, FontAsset, Preferences.HudScale);
	const FLinearColor Color = Announcement.bGood ? Preferences.SideColors.Ally : Preferences.SideColors.Enemy;
	const double Age = Preferences.bReduceUiAnimation ? Settings.MotionBannerSeconds + Settings.MotionGlintSeconds : Announcement.Progress * Settings.AnnouncementSeconds;
	// Below the top strip and its notices, above the battleground's middle.
	DrawBanner(Paint, Inset.Y + Paint.S(Settings.DeckGap) + Paint.S(140.0f), Announcement.Text, FString(), Color, Settings.AnnouncementFontSize, Age, Settings.AnnouncementSeconds,
		Settings.LevelUpFadeShare);
}

void DrawTargetFrame(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, const UFont* FontAsset,
	const FVeyraTargetFrame& Frame, EVeyraTeam OwnSide, const FBox2D& Box, float Scale, FVeyraHudMotion* Motion)
{
	const FPainter Paint(Canvas, Settings, FontAsset, Scale, Motion);
	const bool bVanguard = Frame.Vanguard.IsValid() || !Frame.Spells.IsEmpty();
	const bool bResource = Frame.Vitals.MaxResource > 0.0;
	const float Pad = Paint.S(8.0f);
	const float Header = bVanguard ? Paint.S(48.0f) : Paint.S(36.0f);
	const float HealthHeight = Paint.S(14.0f);
	const float ResourceHeight = Paint.S(10.0f);
	const float BarGap = Paint.S(4.0f);
	const float Tile = Paint.S(24.0f);
	const float TileGap = Paint.S(4.0f);
	const float Width = static_cast<float>(Box.GetSize().X);
	const float Height = Pad + Header + Pad + HealthHeight + (bResource ? BarGap + ResourceHeight : 0.0f) + (bVanguard ? Pad + Tile : 0.0f) + Pad;
	const FVector2D TopLeft = Box.Min;
	const FLinearColor Side = Frame.Side == EVeyraTeam::None ? Preferences.SideColors.Neutral
		: Frame.Side == OwnSide ? Preferences.SideColors.Ally : Preferences.SideColors.Enemy;
	// A new unit's frame cuts in (ADR-082 §2).
	const double Opened = Motion ? Motion->Held(TEXT("TargetFrame"), true, GetTypeHash(Frame.Name)) : FVeyraHudMotion::LongAgo;
	const FPainter Shown = Paint.CuttingIn(Opened);
	Shown.Surface(TopLeft, FVector2D(Width, Height));
	// Its side's light along the head of the frame.
	Shown.Gradient(TopLeft + FVector2D(Paint.S(2.0f)), FVector2D(Width - Paint.S(4.0f), Header * 0.8f), Side.CopyWithNewOpacity(0.16f), Side.CopyWithNewOpacity(0.0f));

	// Its face and Level, its name in the display face and its side's word, and its player.
	float X = TopLeft.X + Pad;
	const float Y = TopLeft.Y + Pad;
	if (bVanguard)
	{
		DrawTileFrame(Shown, FVector2D(X, Y), Header, false, TOptional<FLinearColor>(Side.CopyWithNewOpacity(0.8f)));
		const float Inset = IconInset(Shown);
		FVector2D UV0;
		FVector2D UV1;
		if (UTexture2D* Hero = PortraitOf(Frame.Vanguard, UV0, UV1))
		{
			const FVector2D FaceAt(X + Inset, Y + Inset);
			const FVector2D FaceSize(Header - Inset * 2.0f);
			Shown.ImageIn(Hero, FPainter::Chamfered(FaceAt, FaceSize, TileCut(Shown, Header)), FBox2D(FaceAt, FaceAt + FaceSize), FLinearColor::White, UV0, UV1);
		}
		else
		{
			DrawFace(Shown, Frame.Vanguard, FVector2D(X + Inset, Y + Inset), Header - Inset * 2.0f, Settings.HudHeadingFontSize);
		}
		if (Frame.Level > 0)
		{
			const FVector2D Badge(Paint.S(20.0f), Paint.S(17.0f));
			const FVector2D BadgeAt(X + Header - Badge.X + Paint.S(3.0f), Y + Header - Badge.Y + Paint.S(3.0f));
			DrawPlate(Shown, BadgeAt, Badge, Settings.HudSurfaceColor.CopyWithNewOpacity(1.0f), Side);
			Shown.TextCentred(BadgeAt + Badge / 2.0, FString::FromInt(Frame.Level), Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor);
		}
		X += Header + Pad;
	}
	const FSlateFontInfo NameFont = Paint.Font(TEXT("Display"), Settings.HudHeadingFontSize);
	Shown.Text(FVector2D(X, Y), Frame.Name, NameFont, Settings.TextColor, true);
	FSlateFontInfo SideFont = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
	SideFont.LetterSpacing = 160;
	const FString SideWord = Frame.Side == EVeyraTeam::None ? FString(TEXT("NEUTRAL")) : Frame.Side == OwnSide ? FString(TEXT("ALLY")) : FString(TEXT("ENEMY"));
	Shown.Text(FVector2D(TopLeft.X + Width - Pad - Paint.Measure(SideWord, SideFont).X, Y + Paint.S(3.0f)), SideWord, SideFont, Side);
	const FString Detail = Frame.bAlive ? Frame.Detail : Frame.Detail.IsEmpty() ? FString(TEXT("Fallen")) : Frame.Detail + TEXT("   Fallen");
	Shown.Text(FVector2D(X, Y + Paint.S(24.0f)), Detail, Paint.Font(TEXT("Regular"), Settings.HudSmallFontSize), Settings.DescriptionColor);

	// Health with its shields, its lost part trailing (ADR-082 §3), then its resource, each with its figures.
	const float BarsX = TopLeft.X + Pad;
	const float BarWidth = Width - Pad * 2.0f;
	float BarY = Y + Header + Pad;
	const double Total = FMath::Max(Frame.Vitals.MaxHealth, Frame.Vitals.Health + Frame.Vitals.Shield);
	const double HealthShare = Total > 0.0 ? Frame.Vitals.Health / Total : 0.0;
	const double Trail = Motion ? Motion->Trail(FName(TEXT("TargetHealth"), static_cast<int32>(GetTypeHash(Frame.Name) & 0x7fffffff)), HealthShare, Settings.MotionTrailHoldSeconds,
		Settings.MotionTrailDrainSeconds, Settings.MotionSmear) : HealthShare;
	DrawBar(Shown, FVector2D(BarsX, BarY), FVector2D(BarWidth, HealthHeight), HealthShare, Side,
		FString::Printf(TEXT("%d / %d"), FMath::CeilToInt32(Frame.Vitals.Health), FMath::CeilToInt32(Frame.Vitals.MaxHealth)), Total > 0.0 ? Frame.Vitals.Shield / Total : 0.0,
		Total > 0.0 ? Settings.HealthTickAmount / Total : 0.0, Trail);
	BarY += HealthHeight;
	if (bResource)
	{
		BarY += BarGap;
		DrawBar(Shown, FVector2D(BarsX, BarY), FVector2D(BarWidth, ResourceHeight), FMath::Clamp(Frame.Vitals.Resource / Frame.Vitals.MaxResource, 0.0, 1.0),
			Settings.ResourceColorOf(Frame.Vitals.Family), FString::Printf(TEXT("%d / %d"), FMath::FloorToInt32(Frame.Vitals.Resource), FMath::CeilToInt32(Frame.Vitals.MaxResource)));
		BarY += ResourceHeight;
	}
	if (!bVanguard)
	{
		return;
	}

	// Its six items, then its Flux Spells: locked, cooling with the seconds left, or ready (ADR-066 §3).
	const float TilesY = BarY + Pad;
	float TileX = BarsX;
	const FSlateFontInfo Mark = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
	for (const FVeyraContentId& Item : Frame.Items)
	{
		const FVector2D At(TileX, TilesY);
		DrawTileFrame(Shown, At, Tile, false);
		const float Inset = IconInset(Shown);
		if (!Item.IsValid())
		{
			Shown.Polygon(FPainter::Chamfered(At + FVector2D(Inset), FVector2D(Tile - Inset * 2.0f), TileCut(Shown, Tile)), Settings.BarBackgroundColor);
		}
		else if (!DrawTileIcon(Shown, VeyraShellArt::ItemIconOf(Item.ToString()), At, Tile))
		{
			Shown.TextCentred(At + FVector2D(Tile / 2.0f), Monogram(VeyraContentText::ItemName(Item).ToString()), Mark, Settings.TextColor);
		}
		TileX += Tile + TileGap;
	}
	TileX += Pad;
	for (const FVeyraTargetFrameSpell& Spell : Frame.Spells)
	{
		const FVector2D At(TileX, TilesY);
		const bool bReady = Spell.Spell.IsValid() && !Spell.bLocked && Spell.CooldownSeconds <= 0.0;
		DrawTileFrame(Shown, At, Tile, bReady);
		if (Spell.Spell.IsValid() && !DrawTileIcon(Shown, VeyraShellArt::AbilityIconOf(Spell.Spell.ToString()), At, Tile, bReady ? FLinearColor::White : Settings.TileDimTint))
		{
			Shown.TextCentred(At + FVector2D(Tile / 2.0f), Monogram(VeyraContentText::AbilityName(Spell.Spell).ToString()), Mark, Settings.TextColor);
		}
		const float Inset = IconInset(Shown);
		if (Spell.bLocked)
		{
			Shown.Polygon(FPainter::Chamfered(At + FVector2D(Inset), FVector2D(Tile - Inset * 2.0f), TileCut(Shown, Tile)), Settings.ShadeColor);
			Shown.TextCentred(At + FVector2D(Tile / 2.0f), TEXT("-"), Mark, Settings.DescriptionColor);
		}
		else
		{
			DrawCooldown(Shown, Preferences.Cooldowns, At + FVector2D(Inset), Tile - Inset * 2.0f, Spell.CooldownSeconds, Spell.CooldownTotal);
		}
		TileX += Tile + TileGap;
	}
}
}
