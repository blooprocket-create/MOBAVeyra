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
		{ TEXT("ClientRetryIntervalSeconds"), ClientRetryIntervalSeconds },
		{ TEXT("SelectPollIntervalSeconds"), SelectPollIntervalSeconds },
		{ TEXT("ResultPollIntervalSeconds"), ResultPollIntervalSeconds },
		{ TEXT("ResultWaitTimeoutSeconds"), ResultWaitTimeoutSeconds },
		{ TEXT("ReconnectPollIntervalSeconds"), ReconnectPollIntervalSeconds },
		{ TEXT("PartyPollIntervalSeconds"), PartyPollIntervalSeconds },
		{ TEXT("MatchFoundPollIntervalSeconds"), MatchFoundPollIntervalSeconds },
		{ TEXT("LobbyPollIntervalSeconds"), LobbyPollIntervalSeconds },
		{ TEXT("SocialPollIntervalSeconds"), SocialPollIntervalSeconds },
		{ TEXT("ChatPollIntervalSeconds"), ChatPollIntervalSeconds },
		{ TEXT("PlayStreakGapSeconds"), PlayStreakGapSeconds },
		{ TEXT("AccountSettingsSendDelaySeconds"), AccountSettingsSendDelaySeconds },
		{ TEXT("AccountSettingsRetrySeconds"), AccountSettingsRetrySeconds },
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
	const TPair<const TCHAR*, int32> Attempts[] = {
		{ TEXT("ClientRequestAttempts"), ClientRequestAttempts },
		{ TEXT("ReportAttempts"), ReportAttempts },
		{ TEXT("ChatKeepMessages"), ChatKeepMessages },
	};
	for (const TPair<const TCHAR*, int32>& Attempt : Attempts)
	{
		if (Attempt.Value < 1)
		{
			Problems.Add(FString::Printf(TEXT("%s must be at least 1"), Attempt.Key));
		}
	}
	return Problems;
}
