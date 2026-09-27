// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraServicesSettings.h"

#include "Backend/VeyraBackendProtocol.h"

TArray<FString> UVeyraServicesSettings::Validate() const
{
	TArray<FString> Problems;
	if (!VeyraBackendProtocol::IsBaseUrl(BackendBaseUrl))
	{
		Problems.Add(TEXT("BackendBaseUrl must be an http or https URL with no path, such as http://127.0.0.1:8080"));
	}
	const TPair<const TCHAR*, float> Durations[] = {
		{ TEXT("RequestTimeoutSeconds"), RequestTimeoutSeconds },
		{ TEXT("LaunchCodeReadTimeoutSeconds"), LaunchCodeReadTimeoutSeconds },
		{ TEXT("MatchPollIntervalSeconds"), MatchPollIntervalSeconds },
		{ TEXT("MatchWaitTimeoutSeconds"), MatchWaitTimeoutSeconds },
		{ TEXT("AssignmentReadTimeoutSeconds"), AssignmentReadTimeoutSeconds },
		{ TEXT("AssignmentPollIntervalSeconds"), AssignmentPollIntervalSeconds },
		{ TEXT("ReportRetryIntervalSeconds"), ReportRetryIntervalSeconds },
	};
	for (const TPair<const TCHAR*, float>& Duration : Durations)
	{
		if (!(Duration.Value > 0.0f))
		{
			Problems.Add(FString::Printf(TEXT("%s must be positive"), Duration.Key));
		}
	}
	if (ReportAttempts < 1)
	{
		Problems.Add(TEXT("ReportAttempts must be at least 1"));
	}
	return Problems;
}
