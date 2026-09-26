// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Hash/Blake3.h"

class UScriptStruct;

/**
 * The tuning-data framework (ADR-006 §6). Each owning domain keeps its gameplay tuning in
 * Game/Tuning/<Domain>.json, validated against Game/Tuning/Schemas/<Domain>.schema.json and bound
 * into a reflected struct. Validation fails explicitly: there are no silent defaults
 * (ARCHITECTURE.md §1.3). The rules of the schema dialect live in Game/Tuning/README.md.
 */
namespace VeyraTuning
{
	/** Every problem found, each prefixed with a JSON pointer or a file path. Empty means success. */
	using FErrors = TArray<FString>;

	/** A domain's files as read from disk, before validation. */
	struct FDomainFiles
	{
		FString DocumentText;
		FString SchemaText;

		/** BLAKE3 of the document's exact bytes, for comparing tuning between builds. */
		FBlake3Hash DocumentHash;
	};

	/**
	 * Validates a tuning document against its schema and binds it to a struct. OutStruct is written
	 * only when there are no errors. The document's root "schemaVersion" must equal
	 * ExpectedSchemaVersion and is not bound.
	 */
	VEYRACORE_API FErrors ValidateAndBind(FStringView DocumentText, FStringView SchemaText, int32 ExpectedSchemaVersion,
		const UScriptStruct& Struct, void* OutStruct);

	template <typename StructType>
	FErrors ValidateAndBind(FStringView DocumentText, FStringView SchemaText, int32 ExpectedSchemaVersion, StructType& OutStruct)
	{
		return ValidateAndBind(DocumentText, SchemaText, ExpectedSchemaVersion, *StructType::StaticStruct(), &OutStruct);
	}

	/** Decodes a tuning file's UTF-8 bytes. The text is not checked here; ValidateAndBind does that. */
	VEYRACORE_API FString DecodeUtf8(TConstArrayView<uint8> Bytes);

	/** Reads a domain's document and schema from Game/Tuning and hashes the document. */
	VEYRACORE_API FErrors ReadDomainFiles(FStringView Domain, FDomainFiles& OutFiles);

	/** Reads, validates and binds a domain's tuning in one call. */
	template <typename StructType>
	FErrors LoadDomain(FStringView Domain, int32 ExpectedSchemaVersion, StructType& OutStruct, FBlake3Hash& OutDocumentHash)
	{
		FDomainFiles Files;
		FErrors Errors = ReadDomainFiles(Domain, Files);
		if (Errors.IsEmpty())
		{
			Errors = ValidateAndBind(Files.DocumentText, Files.SchemaText, ExpectedSchemaVersion, OutStruct);
		}
		if (Errors.IsEmpty())
		{
			OutDocumentHash = Files.DocumentHash;
		}
		return Errors;
	}

	/**
	 * Logs every error for a domain that failed to load. Outside the editor this is fatal: a build
	 * must never run with missing or invalid tuning.
	 */
	VEYRACORE_API void ReportLoadFailure(FStringView Domain, const FErrors& Errors);
}
