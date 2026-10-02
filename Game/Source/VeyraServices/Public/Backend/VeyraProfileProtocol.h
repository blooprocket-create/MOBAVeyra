// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"
#include "Misc/Optional.h"

/**
 * Player profiles on the wire (ADR-048): a public profile, the player's own choices and the official catalog,
 * and the paths of a profile's shared Match History. Profiles never carry an account ID.
 */
namespace VeyraBackendProtocol
{
	/** A profile's featured Vanguard with its owner's Mastery Level. */
	struct FProfileFeatured
	{
		FString VanguardId;
		int32 MasteryLevel = 1;
	};

	/** A player's public profile, as GET /v1/profiles/{name} reports it (Profiles Bible §1). */
	struct FPublicProfile
	{
		FString Name;
		FString Icon;
		FString Background;
		int32 Level = 1;
		/** Unset when the owner features no Vanguard. */
		TOptional<FProfileFeatured> Featured;
		bool bSharesMatchHistory = false;
	};

	/** What the player chose for their profile. */
	struct FProfileSettings
	{
		FString Icon;
		FString Background;
		/** Empty for none. */
		FString FeaturedVanguardId;
		bool bShowMatchHistory = false;

		bool operator==(const FProfileSettings&) const = default;
	};

	/** The official icons and backgrounds every account may choose (ADR-048 §2). */
	struct FProfileCatalog
	{
		TArray<FString> Icons;
		TArray<FString> Backgrounds;
		FString DefaultIcon;
		FString DefaultBackground;
		/** The Vanguards the player permanently owns: the featured Vanguard's choices (Profiles Bible §2). */
		TArray<FString> FeaturedChoices;
	};

	/** The player's display name and what changing it takes, as GET and PUT /v1/me/display-name report it (ADR-049). */
	struct FDisplayNameStatus
	{
		FString Name;
		bool bFreeChangeAvailable = false;
		/** When a voluntary change is next allowed; unset when it is now. */
		TOptional<FDateTime> NextChangeAt;
		bool bRenameRequired = false;
		int64 PriceFlux = 0;
		int64 PriceRefinedFlux = 0;
	};

	/** Reads the answer to GET or PUT /v1/me/display-name. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParseDisplayNameStatus(const FString& Body, FDisplayNameStatus& Out, FString& OutProblem);

	/** The body of PUT /v1/me/display-name; Currency is "flux" or "refinedFlux", or empty for a free change. */
	VEYRASERVICES_API FString BuildDisplayNameBody(const FString& Name, const FString& Currency);

	/** GET /v1/profiles/{name}. */
	VEYRASERVICES_API FString ProfilePath(const FString& Name);

	/** GET /v1/profiles/{name}/matches, from Cursor (empty for the first page). */
	VEYRASERVICES_API FString ProfileMatchesPath(const FString& Name, const FString& Cursor);

	/** GET /v1/profiles/{name}/matches/{matchId}. */
	VEYRASERVICES_API FString ProfileMatchPath(const FString& Name, const FString& MatchId);

	/** Reads the answer to GET /v1/profiles/{name}. False, with the problem, if it is not one. */
	VEYRASERVICES_API bool ParsePublicProfile(const FString& Body, FPublicProfile& Out, FString& OutProblem);

	/** Reads the answer to GET or PUT /v1/me/profile-settings: the player's choices and the catalog. */
	VEYRASERVICES_API bool ParseProfileSettings(const FString& Body, FProfileSettings& OutSettings, FProfileCatalog& OutCatalog, FString& OutProblem);

	/** The body of PUT /v1/me/profile-settings. */
	VEYRASERVICES_API FString BuildProfileSettingsBody(const FProfileSettings& Settings);
}
