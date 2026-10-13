// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Hud/VeyraGreyboxHud.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attacks/VeyraBasicAttackComponent.h"
#include "Attacks/VeyraBasicAttackTypes.h"
#include "Attributes/VeyraOffenceSet.h"

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
#include "Hud/VeyraHudPainter.h"
#include "Hud/VeyraKillFeedModel.h"
#include "Hud/VeyraLastHit.h"
#include "Ending/VeyraMatchEnding.h"
#include "Hud/VeyraMinimapModel.h"
#include "Hud/VeyraTargetFrameModel.h"
#include "Movement/VeyraDrawnBody.h"
#include "Progression/VeyraProgressionComponent.h"
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
#include "VeyraVanguardCharacter.h"
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

	/** The player's own basic attack, as the last-hit cue reads it (ADR-065 §6). */
	struct FLastHitReach
	{
		const FVeyraBasicAttackProfile* Profile = nullptr;
		double PhysicalPower = 0.0;
		double MagicPower = 0.0;

		/** Its windup now, and where the player's body stands, for how long an attack takes to land (ADR-071 §5). */
		double WindupSeconds = 0.0;
		const AActor* Attacker = nullptr;
	};

	/** Viewer's Vanguard's basic attack and power now, while the player keeps the cue on; its power is the owner's to see. */
	TOptional<FLastHitReach> LastHitReachOf(const FVeyraInterfacePreferences& Preferences, const APlayerController* Viewer)
	{
		const AVeyraPlayerState* Own = Viewer ? Viewer->GetPlayerState<AVeyraPlayerState>() : nullptr;
		const UAbilitySystemComponent* AbilitySystem = Own ? Own->GetAbilitySystemComponent() : nullptr;
		const FVeyraVanguardDefinition* Definition = Own ? UVeyraVanguardsTuningSubsystem::FindVanguard(Own->GetVanguardId()) : nullptr;
		if (!Preferences.bLastHitCue || !AbilitySystem || !Definition)
		{
			return {};
		}
		const UVeyraBasicAttackComponent* Attacks = Own->FindComponentByClass<UVeyraBasicAttackComponent>();
		const double Interval = Attacks ? Attacks->GetTiming().IntervalSeconds : 0.0;
		return FLastHitReach{ &Definition->BasicAttack, AbilitySystem->GetNumericAttribute(UVeyraOffenceSet::GetPhysicalPowerAttribute()),
			AbilitySystem->GetNumericAttribute(UVeyraOffenceSet::GetMagicPowerAttribute()), Interval * Definition->BasicAttack.WindupFraction, Own->GetPawn() };
	}

	/** Words of a content ID for a label: "wildlife_grub" as "Wildlife Grub". */
	FString Worded(const FString& Id)
	{
		TArray<FString> Words;
		Id.ParseIntoArray(Words, TEXT("_"));
		for (FString& Word : Words)
		{
			Word = Word.Left(1).ToUpper() + Word.RightChop(1);
		}
		return FString::Join(Words, TEXT(" "));
	}

	/**
	 * Health with shields after it, the resource under them, and the unit's statuses above; a Fluxborn's or a
	 * jungle creature's only when the player's bar settings show it (ADR-052 §2). Targeted are the units the
	 * player targets now; LastHit, the player's attack for the last-hit cue (ADR-065 §6). Drawn in the client's
	 * type, the fill lit at its head, its lost part trailing (ADR-082 §3-§4).
	 */
	void DrawOverheadBars(UCanvas& Canvas, const UVeyraGreyboxSubsystem& Greybox, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences,
		TConstArrayView<const AActor*> Targeted, const TOptional<FLastHitReach>& LastHit, const APawn& Unit, const APawn* OwnVanguard, double Now)
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
		// Over the body as it is drawn, which may stand taller than its capsule (ADR-065 §11).
		const FVector OnScreen = Canvas.Project(VeyraDrawnBody::LocationOf(Unit) + FVector::UpVector * (Settings.VisualTopOf(Unit) + Settings.BarLift));
		if (OnScreen.Z <= 0.0)
		{
			return; // Behind the camera.
		}

		// At the player's Overhead Bar Size (ADR-059 §1).
		const float Scale = Preferences.HudScales.OverheadBars;
		const FVeyraHudPainter Paint = FVeyraHudPainter(Canvas, Settings, Greybox.GetHudFont(), 1.0f, &Greybox.GetHudMotion()).At(Scale);
		const float BarWidth = Settings.BarWidth * Scale;
		const float BarHeight = Settings.BarHeight * Scale;
		const float ResourceBarHeight = Settings.ResourceBarHeight * Scale;
		// Labels a size under the HUD's small type, so they stay out of the fight's way.
		const FSlateFontInfo LabelFont = Paint.Font(TEXT("SemiBold"), FMath::Max(1, Settings.HudSmallFontSize - 1));
		// Before Slate measures (a server, a test), a line is its type's size and a little.
		constexpr float LineOverType = 1.3f;
		const float LineHeight = FMath::Max(static_cast<float>(Paint.Measure(TEXT("Ag"), LabelFont).Y), LabelFont.Size * LineOverType);
		const FVector2D TopLeft(OnScreen.X - BarWidth / 2.0f, OnScreen.Y - BarHeight);
		// Health and shields share the bar; when together they pass Max Health, the bar holds their total.
		const double Total = FMath::Max(Vitals->MaxHealth, Vitals->Health + Vitals->Shield);
		const float HealthWidth = BarWidth * Vitals->Health / Total;
		// The last-hit cue, on an enemy Fluxborn or creature: a mark at the Health the player's next basic attack would finish,
		// so the moment its Health falls to the mark, that attack kills; its Health is lit from then (ADR-065 §6). Before
		// that, it reads Ready while an attack started now would land as its Health falls into reach, at the rate it is
		// losing Health (ADR-071 §5). A guide: crits and on-hit are left out.
		FLinearColor HealthColor = Greybox.SideColorOf(Unit);
		TOptional<float> LastHitMark;
		const UAbilitySystemComponent* Defender = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
		if (LastHit && Defender && !Facts.bAllied && (Facts.Kind == EVeyraUnitKind::Fluxborn || Facts.Kind == EVeyraUnitKind::Wildlife))
		{
			const double Hit = VeyraBasicAttacks::ExpectedHit(*LastHit->Profile, LastHit->PhysicalPower, LastHit->MagicPower, *Defender);
			const double Speed = LastHit->Profile->Projectile.IsEmpty() ? 0.0 : LastHit->Profile->Projectile[0].Speed;
			const double Distance = LastHit->Attacker ? FVector::Dist2D(LastHit->Attacker->GetActorLocation(), Unit.GetActorLocation()) : 0.0;
			const double Lead = VeyraLastHit::LeadSecondsOf(LastHit->WindupSeconds, Distance, Speed);
			switch (VeyraLastHit::StageOf(Vitals->Health, Hit, Greybox.GetHealthLossPerSecond(Unit), Lead))
			{
			case EVeyraLastHitStage::Now:
				HealthColor = Settings.LastHitColor;
				break;
			case EVeyraLastHitStage::Ready:
				HealthColor = Settings.LastHitReadyColor;
				LastHitMark = static_cast<float>(BarWidth * Hit / Total);
				break;
			case EVeyraLastHitStage::None:
				if (Hit > 0.0 && Facts.bDamaged)
				{
					LastHitMark = static_cast<float>(BarWidth * Hit / Total);
				}
				break;
			}
		}
		// Its lost part trails behind the fill (ADR-082 §3).
		const double Share = Vitals->Health / Total;
		const double Trail = Greybox.GetHudMotion().Trail(FName(TEXT("Bar"), static_cast<int32>(Unit.GetUniqueID())), Share, Settings.MotionTrailHoldSeconds,
			Settings.MotionTrailDrainSeconds, Settings.MotionSmear);
		// A dark rim a pixel out, so the bar stands off the ground.
		Paint.Rect(TopLeft - FVector2D(1.0f), FVector2D(BarWidth + 2.0f, BarHeight + 2.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
		Paint.Rect(TopLeft, FVector2D(BarWidth, BarHeight), Settings.BarBackgroundColor);
		if (Trail > Share)
		{
			Paint.Rect(TopLeft + FVector2D(HealthWidth, 0.0f), FVector2D(BarWidth * static_cast<float>(FMath::Min(Trail, 1.0) - Share), BarHeight), Settings.MotionTrailColor);
		}
		Paint.Gradient(TopLeft, FVector2D(HealthWidth, BarHeight), FMath::Lerp(HealthColor, FLinearColor::White, 0.25f).CopyWithNewOpacity(HealthColor.A),
			FLinearColor(HealthColor.R * 0.7f, HealthColor.G * 0.7f, HealthColor.B * 0.7f, HealthColor.A));
		Paint.Rect(TopLeft + FVector2D(HealthWidth, 0.0f), FVector2D(BarWidth * Vitals->Shield / Total, BarHeight), Settings.ShieldColor);
		Paint.Rect(TopLeft, FVector2D(HealthWidth, FMath::Max(1.0f, BarHeight * 0.18f)), FLinearColor(1.0f, 1.0f, 1.0f, 0.25f));
		if (LastHitMark.IsSet())
		{
			Paint.Rect(FVector2D(TopLeft.X + LastHitMark.GetValue(), TopLeft.Y - Scale), FVector2D(FMath::Max(2.0f, 2.0f * Scale), BarHeight + 2.0f * Scale), Settings.LastHitMarkColor);
		}
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
				Paint.Rect(FVector2D(TopLeft.X + TickGap * Tick, TopLeft.Y), FVector2D(1.0f, Height), FLinearColor(0.0f, 0.0f, 0.0f, bLong ? 0.8f : 0.5f));
			}
		}
		if (Vitals->MaxResource > 0.0)
		{
			const FVector2D ResourceTopLeft = TopLeft + FVector2D(0.0f, BarHeight);
			Paint.Rect(ResourceTopLeft - FVector2D(1.0f, 0.0f), FVector2D(BarWidth + 2.0f, ResourceBarHeight + 1.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
			Paint.Rect(ResourceTopLeft, FVector2D(BarWidth, ResourceBarHeight), Settings.BarBackgroundColor);
			Paint.Rect(ResourceTopLeft, FVector2D(BarWidth * Vitals->Resource / Vitals->MaxResource, ResourceBarHeight), Settings.ResourceColorOf(Vitals->Family));
		}

		float Y = TopLeft.Y - Paint.S(2.0f);
		const auto Label = [&](const FString& Text, const FLinearColor& Color) {
			Y -= LineHeight;
			Paint.Text(FVector2D(TopLeft.X, Y), Text, LabelFont, Color, true);
		};
		// A structure says what it is, and whether the structures before it still protect it.
		if (const TOptional<FVeyraHudStructure> Structure = VeyraHud::StructureOf(Unit, Now))
		{
			FString Name = VeyraKillFeedView::StructureName(Structure->Kind, TOptional<EVeyraLane>(), 0);
			Name = Name.Left(1).ToUpper() + Name.RightChop(1);
			if (Structure->bInvulnerable)
			{
				Name += TEXT("  invulnerable");
			}
			if (Structure->RebuildSeconds > 0.0)
			{
				Name += FString::Printf(TEXT("  rebuilds in %d s"), FMath::CeilToInt32(Structure->RebuildSeconds));
			}
			Label(Name, Settings.TextColor);
		}
		// A Flux Well says where it stands in its cycle; a creature, what it is (ADR-014).
		if (const TOptional<FVeyraHudFluxWell> Well = VeyraHud::FluxWellOf(Unit, Now))
		{
			FString Text = TEXT("Flux Well");
			switch (Well->State)
			{
			case EVeyraFluxWellState::Closed:
				Text += FString::Printf(TEXT("  opens in %d s"), FMath::CeilToInt32(Well->OpensInSeconds));
				break;
			case EVeyraFluxWellState::Open:
				Text += TEXT("  open");
				break;
			case EVeyraFluxWellState::Respawning:
				Text += FString::Printf(TEXT("  returns in %d s"), FMath::CeilToInt32(Well->OpensInSeconds));
				break;
			}
			Label(Text, Settings.HudAccentColor);
		}
		// The mastery emote: the player's Mastery Level in its tier's colour, nearest the bar (ADR-045 §9).
		if (const TOptional<FVeyraHudMasteryEmote> Emote = VeyraHud::MasteryEmoteOf(Unit, Now); Emote && !Settings.MasteryEmoteTierColors.IsEmpty())
		{
			const int32 Index = FMath::Clamp(Emote->Tier - 1, 0, Settings.MasteryEmoteTierColors.Num() - 1);
			Label(FString::Printf(TEXT("Mastery %d"), Emote->Level), Settings.MasteryEmoteTierColors[Index]);
		}
		if (const TOptional<FVeyraContentId> Species = VeyraHud::SpeciesOf(Unit))
		{
			Label(Worded(Species->ToString()), Settings.TextColor);
		}
		// Its statuses, the harmful first and at most a few, a smaller line each (ADR-082 §4). The player's own Vanguard's
		// beneficial ones read in the deck's status row instead (ADR-059 §4).
		TArray<FVeyraHudStatus> Statuses = VeyraHud::StatusesOf(Unit, Now, Greybox.GetViewerTeam());
		if (&Unit == OwnVanguard)
		{
			Statuses.RemoveAll([](const FVeyraHudStatus& Status) { return Status.Group == EVeyraStatusGroup::Beneficial; });
		}
		Statuses.StableSort([](const FVeyraHudStatus& A, const FVeyraHudStatus& B) { return A.Group != EVeyraStatusGroup::Beneficial && B.Group == EVeyraStatusGroup::Beneficial; });
		constexpr int32 MostStatuses = 3;
		for (int32 Index = 0; Index < FMath::Min(Statuses.Num(), MostStatuses); ++Index)
		{
			const FVeyraHudStatus& Status = Statuses[Index];
			// Each status in its kind's words (ADR-080 §2); a mark has no effect of its own, so its name says what it is.
			const FString Count = Status.Stacks > 1 ? FString::Printf(TEXT(" x%d"), Status.Stacks) : FString();
			const FString Name = Status.Kind == EVeyraStatusKind::Counter ? Worded(Status.Id.ToString()) : VeyraContentText::StatusKindName(Status.Kind).ToString();
			const FString More = Index == MostStatuses - 1 && Statuses.Num() > MostStatuses ? FString::Printf(TEXT("   +%d"), Statuses.Num() - MostStatuses) : FString();
			Label(FString::Printf(TEXT("%s%s %.1f%s"), *Name, *Count, Status.RemainingSeconds, *More), Status.Group == EVeyraStatusGroup::Beneficial ? Settings.TextColor : Settings.WarningColor);
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
		case EVeyraCombatTextKind::Gold:
			return Settings.GoldColor;
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

	/**
	 * The player's combat text, each number rising from its unit's bars and fading as it goes (ADR-052 §1); Gold a fall
	 * earned rises from where the unit fell (ADR-065 §4). In the client's type, each number pops in large and settles to its
	 * size (ADR-082 §3); a crit stays larger.
	 */
	void DrawCombatText(UCanvas& Canvas, const UVeyraGreyboxSubsystem& Greybox, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences,
		TConstArrayView<FVeyraCombatTextShown> Numbers)
	{
		const FVeyraHudPainter Paint(Canvas, Settings, Greybox.GetHudFont(), 1.0f);
		const FSlateFontInfo Font = Paint.At(1.0f).Font(TEXT("Bold"), Settings.HudSmallFontSize);
		// Whole through most of its rise, then fading.
		constexpr double FadeFrom = 0.55;
		for (const FVeyraCombatTextShown& Number : Numbers)
		{
			const double Rise = Settings.BarLift + Settings.CombatTextRise * Number.Progress;
			FVector Anchor = Number.Where + FVector::UpVector * Rise;
			if (!Number.bFixed)
			{
				const AActor* Unit = Number.Unit.Get();
				if (!Unit || Unit->IsHidden())
				{
					continue;
				}
				Anchor = VeyraDrawnBody::LocationOf(*Unit) + FVector::UpVector * (Settings.VisualTopOf(*Unit) + Rise);
			}
			const FVector OnScreen = Canvas.Project(Anchor);
			if (OnScreen.Z <= 0.0)
			{
				continue;
			}
			const bool bGiven = Number.Kind == EVeyraCombatTextKind::Healing || Number.Kind == EVeyraCombatTextKind::Shielding || Number.Kind == EVeyraCombatTextKind::Gold;
			const FString Text = FString::Printf(TEXT("%s%d"), bGiven ? TEXT("+") : TEXT(""), FMath::RoundToInt(Number.Amount));
			const double Age = Number.Progress * Settings.CombatTextShowSeconds;
			const double Pop = Preferences.bReduceUiAnimation ? 1.0 : FMath::Lerp(static_cast<double>(Settings.MotionPopScale), 1.0, FVeyraHudPainter::Eased(Settings.MotionSettle, Age, Settings.MotionPopSeconds));
			const float Size = Settings.CombatTextScale * Preferences.HudScales.CombatText * (Number.bCritical ? Settings.CombatTextCritScale : 1.0f) * static_cast<float>(Pop);
			const float Opacity = static_cast<float>(Number.Progress < FadeFrom ? 1.0 : 1.0 - (Number.Progress - FadeFrom) / (1.0 - FadeFrom));
			FCanvasTextItem Item(FVector2D(OnScreen.X, OnScreen.Y), FText::FromString(Text), Font, CombatTextColor(Settings, Preferences.bUniformDamageColors, Number).CopyWithNewOpacity(Opacity));
			Item.bCentreX = true;
			Item.bCentreY = true;
			Item.Scale = FVector2D(Size);
			Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f * Opacity));
			Canvas.DrawItem(Item);
		}
	}

	/** The HUD on a Viewport-sized screen as the player set it: the deck, the minimap and the chat inside the safe area (ADR-059 §1-§2). */
	FVeyraHudArrangement Arrangement(const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, const FVector2D& Viewport)
	{
		return VeyraHudLayout::ArrangeFor(Viewport, Settings, Preferences);
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
	 * The minimap, bottom-right (Settings Bible §3.2; ADR-020 §2): the river, the masses and keep walls
	 * (ADR-073 §3), the lanes, every unit this client has (so only what its side sees), its side's presence
	 * pings, and where the camera looks.
	 */
	void DrawMinimap(UCanvas& Canvas, const UVeyraGreyboxSettings& Settings, const FVeyraInterfacePreferences& Preferences, const FVeyraMinimapView& View, float Scale)
	{
		const FVeyraMinimapFrame& Frame = View.Frame;
		// In the client's chamfered, brass-edged surface, a little outside the map (ADR-082 §4).
		const FVeyraHudPainter Paint = FVeyraHudPainter(Canvas, Settings, nullptr).At(Scale);
		const float Rim = Paint.S(6.0f);
		Paint.Surface(Frame.Origin - FVector2D(Rim), FVector2D(Frame.Size) + FVector2D(Rim * 2.0f));
		DrawHudRect(Canvas, Frame.Origin, FVector2D(Frame.Size), Settings.MinimapBackgroundColor);
		FCanvasBoxItem Edge(Frame.Origin, FVector2D(Frame.Size));
		Edge.SetColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		Edge.BlendMode = SE_BLEND_Translucent;
		Canvas.DrawItem(Edge);
		// The authored river is a triangle list, including both sampled branches.
		for (int32 Index = 2; Index < View.River.Num(); Index += 3)
		{
			FCanvasTriangleItem Piece(View.River[Index - 2], View.River[Index - 1], View.River[Index], GWhiteTexture);
			Piece.SetColor(Settings.RiverColor);
			Canvas.DrawItem(Piece);
		}
		for (int32 Index = 2; Index < View.Masses.Num(); Index += 3)
		{
			FCanvasTriangleItem Piece(View.Masses[Index - 2], View.Masses[Index - 1], View.Masses[Index], GWhiteTexture);
			Piece.SetColor(Settings.MinimapWallColor);
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

	/**
	 * Gives the local controller the HUD's hit test (ADR-066 §2): the deck, the minimap, and the selected unit's frame while one
	 * shows, so a click on them neither selects nor clears.
	 */
	void BindHudClicks(AVeyraPlayerController& Controller)
	{
		if (Controller.HasHudHitTest())
		{
			return;
		}
		Controller.SetHudHitTest([WeakController = TWeakObjectPtr<AVeyraPlayerController>(&Controller)](const FVector2D& Screen) {
			const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
			const AVeyraPlayerController* Owner = WeakController.Get();
			const UGameViewportClient* Viewport = Owner && Owner->GetLocalPlayer() ? Owner->GetLocalPlayer()->ViewportClient : nullptr;
			if (!Viewport)
			{
				return false;
			}
			FVector2D Size;
			Viewport->GetViewportSize(Size);
			const FVeyraHudArrangement Layout = Arrangement(Settings, VeyraInterfacePreferences::Resolve(Settings, VeyraInterfacePreferences::StoreOf(Owner)), Size);
			const FBox2D Deck(Layout.DeckTopLeft, Layout.DeckTopLeft + Layout.Deck.Size);
			const FBox2D Minimap(Layout.Minimap.Origin, Layout.Minimap.Origin + FVector2D(Layout.Minimap.Size));
			// The stats panel too, its more stats while they show (ADR-080 §3).
			const UVeyraGreyboxSubsystem* Presentation = Owner->GetWorld() ? Owner->GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>() : nullptr;
			const bool bOnStats = Layout.StatsCore.IsInsideOrOn(Screen) || (Presentation && Presentation->IsStatsPanelOpen() && Layout.StatsMore.IsInsideOrOn(Screen));
			return Deck.IsInsideOrOn(Screen) || Minimap.IsInsideOrOn(Screen) || bOnStats || (Owner->GetSelectedUnit() && Layout.TargetFrame.IsInsideOrOn(Screen));
		});
	}

}

void VeyraGreyboxHud::Draw(UCanvas& Canvas, const UVeyraGreyboxSubsystem& Greybox, const APlayerController* Viewer)
{
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FVeyraInterfacePreferences Preferences = VeyraInterfacePreferences::Resolve(Settings, VeyraInterfacePreferences::StoreOf(Greybox.GetWorld()));
	const double Now = Greybox.GetServerNow();
	// A HUD frame starts: what moves runs on real time, and Reduce UI Animation holds it still (ADR-082 §2; ADR-055 §3).
	Greybox.GetHudMotion().Begin(FPlatformTime::Seconds(), Preferences.bReduceUiAnimation);
	const FVeyraHudArrangement Layout = Arrangement(Settings, Preferences, FVector2D(Canvas.ClipX, Canvas.ClipY));
	const AVeyraPlayerController* Targeting = Cast<AVeyraPlayerController>(Viewer);
	const TArray<const AActor*> Targeted = Targeting ? Targeting->GetTargetedUnits() : TArray<const AActor*>();
	const TOptional<FLastHitReach> LastHit = LastHitReachOf(Preferences, Viewer);
	const APawn* OwnVanguard = Targeting ? Targeting->GetVanguard() : nullptr;
	for (TActorIterator<APawn> It(Greybox.GetWorld()); It; ++It)
	{
		if (VeyraUnits::KindOf(*It).IsSet())
		{
			DrawOverheadBars(Canvas, Greybox, Settings, Preferences, Targeted, LastHit, **It, OwnVanguard, Now);
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
	DrawCombatText(Canvas, Greybox, Settings, Preferences, VeyraCombatTextView::Describe(Greybox.GetCombatText(), FPlatformTime::Seconds(), Preferences.CombatText));
	if (const AVeyraGameState* GameState = Greybox.GetWorld()->GetGameState<AVeyraGameState>())
	{
		const AVeyraPlayerController* Player = Cast<AVeyraPlayerController>(Viewer);
		const AVeyraPlayerState* Own = Viewer ? Viewer->GetPlayerState<AVeyraPlayerState>() : nullptr;
		if (Player && Own)
		{
			BindMinimapClicks(*const_cast<AVeyraPlayerController*>(Player));
			BindHudClicks(*const_cast<AVeyraPlayerController*>(Player));
			const AVeyraCameraRig* Rig = Player->GetCameraRig();
			const TOptional<FVector> Focus = Rig ? TOptional<FVector>(Rig->GetFocus()) : TOptional<FVector>();
			const FVeyraMinimapFrame& Frame = Layout.Minimap;
			FVeyraMinimapView View = VeyraMinimap::Describe(*Greybox.GetWorld(), Frame, Own->GetVeyraTeam(), Own->GetPawn(), Focus, Now);
			const double RealNow = FPlatformTime::Seconds();
			View.TeamPings = VeyraMinimap::DescribeTeamPings(Frame, Player->GetPings(), RealNow, Preferences.PingSeconds);
			DrawWorldPings(Canvas, Settings, Preferences.PingSeconds, Player->GetPings(), RealNow);
			DrawMinimap(Canvas, Settings, Preferences, View, Layout.Base);
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
		// The selected unit's frame, under Team Flux (ADR-066 §3).
		const AActor* Selected = Player ? Player->GetSelectedUnit() : nullptr;
		if (const TOptional<FVeyraTargetFrame> Frame = Selected && Own ? VeyraTargetFrame::Describe(*Selected, Own->GetVeyraTeam(), Now) : TOptional<FVeyraTargetFrame>())
		{
			VeyraHudDeck::DrawTargetFrame(Canvas, Settings, Preferences, Greybox.GetHudFont(), Frame.GetValue(), Own->GetVeyraTeam(), Layout.TargetFrame, Layout.TeamPanels,
				&Greybox.GetHudMotion());
		}
		// The player's own level-up, for a while after it comes (ADR-065 §5).
		const TOptional<FVeyraLevelUpMoment>& LevelUp = Greybox.GetOwnLevelUp();
		const double Shown = LevelUp.IsSet() ? (FPlatformTime::Seconds() - LevelUp->At) / Settings.LevelUpBannerSeconds : 1.0;
		if (Own && Shown >= 0.0 && Shown < 1.0)
		{
			const UVeyraProgressionComponent* Progression = Own->FindComponentByClass<UVeyraProgressionComponent>();
			VeyraHudDeck::DrawLevelUp(Canvas, Settings, Greybox.GetHudFont(), LevelUp->Level, Progression ? Progression->GetUnspentSkillPoints() : 0, Shown);
		}
		// Who fell to whom, and the moments that are the player's own (ADR-065 §10).
		const double RealNow = FPlatformTime::Seconds();
		const EVeyraTeam OwnSide = Own ? Own->GetVeyraTeam() : EVeyraTeam::None;
		VeyraHudDeck::DrawKillFeed(Canvas, Settings, Preferences, Greybox.GetHudFont(),
			VeyraKillFeedView::Rows(Greybox.GetKillFeed(), RealNow, Settings.KillFeedSeconds, Settings.KillFeedRows), OwnSide, Layout.Inset);
		if (const TOptional<FVeyraAnnouncement> Announcement =
				VeyraKillFeedView::Announcement(Greybox.GetKillFeed(), RealNow, Settings.AnnouncementSeconds, Own ? Own->GetPlayerId() : INDEX_NONE, OwnSide))
		{
			VeyraHudDeck::DrawAnnouncement(Canvas, Settings, Preferences, Greybox.GetHudFont(), Announcement.GetValue(), Layout.Inset);
		}
		// How the match ended, while its players watch the end (ADR-020 §1).
		if (GameState->GetPhase() == EVeyraMatchPhase::Ended)
		{
			const AVeyraStructure* Fallen = VeyraMatchEnding::FindFallenPrimeWell(*Greybox.GetWorld());
			const EVeyraEndingHeadline Headline = VeyraMatchEnding::Headline(Fallen ? TOptional<EVeyraTeam>(Fallen->GetVeyraTeam()) : TOptional<EVeyraTeam>(),
				Own ? Own->GetVeyraTeam() : EVeyraTeam::None);
			VeyraHudDeck::DrawHeadline(Canvas, Settings, Greybox.GetHudFont(), HudEnumName(Headline), Headline == EVeyraEndingHeadline::Defeat ? Settings.WarningColor : Settings.GoldColor,
				&Greybox.GetHudMotion());
		}
	}
}
