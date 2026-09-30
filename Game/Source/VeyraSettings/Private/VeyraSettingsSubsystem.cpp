// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "VeyraSettingsSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tuning/VeyraTuning.h"
#include "VeyraSettingsLog.h"
#include "VeyraUserSettings.h"

namespace
{
	const FVeyraSettingsRegistry* TestRegistry = nullptr;
	FString TestCacheDirectory;
	TWeakObjectPtr<UVeyraUserSettings> TestDevice;

	/** Where device settings live: a test's object, or the engine's user settings. */
	UVeyraUserSettings* DeviceSettings()
	{
		return TestDevice.IsValid() ? TestDevice.Get() : UVeyraUserSettings::Get();
	}

	/** An account ID as a file name: the backend's IDs are UUIDs; anything else keeps no cache. */
	bool IsSafeFileName(const FString& Id)
	{
		if (Id.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Id)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('-'))
			{
				return false;
			}
		}
		return true;
	}

	bool ReadText(const FString& Path, FString& OutText)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
		{
			return false;
		}
		OutText = VeyraTuning::DecodeUtf8(Bytes);
		return true;
	}
}

bool UVeyraSettingsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Start();
}

void UVeyraSettingsSubsystem::Start()
{
	TArray<FString> Problems;
	if (TestRegistry)
	{
		Registry = *TestRegistry;
		Problems = VeyraSettings::Validate(Registry);
	}
	else
	{
		Problems = LoadRegistry(Registry);
	}
	if (!Problems.IsEmpty())
	{
		// A build must never offer settings it cannot describe; outside the editor this is fatal.
		VeyraTuning::ReportLoadFailure(TEXT("Settings"), Problems);
		return;
	}
	Store = MakeUnique<FVeyraSettingsStore>(Registry);
	if (const UVeyraUserSettings* Device = DeviceSettings())
	{
		TGuardValue<bool> Loading(bLoading, true);
		if (const int32 Dropped = Store->LoadScope(EVeyraSettingScope::Device, Device->DeviceSettings))
		{
			UE_LOG(LogVeyraSettings, Log, TEXT("Dropped %d saved device setting(s) this build does not offer."), Dropped);
		}
	}
	ChangedHandle = Store->OnChanged.AddUObject(this, &UVeyraSettingsSubsystem::OnStoreChanged);
}

void UVeyraSettingsSubsystem::Deinitialize()
{
	if (Store)
	{
		Store->OnChanged.Remove(ChangedHandle);
	}
	Store.Reset();
	Super::Deinitialize();
}

UVeyraSettingsSubsystem* UVeyraSettingsSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UVeyraSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UVeyraSettingsSubsystem>() : nullptr;
	return Settings && Settings->IsReady() ? Settings : nullptr;
}

void UVeyraSettingsSubsystem::UseAccount(const FString& InAccountId)
{
	if (!Store || InAccountId == AccountId)
	{
		return;
	}
	AccountId = InAccountId;
	FVeyraAccountSettingsDocument Cached;
	FString Text;
	FString Problem;
	if (!CachePath().IsEmpty() && ReadText(CachePath(), Text) && !VeyraSettingsDocument::Read(Text, Cached, Problem))
	{
		UE_LOG(LogVeyraSettings, Warning, TEXT("Ignored this machine's cached settings for account %s: %s."), *AccountId, *Problem);
		Cached = FVeyraAccountSettingsDocument();
	}
	TGuardValue<bool> Loading(bLoading, true);
	Store->LoadScope(EVeyraSettingScope::Account, Cached.Values);
	AccountRevision = Cached.Revision;
	bAccountUnsent = Cached.bUnsent;
}

FVeyraAccountSettingsDocument UVeyraSettingsSubsystem::GetAccountDocument() const
{
	FVeyraAccountSettingsDocument Document;
	Document.Revision = AccountRevision;
	Document.bUnsent = bAccountUnsent;
	if (Store)
	{
		Document.Values = Store->SaveScope(EVeyraSettingScope::Account);
	}
	return Document;
}

void UVeyraSettingsSubsystem::TakeAccountDocument(const FVeyraAccountSettingsDocument& Document)
{
	if (!Store)
	{
		return;
	}
	{
		TGuardValue<bool> Loading(bLoading, true);
		Store->LoadScope(EVeyraSettingScope::Account, Document.Values);
	}
	AccountRevision = Document.Revision;
	bAccountUnsent = false;
	SaveAccountCache();
}

void UVeyraSettingsSubsystem::MarkAccountSent(int64 Revision, uint32 SentChangeCount)
{
	AccountRevision = Revision;
	bAccountUnsent = SentChangeCount != AccountChangeCount;
	SaveAccountCache();
}

TArray<FString> UVeyraSettingsSubsystem::LoadRegistry(FVeyraSettingsRegistry& OutRegistry)
{
	const FString Directory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Settings"));
	FString Document;
	FString Schema;
	if (!ReadText(FPaths::Combine(Directory, TEXT("Settings.json")), Document) || !ReadText(FPaths::Combine(Directory, TEXT("Settings.schema.json")), Schema))
	{
		return { FString::Printf(TEXT("%s: Settings.json or its schema is missing"), *Directory) };
	}
	FVeyraSettingsRegistry Loaded;
	TArray<FString> Problems = VeyraTuning::ValidateAndBind(Document, Schema, FVeyraSettingsRegistry::SchemaVersion, Loaded);
	if (Problems.IsEmpty())
	{
		Problems = VeyraSettings::Validate(Loaded);
	}
	if (Problems.IsEmpty())
	{
		OutRegistry = MoveTemp(Loaded);
	}
	return Problems;
}

void UVeyraSettingsSubsystem::SetTestRegistry(const FVeyraSettingsRegistry* InRegistry)
{
	TestRegistry = InRegistry;
}

void UVeyraSettingsSubsystem::SetTestCacheDirectory(const FString& Directory)
{
	TestCacheDirectory = Directory;
}

void UVeyraSettingsSubsystem::SetTestDeviceSettings(UVeyraUserSettings* Device)
{
	TestDevice = Device;
}

void UVeyraSettingsSubsystem::OnStoreChanged(const FVeyraContentId& Id)
{
	const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(Registry, Id);
	if (bLoading || !Setting.IsSet())
	{
		return;
	}
	if (Setting->Scope == EVeyraSettingScope::Device)
	{
		if (UVeyraUserSettings* Device = DeviceSettings())
		{
			Device->DeviceSettings = Store->SaveScope(EVeyraSettingScope::Device);
			if (!TestDevice.IsValid())
			{
				Device->SaveSettings();
			}
		}
		return;
	}
	bAccountUnsent = true;
	++AccountChangeCount;
	SaveAccountCache();
	OnAccountChanged.Broadcast();
}

void UVeyraSettingsSubsystem::SaveAccountCache() const
{
	const FString Path = CachePath();
	if (Path.IsEmpty() || !Store)
	{
		return;
	}
	if (!FFileHelper::SaveStringToFile(VeyraSettingsDocument::Write(GetAccountDocument(), /*bForCache*/ true), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogVeyraSettings, Warning, TEXT("Could not save this machine's settings for account %s to %s."), *AccountId, *Path);
	}
}

FString UVeyraSettingsSubsystem::CachePath() const
{
	if (!IsSafeFileName(AccountId))
	{
		return FString();
	}
	const FString Directory = TestCacheDirectory.IsEmpty() ? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VeyraSettings")) : TestCacheDirectory;
	return FPaths::Combine(Directory, AccountId + TEXT(".json"));
}
