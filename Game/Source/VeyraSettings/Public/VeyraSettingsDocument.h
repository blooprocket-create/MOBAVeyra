// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"

/**
 * A player's account-level settings as one document (ADR-024 §1): what the backend keeps, and what
 * the client caches per account. Values are text by setting ID, as FVeyraSettingsStore saves them.
 */
struct FVeyraAccountSettingsDocument
{
	/** The backend's revision this document is, or was based on; 0 before the backend has one. */
	int64 Revision = 0;
	TMap<FString, FString> Values;
	/** The client cache only: changes made since the backend last took the document. */
	bool bUnsent = false;
};

/** The document's JSON: {"schemaVersion": 1, "revision": N, "values": {id: value}}, and "unsent" in the cache. */
namespace VeyraSettingsDocument
{
	/** The version of the document's format; a document of any other version is refused. */
	inline constexpr int32 SchemaVersion = 1;

	VEYRASETTINGS_API FString Write(const FVeyraAccountSettingsDocument& Document, bool bForCache);

	/** Reads Json into Out; false, with a problem, when it is not a document of this version. */
	VEYRASETTINGS_API bool Read(FStringView Json, FVeyraAccountSettingsDocument& Out, FString& OutProblem);
}
