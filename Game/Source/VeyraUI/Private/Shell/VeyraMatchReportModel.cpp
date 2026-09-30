// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraMatchReportModel.h"

#include "Algo/Count.h"
#include "Shell/VeyraShellModels.h"
#include "Text/VeyraContentText.h"

#define LOCTEXT_NAMESPACE "VeyraMatchReport"

namespace VeyraMatchReportModelPrivate
{
	using VeyraBackendProtocol::FMatchOutcome;
	using VeyraBackendProtocol::FPlayerOutcome;

	/** Gold as the UI shows it everywhere: whole, rounded down (Economy & Progression Bible §1). */
	FText GoldText(double Gold)
	{
		return FText::AsNumber(FMath::FloorToInt64(Gold));
	}

	/** Damage, healing and shielding: whole points. */
	FText PointsText(double Points)
	{
		return FText::AsNumber(FMath::RoundToInt64(Points));
	}

	/** Crowd control, in seconds to a tenth. */
	FText SecondsText(double Seconds)
	{
		FNumberFormattingOptions Tenths;
		Tenths.MinimumFractionalDigits = 1;
		Tenths.MaximumFractionalDigits = 1;
		return FText::Format(LOCTEXT("Seconds", "{0} s"), FText::AsNumber(Seconds, &Tenths));
	}

	FText KdaText(const FVeyraPlayerStatistics& Statistics)
	{
		return FText::Format(LOCTEXT("Kda", "{0} / {1} / {2}"), FText::AsNumber(Statistics.Kills), FText::AsNumber(Statistics.Deaths),
			FText::AsNumber(Statistics.Assists));
	}

	FText SlotsText(const TArray<FVeyraContentId>& Slots, FText (*NameOf)(const FVeyraContentId&), const FText& Separator)
	{
		TArray<FText> Names;
		for (const FVeyraContentId& Slot : Slots)
		{
			Names.Add(Slot.IsValid() ? NameOf(Slot) : LOCTEXT("EmptySlot", "-"));
		}
		return FText::Join(Separator, Names);
	}

	FText TeamTitle(const FString& Side, const FMatchOutcome& Outcome)
	{
		const FText Team = Outcome.Side.IsEmpty() ? FText::Format(LOCTEXT("Side", "Side {0}"), FText::FromString(Side))
			: Side == Outcome.Side					  ? LOCTEXT("YourTeam", "Your Team")
													  : LOCTEXT("EnemyTeam", "Enemy Team");
		// The adjudicated outcome, only when a side won (UX-50); no invented grade.
		if (Outcome.Winner.IsEmpty())
		{
			return Team;
		}
		return FText::Format(LOCTEXT("TeamOutcome", "{0}: {1}"), Team, Side == Outcome.Winner ? LOCTEXT("Victory", "Victory") : LOCTEXT("Defeat", "Defeat"));
	}

	using FFigure = TFunction<FText(const FVeyraPlayerStatistics&)>;

	void AddRow(FVeyraReportGroup& Group, const TArray<const FPlayerOutcome*>& Players, const FText& Label, const FFigure& Figure)
	{
		FVeyraReportRow& Row = Group.Rows.AddDefaulted_GetRef();
		Row.Label = Label;
		for (const FPlayerOutcome* Player : Players)
		{
			Row.Values.Add(Figure(Player->Statistics));
		}
	}
}

namespace VeyraMatchReportModel
{
FVeyraMatchReport Describe(const VeyraBackendProtocol::FMatchOutcome& Outcome)
{
	using namespace VeyraMatchReportModelPrivate;
	FVeyraMatchReport Report;
	if (!Outcome.bHasResult)
	{
		Report.Pending = LOCTEXT("Pending", "Statistics Pending: Veyra's services have not recorded this match's result yet.");
		return Report;
	}
	if (!Outcome.bHasScoreboard)
	{
		Report.Pending = LOCTEXT("NoScoreboard", "No statistics were recorded for this match.");
		return Report;
	}
	Report.bHasScoreboard = true;

	// The viewer's team first; without a side of its own, side A.
	const FString First = Outcome.Side == TEXT("B") ? TEXT("B") : TEXT("A");
	const FString Second = First == TEXT("A") ? TEXT("B") : TEXT("A");
	TArray<const FPlayerOutcome*> Ordered;
	for (const FString& Side : { First, Second })
	{
		FVeyraReportTeam& Team = Report.Teams.AddDefaulted_GetRef();
		Team.Title = TeamTitle(Side, Outcome);
		int32 Kills = 0;
		double Gold = 0.0;
		for (const FPlayerOutcome& Player : Outcome.Players)
		{
			if (Player.Side != Side)
			{
				continue;
			}
			Ordered.Add(&Player);
			const FVeyraPlayerStatistics& Statistics = Player.Statistics;
			Kills += Statistics.Kills;
			Gold += Statistics.GoldEarned;
			FVeyraReportLine& Line = Team.Lines.AddDefaulted_GetRef();
			Line.Vanguard = VeyraShellModels::VanguardNameOf(Player.VanguardId);
			Line.Name = Player.bYou ? FText::Format(LOCTEXT("You", "{0} (you)"), FText::FromString(Player.Name)) : FText::FromString(Player.Name);
			Line.bYou = Player.bYou;
			Line.Level = FText::Format(LOCTEXT("Level", "Level {0}"), FText::AsNumber(Statistics.Level));
			Line.Kda = KdaText(Statistics);
			Line.Gold = FText::Format(LOCTEXT("Gold", "{0} Gold"), GoldText(Statistics.GoldEarned));
			Line.LastHits = FText::Format(LOCTEXT("LastHits", "{0} minions, {1} monsters"), FText::AsNumber(Statistics.MinionKills), FText::AsNumber(Statistics.JungleKills));
			Line.Items = SlotsText(Statistics.Items, &VeyraContentText::ItemName, LOCTEXT("ItemSeparator", " | "));
			Line.FluxSpells = FText::Format(LOCTEXT("FluxSpells", "Flux Spells: {0}"),
				SlotsText(Statistics.FluxSpells, &VeyraContentText::AbilityName, LOCTEXT("SpellSeparator", ", ")));
		}
		// Each capture once, not each capturer's participation (UX-53).
		const int32 Wells = Algo::CountIf(Outcome.Wells, [&Side](const VeyraBackendProtocol::FWellOutcome& Capture) { return Capture.Side == Side; });
		Team.Summary = FText::Format(LOCTEXT("Summary", "{0} kills, {1} Gold earned, {2} Flux Wells secured"), FText::AsNumber(Kills), GoldText(Gold), FText::AsNumber(Wells));
	}

	for (const FPlayerOutcome* Player : Ordered)
	{
		Report.Columns.Add(FText::Format(LOCTEXT("Column", "{0}\n{1}"), VeyraShellModels::VanguardNameOf(Player->VanguardId), FText::FromString(Player->Name)));
	}
	// Grouped as UX-50 approves; Vision Score waits for its formula (ADR-017 §9.5).
	FVeyraReportGroup& Combat = Report.Groups.AddDefaulted_GetRef();
	Combat.Title = LOCTEXT("Combat", "Combat");
	AddRow(Combat, Ordered, LOCTEXT("KdaRow", "Kills / Deaths / Assists"), [](const FVeyraPlayerStatistics& S) { return KdaText(S); });
	AddRow(Combat, Ordered, LOCTEXT("VanguardDamage", "Damage to Vanguards"), [](const FVeyraPlayerStatistics& S) { return PointsText(S.VanguardDamage); });
	AddRow(Combat, Ordered, LOCTEXT("PhysicalDealt", "Physical Damage Dealt"), [](const FVeyraPlayerStatistics& S) { return PointsText(S.DamageDealt.Physical); });
	AddRow(Combat, Ordered, LOCTEXT("MagicDealt", "Magic Damage Dealt"), [](const FVeyraPlayerStatistics& S) { return PointsText(S.DamageDealt.Magic); });
	AddRow(Combat, Ordered, LOCTEXT("TrueDealt", "True Damage Dealt"), [](const FVeyraPlayerStatistics& S) { return PointsText(S.DamageDealt.TrueDamage); });
	AddRow(Combat, Ordered, LOCTEXT("Taken", "Damage Taken"), [](const FVeyraPlayerStatistics& S) { return PointsText(S.DamageTaken.Total()); });
	AddRow(Combat, Ordered, LOCTEXT("Shielded", "Damage Shielded"), [](const FVeyraPlayerStatistics& S) { return PointsText(S.DamageShielded); });
	AddRow(Combat, Ordered, LOCTEXT("SelfHealing", "Healing (Self)"), [](const FVeyraPlayerStatistics& S) { return PointsText(S.SelfHealing); });
	AddRow(Combat, Ordered, LOCTEXT("TeammateHealing", "Healing (Teammates)"), [](const FVeyraPlayerStatistics& S) { return PointsText(S.TeammateHealing); });
	AddRow(Combat, Ordered, LOCTEXT("Stun", "Stun on Enemy Vanguards"), [](const FVeyraPlayerStatistics& S) { return SecondsText(S.CrowdControl.Stun); });
	AddRow(Combat, Ordered, LOCTEXT("Slow", "Slow on Enemy Vanguards"), [](const FVeyraPlayerStatistics& S) { return SecondsText(S.CrowdControl.Slow); });
	// A stun and a slow at once count once here (§4).
	AddRow(Combat, Ordered, LOCTEXT("CrowdControlTotal", "Crowd Control, Total"), [](const FVeyraPlayerStatistics& S) { return SecondsText(S.CrowdControl.Total); });

	FVeyraReportGroup& Objectives = Report.Groups.AddDefaulted_GetRef();
	Objectives.Title = LOCTEXT("Objectives", "Objectives");
	AddRow(Objectives, Ordered, LOCTEXT("Towers", "Damage to Towers"), [](const FVeyraPlayerStatistics& S) { return PointsText(S.TowerDamage); });
	AddRow(Objectives, Ordered, LOCTEXT("WellsSecured", "Flux Wells Secured"), [](const FVeyraPlayerStatistics& S) { return FText::AsNumber(S.WellsSecured); });
	AddRow(Objectives, Ordered, LOCTEXT("WellDamage", "Damage to Flux Wells"), [](const FVeyraPlayerStatistics& S) { return PointsText(S.WellDamage); });
	AddRow(Objectives, Ordered, LOCTEXT("WellFinalHits", "Flux Well Final Hits"), [](const FVeyraPlayerStatistics& S) { return FText::AsNumber(S.WellFinalHits); });

	FVeyraReportGroup& Economy = Report.Groups.AddDefaulted_GetRef();
	Economy.Title = LOCTEXT("Economy", "Economy");
	AddRow(Economy, Ordered, LOCTEXT("LevelRow", "Level"), [](const FVeyraPlayerStatistics& S) { return FText::AsNumber(S.Level); });
	AddRow(Economy, Ordered, LOCTEXT("GoldEarned", "Gold Earned"), [](const FVeyraPlayerStatistics& S) { return GoldText(S.GoldEarned); });
	AddRow(Economy, Ordered, LOCTEXT("GoldStarting", "Starting Gold"), [](const FVeyraPlayerStatistics& S) { return GoldText(S.GoldBySource.Starting); });
	AddRow(Economy, Ordered, LOCTEXT("GoldKills", "Gold from Kills"), [](const FVeyraPlayerStatistics& S) { return GoldText(S.GoldBySource.Kills); });
	AddRow(Economy, Ordered, LOCTEXT("GoldAssists", "Gold from Assists"), [](const FVeyraPlayerStatistics& S) { return GoldText(S.GoldBySource.Assists); });
	AddRow(Economy, Ordered, LOCTEXT("GoldMinions", "Gold from Minions"), [](const FVeyraPlayerStatistics& S) { return GoldText(S.GoldBySource.Minions); });
	AddRow(Economy, Ordered, LOCTEXT("GoldJungle", "Gold from the Jungle"), [](const FVeyraPlayerStatistics& S) { return GoldText(S.GoldBySource.Jungle); });
	AddRow(Economy, Ordered, LOCTEXT("GoldObjectives", "Gold from Objectives"), [](const FVeyraPlayerStatistics& S) { return GoldText(S.GoldBySource.Objectives); });
	AddRow(Economy, Ordered, LOCTEXT("GoldWards", "Gold from Wards"), [](const FVeyraPlayerStatistics& S) { return GoldText(S.GoldBySource.Wards); });
	AddRow(Economy, Ordered, LOCTEXT("GoldPassive", "Passive Gold"), [](const FVeyraPlayerStatistics& S) { return GoldText(S.GoldBySource.Passive); });
	AddRow(Economy, Ordered, LOCTEXT("MinionKills", "Minion Last Hits"), [](const FVeyraPlayerStatistics& S) { return FText::AsNumber(S.MinionKills); });
	AddRow(Economy, Ordered, LOCTEXT("JungleKills", "Jungle Last Hits"), [](const FVeyraPlayerStatistics& S) { return FText::AsNumber(S.JungleKills); });
	AddRow(Economy, Ordered, LOCTEXT("Buybacks", "Buybacks"), [](const FVeyraPlayerStatistics& S) { return FText::AsNumber(S.Buybacks); });

	FVeyraReportGroup& Vision = Report.Groups.AddDefaulted_GetRef();
	Vision.Title = LOCTEXT("Vision", "Vision");
	AddRow(Vision, Ordered, LOCTEXT("WardsPlaced", "Wards Placed"), [](const FVeyraPlayerStatistics& S) { return FText::AsNumber(S.WardsPlaced); });
	AddRow(Vision, Ordered, LOCTEXT("WardsDestroyed", "Wards Destroyed"), [](const FVeyraPlayerStatistics& S) { return FText::AsNumber(S.WardsDestroyed); });
	return Report;
}
}

#undef LOCTEXT_NAMESPACE
