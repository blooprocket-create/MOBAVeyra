// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Engine/Texture2D.h"
#include "Shell/VeyraShellArt.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraItemsTuning.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "Tuning/VeyraVanguardsTuning.h"
#include "Tuning/VeyraVanguardsTuningSubsystem.h"

namespace VeyraUITests
{
	// Veyra.UI.ItemArt.*: every item the committed catalog defines has its icon imported
	// (ConceptArt/Items, Game/Scripts/BuildIconArt.ps1 -Kind Items), so no shop tile falls back to initials.
	TEST_CLASS(ItemArt, "Veyra.UI")
	{
		TEST_METHOD(EveryCatalogItemHasItsIcon)
		{
			const FVeyraItemsTuning& Catalog = UVeyraItemsTuningSubsystem::Get();
			ASSERT_THAT(IsFalse(Catalog.Items.IsEmpty()));
			TArray<FString> Missing;
			for (const TPair<FVeyraContentId, FVeyraItemDefinition>& Item : Catalog.Items)
			{
				if (!VeyraShellArt::ItemIconOf(Item.Key.ToString()))
				{
					Missing.Add(Item.Key.ToString());
				}
			}
			ASSERT_THAT(IsTrue(Missing.IsEmpty(), FString::Printf(TEXT("no icon for %s; run Game/Scripts/BuildIconArt.ps1 -Kind Items"), *FString::Join(Missing, TEXT(", ")))));
		}

		TEST_METHOD(AnUnknownItemHasNoIcon)
		{
			ASSERT_THAT(IsNull(VeyraShellArt::ItemIconOf(TEXT("no_such_item"))));
			ASSERT_THAT(IsNull(VeyraShellArt::ItemIconOf(FString())));
		}
	};

	// Veyra.UI.AbilityArt.*: every Flux Spell on the roster has its icon (ConceptArt/Skills,
	// Game/Scripts/BuildIconArt.ps1 -Kind Abilities), and a Vanguard's kit has all of its icons or none:
	// a kit half drawn is an import that went wrong.
	TEST_CLASS(AbilityArt, "Veyra.UI")
	{
		TEST_METHOD(EveryFluxSpellHasItsIcon)
		{
			TArray<FString> Missing;
			for (const FVeyraContentId& Spell : UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster)
			{
				if (!VeyraShellArt::AbilityIconOf(Spell.ToString()))
				{
					Missing.Add(Spell.ToString());
				}
			}
			ASSERT_THAT(IsTrue(Missing.IsEmpty(), FString::Printf(TEXT("no icon for %s; run Game/Scripts/BuildIconArt.ps1 -Kind Abilities"), *FString::Join(Missing, TEXT(", ")))));
		}

		TEST_METHOD(AKitHasAllItsIconsOrNone)
		{
			int32 Drawn = 0;
			for (const TPair<FVeyraContentId, FVeyraVanguardDefinition>& Vanguard : UVeyraVanguardsTuningSubsystem::Get().Vanguards)
			{
				TArray<FVeyraContentId> Kit = Vanguard.Value.Passive;
				for (const TArray<FVeyraContentId>* Slot : { &Vanguard.Value.Abilities.Q, &Vanguard.Value.Abilities.W, &Vanguard.Value.Abilities.E, &Vanguard.Value.Abilities.R })
				{
					Kit.Append(*Slot);
				}
				TArray<FString> Missing;
				for (const FVeyraContentId& Ability : Kit)
				{
					if (!VeyraShellArt::AbilityIconOf(Ability.ToString()))
					{
						Missing.Add(Ability.ToString());
					}
				}
				ASSERT_THAT(IsTrue(Missing.IsEmpty() || Missing.Num() == Kit.Num(),
					FString::Printf(TEXT("%s has some icons but none for %s"), *Vanguard.Key.ToString(), *FString::Join(Missing, TEXT(", ")))));
				Drawn += Missing.IsEmpty() && !Kit.IsEmpty() ? 1 : 0;
			}
			ASSERT_THAT(IsTrue(Drawn > 0, TEXT("some Vanguard's kit has its icons")));
		}

		TEST_METHOD(AnUnknownAbilityHasNoIcon)
		{
			ASSERT_THAT(IsNull(VeyraShellArt::AbilityIconOf(TEXT("no_such_ability"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
