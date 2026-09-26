// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "VeyraServicesSettings.generated.h"

/**
 * Where the backend is and how long to wait for it (ADR-005: endpoints, timeouts and retry
 * policies are validated configuration, never literals). This is deployment configuration, not
 * gameplay tuning: it lives in Config/DefaultGame.ini, can differ per environment, and is not part
 * of the tuning hash clients and servers compare. The zero defaults are invalid on purpose, so a
 * missing value is caught by Validate rather than used.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Veyra Services"))
class VEYRASERVICES_API UVeyraServicesSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/**
	 * Client: the backend's base URL, such as http://127.0.0.1:8080. A match server uses the URL in
	 * its assignment instead, since the backend knows how its servers reach it.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Backend")
	FString BackendBaseUrl;

	/** Seconds before an HTTP request to the backend fails. */
	UPROPERTY(Config, EditAnywhere, Category = "Backend")
	float RequestTimeoutSeconds = 0.0f;

	/** Client: seconds to wait for the launch code on standard input. */
	UPROPERTY(Config, EditAnywhere, Category = "Client")
	float LaunchCodeReadTimeoutSeconds = 0.0f;

	/** Client: seconds between asks for the player's match while it is not ready. */
	UPROPERTY(Config, EditAnywhere, Category = "Client")
	float MatchPollIntervalSeconds = 0.0f;

	/** Client: seconds to wait for the player's match to be ready before giving up. */
	UPROPERTY(Config, EditAnywhere, Category = "Client")
	float MatchWaitTimeoutSeconds = 0.0f;

	/**
	 * Match server: seconds to wait for its assignment on standard input. The server waits before
	 * its first map loads, because the map's game mode reads the roster.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Match server")
	float AssignmentReadTimeoutSeconds = 0.0f;

	/** Match server: seconds between checks of standard input while it waits for the assignment. */
	UPROPERTY(Config, EditAnywhere, Category = "Match server")
	float AssignmentPollIntervalSeconds = 0.0f;

	/** Match server: attempts at each report (ready, result) before giving up. */
	UPROPERTY(Config, EditAnywhere, Category = "Match server")
	int32 ReportAttempts = 0;

	/** Match server: seconds between attempts at a report. */
	UPROPERTY(Config, EditAnywhere, Category = "Match server")
	float ReportRetryIntervalSeconds = 0.0f;

	/** Every problem with these settings; empty when they are usable. */
	TArray<FString> Validate() const;
};
