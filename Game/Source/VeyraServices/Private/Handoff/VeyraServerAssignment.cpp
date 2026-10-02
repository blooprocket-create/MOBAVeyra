// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Handoff/VeyraServerAssignment.h"

#include "Backend/VeyraBackendProtocol.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tuning/VeyraTuning.h"

namespace VeyraServerAssignment
{
FString SchemaPath()
{
	// Staged with the module (VeyraServices.Build.cs), under the same path as in the project.
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"), TEXT("VeyraServices"), TEXT("Schemas"), TEXT("MatchAssignment.schema.json"));
}

TArray<FString> ReadSchema(FString& OutSchemaText)
{
	const FString Path = SchemaPath();
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
	{
		return { FString::Printf(TEXT("%s: cannot be read"), *Path) };
	}
	OutSchemaText = VeyraTuning::DecodeUtf8(Bytes);
	return {};
}

TArray<FString> Parse(FStringView Line, FStringView SchemaText, FVeyraServerAssignment& Out)
{
	FVeyraAssignmentDocument Document;
	TArray<FString> Problems = VeyraTuning::ValidateAndBind(Line, SchemaText, SchemaVersion, Document);
	if (!Problems.IsEmpty())
	{
		// A JSON syntax error quotes the text around it, which is the whole line.
		for (FString& Problem : Problems)
		{
			Problem = VeyraBackendProtocol::RedactCredentials(Problem);
		}
		return Problems;
	}

	FVeyraServerAssignment Assignment;
	Assignment.Match.MatchId = MoveTemp(Document.MatchId);
	Assignment.Match.Mode = Document.Mode;
	Assignment.Match.Rules = Document.Rules;
	Assignment.Match.HostAccountId = Document.HostAccountId.IsEmpty() ? FString() : MoveTemp(Document.HostAccountId[0]);
	Assignment.BackendUrl = MoveTemp(Document.BackendUrl);
	Assignment.ServerCredential = MoveTemp(Document.ServerCredential);
	const auto SideOf = [](EVeyraAssignedSide Side) { return Side == EVeyraAssignedSide::A ? EVeyraTeam::A : EVeyraTeam::B; };
	for (FVeyraAssignmentParticipantDocument& Participant : Document.Participants)
	{
		FVeyraAssignedParticipant& Assigned = Assignment.Match.Participants.Add_GetRef({ MoveTemp(Participant.AccountId), MoveTemp(Participant.DisplayName),
			SideOf(Participant.Side), MoveTemp(Participant.TicketHash), Participant.VanguardId });
		Assigned.MasteryLevel = Participant.MasteryLevel;
		Assigned.EmoteTier = Participant.EmoteTier;
		// The schema holds each to a content ID or empty, and an empty slot has no ID.
		for (const FString& Spell : Participant.FluxSpells)
		{
			Assigned.FluxSpells.Add(Spell.IsEmpty() ? FVeyraContentId() : FVeyraContentId::FromText(Spell).Get(FVeyraContentId()));
		}
	}
	for (const FVeyraAssignmentBotDocument& Bot : Document.Bots)
	{
		Assignment.Match.Bots.Add({ SideOf(Bot.Side), Bot.VanguardId, Bot.Difficulty });
	}
	if (!Document.Settings.IsEmpty())
	{
		const FVeyraAssignmentSettingsDocument& Settings = Document.Settings[0];
		FVeyraCustomSettings& Custom = Assignment.Match.Custom.Emplace();
		Custom.bVictoryEnabled = Settings.Victory == EVeyraAssignedVictory::Enabled;
		if (!Settings.StartingGold.IsEmpty())
		{
			Custom.StartingGold = Settings.StartingGold[0];
		}
	}
	Out = MoveTemp(Assignment);
	return Problems;
}
}
