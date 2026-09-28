// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Templates/Function.h"

/** What the backend answered, or that no answer came. */
struct VEYRASERVICES_API FVeyraBackendResponse
{
	/** False when no answer came: no connection, or the request timed out. */
	bool bAnswered = false;
	int32 Status = 0;
	FString Body;

	bool IsSuccess() const;

	/** Trying again could succeed: no answer came, or the backend had a server error. */
	bool IsTransient() const;

	/** The backend refused the credential: HTTP 401. */
	bool IsUnauthorized() const;

	/** The status and the backend's error code, such as "HTTP 401 (invalid_credentials)", for logs. Never the body. */
	FString Describe() const;
};

using FVeyraBackendCallback = TFunction<void(const FVeyraBackendResponse&)>;

/**
 * How the game reaches the backend (ADR-005, ADR-007). The HTTP client implements it; tests answer
 * requests by hand. A path starts after the base URL, such as "/v1/me/match", and a credential goes
 * only in the Authorization header. Callbacks run on the game thread, never before the call returns.
 */
class IVeyraBackendTransport
{
public:
	virtual ~IVeyraBackendTransport() = default;

	virtual void Get(const FString& Path, const FString& Credential, FVeyraBackendCallback OnDone) = 0;

	virtual void Post(const FString& Path, const FString& Credential, const FString& Body, FVeyraBackendCallback OnDone) = 0;

	virtual void Put(const FString& Path, const FString& Credential, const FString& Body, FVeyraBackendCallback OnDone) = 0;

	virtual void Delete(const FString& Path, const FString& Credential, FVeyraBackendCallback OnDone) = 0;
};
