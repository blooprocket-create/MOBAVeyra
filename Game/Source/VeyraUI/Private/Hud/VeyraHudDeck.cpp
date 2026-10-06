// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudDeck.h"

#include "CanvasItem.h"
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
#include "Hud/VeyraChatLogModel.h"
#include "Hud/VeyraHudLayout.h"
#include "Hud/VeyraHudModel.h"
#include "Hud/VeyraKillFeedModel.h"
#include "Input/VeyraInputSettings.h"
#include "Rendering/SlateRenderer.h"
#include "Engine/GameInstance.h"
#include "Match/VeyraMatchMenuSubsystem.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "Shell/VeyraShellArt.h"
#include "Shell/VeyraUIInputSettings.h"
#include "Statistics/VeyraScoreComponent.h"
#include "Styling/CoreStyle.h"
#include "Text/VeyraContentText.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

// The engine keeps a smoothed frame rate, declared only where it is defined.
extern ENGINE_API float GAverageFPS;

namespace
{
	constexpr int32 SecondsPerMinute = 60;

	/** Draws on the canvas at the HUD's scale, in its surfaces and type. */
	struct FPainter
	{
		UCanvas& Canvas;
		const UVeyraGreyboxSettings& Settings;
		const UFont* FontAsset = nullptr;
		float Scale = 1.0f;

		FPainter(UCanvas& InCanvas, const UVeyraGreyboxSettings& InSettings, const UFont* InFont, float HudScale = 1.0f)
			: Canvas(InCanvas)
			, Settings(InSettings)
			, FontAsset(InFont)
			, Scale((InSettings.HudReferenceHeight > 0.0f ? InCanvas.ClipY / InSettings.HudReferenceHeight : 1.0f) * HudScale)
		{
		}

		float S(float Units) const
		{
			return Units * Scale;
		}

		/** Typeface at Size, scaled; from the HUD's font asset, without which the canvas draws no text. */
		FSlateFontInfo Font(const TCHAR* Typeface, int32 Size) const
		{
			const int32 Scaled = FMath::Max(1, FMath::RoundToInt32(Size * Scale));
			return FontAsset ? FSlateFontInfo(FontAsset, Scaled, FName(Typeface)) : FCoreStyle::GetDefaultFontStyle(Typeface, Scaled);
		}

		void Rect(const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color) const
		{
			FCanvasTileItem Tile(TopLeft, GWhiteTexture, Size, Color);
			Tile.BlendMode = SE_BLEND_Translucent;
			Canvas.DrawItem(Tile);
		}

		void Outline(const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color, float Thickness = 1.0f) const
		{
			FCanvasBoxItem Box(TopLeft, Size);
			Box.SetColor(Color);
			Box.LineThickness = Thickness;
			Box.BlendMode = SE_BLEND_Translucent;
			Canvas.DrawItem(Box);
		}

		/** A smoked surface with its thin outline. */
		void Surface(const FVector2D& TopLeft, const FVector2D& Size) const
		{
			Rect(TopLeft, Size, Settings.HudSurfaceColor);
			Outline(TopLeft, Size, Settings.HudHairlineColor);
		}

		FVector2D Measure(const FString& Text, const FSlateFontInfo& Font) const
		{
			if (Text.IsEmpty() || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
			{
				return FVector2D::ZeroVector;
			}
			return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font);
		}

		void Text(const FVector2D& TopLeft, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow = false) const
		{
			FCanvasTextItem Item(TopLeft, FText::FromString(Text), Font, Color);
			if (bShadow)
			{
				Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
			}
			Canvas.DrawItem(Item);
		}

		void TextCentred(const FVector2D& Centre, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& Color, bool bShadow = false) const
		{
			this->Text(Centre - Measure(Text, Font) / 2.0, Text, Font, Color, bShadow);
		}

		/** A key's name in a small cap at a tile's top-left corner. */
		void KeyCap(const FVector2D& TileTopLeft, const FString& Key) const
		{
			const FSlateFontInfo KeyFont = Font(TEXT("Bold"), Settings.HudSmallFontSize);
			const FVector2D TextSize = Measure(Key, KeyFont);
			const FVector2D Pad(S(4.0f), S(1.0f));
			Rect(TileTopLeft, TextSize + Pad * 2.0, FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
			Text(TileTopLeft + Pad, Key, KeyFont, Settings.TextColor);
		}

		/** Up to MaxLines lines of Text, each no wider than Width, in Font. */
		TArray<FString> Wrap(const FString& Text, const FSlateFontInfo& Font, float Width, int32 MaxLines = MAX_int32) const
		{
			TArray<FString> Words;
			Text.ParseIntoArrayWS(Words);
			TArray<FString> Lines;
			FString Line;
			for (const FString& Word : Words)
			{
				const FString Tried = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
				if (!Line.IsEmpty() && Measure(Tried, Font).X > Width)
				{
					Lines.Add(Line);
					Line = Word;
				}
				else
				{
					Line = Tried;
				}
			}
			if (!Line.IsEmpty())
			{
				Lines.Add(Line);
			}
			if (Lines.Num() > MaxLines)
			{
				Lines.SetNum(MaxLines);
				Lines.Last() += TEXT("...");
			}
			return Lines;
		}
	};

	/** What the cursor rests on, for its tooltip. */
	struct FHover
	{
		FString Title;
		FString Detail;
		FString Description;
		/** An ability's numbers, a line each, between its detail and its description (ADR-065 §7). */
		TArray<FString> Numbers;
	};

	FString KeyName(const FKey& Key)
	{
		return Key.GetDisplayName(false).ToString();
	}

	/** Draws Icon whole in the square at At, Size across; false when there is none to draw. */
	bool DrawIcon(const FPainter& Paint, UTexture2D* Icon, const FVector2D& At, float Size)
	{
		if (!Icon || !Icon->GetResource())
		{
			return false;
		}
		FCanvasTileItem Tile(At, Icon->GetResource(), FVector2D(Size), FLinearColor::White);
		Paint.Canvas.DrawItem(Tile);
		return true;
	}

	FString Monogram(const FString& Name);

	/** Vanguard's face in the square at At, Size across: its hero art cropped to a portrait, or its monogram at MonogramSize. */
	void DrawFace(const FPainter& Paint, const FVeyraContentId& Vanguard, const FVector2D& At, float Size, int32 MonogramSize)
	{
		const FString VanguardId = Vanguard.ToString();
		if (UTexture2D* Hero = VeyraShellArt::HeroOf(VanguardId); Hero && Hero->GetResource())
		{
			const FBox2f Crop = VeyraShellArt::Crop(VanguardId, Hero->GetSizeX(), Hero->GetSizeY(), 1.0f, true);
			FCanvasTileItem Face(At, Hero->GetResource(), FVector2D(Size), FVector2D(Crop.Min), FVector2D(Crop.Max), FLinearColor::White);
			Paint.Canvas.DrawItem(Face);
			return;
		}
		Paint.Rect(At, FVector2D(Size), Paint.Settings.BarBackgroundColor);
		Paint.TextCentred(At + FVector2D(Size / 2.0f), Monogram(VeyraContentText::VanguardName(Vanguard).ToString()), Paint.Font(TEXT("Black"), MonogramSize),
			Paint.Settings.TextColor);
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

	/**
	 * A cooling slot (ADR-059 §3): a sweep over the part still to wait, or with the sweep off the whole tile shaded, and the
	 * seconds left over it as the player chose. Total is what the cooldown started with; without one the whole tile shades.
	 */
	void DrawCooldown(const FPainter& Paint, const FVeyraCooldownDisplay& Display, const FVector2D& TopLeft, float Side, double SecondsLeft, double Total)
	{
		if (SecondsLeft <= 0.0)
		{
			return;
		}
		if (Display.bSweep && Total > 0.0)
		{
			const TArray<FVector2D> Fan = VeyraHud::SweepOutline(TopLeft, Side, 1.0 - SecondsLeft / Total);
			for (int32 Index = 2; Index < Fan.Num(); ++Index)
			{
				FCanvasTriangleItem Triangle(Fan[0], Fan[Index - 1], Fan[Index], GWhiteTexture);
				Triangle.SetColor(Paint.Settings.ShadeColor);
				Triangle.BlendMode = SE_BLEND_Translucent;
				Paint.Canvas.DrawItem(Triangle);
			}
		}
		else
		{
			Paint.Rect(TopLeft, FVector2D(Side), Paint.Settings.ShadeColor);
		}
		if (Display.bNumbers)
		{
			Paint.TextCentred(TopLeft + FVector2D(Side / 2.0f), VeyraHud::CooldownLabel(SecondsLeft, Display.bTenths),
				Paint.Font(TEXT("Bold"), Paint.Settings.HudHeadingFontSize), Paint.Settings.TextColor, true);
		}
	}

	/** A slot's outline: the accent when it is ready to use, so ready always shows whatever the cooldown options (ADR-059 §3). */
	FLinearColor ReadyOutline(const UVeyraGreyboxSettings& Settings, bool bReady)
	{
		return bReady ? Settings.HudAccentColor.CopyWithNewOpacity(0.55f) : Settings.HudHairlineColor;
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

	/** The top strip: each side's kills around the match clock, under the safe area's top edge (Inset). */
	void DrawTopStrip(const FPainter& Paint, const AVeyraGameState& GameState, EVeyraTeam Viewer, const FVeyraSideColors& Sides, const FVector2D& Inset)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const int32 Clock = FMath::FloorToInt32(GameState.GetMatchClockSeconds());
		const FString ClockText = FString::Printf(TEXT("%02d:%02d"), Clock / SecondsPerMinute, Clock % SecondsPerMinute);
		int32 OwnKills = 0;
		int32 EnemyKills = 0;
		CountKills(GameState, Viewer, OwnKills, EnemyKills);
		const FSlateFontInfo ClockFont = Paint.Font(TEXT("Bold"), Settings.HudClockFontSize);
		const FSlateFontInfo KillFont = Paint.Font(TEXT("Black"), Settings.HudClockFontSize);
		const float Width = Paint.S(220.0f);
		const float Height = Paint.S(40.0f);
		const FVector2D TopLeft((Paint.Canvas.ClipX - Width) / 2.0f, Inset.Y + Paint.S(Settings.DeckGap));
		Paint.Surface(TopLeft, FVector2D(Width, Height));
		const float Middle = TopLeft.Y + Height / 2.0f;
		Paint.TextCentred(FVector2D(Paint.Canvas.ClipX / 2.0f, Middle), ClockText, ClockFont, Settings.TextColor);
		Paint.TextCentred(FVector2D(TopLeft.X + Width * 0.14f, Middle), FString::FromInt(OwnKills), KillFont, Sides.Ally);
		Paint.TextCentred(FVector2D(TopLeft.X + Width * 0.86f, Middle), FString::FromInt(EnemyKills), KillFont, Sides.Enemy);
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

	/** Each side's Team Flux, the viewer's first, top left (ADR-011 §10). */
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
		const FSlateFontInfo Label = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		const FSlateFontInfo Value = Paint.Font(TEXT("Bold"), Settings.HudHeadingFontSize);
		const FSlateFontInfo Note = Paint.Font(TEXT("Regular"), Settings.HudSmallFontSize);
		const float Row = Paint.S(26.0f);
		const FVector2D TopLeft(Inset.X + Paint.S(Settings.DeckGap), Inset.Y + Paint.S(Settings.DeckGap));
		const FVector2D Size(Paint.S(250.0f), Paint.S(Settings.DeckPadding) + Row * Teams.Num() + Paint.S(18.0f));
		Paint.Surface(TopLeft, Size);
		Paint.Text(TopLeft + FVector2D(Paint.S(Settings.DeckPadding), Paint.S(6.0f)), TEXT("TEAM FLUX"), Label, Settings.HudAccentColor);
		float Y = TopLeft.Y + Paint.S(22.0f);
		for (const FVeyraHudTeamFlux& Team : Teams)
		{
			const FLinearColor Side = Team.Team == Viewer ? Sides.Ally : Sides.Enemy;
			const float X = TopLeft.X + Paint.S(Settings.DeckPadding);
			Paint.Rect(FVector2D(X, Y + Paint.S(4.0f)), FVector2D(Paint.S(3.0f), Row - Paint.S(8.0f)), Side);
			Paint.Text(FVector2D(X + Paint.S(10.0f), Y), FString::Printf(TEXT("%.0f"), Team.Active), Value, Settings.TextColor);
			FString Detail = FString::Printf(TEXT("%.0f permanent   Fluxborn +%.0f%%"), Team.Permanent, Team.FluxbornBonus * Percent);
			for (const double Left : Team.TemporarySeconds)
			{
				Detail += FString::Printf(TEXT("   %ds"), FMath::CeilToInt32(Left));
			}
			Paint.Text(FVector2D(X + Paint.S(60.0f), Y + Paint.S(5.0f)), Detail, Note, Settings.DescriptionColor);
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

	/** The hovered slot's tooltip, centred over the deck at DeckCentre. */
	void DrawTooltip(const FPainter& Paint, const FHover& Hover, float DeckTop, float DeckCentre)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const float Pad = Paint.S(Settings.DeckPadding);
		const float Width = Paint.S(Settings.TooltipWidth);
		const FSlateFontInfo TitleFont = Paint.Font(TEXT("Bold"), Settings.HudHeadingFontSize);
		const FSlateFontInfo DetailFont = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		const FSlateFontInfo BodyFont = Paint.Font(TEXT("Regular"), Settings.HudBodyFontSize);
		const TArray<FString> Lines = Paint.Wrap(Hover.Description, BodyFont, Width - Pad * 2.0f);
		TArray<FString> NumberLines;
		for (const FString& Number : Hover.Numbers)
		{
			NumberLines.Append(Paint.Wrap(Number, DetailFont, Width - Pad * 2.0f));
		}
		const float LineHeight = Paint.Measure(TEXT("Ag"), BodyFont).Y;
		const float NumberHeight = Paint.Measure(TEXT("Ag"), DetailFont).Y;
		const float TitleHeight = Paint.Measure(Hover.Title, TitleFont).Y;
		const float DetailHeight = Hover.Detail.IsEmpty() ? 0.0f : Paint.Measure(Hover.Detail, DetailFont).Y;
		const float NumbersHeight = NumberLines.IsEmpty() ? 0.0f : Paint.S(6.0f) + NumberHeight * NumberLines.Num();
		const float Height = Pad * 2.0f + TitleHeight + DetailHeight + NumbersHeight + (Lines.IsEmpty() ? 0.0f : Paint.S(6.0f) + LineHeight * Lines.Num());
		const float Left = FMath::Clamp(DeckCentre - Width / 2.0f, 0.0f, FMath::Max(0.0f, Paint.Canvas.ClipX - Width));
		const FVector2D TopLeft(Left, DeckTop - Height - Paint.S(Settings.DeckGap));
		Paint.Surface(TopLeft, FVector2D(Width, Height));
		Paint.Rect(TopLeft, FVector2D(Width, Paint.S(2.0f)), Settings.HudAccentColor);
		float Y = TopLeft.Y + Pad;
		Paint.Text(FVector2D(TopLeft.X + Pad, Y), Hover.Title, TitleFont, Settings.TextColor);
		Y += TitleHeight;
		if (!Hover.Detail.IsEmpty())
		{
			Paint.Text(FVector2D(TopLeft.X + Pad, Y), Hover.Detail.ToUpper(), DetailFont, Settings.HudAccentColor);
			Y += DetailHeight;
		}
		if (!NumberLines.IsEmpty())
		{
			Y += Paint.S(6.0f);
			for (const FString& Line : NumberLines)
			{
				Paint.Text(FVector2D(TopLeft.X + Pad, Y), Line, DetailFont, Settings.TextColor);
				Y += NumberHeight;
			}
		}
		Y += Paint.S(6.0f);
		for (const FString& Line : Lines)
		{
			Paint.Text(FVector2D(TopLeft.X + Pad, Y), Line, BodyFont, Settings.DescriptionColor);
			Y += LineHeight;
		}
	}

	/** A bar filled Fill of the way in Color, with Label centred over it. */
	void DrawBar(const FPainter& Paint, const FVector2D& TopLeft, const FVector2D& Size, double Fill, const FLinearColor& Color, const FString& Label,
		double ShieldFill = 0.0)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		Paint.Rect(TopLeft, Size, Settings.BarBackgroundColor);
		const float Filled = Size.X * FMath::Clamp(Fill, 0.0, 1.0);
		Paint.Rect(TopLeft, FVector2D(Filled, Size.Y), Color);
		if (ShieldFill > 0.0)
		{
			Paint.Rect(TopLeft + FVector2D(Filled, 0.0f), FVector2D(Size.X * FMath::Clamp(ShieldFill, 0.0, 1.0 - Fill), Size.Y), Settings.ShieldColor);
		}
		// A light edge along the top gives the bar depth.
		Paint.Rect(TopLeft, FVector2D(Filled, FMath::Max(1.0f, Size.Y * 0.18f)), FLinearColor(1.0f, 1.0f, 1.0f, 0.18f));
		Paint.Outline(TopLeft, Size, FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		if (!Label.IsEmpty())
		{
			Paint.TextCentred(TopLeft + Size / 2.0, Label, Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor, true);
		}
	}

	/**
	 * The player's own statuses in a row just above the deck at DeckTopLeft (Proposals 38, 43, 53; ADR-059 §4): the beneficial,
	 * then the harmful, as Display orders them, each chip marked so its group reads without colour. Those that do not fit
	 * across the deck are counted at the row's end.
	 */
	void DrawStatusRow(const FPainter& Paint, const FVeyraStatusDisplay& Display, const AVeyraPlayerState& Participant, double Now, const FVector2D& DeckTopLeft,
		float DeckWidth)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const TArray<FVeyraHudStatus> Statuses = VeyraHud::OrderOwnStatuses(VeyraHud::StatusesOf(Participant, Now, Participant.GetVeyraTeam()), Display.Sort);
		if (Statuses.IsEmpty())
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

	/** A painter like Like at Scale pixels per designed unit: one of the deck's sections, or the team panels (ADR-059 §1). */
	FPainter PainterAt(const FPainter& Like, float Scale)
	{
		FPainter Painter = Like;
		Painter.Scale = Scale;
		return Painter;
	}

	/**
	 * The deck along the bottom, measured as Deck and placed at TopLeft, each section at its own scale (ADR-059 §1); returns
	 * its top edge, and what the cursor rests on.
	 */
	float DrawDeck(const FPainter& Paint, const FVeyraDeckGeometry& Deck, const FVector2D& TopLeft, const FVeyraCooldownDisplay& Cooldowns,
		const FVeyraStatusDisplay& Statuses, const AVeyraPlayerState& Participant, const UVeyraInputSettings& Input, const FString& ShopKey,
		const TOptional<FVector2D>& Mouse, double Now, float RankMarkOpacity, TOptional<FHover>& Hover)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const FVeyraHudPlayer Player = VeyraHud::DescribePlayer(Participant, Now);
		const FPainter Frame = PainterAt(Paint, Deck.Scales.Base);
		const FPainter Bar = PainterAt(Paint, Deck.Scales.AbilityBar);
		const FPainter Vitals = PainterAt(Paint, Deck.Scales.Vitals);
		const FPainter Spells = PainterAt(Paint, Deck.Scales.Spells);
		const FPainter Items = PainterAt(Paint, Deck.Scales.Items);
		Frame.Surface(TopLeft, Deck.Size);
		Frame.Rect(TopLeft, FVector2D(Deck.Size.X, Frame.S(2.0f)), Settings.HudAccentColor.CopyWithNewOpacity(0.5f));

		// The portrait, with the level on it and XP along its foot.
		const float Portrait = Deck.Portrait;
		const FVector2D PortraitAt = TopLeft + Deck.PortraitAt;
		DrawFace(Bar, Player.Vanguard, PortraitAt, Portrait, Settings.HudClockFontSize);
		Bar.Outline(PortraitAt, FVector2D(Portrait), Settings.HudHairlineColor);
		const float XpHeight = Bar.S(5.0f);
		const double Xp = Player.ExperienceToNextLevel > 0 ? static_cast<double>(Player.Experience) / Player.ExperienceToNextLevel : 1.0;
		Bar.Rect(PortraitAt + FVector2D(0.0f, Portrait - XpHeight), FVector2D(Portrait, XpHeight), FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
		Bar.Rect(PortraitAt + FVector2D(0.0f, Portrait - XpHeight), FVector2D(Portrait * Xp, XpHeight), Settings.HudAccentColor);
		const FSlateFontInfo LevelFont = Bar.Font(TEXT("Black"), Settings.HudBodyFontSize);
		const FString Level = FString::FromInt(Player.Level);
		const FVector2D LevelSize(Bar.S(26.0f), Bar.S(22.0f));
		const FVector2D LevelAt = PortraitAt + FVector2D(Portrait, Portrait) - LevelSize - FVector2D(0.0f, XpHeight);
		Bar.Rect(LevelAt, LevelSize, Settings.HudSurfaceColor.CopyWithNewOpacity(0.95f));
		Bar.Outline(LevelAt, LevelSize, Settings.HudAccentColor.CopyWithNewOpacity(0.7f));
		Bar.TextCentred(LevelAt + LevelSize / 2.0, Level, LevelFont, Settings.TextColor);
		if (Contains(PortraitAt, FVector2D(Portrait), Mouse))
		{
			FString Title = VeyraContentText::VanguardName(Player.Vanguard).ToString();
			Hover = FHover{ Title, FString::Printf(TEXT("Level %d   XP %d / %d"), Player.Level, Player.Experience, Player.ExperienceToNextLevel),
				VeyraContentText::VanguardTitle(Player.Vanguard).ToString() };
		}

		// The passive, then Q W E R.
		const float Ability = Deck.Ability;
		const float Small = Deck.Passive;
		const float Gap = Deck.AbilityGap;
		const float Pip = Deck.Pip;
		float X = TopLeft.X + Deck.AbilitiesAt.X;
		const float RowTop = TopLeft.Y + Deck.AbilitiesAt.Y;
		const FVector2D PassiveAt(X, RowTop + (Ability - Small) / 2.0f);
		Bar.Rect(PassiveAt, FVector2D(Small), Settings.BarBackgroundColor);
		Bar.Outline(PassiveAt, FVector2D(Small), Settings.HudHairlineColor);
		const FString PassiveName = Player.Passive.IsValid() ? VeyraContentText::PassiveName(Player.Passive).ToString() : FString();
		if (!DrawIcon(Bar, VeyraShellArt::AbilityIconOf(Player.Passive.ToString()), PassiveAt, Small))
		{
			Bar.TextCentred(PassiveAt + FVector2D(Small / 2.0f), Monogram(PassiveName), Bar.Font(TEXT("Bold"), Settings.HudBodyFontSize), Settings.DescriptionColor);
		}
		if (Player.Passive.IsValid() && Contains(PassiveAt, FVector2D(Small), Mouse))
		{
			Hover = FHover{ PassiveName, TEXT("Passive"), VeyraContentText::PassiveDescription(Player.Passive).ToString() };
		}
		X += Small + Gap;
		const FSlateFontInfo NameFont = Bar.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		for (const FVeyraHudSlot& Slot : Player.Slots)
		{
			const FVector2D At(X, RowTop);
			const bool bLearned = Slot.Rank > 0;
			const FString Name = Slot.Ability.IsValid() ? VeyraContentText::AbilityName(Slot.Ability).ToString() : FString();
			Bar.Rect(At, FVector2D(Ability), Settings.BarBackgroundColor);
			if (!DrawIcon(Bar, VeyraShellArt::SlotIconOf(Slot.Ability.ToString(), Slot.OwnAbility.ToString()), At, Ability))
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
			if (!bLearned)
			{
				Bar.Rect(At, FVector2D(Ability), Settings.ShadeColor);
			}
			DrawCooldown(Bar, Cooldowns, At, Ability, bLearned ? Slot.CooldownSeconds : 0.0, Slot.CooldownTotal);
			const bool bEmpowered = Slot.EmpoweredSeconds > 0.0;
			const bool bReady = bLearned && Slot.CooldownSeconds <= 0.0;
			Bar.Outline(At, FVector2D(Ability), bEmpowered ? Settings.EmpoweredColor : ReadyOutline(Settings, bReady),
				bEmpowered ? Bar.S(2.0f) : 1.0f);
			Bar.KeyCap(At, KeyName(Input.GetAbilityKey(Slot.Slot)));
			// Its ranks as pips under it.
			const float PipWidth = Slot.MaxRank > 0 ? (Ability - (Slot.MaxRank - 1) * Bar.S(2.0f)) / Slot.MaxRank : 0.0f;
			for (int32 Rank = 0; Rank < Slot.MaxRank; ++Rank)
			{
				const FVector2D PipAt(At.X + Rank * (PipWidth + Bar.S(2.0f)), At.Y + Ability + Gap / 2.0f);
				Bar.Rect(PipAt, FVector2D(PipWidth, Pip), Rank < Slot.Rank ? Settings.HudAccentColor : Settings.BarBackgroundColor);
			}
			// A skill point to spend on it: a lit mark above the tile, pulsing while it waits (ADR-065 §5).
			if (Slot.bCanRankUp)
			{
				const FVector2D MarkSize(Bar.S(22.0f), Bar.S(16.0f));
				const FVector2D MarkAt(At.X + (Ability - MarkSize.X) / 2.0f, At.Y - MarkSize.Y - Bar.S(4.0f));
				Bar.Rect(MarkAt, MarkSize, Settings.HudAccentColor.CopyWithNewOpacity(Settings.HudAccentColor.A * RankMarkOpacity));
				Bar.TextCentred(MarkAt + MarkSize / 2.0, TEXT("+"), Bar.Font(TEXT("Black"), Settings.HudBodyFontSize), Settings.HudSurfaceColor.CopyWithNewOpacity(1.0f));
			}
			if (Slot.Ability.IsValid() && Contains(At, FVector2D(Ability), Mouse))
			{
				FString Detail = FString::Printf(TEXT("Rank %d / %d"), Slot.Rank, Slot.MaxRank);
				if (Slot.bCanRankUp)
				{
					Detail += FString::Printf(TEXT("   %s+%s to rank up"), *KeyName(Input.RankUpModifierKey), *KeyName(Input.GetAbilityKey(Slot.Slot)));
				}
				if (bEmpowered)
				{
					Detail += FString::Printf(TEXT("   next attack empowered %.1f s"), Slot.EmpoweredSeconds);
				}
				Hover = FHover{ Name, Detail, VeyraContentText::AbilityDescription(Slot.Ability).ToString(), Slot.Numbers };
			}
			X += Ability + Gap;
		}

		// Health and the resource under the abilities, as wide as they are.
		const FVector2D BarsAt = TopLeft + Deck.VitalsAt;
		const float BarsWidth = Deck.AbilityRow;
		const double Total = FMath::Max(Player.Vitals.MaxHealth, Player.Vitals.Health + Player.Vitals.Shield);
		const FString HealthLabel = Player.Vitals.Shield > 0.0
			? FString::Printf(TEXT("%.0f / %.0f  +%.0f"), Player.Vitals.Health, Player.Vitals.MaxHealth, Player.Vitals.Shield)
			: FString::Printf(TEXT("%.0f / %.0f"), Player.Vitals.Health, Player.Vitals.MaxHealth);
		DrawBar(Vitals, BarsAt, FVector2D(BarsWidth, Deck.Health), Total > 0.0 ? Player.Vitals.Health / Total : 0.0, Settings.HealthColor, HealthLabel,
			Total > 0.0 ? Player.Vitals.Shield / Total : 0.0);
		if (Player.Vitals.MaxResource > 0.0)
		{
			DrawBar(Vitals, BarsAt + FVector2D(0.0f, Deck.Health + Deck.BarSpacing), FVector2D(BarsWidth, Deck.Resource), Player.Vitals.Resource / Player.Vitals.MaxResource,
				Settings.ResourceColorOf(Player.Vitals.Family), FString());
		}

		// The Flux Spells and, under them, the vision tool.
		X = TopLeft.X + Deck.SpellsAt.X;
		const float SpellTop = TopLeft.Y + Deck.SpellsAt.Y;
		for (const FVeyraHudSpellSlot& Spell : Player.Spells)
		{
			const FVector2D At(X, SpellTop);
			const FString Name = Spell.Spell.IsValid() ? VeyraContentText::AbilityName(Spell.Spell).ToString() : FString();
			Spells.Rect(At, FVector2D(Deck.Spell), Settings.BarBackgroundColor);
			if (!DrawIcon(Spells, VeyraShellArt::AbilityIconOf(Spell.Spell.ToString()), At, Deck.Spell))
			{
				Spells.TextCentred(At + FVector2D(Deck.Spell / 2.0f), Monogram(Name), Spells.Font(TEXT("Bold"), Settings.HudBodyFontSize), Settings.TextColor);
			}
			if (Spell.bLocked)
			{
				Spells.Rect(At, FVector2D(Deck.Spell), Settings.ShadeColor);
			}
			DrawCooldown(Spells, Cooldowns, At, Deck.Spell, Spell.bLocked ? 0.0 : Spell.CooldownSeconds, Spell.CooldownTotal);
			Spells.Outline(At, FVector2D(Deck.Spell), ReadyOutline(Settings, !Spell.bLocked && Spell.Spell.IsValid() && Spell.CooldownSeconds <= 0.0));
			Spells.KeyCap(At, KeyName(Input.GetAbilityKey(Spell.Slot)));
			if (Spell.Spell.IsValid() && Contains(At, FVector2D(Deck.Spell), Mouse))
			{
				const FString Detail = Spell.bLocked ? FString::Printf(TEXT("Flux Spell   locked until %.0f permanent Team Flux"), Spell.UnlockFlux) : FString(TEXT("Flux Spell"));
				Hover = FHover{ Name, Detail, VeyraContentText::AbilityDescription(Spell.Spell).ToString() };
			}
			X += Deck.Spell + Deck.SpellGap;
		}
		if (Player.VisionTool.bPresent)
		{
			const bool bWard = Player.VisionTool.Tool == EVeyraVisionTool::PersistentWard;
			const FString Label = bWard ? FString::Printf(TEXT("Ward %d/%d"), Player.VisionTool.WardCharges, Player.VisionTool.MaxWardCharges)
										: FString(Player.VisionTool.Tool == EVeyraVisionTool::Sweeper ? TEXT("Sweeper") : TEXT("Quick Sight"));
			const FString Key = KeyName(Input.GetAbilityKey(EVeyraAbilitySlot::VisionTool));
			const FSlateFontInfo ToolFont = Spells.Font(TEXT("Bold"), Settings.HudSmallFontSize);
			const FString Line = FString::Printf(TEXT("%s  %s"), *Key, *Label);
			const double Waiting = bWard ? Player.VisionTool.NextChargeSeconds : Player.VisionTool.CooldownSeconds;
			const FString Shown = Waiting > 0.0 ? FString::Printf(TEXT("%s  %ss"), *Line, *VeyraHud::CooldownLabel(Waiting, Cooldowns.bTenths)) : Line;
			Spells.TextCentred(TopLeft + Deck.ToolAt + FVector2D(Deck.SpellsWidth / 2.0f, Deck.ToolLine / 2.0f), Shown, ToolFont, Settings.DescriptionColor);
		}

		// The items, two rows of three, with Gold and the shop under them.
		const float ItemsLeft = TopLeft.X + Deck.ItemsAt.X;
		const float ItemsTop = TopLeft.Y + Deck.ItemsAt.Y;
		const float Item = Deck.Item;
		for (int32 Index = 0; Index < Player.Items.Num(); ++Index)
		{
			const FVeyraHudItemSlot& Held = Player.Items[Index];
			const FVector2D At(ItemsLeft + (Index % 3) * (Item + Deck.ItemGap), ItemsTop + (Index / 3) * (Item + Deck.ItemGap));
			Items.Rect(At, FVector2D(Item), Settings.BarBackgroundColor);
			if (Held.Item.IsValid())
			{
				const FString Name = VeyraContentText::ItemName(Held.Item).ToString();
				if (!DrawIcon(Items, VeyraShellArt::ItemIconOf(Held.Item.ToString()), At, Item))
				{
					Items.TextCentred(At + FVector2D(Item / 2.0f), Monogram(Name), Items.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor);
				}
				// A stack shows how many; a refillable consumable its charges, even none (ADR-023 §6).
				if (Held.Charges.IsSet() || Held.Count > 1)
				{
					Items.Text(At + FVector2D(Item - Items.S(14.0f), Item - Items.S(16.0f)), FString::FromInt(Held.Charges.Get(Held.Count)),
						Items.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor, true);
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
				DrawCooldown(Items, Cooldowns, At, Item, Held.CooldownSeconds, Held.CooldownTotal);
				if (Contains(At, FVector2D(Item), Mouse))
				{
					Hover = FHover{ Name, FString::Printf(TEXT("Item   %s"), *KeyName(Input.GetAbilityKey(Held.Slot))), VeyraContentText::ItemDescription(Held.Item).ToString() };
				}
			}
			// An item whose Active is ready shows ready, as an ability does.
			Items.Outline(At, FVector2D(Item), ReadyOutline(Settings, Held.Item.IsValid() && Held.bActive && Held.CooldownSeconds <= 0.0));
			Items.KeyCap(At, KeyName(Input.GetAbilityKey(Held.Slot)));
		}
		const FString Gold = FString::Printf(TEXT("%s"), *FText::AsNumber(Player.Gold).ToString());
		const float GoldY = ItemsTop + 2.0f * Item + Deck.ItemGap * 1.5f;
		Items.Text(FVector2D(ItemsLeft, GoldY), Gold, Items.Font(TEXT("Black"), Settings.HudBodyFontSize), Settings.GoldColor);
		const FString Shop = Player.PendingPurchases > 0 ? FString::Printf(TEXT("%s  shop   %d at the fountain"), *ShopKey, Player.PendingPurchases)
														 : FString::Printf(TEXT("%s  shop"), *ShopKey);
		const FSlateFontInfo ShopFont = Items.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		Items.Text(FVector2D(ItemsLeft + Deck.ItemsWidth - Items.Measure(Shop, ShopFont).X, GoldY + Items.S(2.0f)), Shop, ShopFont, Settings.DescriptionColor);

		// The player's own statuses just above the deck (ADR-059 §4).
		DrawStatusRow(Frame, Statuses, Participant, Now, TopLeft, Deck.Size.X);

		// Recalling: the channel over the deck, filling toward home (ADR-012 §8).
		const float DeckCentre = TopLeft.X + Deck.Size.X / 2.0f;
		if (Player.bRecalling)
		{
			const FVector2D Size(Frame.S(320.0f), Frame.S(12.0f));
			const FVector2D At(DeckCentre - Size.X / 2.0f, TopLeft.Y - Size.Y - Frame.S(34.0f));
			DrawBar(Frame, At, Size, Player.RecallProgress, Settings.ChannelColor, FString());
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

		// Dead: the world dims, and the wait for the fountain counts down (Economy & Progression Bible §14).
		if (Player.bDead)
		{
			Frame.Rect(FVector2D::ZeroVector, FVector2D(Frame.Canvas.ClipX, TopLeft.Y), Settings.ShadeColor.CopyWithNewOpacity(Settings.ShadeColor.A * 0.6f));
			const FVector2D Centre(Frame.Canvas.ClipX / 2.0f, Frame.Canvas.ClipY * 0.4f);
			Frame.TextCentred(Centre - FVector2D(0.0f, Frame.S(44.0f)), TEXT("RESPAWNING IN"), Frame.Font(TEXT("Bold"), Settings.HudBodyFontSize), Settings.DescriptionColor);
			Frame.TextCentred(Centre, FString::FromInt(FMath::CeilToInt32(Player.RespawnSeconds)), Frame.Font(TEXT("Black"), Settings.HudHeadlineFontSize),
				Settings.TextColor, true);
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
	const FPainter Paint(Canvas, Settings, Font, Preferences.HudScale);
	// The team panels at their own scale, every edge-anchored part inside the safe area (ADR-059 §1-§2).
	const FPainter Panels = PainterAt(Paint, Layout.TeamPanels);
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
	const float DeckTop = DrawDeck(Paint, Layout.Deck, Layout.DeckTopLeft, Preferences.Cooldowns, Preferences.Statuses, *Own,
		Player ? Player->GetKeys() : *GetDefault<UVeyraInputSettings>(), ShopKey, Mouse, ServerNow, RankMarkOpacity, Hover);
	if (Player)
	{
		DrawChat(Paint, Preferences, Layout.Chat, *Player, GameState, Side, Screens && Screens->IsChatOpen());
	}
	if (Hover.IsSet())
	{
		DrawTooltip(Paint, Hover.GetValue(), DeckTop, Layout.DeckTopLeft.X + Layout.Deck.Size.X / 2.0f);
	}
}

void DrawHeadline(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const UFont* FontAsset, const FString& Text, const FLinearColor& Color)
{
	const FPainter Paint(Canvas, Settings, FontAsset);
	FSlateFontInfo Font = Paint.Font(TEXT("Black"), Settings.HudHeadlineFontSize);
	Font.LetterSpacing = 120;
	const FVector2D Centre(Canvas.ClipX / 2.0f, Canvas.ClipY / 3.0f);
	const FVector2D Size = Paint.Measure(Text, Font);
	// A band of shade behind it, so it reads over the fallen Well.
	Paint.Rect(FVector2D(0.0f, Centre.Y - Size.Y), FVector2D(Canvas.ClipX, Size.Y * 2.0f), Settings.ShadeColor);
	Paint.TextCentred(Centre, Text.ToUpper(), Font, Color, true);
}

void DrawLevelUp(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const UFont* FontAsset, int32 Level, int32 UnspentPoints, double Shown)
{
	const FPainter Paint(Canvas, Settings, FontAsset);
	// Whole until its last part, then fading out.
	const float Opacity = static_cast<float>(FMath::Clamp((1.0 - Shown) / Settings.LevelUpFadeShare, 0.0, 1.0));
	FSlateFontInfo Font = Paint.Font(TEXT("Black"), Settings.LevelUpFontSize);
	Font.LetterSpacing = 120;
	const FString Headline = FString::Printf(TEXT("LEVEL %d"), Level);
	const FVector2D Centre(Canvas.ClipX / 2.0f, Canvas.ClipY * Settings.LevelUpHeightShare);
	Paint.TextCentred(Centre, Headline, Font, Settings.GoldColor.CopyWithNewOpacity(Opacity), true);
	if (UnspentPoints > 0)
	{
		const FSlateFontInfo Note = Paint.Font(TEXT("Bold"), Settings.HudBodyFontSize);
		const float Below = Paint.Measure(Headline, Font).Y * 0.5f + Paint.Measure(TEXT("Ag"), Note).Y;
		Paint.TextCentred(Centre + FVector2D(0.0f, Below), UnspentPoints > 1 ? FString::Printf(TEXT("%d skill points to spend"), UnspentPoints) : FString(TEXT("A skill point to spend")),
			Note, Settings.TextColor.CopyWithNewOpacity(Opacity), true);
	}
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
		const float Opacity = static_cast<float>(FMath::Clamp((1.0 - Row.Progress) / FadeShare, 0.0, 1.0));
		// The killer's side: a takedown's killer, else the side that took the fallen.
		const bool bKiller = Line.KillerPlayerId != INDEX_NONE;
		const FString KillerText = bKiller ? Line.KillerName : Line.Kind == EVeyraKillFeedKind::Execution ? FString(TEXT("Executed")) : FString(TEXT("Fluxborn"));
		const FLinearColor KillerColor = SideColor(bKiller ? Line.KillerSide : Line.Kind == EVeyraKillFeedKind::Execution ? EVeyraTeam::None : VeyraTeams::Opposing(Line.VictimSide));
		const bool bStructure = Line.Kind == EVeyraKillFeedKind::Structure;
		const FString VictimText = bStructure ? VeyraKillFeedView::StructureName(Line) : Line.VictimName;
		const FString Arrow = Line.Assists > 0 ? FString::Printf(TEXT("+%d  >"), Line.Assists) : FString(TEXT(">"));
		const float KillerWidth = (bKiller ? Face + Gap : 0.0f) + Paint.Measure(KillerText, NameFont).X;
		const float ArrowWidth = Paint.Measure(Arrow, NameFont).X;
		const float VictimWidth = (bStructure ? 0.0f : Face + Gap) + Paint.Measure(VictimText, NameFont).X;
		const float Width = Gap * 6.0f + KillerWidth + ArrowWidth + VictimWidth;
		const float Height = Face + Gap;
		const FVector2D TopLeft(Right - Width, Y);
		Paint.Rect(TopLeft, FVector2D(Width, Height), Settings.HudSurfaceColor.CopyWithNewOpacity(Settings.HudSurfaceColor.A * Opacity));
		if (Line.bFirstBlood)
		{
			Paint.Outline(TopLeft, FVector2D(Width, Height), Settings.GoldColor.CopyWithNewOpacity(Opacity));
		}
		const float TextY = Y + (Height - Paint.Measure(TEXT("Ag"), NameFont).Y) / 2.0f;
		float X = TopLeft.X + Gap * 2.0f;
		if (bKiller)
		{
			DrawFace(Paint, Line.KillerVanguard, FVector2D(X, Y + Gap / 2.0f), Face, Settings.HudSmallFontSize);
			X += Face + Gap;
		}
		Paint.Text(FVector2D(X, TextY), KillerText, NameFont, KillerColor.CopyWithNewOpacity(Opacity), true);
		X += Paint.Measure(KillerText, NameFont).X + Gap;
		Paint.Text(FVector2D(X, TextY), Arrow, NameFont, Settings.TextColor.CopyWithNewOpacity(Opacity), true);
		X += ArrowWidth + Gap;
		if (!bStructure)
		{
			DrawFace(Paint, Line.VictimVanguard, FVector2D(X, Y + Gap / 2.0f), Face, Settings.HudSmallFontSize);
			X += Face + Gap;
		}
		Paint.Text(FVector2D(X, TextY), VictimText, NameFont, SideColor(Line.VictimSide).CopyWithNewOpacity(Opacity), true);
		Y += Height + Gap / 2.0f;
	}
}

void DrawAnnouncement(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, const UFont* FontAsset,
	const FVeyraAnnouncement& Announcement, const FVector2D& Inset)
{
	const FPainter Paint(Canvas, Settings, FontAsset, Preferences.HudScale);
	const float Opacity = static_cast<float>(FMath::Clamp((1.0 - Announcement.Progress) / Settings.LevelUpFadeShare, 0.0, 1.0));
	FSlateFontInfo Font = Paint.Font(TEXT("Black"), Settings.AnnouncementFontSize);
	Font.LetterSpacing = 60;
	const FVector2D Size = Paint.Measure(Announcement.Text, Font) + FVector2D(Paint.S(32.0f), Paint.S(12.0f));
	// Below the top strip and its notices, above the battleground's middle.
	const FVector2D TopLeft((Canvas.ClipX - Size.X) / 2.0f, Inset.Y + Paint.S(Settings.DeckGap) + Paint.S(120.0f));
	Paint.Rect(TopLeft, Size, Settings.HudSurfaceColor.CopyWithNewOpacity(Settings.HudSurfaceColor.A * Opacity));
	const FLinearColor Color = Announcement.bGood ? Preferences.SideColors.Ally : Preferences.SideColors.Enemy;
	Paint.Outline(TopLeft, Size, Color.CopyWithNewOpacity(0.6f * Opacity));
	Paint.TextCentred(TopLeft + Size / 2.0, Announcement.Text, Font, Color.CopyWithNewOpacity(Opacity), true);
}
}
