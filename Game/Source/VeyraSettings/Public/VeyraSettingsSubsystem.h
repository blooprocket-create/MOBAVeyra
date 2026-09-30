// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "VeyraSettingsDocument.h"
#include "VeyraSettingsRegistry.h"
#include "VeyraSettingsStore.h"
#include "VeyraSettingsSubsystem.generated.h"

class UVeyraUserSettings;

/**
 * The player's settings on this client (ADR-024): the registry, one store over it, and where each
 * scope is kept. Device settings live in UVeyraUserSettings; account settings in a cache per account
 * that the services sync with the backend. The systems that apply a setting read the store and listen
 * to its change event. Never on a dedicated server: settings are presentation.
 */
UCLASS()
class VEYRASETTINGS_API UVeyraSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Loads the registry and this machine's device settings. Initialize calls it; a test calls it on a subsystem it made. */
	void Start();

	/** The settings of WorldContext's game instance; null where there are none (a server, a commandlet). */
	static UVeyraSettingsSubsystem* Get(const UObject* WorldContext);

	/** Whether the registry loaded, and the store exists. */
	bool IsReady() const { return Store.IsValid(); }
	FVeyraSettingsStore& GetStore() { return *Store; }
	const FVeyraSettingsStore& GetStore() const { return *Store; }

	/**
	 * Takes AccountId's cached account settings as its player signs in (ADR-024 §1), or the defaults
	 * when this machine has none for it. An empty ID returns the account settings to their defaults.
	 */
	void UseAccount(const FString& AccountId);
	const FString& GetAccountId() const { return AccountId; }

	/** The account settings as the document the backend keeps. */
	FVeyraAccountSettingsDocument GetAccountDocument() const;

	/** Takes the backend's document in place of the account settings here, with nothing left to send. */
	void TakeAccountDocument(const FVeyraAccountSettingsDocument& Document);

	/**
	 * The backend took the account settings as they were at SentChangeCount, as Revision. Changes the
	 * player made while they were on their way stay unsent, based on Revision.
	 */
	void MarkAccountSent(int64 Revision, uint32 SentChangeCount);

	bool HasUnsentAccountChanges() const { return bAccountUnsent; }

	/** Counts the player's changes to account settings, so a sender knows whether more came while it waited. */
	uint32 GetAccountChangeCount() const { return AccountChangeCount; }

	/** The player changed an account setting here; it waits to be sent. */
	FSimpleMulticastDelegate OnAccountChanged;

	/** Loads and validates the registry from Game/Settings (ADR-024 §2). Empty when it loaded. */
	static TArray<FString> LoadRegistry(FVeyraSettingsRegistry& OutRegistry);

	/** Tests: subsystems created after this use Registry; null restores the committed one. */
	static void SetTestRegistry(const FVeyraSettingsRegistry* InRegistry);

	/** Tests: where the account cache lives instead of Saved/VeyraSettings; empty restores it. */
	static void SetTestCacheDirectory(const FString& Directory);

	/** Tests: keep device settings in Device, and never save them to disk; null restores the engine's. */
	static void SetTestDeviceSettings(UVeyraUserSettings* Device);

private:
	void OnStoreChanged(const FVeyraContentId& Id);
	void SaveAccountCache() const;
	FString CachePath() const;

	FVeyraSettingsRegistry Registry;
	TUniquePtr<FVeyraSettingsStore> Store;
	FDelegateHandle ChangedHandle;

	FString AccountId;
	int64 AccountRevision = 0;
	bool bAccountUnsent = false;
	uint32 AccountChangeCount = 0;

	/** Set while values come from a file or the backend: they are not the player's new changes. */
	bool bLoading = false;
};
