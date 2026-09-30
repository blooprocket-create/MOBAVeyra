// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Backend/VeyraBackendTransport.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "VeyraSettingsDocument.h"

/** Where a signed-in player's account settings are kept on this client: the settings subsystem, or a test's fake. */
class IVeyraAccountSettingsCache
{
public:
	virtual ~IVeyraAccountSettingsCache() = default;

	/** Takes AccountId's cached settings as its player signs in; an empty ID returns them to their defaults. */
	virtual void UseAccount(const FString& AccountId) = 0;

	/** The account settings as the document the backend keeps, based on the revision it last took. */
	virtual FVeyraAccountSettingsDocument GetDocument() const = 0;

	/** Takes the backend's document in place of the account settings here, with nothing left to send. */
	virtual void TakeDocument(const FVeyraAccountSettingsDocument& Document) = 0;

	/** The backend took the settings as they were at SentChangeCount, as Revision; later changes stay unsent. */
	virtual void MarkSent(int64 Revision, uint32 SentChangeCount) = 0;

	/** The player changed account settings the backend has not taken yet. */
	virtual bool HasUnsentChanges() const = 0;

	/** Counts the player's changes to account settings. */
	virtual uint32 GetChangeCount() const = 0;
};

/** When the sync sends. From UVeyraServicesSettings. */
struct FVeyraAccountSettingsSyncConfig
{
	/** How long the player's changes must settle before they are sent, so a slider's drag sends once. */
	double SendDelaySeconds = 0.0;
	/** How long after a send that got no answer the next attempt waits. */
	double RetrySeconds = 0.0;
};

/**
 * Keeps a signed-in player's account settings and the backend's copy the same (ADR-024 §1).
 *
 * On sign-in it reads the backend's document. With nothing unsent here it takes that document; with
 * unsent changes based on the backend's revision it sends them; with unsent changes whose revision the
 * backend moved on from, another machine saved settings meanwhile and the player chooses which to keep
 * ("This device" or "Your account", Settings & Accessibility §7), before the sign-in goes on. Later
 * changes are sent once they settle. A send the backend refuses as stale raises the same choice.
 *
 * Settings never stop the player: a read or send that fails leaves the changes cached and unsent, and
 * a later attempt sends them. It never logs a credential.
 */
class VEYRASERVICES_API FVeyraAccountSettingsSync
{
public:
	struct FCallbacks
	{
		/** The sign-in read is settled, the player's choice included: the sign-in goes on. */
		TFunction<void()> OnReady;
		/** A choice between this device's and the account's settings appeared, or was made. */
		TFunction<void()> OnConflictChanged;
		/** The backend refused the game session. */
		TFunction<void()> OnSessionRefused;
	};

	/** Backend and Cache must outlive the sync. Callbacks that arrive after it is gone are ignored. */
	FVeyraAccountSettingsSync(IVeyraBackendTransport& InBackend, IVeyraAccountSettingsCache& InCache, FVeyraAccountSettingsSyncConfig InConfig, FCallbacks InCallbacks);
	~FVeyraAccountSettingsSync();

	FVeyraAccountSettingsSync(const FVeyraAccountSettingsSync&) = delete;
	FVeyraAccountSettingsSync& operator=(const FVeyraAccountSettingsSync&) = delete;

	/** The player signed in as AccountId with GameSession: their cached settings apply, and the backend's are read. */
	void SignIn(const FString& AccountId, const FString& GameSession, double Now);

	/** The game session ended: nothing more is read or sent, and a pending choice is dropped. */
	void SignOut();

	/** Sends changes that have settled. The owner calls it every frame. */
	void Tick(double Now);

	/** A choice between this device's and the account's settings waits for the player. */
	bool HasConflict() const { return Conflict.IsSet(); }

	/** Keeps this device's settings, sending them over the account's, or takes the account's. False without a choice to make. */
	bool Resolve(bool bKeepThisDevice);

private:
	void OnRead(const FVeyraBackendResponse& Response);
	/** Sends the settings as based on Base. */
	void Send(int64 Base);
	void OnSent(const FVeyraBackendResponse& Response, uint32 SentChangeCount);
	/** The backend's document differs from the unsent one here: the player chooses. */
	void RaiseConflict(FVeyraAccountSettingsDocument Account);
	void Ready();

	IVeyraBackendTransport& Backend;
	IVeyraAccountSettingsCache& Cache;
	FVeyraAccountSettingsSyncConfig Config;
	FCallbacks Callbacks;
	/** Expires with the sync, so late callbacks know to do nothing. */
	TSharedRef<bool> Alive;

	/** The game session credential; empty while signed out. In memory only. */
	FString Session;
	/** Increases on every sign-in and sign-out; an answer from an older session is stale. */
	uint32 SessionEpoch = 0;
	bool bReading = false;
	bool bSending = false;
	/** The sign-in waits for the read, or for the player's choice. */
	bool bReadyPending = false;
	/** The account's document, while the player chooses between it and this device's. */
	TOptional<FVeyraAccountSettingsDocument> Conflict;
	/** The change count last seen, and when the next send may go. */
	uint32 SeenChangeCount = 0;
	double SendAt = 0.0;
	double LastNow = 0.0;
	/** The change count the backend refused as invalid: not sent again until the player changes more. */
	TOptional<uint32> RefusedChangeCount;
};
