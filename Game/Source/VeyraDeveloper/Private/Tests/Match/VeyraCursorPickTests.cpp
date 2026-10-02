// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER

#include "GameFramework/Character.h"
#include "GameFramework/Info.h"
#include "Input/VeyraCursorPicks.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"

namespace VeyraCursorPickTests
{
	// Veyra.Match.CursorPicks.*: which unit under the cursor an order or a cast names, with Target Vanguards Only and
	// for Smart Self-Cast (Settings Bible §1.4, §1.5; ADR-041 §3).
	TEST_CLASS(CursorPicks, "Veyra.Match")
	{
		/** Four distinct stand-ins for the units under the cursor; the picks only pass them on, so no world is needed. */
		AActor* Minion = nullptr;
		AActor* Foe = nullptr;
		AActor* Friend = nullptr;
		AActor* Tower = nullptr;

		BEFORE_EACH()
		{
			Minion = GetMutableDefault<AActor>();
			Foe = GetMutableDefault<APawn>();
			Friend = GetMutableDefault<ACharacter>();
			Tower = GetMutableDefault<AInfo>();
		}

		TEST_METHOD(AnAttackTakesTheFirstEnemyUnderTheCursor)
		{
			const TArray<FVeyraCursorUnit> Under = { { Friend, EVeyraUnitKind::Vanguard, false }, { Minion, EVeyraUnitKind::Fluxborn, true },
				{ Foe, EVeyraUnitKind::Vanguard, true } };
			ASSERT_THAT(IsTrue(VeyraCursorPicks::Enemy(Under, false) == Minion));
		}

		TEST_METHOD(TargetVanguardsOnlyLooksPastEveryOtherUnit)
		{
			const TArray<FVeyraCursorUnit> Under = { { Tower, EVeyraUnitKind::Structure, true }, { Minion, EVeyraUnitKind::Fluxborn, true },
				{ Foe, EVeyraUnitKind::Vanguard, true } };
			ASSERT_THAT(IsTrue(VeyraCursorPicks::Enemy(Under, true) == Foe));
			ASSERT_THAT(IsTrue(VeyraCursorPicks::ForCast(Under, true, false) == Foe));
			const TArray<FVeyraCursorUnit> NoVanguard = { { Minion, EVeyraUnitKind::Fluxborn, true } };
			ASSERT_THAT(IsNull(VeyraCursorPicks::Enemy(NoVanguard, true), TEXT("no attack on the minion instead")));
		}

		TEST_METHOD(ACastAtEnemiesLooksPastANearerAlly)
		{
			// An allied Vanguard nearer the camera than the enemy: a cast at enemies names the enemy, with Target
			// Vanguards Only or without (Settings Bible §1.4).
			const TArray<FVeyraCursorUnit> Under = { { Friend, EVeyraUnitKind::Vanguard, false }, { Minion, EVeyraUnitKind::Fluxborn, true },
				{ Foe, EVeyraUnitKind::Vanguard, true } };
			ASSERT_THAT(IsTrue(VeyraCursorPicks::ForCast(Under, true, false) == Foe));
			ASSERT_THAT(IsTrue(VeyraCursorPicks::ForCast(Under, false, false) == Minion));
			ASSERT_THAT(IsNull(VeyraCursorPicks::ForCast({}, false, false)));
		}

		TEST_METHOD(ACastThatMayLandOnAnAllyNamesTheFirstAlliedVanguard)
		{
			const TArray<FVeyraCursorUnit> Under = { { Foe, EVeyraUnitKind::Vanguard, true }, { Minion, EVeyraUnitKind::Fluxborn, false },
				{ Friend, EVeyraUnitKind::Vanguard, false } };
			ASSERT_THAT(IsTrue(VeyraCursorPicks::ForCast(Under, false, true) == Friend));
			ASSERT_THAT(IsTrue(VeyraCursorPicks::ForCast(Under, true, true) == Friend, TEXT("Target Vanguards Only leaves it as it is")));
			ASSERT_THAT(IsTrue(VeyraCursorPicks::Ally(Under) == Friend));
			ASSERT_THAT(IsNull(VeyraCursorPicks::Ally({ { Foe, EVeyraUnitKind::Vanguard, true } })));
		}
	};

	// Veyra.Abilities.AllyTargets.*: which abilities may name an allied unit, and so take the Self-Cast Modifier (ADR-041 §3).
	TEST_CLASS(AllyTargets, "Veyra.Abilities")
	{
		static bool Accepts(const TCHAR* Ability)
		{
			return VeyraAbilityRules::AcceptsAllyTarget(UVeyraAbilitiesTuningSubsystem::Get(), FVeyraContentId::FromText(Ability).GetValue());
		}

		TEST_METHOD(ABuffThatMayLandOnAnAllyAcceptsOne)
		{
			ASSERT_THAT(IsTrue(Accepts(TEXT("aurelisse_windward"))));
			ASSERT_THAT(IsFalse(Accepts(TEXT("mend")), TEXT("its caster's alone")));
		}

		TEST_METHOD(ACompanionCommandBoundToAnAllyAcceptsOne)
		{
			ASSERT_THAT(IsTrue(Accepts(TEXT("neris_little_current"))));
			ASSERT_THAT(IsTrue(Accepts(TEXT("eudora_move_the_line"))));
			ASSERT_THAT(IsFalse(Accepts(TEXT("neris_little_current_storm")), TEXT("bound to an enemy")));
			ASSERT_THAT(IsFalse(Accepts(TEXT("eudora_set_the_picket"))));
		}

		TEST_METHOD(AnAbilityAimedAtFoesOrGroundAcceptsNoAlly)
		{
			ASSERT_THAT(IsFalse(Accepts(TEXT("scorch"))));
			ASSERT_THAT(IsFalse(Accepts(TEXT("blink"))));
			ASSERT_THAT(IsFalse(Accepts(TEXT("eudora_drive_rivet"))));
		}
	};
}

#endif
