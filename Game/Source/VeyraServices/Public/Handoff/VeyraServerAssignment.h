// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Join/VeyraMatchAssignment.h"

#include "VeyraServerAssignment.generated.h"

/** A side as an assignment names it. Unlike EVeyraTeam, it has no None. */
UENUM()
enum class EVeyraAssignedSide : uint8
{
	A,
	B,
};

/** One participant in the assignment document. Schemas/MatchAssignment.schema.json describes it. */
USTRUCT()
struct FVeyraAssignmentParticipantDocument
{
	GENERATED_BODY()

	UPROPERTY()
	FString AccountId;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	EVeyraAssignedSide Side = EVeyraAssignedSide::A;

	UPROPERTY()
	FString TicketHash;

	UPROPERTY()
	FVeyraContentId VanguardId;
};

/** The assignment document a match server reads on standard input (ADR-007 §5, ADR-010 §9). */
USTRUCT()
struct FVeyraAssignmentDocument
{
	GENERATED_BODY()

	UPROPERTY()
	FString MatchId;

	UPROPERTY()
	FString BackendUrl;

	UPROPERTY()
	FString ServerCredential;

	UPROPERTY()
	FVeyraContentId Mode;

	UPROPERTY()
	EVeyraMatchRules Rules = EVeyraMatchRules::Standard;

	/** The practice host, or nothing: at most one entry. */
	UPROPERTY()
	TArray<FString> HostAccountId;

	UPROPERTY()
	TArray<FVeyraAssignmentParticipantDocument> Participants;
};

/** A match server's assignment, read and checked. */
struct FVeyraServerAssignment
{
	FVeyraMatchAssignment Match;
	/** The backend's base URL as this server reaches it. */
	FString BackendUrl;
	/** The match-server credential ("vms_"). Kept in memory only and never logged. */
	FString ServerCredential;
};

namespace VeyraServerAssignment
{
	/** The only assignment version this build reads (AssignmentSchemaVersion in Backend/internal/match). */
	constexpr int32 SchemaVersion = 2;

	/** Where the assignment's schema is, in the project folder or the packaged build. */
	VEYRASERVICES_API FString SchemaPath();

	/** Reads the assignment's schema. Returns every problem; empty on success. */
	VEYRASERVICES_API TArray<FString> ReadSchema(FString& OutSchemaText);

	/**
	 * Validates one assignment line against the schema and converts it. Returns every problem, with
	 * any credential in them redacted; Out is written only when there are none.
	 */
	VEYRASERVICES_API TArray<FString> Parse(FStringView Line, FStringView SchemaText, FVeyraServerAssignment& Out);
}
