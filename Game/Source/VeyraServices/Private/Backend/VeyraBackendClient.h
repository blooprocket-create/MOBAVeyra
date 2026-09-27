// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraBackendTransport.h"

/**
 * Sends JSON requests to the backend over HTTP (ADR-005, ADR-007). A credential goes only in the
 * Authorization header, and nothing here logs a request or a response. Callbacks run on the game
 * thread.
 */
class FVeyraBackendClient final : public IVeyraBackendTransport
{
public:
	FVeyraBackendClient(FString InBaseUrl, float InTimeoutSeconds);

	virtual void Get(const FString& Path, const FString& Credential, FVeyraBackendCallback OnDone) override;

	virtual void Post(const FString& Path, const FString& Credential, const FString& Body, FVeyraBackendCallback OnDone) override;

	virtual void Put(const FString& Path, const FString& Credential, const FString& Body, FVeyraBackendCallback OnDone) override;

	const FString& GetBaseUrl() const { return BaseUrl; }

private:
	void Send(const TCHAR* Verb, const FString& Path, const FString& Credential, const FString* Body, FVeyraBackendCallback OnDone) const;

	FString BaseUrl;
	float TimeoutSeconds = 0.0f;
};
