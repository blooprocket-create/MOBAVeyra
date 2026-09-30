// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shop/VeyraShopModel.h"

#include "Buyback/VeyraBuybackComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Gold/VeyraGoldComponent.h"
#include "Inventory/VeyraInventoryComponent.h"
#include "Life/VeyraLifeComponent.h"
#include "Loadout/VeyraAbilityLoadoutComponent.h"
#include "Rewards/VeyraEconomyTuningSubsystem.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "Tuning/VeyraItemsTuning.h"
#include "Rules/VeyraMatchRules.h"
#include "Shell/VeyraShellModels.h"
#include "Tuning/VeyraItemsTuningSubsystem.h"
#include "VeyraGameState.h"

#define LOCTEXT_NAMESPACE "VeyraShopModel"

namespace VeyraShopModel
{
FVeyraShopView Describe(const AActor& Participant)
{
	FVeyraShopView View;
	const FVeyraItemsTuning& Tuning = UVeyraItemsTuningSubsystem::Get();
	const UVeyraInventoryComponent* Inventory = Participant.FindComponentByClass<UVeyraInventoryComponent>();
	const UVeyraGoldComponent* Gold = Participant.FindComponentByClass<UVeyraGoldComponent>();
	if (!Inventory || !Gold)
	{
		return View;
	}
	View.Gold = Gold->GetGold();
	View.bAtShop = Inventory->IsAtFountain();
	// A dead Vanguard's buyback, in a standard match: priced by the rules the server uses, from the
	// replicated match clock and the owner's buybacks (§15; ADR-020 §3).
	const UWorld* World = Participant.GetWorld();
	const AVeyraGameState* Match = World ? World->GetGameState<AVeyraGameState>() : nullptr;
	const UVeyraLifeComponent* Life = Participant.FindComponentByClass<UVeyraLifeComponent>();
	const UVeyraBuybackComponent* Buyback = Participant.FindComponentByClass<UVeyraBuybackComponent>();
	if (Match && VeyraMatchRules::AllowsBuyback(Match->GetMatchRules()) && Life && !Life->IsAlive() && Buyback)
	{
		View.bBuybackShown = true;
		View.Buyback = Buyback->Quote(Match->GetMatchClockSeconds(), Match->GetServerWorldTimeSeconds(), /*bDead*/ true, *Gold);
	}
	View.UndoSteps = View.bAtShop ? Inventory->GetUndoStepCount() : 0;
	for (const FVeyraInventorySlot& Slot : Inventory->GetSlots())
	{
		FVeyraShopSlot& Shown = View.Slots.AddDefaulted_GetRef();
		if (!Slot.IsEmpty())
		{
			Shown.Item = Slot.Item;
			Shown.Count = Slot.Count;
			Shown.SaleValue = VeyraInventory::ResaleValue(Tuning, Slot);
		}
	}
	for (const FVeyraPendingPurchase& Entry : Inventory->GetQueue())
	{
		View.Pending.Add(FVeyraShopPending{ Entry.Item, Entry.Paid });
	}
	for (const TPair<FVeyraContentId, FVeyraItemDefinition>& Pair : Tuning.Items)
	{
		FVeyraShopOffer& Offer = View.Offers.AddDefaulted_GetRef();
		Offer.Item = Pair.Key;
		Offer.Tier = Pair.Value.Tier;
		Offer.Category = Pair.Value.Category;
		Offer.TotalCost = VeyraItems::TotalCost(Tuning, Pair.Key);
		// The rules the server prices a purchase by, from the same slots and queue.
		const FVeyraPurchaseQuote Quote = VeyraInventory::Quote(Tuning, Inventory->GetSlots(), Inventory->GetQueue(), Pair.Key);
		Offer.Price = Quote.Price;
		Offer.Refusal = Quote.Refusal;
		if (Offer.Refusal == EVeyraShopRefusal::None && Offer.Price > View.Gold)
		{
			Offer.Refusal = EVeyraShopRefusal::NotEnoughGold;
		}
	}
	// The spell slots: a swap happens only at the fountain, for Gold, never to a spell already equipped.
	View.SpellSwapCost = UVeyraEconomyTuningSubsystem::Get().FluxSpells.SwapCost;
	if (const UVeyraAbilityLoadoutComponent* Loadout = Participant.FindComponentByClass<UVeyraAbilityLoadoutComponent>())
	{
		TArray<FVeyraContentId, TInlineAllocator<2>> Equipped;
		for (const EVeyraAbilitySlot SpellSlot : VeyraAbilitySlots::Spells)
		{
			const FVeyraLoadoutEntry* Entry = Loadout->FindSlot(SpellSlot);
			FVeyraShopSpellSlot& Shown = View.SpellSlots.AddDefaulted_GetRef();
			Shown.Spell = Entry ? Entry->Ability : FVeyraContentId();
			Shown.bLocked = Loadout->IsLocked(SpellSlot);
			Equipped.Add(Shown.Spell);
		}
		for (FVeyraShopSpellSlot& Shown : View.SpellSlots)
		{
			for (const FVeyraContentId& Spell : UVeyraAbilitiesTuningSubsystem::Get().FluxSpells.Roster)
			{
				EVeyraShopRefusal Refusal = EVeyraShopRefusal::None;
				if (Equipped.Contains(Spell))
				{
					Refusal = EVeyraShopRefusal::AlreadyEquipped;
				}
				else if (!View.bAtShop)
				{
					Refusal = EVeyraShopRefusal::NotAtFountain;
				}
				else if (View.SpellSwapCost > View.Gold)
				{
					Refusal = EVeyraShopRefusal::NotEnoughGold;
				}
				Shown.Offers.Add(FVeyraShopSpellOffer{ Spell, Refusal });
			}
		}
	}
	// The vision tool: any other tool, at the fountain, for the same cost each time (Vision Bible §3).
	if (const UVeyraVisionToolComponent* Tool = Participant.FindComponentByClass<UVeyraVisionToolComponent>())
	{
		View.bHasVisionTool = true;
		View.VisionTool = Tool->GetEquipped();
		View.VisionToolSwapCost = UVeyraEconomyTuningSubsystem::Get().VisionTools.SwapCost;
		for (const EVeyraVisionTool Each : { EVeyraVisionTool::PersistentWard, EVeyraVisionTool::Sweeper, EVeyraVisionTool::QuickSight })
		{
			EVeyraShopRefusal Refusal = EVeyraShopRefusal::None;
			if (Each == View.VisionTool)
			{
				Refusal = EVeyraShopRefusal::AlreadyEquipped;
			}
			else if (!View.bAtShop)
			{
				Refusal = EVeyraShopRefusal::NotAtFountain;
			}
			else if (View.VisionToolSwapCost > View.Gold)
			{
				Refusal = EVeyraShopRefusal::NotEnoughGold;
			}
			View.VisionToolOffers.Add(FVeyraShopVisionToolOffer{ Each, Refusal });
		}
	}
	View.Offers.Sort([](const FVeyraShopOffer& A, const FVeyraShopOffer& B) {
		if (A.Tier != B.Tier)
		{
			return A.Tier < B.Tier;
		}
		if (A.TotalCost != B.TotalCost)
		{
			return A.TotalCost < B.TotalCost;
		}
		return A.Item.ToString() < B.Item.ToString();
	});
	return View;
}

FText DescribeStats(const FVeyraItemStatsTuning& Stats)
{
	// Attack Speed is a fraction of base, shown as a whole percentage.
	constexpr double Percent = 100.0;
	TArray<FText> Lines;
	const auto Add = [&Lines](double Value, const FText& Format) {
		if (Value != 0.0)
		{
			Lines.Add(FText::Format(Format, FText::AsNumber(FMath::RoundToInt32(Value))));
		}
	};
	Add(Stats.Health, LOCTEXT("Health", "+{0} Health"));
	Add(Stats.HealthRegeneration, LOCTEXT("HealthRegeneration", "+{0} Health Regeneration"));
	Add(Stats.PhysicalPower, LOCTEXT("PhysicalPower", "+{0} Physical Power"));
	Add(Stats.MagicPower, LOCTEXT("MagicPower", "+{0} Magic Power"));
	Add(Stats.AttackSpeed * Percent, LOCTEXT("AttackSpeed", "+{0}% Attack Speed"));
	Add(Stats.AbilityHaste, LOCTEXT("AbilityHaste", "+{0} Ability Haste"));
	Add(Stats.MoveSpeed, LOCTEXT("MoveSpeed", "+{0} Movement Speed"));
	Add(Stats.MagicPenetrationFlat, LOCTEXT("MagicPenetrationFlat", "+{0} Magic Penetration"));
	return FText::Join(LOCTEXT("StatSeparator", ", "), Lines);
}

TArray<FVeyraContentId> BuildsInto(const FVeyraItemsTuning& Tuning, const FVeyraContentId& Item)
{
	TArray<FVeyraContentId> Out;
	for (const TPair<FVeyraContentId, FVeyraItemDefinition>& Pair : Tuning.Items)
	{
		if (Pair.Value.Components.Contains(Item))
		{
			Out.Add(Pair.Key);
		}
	}
	Out.Sort([&Tuning](const FVeyraContentId& A, const FVeyraContentId& B) {
		const FVeyraItemDefinition& First = Tuning.Items[A];
		const FVeyraItemDefinition& Second = Tuning.Items[B];
		if (First.Tier != Second.Tier)
		{
			return First.Tier < Second.Tier;
		}
		const double FirstCost = VeyraItems::TotalCost(Tuning, A);
		const double SecondCost = VeyraItems::TotalCost(Tuning, B);
		return FirstCost != SecondCost ? FirstCost < SecondCost : A.ToString() < B.ToString();
	});
	return Out;
}

FText DescribeBuybackRefusal(EVeyraBuybackRefusal Refusal)
{
	switch (Refusal)
	{
	case EVeyraBuybackRefusal::None:
		return FText::GetEmpty();
	case EVeyraBuybackRefusal::Unavailable:
		return LOCTEXT("BuybackUnavailable", "Buyback is not available now.");
	case EVeyraBuybackRefusal::TooEarly:
		return FText::Format(LOCTEXT("BuybackTooEarly", "Buyback opens at {0}."),
			VeyraShellModels::FormatCountdown(UVeyraEconomyTuningSubsystem::Get().Buyback.AvailableFromSeconds));
	case EVeyraBuybackRefusal::Alive:
		return LOCTEXT("BuybackAlive", "Only the dead buy back.");
	case EVeyraBuybackRefusal::CoolingDown:
		return LOCTEXT("BuybackCoolingDown", "Buyback is cooling down.");
	case EVeyraBuybackRefusal::NotEnoughGold:
		return LOCTEXT("BuybackGold", "Not enough Gold to buy back.");
	}
	return FText::GetEmpty();
}

FText DescribeRefusal(EVeyraShopRefusal Refusal)
{
	switch (Refusal)
	{
	case EVeyraShopRefusal::None:
		return FText::GetEmpty();
	case EVeyraShopRefusal::UnknownItem:
		return LOCTEXT("UnknownItem", "The shop does not sell that.");
	case EVeyraShopRefusal::InventoryFull:
		return LOCTEXT("InventoryFull", "No slot is free for it.");
	case EVeyraShopRefusal::Unique:
		return LOCTEXT("Unique", "You already hold one.");
	case EVeyraShopRefusal::BootsLimit:
		return LOCTEXT("BootsLimit", "You already have Boots.");
	case EVeyraShopRefusal::NotEnoughGold:
		return LOCTEXT("NotEnoughGold", "Not enough Gold.");
	case EVeyraShopRefusal::NotAtFountain:
		return LOCTEXT("NotAtFountain", "Only at your fountain.");
	case EVeyraShopRefusal::NothingToUndo:
		return LOCTEXT("NothingToUndo", "Nothing to undo.");
	case EVeyraShopRefusal::AlreadyUsed:
		return LOCTEXT("AlreadyUsed", "It has been used: sell it instead.");
	case EVeyraShopRefusal::EmptySlot:
		return LOCTEXT("EmptySlot", "That slot is empty.");
	case EVeyraShopRefusal::MissingComponent:
		return LOCTEXT("MissingComponent", "A component it needs is gone.");
	case EVeyraShopRefusal::NotNow:
		return LOCTEXT("NotNow", "Not now.");
	case EVeyraShopRefusal::StillRestoring:
		return LOCTEXT("StillRestoring", "One is still restoring.");
	case EVeyraShopRefusal::UnknownSpell:
		return LOCTEXT("UnknownSpell", "There is no such Flux Spell.");
	case EVeyraShopRefusal::NoSuchSpellSlot:
		return LOCTEXT("NoSuchSpellSlot", "There is no such spell slot.");
	case EVeyraShopRefusal::AlreadyEquipped:
		return LOCTEXT("AlreadyEquipped", "That Flux Spell is equipped already.");
	}
	return FText::GetEmpty();
}
}

#undef LOCTEXT_NAMESPACE
