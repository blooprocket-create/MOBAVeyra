// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraHudDeck.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Hud/VeyraHudModel.h"
#include "Input/VeyraInputSettings.h"
#include "Rendering/SlateRenderer.h"
#include "Shell/VeyraShellArt.h"
#include "Shell/VeyraUIInputSettings.h"
#include "Statistics/VeyraScoreComponent.h"
#include "Styling/CoreStyle.h"
#include "Text/VeyraContentText.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "VeyraGameState.h"
#include "VeyraPlayerController.h"
#include "VeyraPlayerState.h"

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

		FPainter(UCanvas& InCanvas, const UVeyraGreyboxSettings& InSettings, const UFont* InFont)
			: Canvas(InCanvas)
			, Settings(InSettings)
			, FontAsset(InFont)
			, Scale(InSettings.HudReferenceHeight > 0.0f ? InCanvas.ClipY / InSettings.HudReferenceHeight : 1.0f)
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
	};

	FString KeyName(const FKey& Key)
	{
		return Key.GetDisplayName(false).ToString();
	}

	FString Seconds(double Value)
	{
		return Value >= 10.0 ? FString::Printf(TEXT("%d"), FMath::CeilToInt32(Value)) : FString::Printf(TEXT("%.1f"), Value);
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

	/** A cooling or locked slot's shade, filling as much of the tile as is left, with the seconds over it. */
	void DrawCooldown(const FPainter& Paint, const FVector2D& TopLeft, float Side, double SecondsLeft)
	{
		if (SecondsLeft <= 0.0)
		{
			return;
		}
		Paint.Rect(TopLeft, FVector2D(Side), Paint.Settings.ShadeColor);
		Paint.TextCentred(TopLeft + FVector2D(Side / 2.0f), Seconds(SecondsLeft), Paint.Font(TEXT("Bold"), Paint.Settings.HudHeadingFontSize), Paint.Settings.TextColor,
			true);
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

	/** The top strip: each side's kills around the match clock. */
	void DrawTopStrip(const FPainter& Paint, const AVeyraGameState& GameState, EVeyraTeam Viewer)
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
		const FVector2D TopLeft((Paint.Canvas.ClipX - Width) / 2.0f, Paint.S(Settings.DeckGap));
		Paint.Surface(TopLeft, FVector2D(Width, Height));
		const float Middle = TopLeft.Y + Height / 2.0f;
		Paint.TextCentred(FVector2D(Paint.Canvas.ClipX / 2.0f, Middle), ClockText, ClockFont, Settings.TextColor);
		Paint.TextCentred(FVector2D(TopLeft.X + Width * 0.14f, Middle), FString::FromInt(OwnKills), KillFont, Settings.AllyColor);
		Paint.TextCentred(FVector2D(TopLeft.X + Width * 0.86f, Middle), FString::FromInt(EnemyKills), KillFont, Settings.EnemyColor);
	}

	/** Each side's Team Flux, the viewer's first, top left (ADR-011 §10). */
	void DrawTeamFlux(const FPainter& Paint, const UWorld& World, EVeyraTeam Viewer, double Now)
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
		const FVector2D TopLeft(Paint.S(Settings.DeckGap), Paint.S(Settings.DeckGap));
		const FVector2D Size(Paint.S(250.0f), Paint.S(Settings.DeckPadding) + Row * Teams.Num() + Paint.S(18.0f));
		Paint.Surface(TopLeft, Size);
		Paint.Text(TopLeft + FVector2D(Paint.S(Settings.DeckPadding), Paint.S(6.0f)), TEXT("TEAM FLUX"), Label, Settings.HudAccentColor);
		float Y = TopLeft.Y + Paint.S(22.0f);
		for (const FVeyraHudTeamFlux& Team : Teams)
		{
			const FLinearColor Side = Team.Team == Viewer ? Settings.AllyColor : Settings.EnemyColor;
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
	void DrawNotices(const FPainter& Paint, const AVeyraGameState& GameState, const APlayerController* Viewer)
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
		// A team's vote reaches only that team, as League shows a surrender.
		if (Vote.bOpen && Own)
		{
			const UVeyraInputSettings& Input = *GetDefault<UVeyraInputSettings>();
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
		float Y = Paint.S(Settings.DeckGap) + Paint.S(40.0f) + Paint.S(Settings.DeckGap);
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
	void DrawTooltip(const FPainter& Paint, const FHover& Hover, float DeckTop)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const float Pad = Paint.S(Settings.DeckPadding);
		const float Width = Paint.S(Settings.TooltipWidth);
		const FSlateFontInfo TitleFont = Paint.Font(TEXT("Bold"), Settings.HudHeadingFontSize);
		const FSlateFontInfo DetailFont = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		const FSlateFontInfo BodyFont = Paint.Font(TEXT("Regular"), Settings.HudBodyFontSize);
		const TArray<FString> Lines = Paint.Wrap(Hover.Description, BodyFont, Width - Pad * 2.0f);
		const float LineHeight = Paint.Measure(TEXT("Ag"), BodyFont).Y;
		const float TitleHeight = Paint.Measure(Hover.Title, TitleFont).Y;
		const float DetailHeight = Hover.Detail.IsEmpty() ? 0.0f : Paint.Measure(Hover.Detail, DetailFont).Y;
		const float Height = Pad * 2.0f + TitleHeight + DetailHeight + (Lines.IsEmpty() ? 0.0f : Paint.S(6.0f) + LineHeight * Lines.Num());
		const FVector2D TopLeft((Paint.Canvas.ClipX - Width) / 2.0f, DeckTop - Height - Paint.S(Settings.DeckGap));
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

	/** The deck along the bottom; returns its top edge, and what the cursor rests on. */
	float DrawDeck(const FPainter& Paint, const AVeyraPlayerState& Participant, const TOptional<FVector2D>& Mouse, double Now, TOptional<FHover>& Hover)
	{
		const UVeyraGreyboxSettings& Settings = Paint.Settings;
		const UVeyraInputSettings& Input = *GetDefault<UVeyraInputSettings>();
		const FVeyraHudPlayer Player = VeyraHud::DescribePlayer(Participant, Now);
		const float Gap = Paint.S(Settings.DeckGap);
		const float Pad = Paint.S(Settings.DeckPadding);
		const float Ability = Paint.S(Settings.AbilityTileSize);
		const float Small = Paint.S(Settings.SmallTileSize);
		const float Item = Paint.S(Settings.ItemTileSize);
		const float Portrait = Paint.S(Settings.PortraitSize);
		const float Pip = Paint.S(5.0f);
		const float HealthHeight = Paint.S(Settings.DeckHealthHeight);
		const float ResourceHeight = Paint.S(Settings.DeckResourceHeight);

		const int32 AbilityCount = Player.Slots.Num();
		const float AbilitiesWidth = AbilityCount * Ability + FMath::Max(0, AbilityCount - 1) * Gap;
		const float CentreWidth = Small + Gap + AbilitiesWidth;
		const float SpellsWidth = 2.0f * Small + Gap;
		const float ItemsWidth = 3.0f * Item + 2.0f * Gap;
		const float Width = Pad * 2.0f + Portrait + Gap * 2.0f + CentreWidth + Gap * 2.0f + SpellsWidth + Gap * 2.0f + ItemsWidth;
		const float Height = Pad * 2.0f + Ability + Gap + Pip + Gap + HealthHeight + Paint.S(3.0f) + ResourceHeight;
		const FVector2D TopLeft((Paint.Canvas.ClipX - Width) / 2.0f, Paint.Canvas.ClipY - Height - Gap);
		Paint.Surface(TopLeft, FVector2D(Width, Height));
		Paint.Rect(TopLeft, FVector2D(Width, Paint.S(2.0f)), Settings.HudAccentColor.CopyWithNewOpacity(0.5f));

		// The portrait, with the level on it and XP along its foot.
		const FVector2D PortraitAt = TopLeft + FVector2D(Pad, (Height - Portrait) / 2.0f);
		const FString VanguardId = Player.Vanguard.ToString();
		if (UTexture2D* Hero = VeyraShellArt::HeroOf(VanguardId); Hero && Hero->GetResource())
		{
			const FBox2f Crop = VeyraShellArt::Crop(VanguardId, Hero->GetSizeX(), Hero->GetSizeY(), 1.0f, true);
			FCanvasTileItem Face(PortraitAt, Hero->GetResource(), FVector2D(Portrait), FVector2D(Crop.Min), FVector2D(Crop.Max), FLinearColor::White);
			Paint.Canvas.DrawItem(Face);
		}
		else
		{
			Paint.Rect(PortraitAt, FVector2D(Portrait), Settings.BarBackgroundColor);
			Paint.TextCentred(PortraitAt + FVector2D(Portrait / 2.0f), Monogram(VeyraContentText::VanguardName(Player.Vanguard).ToString()),
				Paint.Font(TEXT("Black"), Settings.HudClockFontSize), Settings.TextColor);
		}
		Paint.Outline(PortraitAt, FVector2D(Portrait), Settings.HudHairlineColor);
		const float XpHeight = Paint.S(5.0f);
		const double Xp = Player.ExperienceToNextLevel > 0 ? static_cast<double>(Player.Experience) / Player.ExperienceToNextLevel : 1.0;
		Paint.Rect(PortraitAt + FVector2D(0.0f, Portrait - XpHeight), FVector2D(Portrait, XpHeight), FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
		Paint.Rect(PortraitAt + FVector2D(0.0f, Portrait - XpHeight), FVector2D(Portrait * Xp, XpHeight), Settings.HudAccentColor);
		const FSlateFontInfo LevelFont = Paint.Font(TEXT("Black"), Settings.HudBodyFontSize);
		const FString Level = FString::FromInt(Player.Level);
		const FVector2D LevelSize(Paint.S(26.0f), Paint.S(22.0f));
		const FVector2D LevelAt = PortraitAt + FVector2D(Portrait, Portrait) - LevelSize - FVector2D(0.0f, XpHeight);
		Paint.Rect(LevelAt, LevelSize, Settings.HudSurfaceColor.CopyWithNewOpacity(0.95f));
		Paint.Outline(LevelAt, LevelSize, Settings.HudAccentColor.CopyWithNewOpacity(0.7f));
		Paint.TextCentred(LevelAt + LevelSize / 2.0, Level, LevelFont, Settings.TextColor);
		if (Contains(PortraitAt, FVector2D(Portrait), Mouse))
		{
			FString Title = VeyraContentText::VanguardName(Player.Vanguard).ToString();
			Hover = FHover{ Title, FString::Printf(TEXT("Level %d   XP %d / %d"), Player.Level, Player.Experience, Player.ExperienceToNextLevel),
				VeyraContentText::VanguardTitle(Player.Vanguard).ToString() };
		}

		// The passive, then Q W E R.
		float X = PortraitAt.X + Portrait + Gap * 2.0f;
		const float RowTop = TopLeft.Y + Pad;
		const FVector2D PassiveAt(X, RowTop + (Ability - Small) / 2.0f);
		Paint.Rect(PassiveAt, FVector2D(Small), Settings.BarBackgroundColor);
		Paint.Outline(PassiveAt, FVector2D(Small), Settings.HudHairlineColor);
		const FString PassiveName = Player.Passive.IsValid() ? VeyraContentText::PassiveName(Player.Passive).ToString() : FString();
		if (!DrawIcon(Paint, VeyraShellArt::AbilityIconOf(Player.Passive.ToString()), PassiveAt, Small))
		{
			Paint.TextCentred(PassiveAt + FVector2D(Small / 2.0f), Monogram(PassiveName), Paint.Font(TEXT("Bold"), Settings.HudBodyFontSize), Settings.DescriptionColor);
		}
		if (Player.Passive.IsValid() && Contains(PassiveAt, FVector2D(Small), Mouse))
		{
			Hover = FHover{ PassiveName, TEXT("Passive"), VeyraContentText::PassiveDescription(Player.Passive).ToString() };
		}
		X += Small + Gap;
		const FSlateFontInfo NameFont = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		for (const FVeyraHudSlot& Slot : Player.Slots)
		{
			const FVector2D At(X, RowTop);
			const bool bLearned = Slot.Rank > 0;
			const FString Name = Slot.Ability.IsValid() ? VeyraContentText::AbilityName(Slot.Ability).ToString() : FString();
			Paint.Rect(At, FVector2D(Ability), Settings.BarBackgroundColor);
			if (!DrawIcon(Paint, VeyraShellArt::AbilityIconOf(Slot.Ability.ToString()), At, Ability))
			{
				// Until it has an icon, its name.
				float LineY = At.Y + Ability / 2.0f;
				const TArray<FString> Lines = Paint.Wrap(Name, NameFont, Ability - Paint.S(8.0f), 2);
				const float LineHeight = Paint.Measure(TEXT("Ag"), NameFont).Y;
				LineY -= LineHeight * Lines.Num() / 2.0f - LineHeight / 2.0f;
				for (const FString& Line : Lines)
				{
					Paint.TextCentred(FVector2D(At.X + Ability / 2.0f, LineY), Line, NameFont, bLearned ? Settings.TextColor : Settings.DescriptionColor);
					LineY += LineHeight;
				}
			}
			if (!bLearned)
			{
				Paint.Rect(At, FVector2D(Ability), Settings.ShadeColor);
			}
			DrawCooldown(Paint, At, Ability, bLearned ? Slot.CooldownSeconds : 0.0);
			const bool bEmpowered = Slot.EmpoweredSeconds > 0.0;
			const bool bReady = bLearned && Slot.CooldownSeconds <= 0.0;
			Paint.Outline(At, FVector2D(Ability), bEmpowered ? Settings.EmpoweredColor : bReady ? Settings.HudAccentColor.CopyWithNewOpacity(0.55f) : Settings.HudHairlineColor,
				bEmpowered ? Paint.S(2.0f) : 1.0f);
			Paint.KeyCap(At, KeyName(Input.GetAbilityKey(Slot.Slot)));
			// Its ranks as pips under it.
			const float PipWidth = Slot.MaxRank > 0 ? (Ability - (Slot.MaxRank - 1) * Paint.S(2.0f)) / Slot.MaxRank : 0.0f;
			for (int32 Rank = 0; Rank < Slot.MaxRank; ++Rank)
			{
				const FVector2D PipAt(At.X + Rank * (PipWidth + Paint.S(2.0f)), At.Y + Ability + Gap / 2.0f);
				Paint.Rect(PipAt, FVector2D(PipWidth, Pip), Rank < Slot.Rank ? Settings.HudAccentColor : Settings.BarBackgroundColor);
			}
			// A skill point to spend on it: a lit mark above the tile.
			if (Slot.bCanRankUp)
			{
				const FVector2D MarkSize(Paint.S(22.0f), Paint.S(16.0f));
				const FVector2D MarkAt(At.X + (Ability - MarkSize.X) / 2.0f, At.Y - MarkSize.Y - Paint.S(4.0f));
				Paint.Rect(MarkAt, MarkSize, Settings.HudAccentColor);
				Paint.TextCentred(MarkAt + MarkSize / 2.0, TEXT("+"), Paint.Font(TEXT("Black"), Settings.HudBodyFontSize), Settings.HudSurfaceColor.CopyWithNewOpacity(1.0f));
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
				Hover = FHover{ Name, Detail, VeyraContentText::AbilityDescription(Slot.Ability).ToString() };
			}
			X += Ability + Gap;
		}

		// Health and the resource under the abilities.
		const FVector2D BarsAt(PassiveAt.X, RowTop + Ability + Gap + Pip + Gap);
		const float BarsWidth = CentreWidth;
		const double Total = FMath::Max(Player.Vitals.MaxHealth, Player.Vitals.Health + Player.Vitals.Shield);
		const FString HealthLabel = Player.Vitals.Shield > 0.0
			? FString::Printf(TEXT("%.0f / %.0f  +%.0f"), Player.Vitals.Health, Player.Vitals.MaxHealth, Player.Vitals.Shield)
			: FString::Printf(TEXT("%.0f / %.0f"), Player.Vitals.Health, Player.Vitals.MaxHealth);
		DrawBar(Paint, BarsAt, FVector2D(BarsWidth, HealthHeight), Total > 0.0 ? Player.Vitals.Health / Total : 0.0, Settings.HealthColor, HealthLabel,
			Total > 0.0 ? Player.Vitals.Shield / Total : 0.0);
		if (Player.Vitals.MaxResource > 0.0)
		{
			DrawBar(Paint, BarsAt + FVector2D(0.0f, HealthHeight + Paint.S(3.0f)), FVector2D(BarsWidth, ResourceHeight), Player.Vitals.Resource / Player.Vitals.MaxResource,
				Settings.ResourceColor, FString());
		}

		// The Flux Spells and the vision tool.
		X = PassiveAt.X + CentreWidth + Gap * 2.0f;
		const float SpellTop = RowTop + (Ability - Small) / 2.0f;
		for (const FVeyraHudSpellSlot& Spell : Player.Spells)
		{
			const FVector2D At(X, SpellTop);
			const FString Name = Spell.Spell.IsValid() ? VeyraContentText::AbilityName(Spell.Spell).ToString() : FString();
			Paint.Rect(At, FVector2D(Small), Settings.BarBackgroundColor);
			if (!DrawIcon(Paint, VeyraShellArt::AbilityIconOf(Spell.Spell.ToString()), At, Small))
			{
				Paint.TextCentred(At + FVector2D(Small / 2.0f), Monogram(Name), Paint.Font(TEXT("Bold"), Settings.HudBodyFontSize), Settings.TextColor);
			}
			if (Spell.bLocked)
			{
				Paint.Rect(At, FVector2D(Small), Settings.ShadeColor);
			}
			DrawCooldown(Paint, At, Small, Spell.bLocked ? 0.0 : Spell.CooldownSeconds);
			Paint.Outline(At, FVector2D(Small), Settings.HudHairlineColor);
			Paint.KeyCap(At, KeyName(Input.GetAbilityKey(Spell.Slot)));
			if (Spell.Spell.IsValid() && Contains(At, FVector2D(Small), Mouse))
			{
				const FString Detail = Spell.bLocked ? FString::Printf(TEXT("Flux Spell   locked until %.0f permanent Team Flux"), Spell.UnlockFlux) : FString(TEXT("Flux Spell"));
				Hover = FHover{ Name, Detail, VeyraContentText::AbilityDescription(Spell.Spell).ToString() };
			}
			X += Small + Gap;
		}
		if (Player.VisionTool.bPresent)
		{
			const FVector2D At(PassiveAt.X + CentreWidth + Gap * 2.0f + (SpellsWidth - Small) / 2.0f, BarsAt.Y - Paint.S(2.0f));
			const bool bWard = Player.VisionTool.Tool == EVeyraVisionTool::PersistentWard;
			const FString Label = bWard ? FString::Printf(TEXT("Ward %d/%d"), Player.VisionTool.WardCharges, Player.VisionTool.MaxWardCharges)
										: FString(Player.VisionTool.Tool == EVeyraVisionTool::Sweeper ? TEXT("Sweeper") : TEXT("Quick Sight"));
			const FString Key = KeyName(Input.GetAbilityKey(EVeyraAbilitySlot::VisionTool));
			const FSlateFontInfo ToolFont = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
			const FString Line = FString::Printf(TEXT("%s  %s"), *Key, *Label);
			const double Waiting = bWard ? Player.VisionTool.NextChargeSeconds : Player.VisionTool.CooldownSeconds;
			const FString Shown = Waiting > 0.0 ? FString::Printf(TEXT("%s  %ss"), *Line, *Seconds(Waiting)) : Line;
			Paint.TextCentred(FVector2D(PassiveAt.X + CentreWidth + Gap * 2.0f + SpellsWidth / 2.0f, BarsAt.Y + HealthHeight / 2.0f), Shown, ToolFont,
				Settings.DescriptionColor);
		}

		// The items, two rows of three, with Gold and the shop under them.
		const float ItemsLeft = PassiveAt.X + CentreWidth + Gap * 2.0f + SpellsWidth + Gap * 2.0f;
		const float ItemsTop = TopLeft.Y + Pad;
		for (int32 Index = 0; Index < Player.Items.Num(); ++Index)
		{
			const FVeyraHudItemSlot& Held = Player.Items[Index];
			const FVector2D At(ItemsLeft + (Index % 3) * (Item + Gap), ItemsTop + (Index / 3) * (Item + Gap));
			Paint.Rect(At, FVector2D(Item), Settings.BarBackgroundColor);
			if (Held.Item.IsValid())
			{
				const FString Name = VeyraContentText::ItemName(Held.Item).ToString();
				if (!DrawIcon(Paint, VeyraShellArt::ItemIconOf(Held.Item.ToString()), At, Item))
				{
					Paint.TextCentred(At + FVector2D(Item / 2.0f), Monogram(Name), Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor);
				}
				if (Held.Count > 1)
				{
					Paint.Text(At + FVector2D(Item - Paint.S(14.0f), Item - Paint.S(16.0f)), FString::FromInt(Held.Count), Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize),
						Settings.TextColor, true);
				}
				DrawCooldown(Paint, At, Item, Held.CooldownSeconds);
				if (Contains(At, FVector2D(Item), Mouse))
				{
					Hover = FHover{ Name, FString::Printf(TEXT("Item   %s"), *KeyName(Input.GetAbilityKey(Held.Slot))), VeyraContentText::ItemDescription(Held.Item).ToString() };
				}
			}
			Paint.Outline(At, FVector2D(Item), Settings.HudHairlineColor);
			Paint.KeyCap(At, KeyName(Input.GetAbilityKey(Held.Slot)));
		}
		const FString ShopKey = KeyName(GetDefault<UVeyraUIInputSettings>()->ShopKey);
		const FString Gold = FString::Printf(TEXT("%s"), *FText::AsNumber(Player.Gold).ToString());
		const float GoldY = ItemsTop + 2.0f * Item + Gap * 1.5f;
		Paint.Text(FVector2D(ItemsLeft, GoldY), Gold, Paint.Font(TEXT("Black"), Settings.HudBodyFontSize), Settings.GoldColor);
		const FString Shop = Player.PendingPurchases > 0 ? FString::Printf(TEXT("%s  shop   %d at the fountain"), *ShopKey, Player.PendingPurchases)
														 : FString::Printf(TEXT("%s  shop"), *ShopKey);
		const FSlateFontInfo ShopFont = Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize);
		Paint.Text(FVector2D(ItemsLeft + ItemsWidth - Paint.Measure(Shop, ShopFont).X, GoldY + Paint.S(2.0f)), Shop, ShopFont, Settings.DescriptionColor);

		// Recalling: the channel over the deck, filling toward home (ADR-012 §8).
		if (Player.bRecalling)
		{
			const FVector2D Size(Paint.S(320.0f), Paint.S(12.0f));
			const FVector2D At((Paint.Canvas.ClipX - Size.X) / 2.0f, TopLeft.Y - Size.Y - Paint.S(34.0f));
			DrawBar(Paint, At, Size, Player.RecallProgress, Settings.ChannelColor, FString());
			Paint.TextCentred(At + FVector2D(Size.X / 2.0f, -Paint.S(12.0f)), FString::Printf(TEXT("RECALL   %.1f"), Player.RecallSeconds),
				Paint.Font(TEXT("Bold"), Settings.HudSmallFontSize), Settings.TextColor, true);
		}

		// Dead: the world dims, and the wait for the fountain counts down (Economy & Progression Bible §14).
		if (Player.bDead)
		{
			Paint.Rect(FVector2D::ZeroVector, FVector2D(Paint.Canvas.ClipX, TopLeft.Y), Settings.ShadeColor.CopyWithNewOpacity(Settings.ShadeColor.A * 0.6f));
			const FVector2D Centre(Paint.Canvas.ClipX / 2.0f, Paint.Canvas.ClipY * 0.4f);
			Paint.TextCentred(Centre - FVector2D(0.0f, Paint.S(44.0f)), TEXT("RESPAWNING IN"), Paint.Font(TEXT("Bold"), Settings.HudBodyFontSize), Settings.DescriptionColor);
			Paint.TextCentred(Centre, FString::FromInt(FMath::CeilToInt32(Player.RespawnSeconds)), Paint.Font(TEXT("Black"), Settings.HudHeadlineFontSize),
				Settings.TextColor, true);
		}
		return TopLeft.Y;
	}
}

namespace VeyraHudDeck
{
void Draw(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const UFont* Font, const UWorld& World, const AVeyraGameState& GameState,
	const APlayerController* Viewer, const AVeyraPlayerState* Own, double ServerNow)
{
	const FPainter Paint(Canvas, Settings, Font);
	const EVeyraTeam Side = Own ? Own->GetVeyraTeam() : EVeyraTeam::None;
	DrawTopStrip(Paint, GameState, Side);
	DrawNotices(Paint, GameState, Viewer);
	DrawTeamFlux(Paint, World, Side, ServerNow);
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
	const float DeckTop = DrawDeck(Paint, *Own, Mouse, ServerNow, Hover);
	if (Hover.IsSet())
	{
		DrawTooltip(Paint, Hover.GetValue(), DeckTop);
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
}
