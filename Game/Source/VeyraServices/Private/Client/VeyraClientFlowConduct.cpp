// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Client/VeyraClientFlow.h"

#include "Backend/VeyraConductProtocol.h"
#include "Misc/Guid.h"

// Reports and commendation (ADR-047). The backend decides every report and commendation and tells the
// reporter nothing past receipt; the flow names players as the match recorded them, never by account.

namespace
{
	/** The feedback after a report was received. */
	const TCHAR* const ReportSentFeedback = TEXT("report_sent");
	/** The feedback after a commendation was recorded. */
	const TCHAR* const CommendedFeedback = TEXT("commended");

	FString ConductPath(const FString& MatchId, const TCHAR* Leaf)
	{
		return FString::Printf(TEXT("/v1/me/matches/%s/%s"), *MatchId, Leaf);
	}

	/** The backend's refusal code, or "http_<status>" when it gave none. */
	FString ConductRefusalCode(const FVeyraBackendResponse& Response)
	{
		const FString Code = VeyraBackendProtocol::ParseErrorCode(Response.Body);
		return Code.IsEmpty() ? FString::Printf(TEXT("http_%d"), Response.Status) : Code;
	}
}

void FVeyraClientFlow::ReadConduct(const FString& MatchId)
{
	Snapshot.Conduct = FVeyraConduct();
	Snapshot.Conduct.MatchId = MatchId;
	if (MatchId.IsEmpty())
	{
		return;
	}
	Probe(EVerb::Get, ConductPath(MatchId, TEXT("conduct")), [this, MatchId](const FVeyraBackendResponse& Response) {
		VeyraBackendProtocol::FConductRecord Record;
		FString Problem;
		// Another match opened since, a backend without reports, or a failed read: nothing to offer.
		if (Snapshot.Conduct.MatchId != MatchId || !Response.IsSuccess() || !VeyraBackendProtocol::ParseConductRecord(Response.Body, Record, Problem))
		{
			return;
		}
		Snapshot.Conduct.Record = MoveTemp(Record);
		Snapshot.Conduct.bLoaded = true;
		Broadcast();
	});
}

bool FVeyraClientFlow::ReportPlayer(const FString& Name, const FString& Reason, const FString& Details)
{
	const FVeyraConduct& Conduct = Snapshot.Conduct;
	// The match whose result or record shows, and a player the player has not reported in it.
	const bool bShown = (Snapshot.State == EVeyraClientState::Results && Snapshot.Result.IsSet() && Snapshot.Result->MatchId == Conduct.MatchId)
		|| (Snapshot.History.Opened.IsSet() && Snapshot.History.Opened->MatchId == Conduct.MatchId);
	if (!CanIssue(EVeyraClientIntent::ReportPlayer) || !Conduct.bLoaded || !bShown || Name.IsEmpty() || Name == Snapshot.DisplayName
		|| !Conduct.Record.Reasons.Contains(Reason) || Conduct.Record.Reported.Contains(Name))
	{
		return false;
	}
	// The same player reported again in the same match, after the answer was lost, keeps its ID: the backend
	// returns the first report instead of filing a second (ADR-047 §2).
	if (!PendingReport.IsSet() || PendingReport->MatchId != Conduct.MatchId || PendingReport->Name != Name)
	{
		PendingReport = FPendingReport{ FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower), Conduct.MatchId, Name };
	}
	Log(FString::Printf(TEXT("reporting a player in match %s."), *Conduct.MatchId));
	SetBusy(true);
	const FString MatchId = Conduct.MatchId;
	Call(EVerb::Post, ConductPath(MatchId, TEXT("reports")), VeyraBackendProtocol::BuildReportBody(Name, Reason, Details, PendingReport->Id),
		[this, MatchId, Name](const FVeyraBackendResponse& Response) {
			SetBusy(false);
			// An answer, received or refused, ends the report; only a lost one keeps its ID.
			PendingReport.Reset();
			if (Snapshot.Conduct.MatchId != MatchId)
			{
				return;
			}
			if (!Response.IsSuccess())
			{
				ShowConductFeedback(ConductRefusalCode(Response), Name);
				return;
			}
			Snapshot.Conduct.Record.Reported.AddUnique(Name);
			ShowConductFeedback(ReportSentFeedback, Name);
		});
	return true;
}

bool FVeyraClientFlow::CommendTeammate(const FString& Name)
{
	const FVeyraConduct& Conduct = Snapshot.Conduct;
	const bool bResults = Snapshot.Result.IsSet() && Snapshot.Result->MatchId == Conduct.MatchId;
	if (!CanIssue(EVeyraClientIntent::CommendTeammate) || !Conduct.bLoaded || !bResults || Name.IsEmpty() || Name == Snapshot.DisplayName
		|| !Conduct.Record.Commended.IsEmpty())
	{
		return false;
	}
	Log(FString::Printf(TEXT("commending a teammate in match %s."), *Conduct.MatchId));
	SetBusy(true);
	const FString MatchId = Conduct.MatchId;
	Call(EVerb::Post, ConductPath(MatchId, TEXT("commendation")), VeyraBackendProtocol::BuildCommendationBody(Name),
		[this, MatchId, Name](const FVeyraBackendResponse& Response) {
			SetBusy(false);
			if (Snapshot.Conduct.MatchId != MatchId)
			{
				return;
			}
			if (!Response.IsSuccess())
			{
				ShowConductFeedback(ConductRefusalCode(Response), Name);
				return;
			}
			Snapshot.Conduct.Record.Commended = Name;
			ShowConductFeedback(CommendedFeedback, Name);
		});
	return true;
}

void FVeyraClientFlow::ShowConductFeedback(const FString& Code, const FString& Name)
{
	Log(FString::Printf(TEXT("conduct: %s."), *Code));
	Snapshot.Conduct.Feedback = Code;
	Snapshot.Conduct.FeedbackName = Name;
	Broadcast();
}
