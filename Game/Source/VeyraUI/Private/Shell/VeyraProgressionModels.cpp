// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Shell/VeyraProgressionModels.h"

#include "Misc/StringBuilder.h"
#include "Shell/VeyraShellModels.h"

#define LOCTEXT_NAMESPACE "VeyraProgressionModels"

namespace VeyraProgressionModels
{
namespace
{
	using VeyraBackendProtocol::ECurrency;

	FText StatusOf(const VeyraBackendProtocol::FCollectionEntry& Entry)
	{
		if (Entry.bOwned)
		{
			return Entry.Source == TEXT("starter") ? LOCTEXT("OwnedStarter", "Your starter") : LOCTEXT("Owned", "Owned");
		}
		// Visible is not playable (Bible §4): only the rotation lends an unowned Vanguard.
		return Entry.bRotation ? LOCTEXT("Rotation", "Free this week") : LOCTEXT("NotOwned", "Not owned");
	}

	/** Why a match gave no account XP, as the player reads it (ADR-045 §3). */
	FText ReasonText(const FString& Reason)
	{
		if (Reason == TEXT("custom"))
		{
			return LOCTEXT("ReasonCustom", "Custom and practice matches give no account rewards.");
		}
		if (Reason == TEXT("no_contest"))
		{
			return LOCTEXT("ReasonNoContest", "A remake gives no rewards.");
		}
		if (Reason == TEXT("not_completed"))
		{
			return LOCTEXT("ReasonNotCompleted", "The match did not finish, so it gives no rewards.");
		}
		if (Reason == TEXT("not_joined"))
		{
			return LOCTEXT("ReasonNotJoined", "You never joined this match, so it gives you no rewards.");
		}
		if (Reason == TEXT("personal_loss"))
		{
			return LOCTEXT("ReasonPersonalLoss", "Your absence cost you this match's rewards.");
		}
		if (Reason == TEXT("coop_level"))
		{
			return LOCTEXT("ReasonCoopLevel", "Co-op vs AI no longer gives you account XP. Its Mastery still counts.");
		}
		return LOCTEXT("ReasonOther", "This match gives no account rewards.");
	}

	FText FeedbackText(const FString& Code, const FText& Name)
	{
		if (Code.IsEmpty())
		{
			return FText::GetEmpty();
		}
		if (Code == TEXT("vanguard_purchased"))
		{
			return FText::Format(LOCTEXT("Purchased", "{0} is yours."), Name);
		}
		if (Code == TEXT("insufficient_balance"))
		{
			return FText::Format(LOCTEXT("Insufficient", "Not enough to buy {0}."), Name);
		}
		if (Code == TEXT("already_owned"))
		{
			return FText::Format(LOCTEXT("AlreadyOwned", "You already own {0}."), Name);
		}
		if (Code == TEXT("not_for_sale"))
		{
			return FText::Format(LOCTEXT("NotForSale", "{0} cannot be bought."), Name);
		}
		return FText::Format(LOCTEXT("PurchaseFailed", "The purchase of {0} did not go through ({1})."), Name, FText::FromString(Code));
	}

	int64 BalanceOf(const TOptional<VeyraBackendProtocol::FProgression>& Progression, ECurrency Currency)
	{
		return !Progression.IsSet() ? MAX_int64 : Currency == ECurrency::RefinedFlux ? Progression->RefinedFlux : Progression->Flux;
	}
}

FText Amount(ECurrency Currency, int64 Value)
{
	return Currency == ECurrency::RefinedFlux ? FText::Format(LOCTEXT("RefinedFluxAmount", "{0} Refined Flux"), FText::AsNumber(Value))
											  : FText::Format(LOCTEXT("FluxAmount", "{0} Flux"), FText::AsNumber(Value));
}

FVeyraProgressionBarModel DescribeBar(const TOptional<VeyraBackendProtocol::FProgression>& Progression)
{
	FVeyraProgressionBarModel Model;
	if (!Progression.IsSet())
	{
		return Model;
	}
	Model.bShown = true;
	Model.Level = FText::Format(LOCTEXT("Level", "Level {0}"), FText::AsNumber(Progression->Level));
	Model.XP = FText::Format(LOCTEXT("XP", "{0} / {1} XP"), FText::AsNumber(Progression->LevelXP), FText::AsNumber(Progression->LevelNeed));
	Model.Fraction = Progression->LevelNeed > 0 ? FMath::Clamp(static_cast<float>(Progression->LevelXP) / Progression->LevelNeed, 0.0f, 1.0f) : 0.0f;
	Model.Currencies = FText::Format(LOCTEXT("Currencies", "{0}  ·  {1}"), Amount(ECurrency::Flux, Progression->Flux), Amount(ECurrency::RefinedFlux, Progression->RefinedFlux));
	return Model;
}

FVeyraCollectionModel DescribeCollection(const FVeyraClientSnapshot& Snapshot, bool bCanPurchase)
{
	FVeyraCollectionModel Model;
	const FVeyraCollection& Collection = Snapshot.Collection;
	Model.bLoaded = Collection.bLoaded;
	Model.Feedback = FeedbackText(Collection.Feedback, VeyraShellModels::VanguardNameOf(Collection.FeedbackVanguard));
	if (Snapshot.Progression.IsSet())
	{
		Model.Balance = FText::Format(LOCTEXT("Balance", "You have {0} and {1}."), Amount(ECurrency::Flux, Snapshot.Progression->Flux),
			Amount(ECurrency::RefinedFlux, Snapshot.Progression->RefinedFlux));
	}
	for (const VeyraBackendProtocol::FCollectionEntry& Entry : Collection.Vanguards)
	{
		FVeyraCollectionCard& Card = Model.Cards.AddDefaulted_GetRef();
		Card.VanguardId = Entry.VanguardId;
		Card.Name = VeyraShellModels::VanguardNameOf(Entry.VanguardId);
		Card.Status = StatusOf(Entry);
		Card.MasteryShort = FText::Format(LOCTEXT("MasteryShort", "Mastery {0}"), FText::AsNumber(Entry.Mastery.Level));
		// The player's own Mastery shows whether or not they can play the Vanguard now (Bible §4).
		Card.Details = {
			FText::Format(LOCTEXT("MasteryLevel", "Mastery Level {0}: {1} / {2} points to the next"), FText::AsNumber(Entry.Mastery.Level),
				FText::AsNumber(Entry.Mastery.LevelPoints), FText::AsNumber(Entry.Mastery.LevelNeed)),
			FText::Format(LOCTEXT("MasteryLifetime", "{0} Mastery points in all"), FText::AsNumber(Entry.Mastery.LifetimePoints)),
			FText::Format(LOCTEXT("MasteryEmote", "Mastery emote: tier {0}"), FText::AsNumber(Entry.Mastery.EmoteTier)),
		};
		Card.bPurchasable = Entry.bPurchasable;
		Card.PriceFlux = Entry.PriceFlux;
		Card.PriceRefinedFlux = Entry.PriceRefinedFlux;
		// The balance as last read only hides a Buy the player cannot afford; the backend decides every purchase.
		Card.bCanBuyWithFlux = bCanPurchase && Entry.bPurchasable && BalanceOf(Snapshot.Progression, ECurrency::Flux) >= Entry.PriceFlux;
		Card.bCanBuyWithRefinedFlux = bCanPurchase && Entry.bPurchasable && BalanceOf(Snapshot.Progression, ECurrency::RefinedFlux) >= Entry.PriceRefinedFlux;
	}
	return Model;
}

FVeyraRewardsModel DescribeRewards(const TOptional<VeyraBackendProtocol::FMatchOutcome>& Result)
{
	FVeyraRewardsModel Model;
	if (!Result.IsSet() || !Result->Rewards.IsSet())
	{
		return Model;
	}
	const VeyraBackendProtocol::FMatchRewards& Rewards = *Result->Rewards;
	Model.bShown = true;
	if (!Rewards.Reason.IsEmpty())
	{
		Model.Lines.Add(ReasonText(Rewards.Reason));
	}
	if (Rewards.AccountXP > 0)
	{
		Model.Lines.Add(FText::Format(LOCTEXT("RewardXP", "+{0} account XP"), FText::AsNumber(Rewards.AccountXP)));
	}
	if (Rewards.LevelAfter > Rewards.LevelBefore)
	{
		Model.Lines.Add(FText::Format(LOCTEXT("RewardLevel", "Level up: Level {0} to {1}"), FText::AsNumber(Rewards.LevelBefore), FText::AsNumber(Rewards.LevelAfter)));
	}
	if (Rewards.Flux > 0)
	{
		Model.Lines.Add(FText::Format(LOCTEXT("RewardFlux", "+{0}"), Amount(ECurrency::Flux, Rewards.Flux)));
	}
	if (Rewards.RefinedFlux > 0)
	{
		Model.Lines.Add(FText::Format(LOCTEXT("RewardRefinedFlux", "+{0}"), Amount(ECurrency::RefinedFlux, Rewards.RefinedFlux)));
	}
	if (Rewards.MasteryPoints > 0)
	{
		const FText Name = VeyraShellModels::VanguardNameOf(Rewards.VanguardId);
		Model.Lines.Add(FText::Format(LOCTEXT("RewardMastery", "+{0} {1} Mastery points"), FText::AsNumber(Rewards.MasteryPoints), Name));
		if (Rewards.MasteryAfter > Rewards.MasteryBefore)
		{
			Model.Lines.Add(FText::Format(LOCTEXT("RewardMasteryLevel", "{0} Mastery Level {1} to {2}"), Name, FText::AsNumber(Rewards.MasteryBefore),
				FText::AsNumber(Rewards.MasteryAfter)));
		}
	}
	return Model;
}

FText CollectionCardLabel(const FString& VanguardId)
{
	return FText::Format(LOCTEXT("CollectionCard", "Collection {0}"), VeyraShellModels::VanguardNameOf(VanguardId));
}

FText BuyLabel(const FString& VanguardId, ECurrency Currency, int64 Price)
{
	return FText::Format(LOCTEXT("Buy", "Buy {0} for {1}"), VeyraShellModels::VanguardNameOf(VanguardId), Amount(Currency, Price));
}

FText ConfirmBuyLabel(const FString& VanguardId, ECurrency Currency, int64 Price)
{
	return FText::Format(LOCTEXT("ConfirmBuy", "Confirm Buy {0} for {1}"), VeyraShellModels::VanguardNameOf(VanguardId), Amount(Currency, Price));
}

FText ConfirmBuyPrompt(const FString& VanguardId, ECurrency Currency, int64 Price, const TOptional<VeyraBackendProtocol::FProgression>& Progression)
{
	const FText Name = VeyraShellModels::VanguardNameOf(VanguardId);
	if (!Progression.IsSet())
	{
		return FText::Format(LOCTEXT("ConfirmPrompt", "Buy {0} for {1}? It is yours to keep."), Name, Amount(Currency, Price));
	}
	const int64 Balance = Currency == ECurrency::RefinedFlux ? Progression->RefinedFlux : Progression->Flux;
	return FText::Format(LOCTEXT("ConfirmPromptBalance", "Buy {0} for {1}? It is yours to keep. You have {2}."), Name, Amount(Currency, Price), Amount(Currency, Balance));
}

FString Signature(const FVeyraClientSnapshot& Snapshot)
{
	TStringBuilder<512> Text;
	if (const TOptional<VeyraBackendProtocol::FProgression>& P = Snapshot.Progression; P.IsSet())
	{
		Text << TEXT("|level:") << P->Level << TEXT(":") << P->LevelXP << TEXT("/") << P->LevelNeed << TEXT(":") << P->Flux << TEXT(":") << P->RefinedFlux;
	}
	const FVeyraCollection& Collection = Snapshot.Collection;
	Text << TEXT("|collection:") << (Collection.bLoaded ? TEXT("loaded") : TEXT("unread")) << TEXT(":") << Collection.Feedback << TEXT(":") << Collection.FeedbackVanguard;
	for (const VeyraBackendProtocol::FCollectionEntry& Entry : Collection.Vanguards)
	{
		Text << TEXT(";") << Entry.VanguardId << TEXT(":") << (Entry.bOwned ? 1 : 0) << (Entry.bRotation ? 1 : 0) << (Entry.bPurchasable ? 1 : 0) << TEXT(":")
			 << Entry.Mastery.Level << TEXT(":") << Entry.Mastery.LevelPoints << TEXT(":") << Entry.PriceFlux << TEXT(":") << Entry.PriceRefinedFlux;
	}
	if (Snapshot.Result.IsSet() && Snapshot.Result->Rewards.IsSet())
	{
		const VeyraBackendProtocol::FMatchRewards& R = *Snapshot.Result->Rewards;
		Text << TEXT("|rewards:") << R.Reason << TEXT(":") << R.AccountXP << TEXT(":") << R.LevelAfter << TEXT(":") << R.MasteryPoints;
	}
	return FString(Text.ToString());
}
}

#undef LOCTEXT_NAMESPACE
