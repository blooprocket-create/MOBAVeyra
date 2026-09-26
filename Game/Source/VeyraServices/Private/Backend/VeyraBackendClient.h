// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Templates/Function.h"

/** What the backend answered, or that no answer came. */
struct FVeyraBackendResponse
{
	/** False when no answer came: no connection, or the request timed out. */
	bool bAnswered = false;
	int32 Status = 0;
	FString Body;

	bool IsSuccess() const;

	/** Trying again could succeed: no answer came, or the backend had a server error. */
	bool IsTransient() const;

	/** The status and the backend's error code, such as "HTTP 401 (invalid_credentials)", for logs. Never the body. */
	FString Describe() const;
};

using FVeyraBackendCallback = TFunction<void(const FVeyraBackendResponse&)>;

/**
 * Sends JSON requests to the backend (ADR-005, ADR-007). A credential goes only in the
 * Authorization header, and nothing here logs a request or a response. Callbacks run on the game
 * thread.
 */
class FVeyraBackendClient
{
public:
	FVeyraBackendClient(FString InBaseUrl, float InTimeoutSeconds);

	void Get(const FString& Path, const FString& Credential, FVeyraBackendCallback OnDone) const;

	void Post(const FString& Path, const FString& Credential, const FString& Body, FVeyraBackendCallback OnDone) const;

	const FString& GetBaseUrl() const { return BaseUrl; }

private:
	void Send(const TCHAR* Verb, const FString& Path, const FString& Credential, const FString* Body, FVeyraBackendCallback OnDone) const;

	FString BaseUrl;
	float TimeoutSeconds = 0.0f;
};
