// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Settings/VeyraAccountSettingsSync.h"

#include "Backend/VeyraBackendProtocol.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "VeyraServicesLog.h"

namespace
{
	const TCHAR* const SettingsPath = TEXT("/v1/account/settings");
	const TCHAR* const ConflictCode = TEXT("settings_conflict");

	/** The account's document in a refusal of a stale send: {"error": "settings_conflict", "current": {document}}. */
	bool ReadConflict(const FString& Body, FVeyraAccountSettingsDocument& Out, FString& OutProblem)
	{
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Body), Root) || !Root.IsValid())
		{
			OutProblem = TEXT("the answer is not a JSON object");
			return false;
		}
		const TSharedPtr<FJsonObject>* Current = nullptr;
		if (!Root->TryGetObjectField(TEXT("current"), Current) || !Current || !Current->IsValid())
		{
			OutProblem = TEXT("the answer has no current document");
			return false;
		}
		FString Text;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
		FJsonSerializer::Serialize(Current->ToSharedRef(), Writer);
		return VeyraSettingsDocument::Read(Text, Out, OutProblem);
	}
}

FVeyraAccountSettingsSync::FVeyraAccountSettingsSync(IVeyraBackendTransport& InBackend, IVeyraAccountSettingsCache& InCache, FVeyraAccountSettingsSyncConfig InConfig,
	FCallbacks InCallbacks)
	: Backend(InBackend)
	, Cache(InCache)
	, Config(InConfig)
	, Callbacks(MoveTemp(InCallbacks))
	, Alive(MakeShared<bool>(true))
{
}

FVeyraAccountSettingsSync::~FVeyraAccountSettingsSync() = default;

void FVeyraAccountSettingsSync::SignIn(const FString& AccountId, const FString& GameSession, double Now)
{
	SignOut();
	Session = GameSession;
	LastNow = Now;
	Cache.UseAccount(AccountId);
	SeenChangeCount = Cache.GetChangeCount();
	SendAt = Now;
	bReadyPending = true;
	bReading = true;
	Backend.Get(SettingsPath, Session, [this, WeakAlive = TWeakPtr<bool>(Alive), Epoch = SessionEpoch](const FVeyraBackendResponse& Response) {
		if (WeakAlive.IsValid() && Epoch == SessionEpoch)
		{
			bReading = false;
			OnRead(Response);
		}
	});
}

void FVeyraAccountSettingsSync::SignOut()
{
	++SessionEpoch;
	Session.Reset();
	bReading = false;
	bSending = false;
	bReadyPending = false;
	Conflict.Reset();
	RefusedChangeCount.Reset();
}

void FVeyraAccountSettingsSync::OnRead(const FVeyraBackendResponse& Response)
{
	if (Response.IsUnauthorized())
	{
		Callbacks.OnSessionRefused();
		return;
	}
	FVeyraAccountSettingsDocument Account;
	FString Problem;
	if (!Response.IsSuccess() || !VeyraSettingsDocument::Read(Response.Body, Account, Problem))
	{
		UE_LOG(LogVeyraServices, Warning, TEXT("VeyraAccountSettings: kept this device's copy of the account settings; the backend's could not be read: %s."),
			Response.IsSuccess() ? *Problem : *Response.Describe());
		Ready();
		return;
	}
	const FVeyraAccountSettingsDocument Here = Cache.GetDocument();
	if (!Cache.HasUnsentChanges() || Here.Values.OrderIndependentCompareEqual(Account.Values))
	{
		Cache.TakeDocument(Account);
		UE_LOG(LogVeyraServices, Log, TEXT("VeyraAccountSettings: took the account settings, revision %lld."), Account.Revision);
		Ready();
		return;
	}
	if (Account.Revision == Here.Revision)
	{
		// Changes made here while the backend could not be reached, and nothing saved elsewhere since.
		Send(Here.Revision);
		Ready();
		return;
	}
	RaiseConflict(MoveTemp(Account));
}

void FVeyraAccountSettingsSync::Tick(double Now)
{
	LastNow = Now;
	if (Session.IsEmpty() || bReading || bSending || Conflict.IsSet())
	{
		return;
	}
	const uint32 Changes = Cache.GetChangeCount();
	if (Changes != SeenChangeCount)
	{
		SeenChangeCount = Changes;
		SendAt = Now + Config.SendDelaySeconds;
		return;
	}
	if (Now < SendAt || !Cache.HasUnsentChanges() || (RefusedChangeCount.IsSet() && *RefusedChangeCount == Changes))
	{
		return;
	}
	Send(Cache.GetDocument().Revision);
}

void FVeyraAccountSettingsSync::Send(int64 Base)
{
	FVeyraAccountSettingsDocument Document = Cache.GetDocument();
	Document.Revision = Base;
	const uint32 Changes = Cache.GetChangeCount();
	bSending = true;
	Backend.Put(SettingsPath, Session, VeyraSettingsDocument::Write(Document, /*bForCache*/ false),
		[this, WeakAlive = TWeakPtr<bool>(Alive), Epoch = SessionEpoch, Changes](const FVeyraBackendResponse& Response) {
			if (WeakAlive.IsValid() && Epoch == SessionEpoch)
			{
				bSending = false;
				OnSent(Response, Changes);
			}
		});
}

void FVeyraAccountSettingsSync::OnSent(const FVeyraBackendResponse& Response, uint32 SentChangeCount)
{
	if (Response.IsUnauthorized())
	{
		Callbacks.OnSessionRefused();
		return;
	}
	FVeyraAccountSettingsDocument Account;
	FString Problem;
	if (Response.IsSuccess() && VeyraSettingsDocument::Read(Response.Body, Account, Problem))
	{
		Cache.MarkSent(Account.Revision, SentChangeCount);
		UE_LOG(LogVeyraServices, Log, TEXT("VeyraAccountSettings: the backend saved the account settings as revision %lld."), Account.Revision);
		return;
	}
	if (Response.bAnswered && VeyraBackendProtocol::ParseErrorCode(Response.Body) == ConflictCode)
	{
		if (!ReadConflict(Response.Body, Account, Problem))
		{
			UE_LOG(LogVeyraServices, Warning, TEXT("VeyraAccountSettings: the backend's settings conflict was not understood: %s."), *Problem);
			SendAt = LastNow + Config.RetrySeconds;
			return;
		}
		if (Cache.GetDocument().Values.OrderIndependentCompareEqual(Account.Values))
		{
			Cache.MarkSent(Account.Revision, SentChangeCount);
			return;
		}
		RaiseConflict(MoveTemp(Account));
		return;
	}
	if (Response.IsTransient() || Response.IsSuccess())
	{
		// No answer, a server error, or an answer not understood: the changes stay, and go later.
		UE_LOG(LogVeyraServices, Warning, TEXT("VeyraAccountSettings: sending the account settings failed, trying again in %.0f s: %s."), Config.RetrySeconds,
			Response.IsSuccess() ? *Problem : *Response.Describe());
		SendAt = LastNow + Config.RetrySeconds;
		return;
	}
	// Refused as they are: sending them again cannot help, but the player's next change may.
	UE_LOG(LogVeyraServices, Error, TEXT("VeyraAccountSettings: the backend refused the account settings: %s."), *Response.Describe());
	RefusedChangeCount = SentChangeCount;
}

void FVeyraAccountSettingsSync::RaiseConflict(FVeyraAccountSettingsDocument Account)
{
	UE_LOG(LogVeyraServices, Log, TEXT("VeyraAccountSettings: the account settings changed elsewhere (revision %lld) while this device's changed too; the player chooses."),
		Account.Revision);
	Conflict = MoveTemp(Account);
	Callbacks.OnConflictChanged();
}

bool FVeyraAccountSettingsSync::Resolve(bool bKeepThisDevice)
{
	if (!Conflict.IsSet())
	{
		return false;
	}
	const FVeyraAccountSettingsDocument Account = MoveTemp(*Conflict);
	Conflict.Reset();
	if (bKeepThisDevice)
	{
		UE_LOG(LogVeyraServices, Log, TEXT("VeyraAccountSettings: the player kept this device's settings."));
		Send(Account.Revision);
	}
	else
	{
		UE_LOG(LogVeyraServices, Log, TEXT("VeyraAccountSettings: the player took the account's settings, revision %lld."), Account.Revision);
		Cache.TakeDocument(Account);
	}
	SeenChangeCount = Cache.GetChangeCount();
	Callbacks.OnConflictChanged();
	Ready();
	return true;
}

void FVeyraAccountSettingsSync::Ready()
{
	if (bReadyPending)
	{
		bReadyPending = false;
		Callbacks.OnReady();
	}
}
