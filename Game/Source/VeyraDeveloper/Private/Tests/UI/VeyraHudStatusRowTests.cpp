// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Hud/VeyraHudModel.h"

namespace VeyraHudStatusRowTests
{
	FVeyraHudStatus StatusOf(EVeyraStatusKind Kind, EVeyraStatusGroup Group, double Remaining, int32 Sequence, int32 Stacks = 1)
	{
		FVeyraHudStatus Status;
		Status.Kind = Kind;
		Status.Group = Group;
		Status.RemainingSeconds = Remaining;
		Status.Sequence = Sequence;
		Status.Stacks = Stacks;
		return Status;
	}

	TArray<EVeyraStatusKind> KindsOf(TConstArrayView<FVeyraHudStatus> Statuses)
	{
		TArray<EVeyraStatusKind> Kinds;
		for (const FVeyraHudStatus& Status : Statuses)
		{
			Kinds.Add(Status.Kind);
		}
		return Kinds;
	}

	// Veyra.UI.HudStatusRow.*: the player's own statuses above the deck, sorted and marked (Settings Bible §3.4,
	// Proposals 38, 43, 53; ADR-059 §4).
	TEST_CLASS(HudStatusRow, "Veyra.UI")
	{
		/** Applied in this order: a Slow, a speed boost, a Stun, damage over time, then Tenacity. */
		TArray<FVeyraHudStatus> Applied() const
		{
			return { StatusOf(EVeyraStatusKind::Slow, EVeyraStatusGroup::CrowdControl, 2.0, 1), StatusOf(EVeyraStatusKind::MoveSpeed, EVeyraStatusGroup::Beneficial, 4.0, 2),
				StatusOf(EVeyraStatusKind::Stun, EVeyraStatusGroup::CrowdControl, 0.5, 3), StatusOf(EVeyraStatusKind::DamageOverTime, EVeyraStatusGroup::Harmful, 1.0, 4),
				StatusOf(EVeyraStatusKind::Tenacity, EVeyraStatusGroup::Beneficial, 3.0, 5) };
		}

		TEST_METHOD(ByCategoryPutsHelpFirstThenCrowdControlThenOtherHarm)
		{
			const TArray<EVeyraStatusKind> Kinds = KindsOf(VeyraHud::OrderOwnStatuses(Applied(), EVeyraStatusSort::ByCategory));
			// Each group by kind: MoveSpeed before Tenacity, Stun before Slow.
			ASSERT_THAT(IsTrue(Kinds == TArray<EVeyraStatusKind>({ EVeyraStatusKind::MoveSpeed, EVeyraStatusKind::Tenacity, EVeyraStatusKind::Stun, EVeyraStatusKind::Slow,
				EVeyraStatusKind::DamageOverTime })));
		}

		TEST_METHOD(TheOtherSortsKeepHelpAndHarmApart)
		{
			const TArray<EVeyraStatusKind> ByDuration = KindsOf(VeyraHud::OrderOwnStatuses(Applied(), EVeyraStatusSort::ByRemainingDuration));
			ASSERT_THAT(IsTrue(ByDuration == TArray<EVeyraStatusKind>({ EVeyraStatusKind::Tenacity, EVeyraStatusKind::MoveSpeed, EVeyraStatusKind::Stun,
				EVeyraStatusKind::DamageOverTime, EVeyraStatusKind::Slow }), TEXT("the soonest to end first in each group")));
			const TArray<EVeyraStatusKind> ByOrder = KindsOf(VeyraHud::OrderOwnStatuses(Applied(), EVeyraStatusSort::ByApplicationOrder));
			ASSERT_THAT(IsTrue(ByOrder == TArray<EVeyraStatusKind>({ EVeyraStatusKind::MoveSpeed, EVeyraStatusKind::Tenacity, EVeyraStatusKind::Slow, EVeyraStatusKind::Stun,
				EVeyraStatusKind::DamageOverTime }), TEXT("as applied in each group")));
		}

		TEST_METHOD(EveryChipCarriesAMarkThatNeedsNoColour)
		{
			ASSERT_THAT(IsTrue(VeyraHud::StatusMark(EVeyraStatusGroup::Beneficial) == TEXT("+") && VeyraHud::StatusMark(EVeyraStatusGroup::Harmful) == TEXT("-")
				&& VeyraHud::StatusMark(EVeyraStatusGroup::CrowdControl) == TEXT("!")));
			const FVeyraHudStatus Slow = StatusOf(EVeyraStatusKind::Slow, EVeyraStatusGroup::CrowdControl, 1.24, 1);
			ASSERT_THAT(AreEqual(VeyraHud::StatusChipText(Slow, true), FString(TEXT("! Slow 1.2"))));
			ASSERT_THAT(AreEqual(VeyraHud::StatusChipText(Slow, false), FString(TEXT("! Slow")), TEXT("no seconds when Status Durations is off")));
			const FVeyraHudStatus Speed = StatusOf(EVeyraStatusKind::MoveSpeed, EVeyraStatusGroup::Beneficial, 12.0, 2, 3);
			ASSERT_THAT(AreEqual(VeyraHud::StatusChipText(Speed, true), FString(TEXT("+ Move Speed x3 12"))));
		}
	};
}

#endif