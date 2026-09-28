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
#include "Input/VeyraInputSettings.h"
#include "Text/VeyraContentText.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"
#include "Units/VeyraUnit.h"
#include "VeyraGameState.h"
#include "VeyraPlayerState.h"

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
		for (const FVeyraHudStatus& Status : VeyraHud::StatusesOf(Unit, Now))
		{
			Y -= HudLineHeight();
			DrawHudText(Canvas, FVector2D(TopLeft.X, Y), FString::Printf(TEXT("%s %.1f s"), *HudEnumName(Status.Kind), Status.RemainingSeconds), Settings.TextColor);
		}
	}

	/** The phase and match clock, top centre; "Paused" while a pause holds the match. */
	void DrawMatchClock(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const AVeyraGameState& GameState)
	{
		const int32 Seconds = FMath::FloorToInt32(GameState.GetMatchClockSeconds());
		FString Text = FString::Printf(TEXT("%s  %02d:%02d"), *HudEnumName(GameState.GetPhase()), Seconds / SecondsPerMinute, Seconds % SecondsPerMinute);
		if (GameState.IsMatchPaused())
		{
			Text += TEXT("  Paused");
		}
		float Width = 0.0f;
		float Height = 0.0f;
		Canvas.TextSize(HudFont(), Text, Width, Height);
		DrawHudText(Canvas, FVector2D((Canvas.ClipX - Width) / 2.0f, Settings.HudMargin), Text, Settings.TextColor);
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
		Lines.Add({ FString::Printf(TEXT("%s   Level %d   XP %d / %d   Skill points %d"), *Name, Player.Level, Player.Experience,
			Player.ExperienceToNextLevel, Player.UnspentSkillPoints), Settings.TextColor });
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

		float Y = Canvas.ClipY - Settings.HudMargin - Lines.Num() * HudLineHeight();
		for (const FHudLine& Line : Lines)
		{
			DrawHudText(Canvas, FVector2D(Settings.HudMargin, Y), Line.Text, Line.Color);
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
		DrawMatchClock(Canvas, Settings, *GameState);
	}
	if (const AVeyraPlayerState* Participant = Viewer ? Viewer->GetPlayerState<AVeyraPlayerState>() : nullptr)
	{
		DrawPlayerPanel(Canvas, Settings, *Participant, Now);
	}
}
