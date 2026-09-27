// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Backend/VeyraBackendClient.h"

#include "Backend/VeyraBackendProtocol.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

bool FVeyraBackendResponse::IsSuccess() const
{
	return bAnswered && EHttpResponseCodes::IsOk(Status);
}

bool FVeyraBackendResponse::IsTransient() const
{
	return !bAnswered || Status >= EHttpResponseCodes::ServerError;
}

FString FVeyraBackendResponse::Describe() const
{
	if (!bAnswered)
	{
		return TEXT("no answer");
	}
	const FString Code = VeyraBackendProtocol::ParseErrorCode(Body);
	return Code.IsEmpty() ? FString::Printf(TEXT("HTTP %d"), Status) : FString::Printf(TEXT("HTTP %d (%s)"), Status, *Code);
}

FVeyraBackendClient::FVeyraBackendClient(FString InBaseUrl, float InTimeoutSeconds)
	: BaseUrl(MoveTemp(InBaseUrl))
	, TimeoutSeconds(InTimeoutSeconds)
{
}

void FVeyraBackendClient::Get(const FString& Path, const FString& Credential, FVeyraBackendCallback OnDone) const
{
	Send(TEXT("GET"), Path, Credential, nullptr, MoveTemp(OnDone));
}

void FVeyraBackendClient::Post(const FString& Path, const FString& Credential, const FString& Body, FVeyraBackendCallback OnDone) const
{
	Send(TEXT("POST"), Path, Credential, &Body, MoveTemp(OnDone));
}

void FVeyraBackendClient::Send(const TCHAR* Verb, const FString& Path, const FString& Credential, const FString* Body, FVeyraBackendCallback OnDone) const
{
	const FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(Verb);
	Request->SetURL(BaseUrl + Path);
	Request->SetTimeout(TimeoutSeconds);
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	if (!Credential.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Credential);
	}
	if (Body)
	{
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetContentAsString(*Body);
	}
	Request->OnProcessRequestComplete().BindLambda(
		[OnDone = MoveTemp(OnDone)](FHttpRequestPtr /*Request*/, FHttpResponsePtr Response, bool bConnected) {
			FVeyraBackendResponse Result;
			if (bConnected && Response.IsValid())
			{
				Result.bAnswered = true;
				Result.Status = Response->GetResponseCode();
				Result.Body = Response->GetContentAsString();
			}
			OnDone(Result);
		});
	Request->ProcessRequest();
}
