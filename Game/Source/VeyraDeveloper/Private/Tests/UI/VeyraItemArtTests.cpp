// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"

#if WITH_AUTOMATION_WORKER && WITH_VEYRA_UI

#include "Engine/Texture2D.h"
#include "Shell/VeyraShellArt.h"
#include "Slots/VeyraAbilitySlot.h"
#include "Tuning/VeyraAbilitiesTuning.h"
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

	// Veyra.UI.AbilityArt.*: every Flux Spell on the roster and every ability of a playable Vanguard's kit has
	// its icon (ConceptArt/Skills, Game/Scripts/BuildIconArt.ps1 -Kind Abilities).
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

		TEST_METHOD(EveryPlayableKitHasItsIcons)
		{
			for (const TPair<FVeyraContentId, FVeyraVanguardDefinition>& Vanguard : UVeyraVanguardsTuningSubsystem::Get().Vanguards)
			{
				// A developer Vanguard, such as the tests' own, needs none.
				if (Vanguard.Value.Availability != EVeyraVanguardAvailability::Playable)
				{
					continue;
				}
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
				ASSERT_THAT(IsTrue(Missing.IsEmpty(), FString::Printf(TEXT("%s has no icon for %s; run Game/Scripts/BuildIconArt.ps1 -Kind Abilities"),
					*Vanguard.Key.ToString(), *FString::Join(Missing, TEXT(", ")))));
			}
		}

		TEST_METHOD(EveryAbilityAKitSlotCanHoldShowsAnIcon)
		{
			// A slot can hold more than its own abilities: a buff's variants (Torr's Overcapacity swaps Q and E), the
			// follow-ups of a recast window, a ride's mounted actions and a volley's shot. While one does, the HUD shows
			// its icon, or its slot's own when it has none (VeyraShellArt::SlotIconOf), never its name in text.
			const FVeyraAbilitiesTuning& Abilities = UVeyraAbilitiesTuningSubsystem::Get();
			TArray<FString> Missing;
			TSet<FString> Reached;
			for (const TPair<FVeyraContentId, FVeyraVanguardDefinition>& Vanguard : UVeyraVanguardsTuningSubsystem::Get().Vanguards)
			{
				if (Vanguard.Value.Availability != EVeyraVanguardAvailability::Playable)
				{
					continue;
				}
				const FVeyraVanguardKitTuning& Kit = Vanguard.Value.Abilities;
				const TMap<EVeyraAbilitySlot, const TArray<FVeyraContentId>*> Own = { { EVeyraAbilitySlot::Q, &Kit.Q }, { EVeyraAbilitySlot::W, &Kit.W },
					{ EVeyraAbilitySlot::E, &Kit.E }, { EVeyraAbilitySlot::R, &Kit.R } };
				TArray<TPair<EVeyraAbilitySlot, FVeyraContentId>> Open;
				for (const TPair<EVeyraAbilitySlot, const TArray<FVeyraContentId>*>& Slot : Own)
				{
					for (const FVeyraContentId& Ability : *Slot.Value)
					{
						Open.Emplace(Slot.Key, Ability);
					}
				}
				TSet<FString> Seen;
				while (!Open.IsEmpty())
				{
					const TPair<EVeyraAbilitySlot, FVeyraContentId> Held = Open.Pop();
					const FString Key = FString::Printf(TEXT("slot %d: %s"), static_cast<int32>(Held.Key), *Held.Value.ToString());
					if (Seen.Contains(Key))
					{
						continue;
					}
					Seen.Add(Key);
					Reached.Add(Held.Value.ToString());
					const TArray<FVeyraContentId>* OwnAbilities = Own.FindRef(Held.Key);
					for (const FVeyraContentId& OwnAbility : OwnAbilities ? *OwnAbilities : TArray<FVeyraContentId>{ FVeyraContentId() })
					{
						if (!VeyraShellArt::SlotIconOf(Held.Value.ToString(), OwnAbility.IsValid() ? OwnAbility.ToString() : FString()))
						{
							Missing.Add(Vanguard.Key.ToString() + TEXT(" ") + Key);
						}
					}
					if (const FVeyraCastTuning* Cast = VeyraAbilityRules::FindCast(Abilities, Held.Value))
					{
						for (const FVeyraRecastTuning& Recast : Cast->RecastWindow)
						{
							Open.Emplace(Held.Key, Recast.Ability);
						}
					}
					if (const FVeyraSelfBuffAbilityTuning* Buff = Abilities.SelfBuff.Find(Held.Value))
					{
						for (const FVeyraVariantTuning& Variant : Buff->Variants)
						{
							Open.Emplace(Variant.Slot, Variant.Ability);
						}
					}
					if (const FVeyraRideAbilityTuning* Ride = Abilities.Ride.Find(Held.Value))
					{
						for (const FVeyraRideSlotTuning& Mounted : Ride->Mounted)
						{
							Open.Emplace(Mounted.Slot, Mounted.Ability);
						}
					}
					if (const FVeyraVolleyAbilityTuning* Volley = Abilities.Volley.Find(Held.Value))
					{
						Open.Emplace(Held.Key, Volley->Shot);
					}
				}
			}
			ASSERT_THAT(IsTrue(Reached.Contains(TEXT("torr_battering_mass_over")) && Reached.Contains(TEXT("torr_anchor_rip_over")),
				TEXT("the walk reaches Overcapacity's variants and their follow-ups")));
			ASSERT_THAT(IsTrue(Missing.IsEmpty(), FString::Printf(TEXT("no icon for %s"), *FString::Join(Missing, TEXT(", ")))));
		}

		TEST_METHOD(AnOverrideWithoutAnIconShowsItsSlotsOwn)
		{
			UTexture2D* Anchor = VeyraShellArt::AbilityIconOf(TEXT("torr_anchor"));
			ASSERT_THAT(IsNotNull(Anchor));
			ASSERT_THAT(IsTrue(VeyraShellArt::SlotIconOf(TEXT("no_such_ability"), TEXT("torr_anchor")) == Anchor));
			ASSERT_THAT(IsTrue(VeyraShellArt::SlotIconOf(TEXT("torr_anchor_rip"), TEXT("torr_anchor")) == VeyraShellArt::AbilityIconOf(TEXT("torr_anchor_rip")),
				TEXT("one with its own icon shows it")));
			ASSERT_THAT(IsNull(VeyraShellArt::SlotIconOf(TEXT("no_such_ability"), FString())));
		}

		TEST_METHOD(AnUnknownAbilityHasNoIcon)
		{
			ASSERT_THAT(IsNull(VeyraShellArt::AbilityIconOf(TEXT("no_such_ability"))));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER && WITH_VEYRA_UI
