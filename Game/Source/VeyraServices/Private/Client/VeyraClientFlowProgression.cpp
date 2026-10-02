// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Client/VeyraClientFlow.h"

#include "Backend/VeyraProgressionProtocol.h"
#include "Misc/Guid.h"

// Account progression, the Collection and purchases (ADR-045 §7). The backend owns every level, balance,
// price and entitlement; the flow reads them and asks for purchases.

namespace
{
	const TCHAR* const ProgressionPath = TEXT("/v1/me/progression");
	const TCHAR* const CollectionPath = TEXT("/v1/me/collection");
	const TCHAR* const PurchasesPath = TEXT("/v1/me/purchases");
	/** The Collection's feedback after a purchase went through. */
	const TCHAR* const PurchasedFeedback = TEXT("vanguard_purchased");

	/** The backend's refusal code, or "http_<status>" when it gave none. */
	FString PurchaseRefusalCode(const FVeyraBackendResponse& Response)
	{
		const FString Code = VeyraBackendProtocol::ParseErrorCode(Response.Body);
		return Code.IsEmpty() ? FString::Printf(TEXT("http_%d"), Response.Status) : Code;
	}
}

void FVeyraClientFlow::ReadProgression()
{
	Probe(EVerb::Get, ProgressionPath, [this](const FVeyraBackendResponse& Response) {
		VeyraBackendProtocol::FProgression Progression;
		FString Problem;
		// A backend without progression, or a failed read, leaves the last read as it was.
		if (!Response.IsSuccess() || !VeyraBackendProtocol::ParseProgression(Response.Body, Progression, Problem))
		{
			return;
		}
		Snapshot.Progression = Progression;
		Broadcast();
	});
}

bool FVeyraClientFlow::LoadCollection()
{
	if (!CanIssue(EVeyraClientIntent::LoadCollection))
	{
		return false;
	}
	Log(TEXT("reading the Collection."));
	SetBusy(true);
	Call(EVerb::Get, CollectionPath, FString(), [this](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		TArray<VeyraBackendProtocol::FCollectionEntry> Vanguards;
		FString Problem;
		if (!Response.IsSuccess())
		{
			ShowRefusal(Response, TEXT("the Collection"), [this] { LoadCollection(); });
		}
		else if (!VeyraBackendProtocol::ParseCollection(Response.Body, Vanguards, Problem))
		{
			ShowBadAnswer(TEXT("the Collection"), Problem, [this] { LoadCollection(); });
		}
		else
		{
			Snapshot.Collection.Vanguards = MoveTemp(Vanguards);
			Snapshot.Collection.bLoaded = true;
			Broadcast();
		}
	});
	ReadProgression();
	return true;
}

bool FVeyraClientFlow::PurchaseVanguard(const FString& VanguardId, VeyraBackendProtocol::ECurrency Currency)
{
	const VeyraBackendProtocol::FCollectionEntry* Entry = Snapshot.Collection.Vanguards.FindByPredicate(
		[&VanguardId](const VeyraBackendProtocol::FCollectionEntry& Candidate) { return Candidate.VanguardId == VanguardId; });
	if (!CanIssue(EVeyraClientIntent::PurchaseVanguard) || !Entry || !Entry->bPurchasable)
	{
		return false;
	}
	// The same purchase asked for again, after its answer was lost, keeps its ID: the backend returns it
	// without spending twice. Any other is a new purchase.
	if (!PendingPurchase.IsSet() || PendingPurchase->VanguardId != VanguardId || PendingPurchase->Currency != Currency)
	{
		PendingPurchase = FPendingPurchase{ FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower), VanguardId, Currency };
	}
	Log(FString::Printf(TEXT("buying %s with %s."), *VanguardId, VeyraBackendProtocol::CurrencyName(Currency)));
	SetBusy(true);
	const FString Body = VeyraBackendProtocol::BuildPurchaseBody(PendingPurchase->Id, VanguardId, Currency);
	Call(EVerb::Post, PurchasesPath, Body, [this, VanguardId](const FVeyraBackendResponse& Response) {
		SetBusy(false);
		// An answer, accepted or refused, ends the purchase; only a lost one keeps its ID.
		PendingPurchase.Reset();
		VeyraBackendProtocol::FProgression Progression;
		FString Problem;
		if (!Response.IsSuccess())
		{
			ShowCollectionFeedback(PurchaseRefusalCode(Response), VanguardId);
		}
		else if (!VeyraBackendProtocol::ParsePurchase(Response.Body, Progression, Problem))
		{
			ShowBadAnswer(TEXT("the purchase"), Problem, [this] { LoadCollection(); });
		}
		else
		{
			Snapshot.Progression = Progression;
			ShowCollectionFeedback(PurchasedFeedback, VanguardId);
		}
		// Ownership, and whether anything else can still be bought, are the backend's: read them again.
		LoadCollection();
	});
	return true;
}

void FVeyraClientFlow::ShowCollectionFeedback(const FString& Code, const FString& VanguardId)
{
	Log(FString::Printf(TEXT("Collection: %s (%s)."), *Code, *VanguardId));
	Snapshot.Collection.Feedback = Code;
	Snapshot.Collection.FeedbackVanguard = VanguardId;
	Broadcast();
}
