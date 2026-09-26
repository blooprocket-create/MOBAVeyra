// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * SHA-256, which the backend uses for every credential hash (ADR-007 §3). A match server checks a
 * joining client's ticket by comparing its hash with the one in its roster. The engine's own
 * FPlatformMisc::GetSHA256Signature has no implementation on Windows or Linux, so this wraps the
 * engine's OpenSSL.
 */
namespace VeyraHash
{
	/** The lowercase hex SHA-256 of Text's UTF-8 bytes. */
	VEYRACORE_API FString Sha256Hex(FStringView Text);
}
