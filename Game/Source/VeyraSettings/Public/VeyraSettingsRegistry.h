// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Content/VeyraContentId.h"
#include "Tuning/VeyraTuningProvenance.h"
#include "InputCoreTypes.h"

#include "VeyraSettingsRegistry.generated.h"

class FStructProperty;
class FVeyraSettingsStore;
class UClass;
class UObject;

/** The Settings screen's categories (Settings & Accessibility Bible §13). */
UENUM()
enum class EVeyraSettingCategory : uint8
{
	Controls,
	Camera,
	Interface,
	Accessibility,
	Audio,
	GraphicsDisplay,
	Communication,
	LanguageAccount,
};

/** Which store keeps a setting (Settings Bible §7; ADR-024 §1). */
UENUM()
enum class EVeyraSettingScope : uint8
{
	/** Local to this machine: display, graphics, frame rate. */
	Device,
	/** Synced to the player's account: controls, camera, interface. */
	Account,
};

/** Whether a setting may change in a live match (Settings Bible §6.2). */
UENUM()
enum class EVeyraSettingAvailability : uint8
{
	Anywhere,
	OutsideMatches,
};

/** When a change to a setting takes effect (Settings Bible §6.3). */
UENUM()
enum class EVeyraSettingApplies : uint8
{
	AtOnce,
	AfterRestart,
};

/** A toggle's value, as the registry spells it: the tuning dialect has no booleans. */
UENUM()
enum class EVeyraToggleValue : uint8
{
	Off,
	On,
};

/** A setting that is On or Off. */
USTRUCT()
struct FVeyraToggleSetting
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	EVeyraSettingCategory Category = EVeyraSettingCategory::Controls;

	UPROPERTY()
	EVeyraSettingScope Scope = EVeyraSettingScope::Account;

	UPROPERTY()
	EVeyraSettingAvailability Availability = EVeyraSettingAvailability::Anywhere;

	UPROPERTY()
	EVeyraSettingApplies Applies = EVeyraSettingApplies::AtOnce;

	UPROPERTY()
	EVeyraToggleValue Default = EVeyraToggleValue::Off;
};

/** A setting on a slider, from Minimum to Maximum in steps of Step. */
USTRUCT()
struct FVeyraRangeSetting
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	EVeyraSettingCategory Category = EVeyraSettingCategory::Controls;

	UPROPERTY()
	EVeyraSettingScope Scope = EVeyraSettingScope::Account;

	UPROPERTY()
	EVeyraSettingAvailability Availability = EVeyraSettingAvailability::Anywhere;

	UPROPERTY()
	EVeyraSettingApplies Applies = EVeyraSettingApplies::AtOnce;

	UPROPERTY()
	double Minimum = 0.0;

	UPROPERTY()
	double Maximum = 0.0;

	UPROPERTY()
	double Step = 0.0;

	UPROPERTY()
	double Default = 0.0;
};

/** A setting that takes one of several options. */
USTRUCT()
struct FVeyraChoiceSetting
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	EVeyraSettingCategory Category = EVeyraSettingCategory::Controls;

	UPROPERTY()
	EVeyraSettingScope Scope = EVeyraSettingScope::Account;

	UPROPERTY()
	EVeyraSettingAvailability Availability = EVeyraSettingAvailability::Anywhere;

	UPROPERTY()
	EVeyraSettingApplies Applies = EVeyraSettingApplies::AtOnce;

	UPROPERTY()
	TArray<FString> Options;

	UPROPERTY()
	FString Default;
};

/**
 * Every player setting (ADR-024 §2), bound from Game/Settings/Settings.json. Presentation data: the
 * server never reads it, and it is outside the tuning hash.
 */
/** Whether an action must have a key: an essential one left without warns (SET-133). */
UENUM()
enum class EVeyraBindingNeed : uint8
{
	Essential,
	Optional,
};

/** Whether a binding's key is its own (SET-81 asks before taking it), or a click or modifier used with others. */
UENUM()
enum class EVeyraBindingSharing : uint8
{
	Exclusive,
	Shared,
};

/**
 * A key binding (Settings Bible §1.1). Its default is the developer's key in Property, a key field of
 * the developer's input settings, so the default lives in one place; the player's value is a key name,
 * or None for no key.
 */
USTRUCT()
struct FVeyraBindingSetting
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraTuningProvenance Provenance = EVeyraTuningProvenance::Provisional;

	UPROPERTY()
	EVeyraSettingCategory Category = EVeyraSettingCategory::Controls;

	UPROPERTY()
	EVeyraSettingScope Scope = EVeyraSettingScope::Account;

	UPROPERTY()
	EVeyraSettingAvailability Availability = EVeyraSettingAvailability::Anywhere;

	UPROPERTY()
	EVeyraSettingApplies Applies = EVeyraSettingApplies::AtOnce;

	UPROPERTY()
	FString Property;

	UPROPERTY()
	EVeyraBindingNeed Need = EVeyraBindingNeed::Optional;

	UPROPERTY()
	EVeyraBindingSharing Sharing = EVeyraBindingSharing::Exclusive;
};

/** One category of the Settings screen and its settings, in the order the screen lists them (Settings Bible §13). */
USTRUCT()
struct FVeyraSettingsSection
{
	GENERATED_BODY()

	UPROPERTY()
	EVeyraSettingCategory Category = EVeyraSettingCategory::Controls;

	UPROPERTY()
	TArray<FVeyraContentId> Settings;
};

USTRUCT()
struct FVeyraSettingsRegistry
{
	GENERATED_BODY()

	/** The Settings.json format this build reads (a schema version marker, not tuning). */
	static constexpr int32 SchemaVersion = 3;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraToggleSetting> Toggles;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraRangeSetting> Ranges;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraChoiceSetting> Choices;

	UPROPERTY()
	TMap<FVeyraContentId, FVeyraBindingSetting> Bindings;

	/**
	 * The Settings screen's categories in order, each with its settings in order. Only categories with
	 * settings appear (ADR-024 §5), so every setting is listed once, in its own category's section.
	 */
	UPROPERTY()
	TArray<FVeyraSettingsSection> Layout;
};

/** A setting's kind. */
enum class EVeyraSettingKind : uint8
{
	Toggle,
	Range,
	Choice,
	Binding,
};

/** One setting as the store and the screen see it, whatever its kind. */
struct FVeyraSettingInfo
{
	FVeyraContentId Id;
	EVeyraSettingKind Kind = EVeyraSettingKind::Toggle;
	EVeyraSettingCategory Category = EVeyraSettingCategory::Controls;
	EVeyraSettingScope Scope = EVeyraSettingScope::Account;
	EVeyraSettingAvailability Availability = EVeyraSettingAvailability::Anywhere;
	EVeyraSettingApplies Applies = EVeyraSettingApplies::AtOnce;
	/** The default as a value (see VeyraSettings::Normalize). */
	FString Default;
	/** The range's bounds, or the choice's options; empty for the other kinds. */
	const FVeyraRangeSetting* Range = nullptr;
	const FVeyraChoiceSetting* Choice = nullptr;
	/** A binding's property, need and sharing; null for the other kinds. */
	const FVeyraBindingSetting* Binding = nullptr;
};

/** A section of the Settings screen with its settings described, in order. */
struct FVeyraSettingsSectionInfo
{
	EVeyraSettingCategory Category = EVeyraSettingCategory::Controls;
	TArray<FVeyraSettingInfo> Settings;
};

/**
 * The registry's rules (ADR-024 §2). A setting's value is text: "On" or "Off" for a toggle, the
 * number for a range, the option for a choice.
 */
namespace VeyraSettings
{
	/** The toggle values as text. */
	VEYRASETTINGS_API const FString& On();
	VEYRASETTINGS_API const FString& Off();

	/** A binding's value for no key at all. Its unset value, empty, is the developer's key. */
	VEYRASETTINGS_API const FString& Unbound();

	/**
	 * Puts the player's keys from Store on Target, one of the developer's input settings: for each
	 * binding whose property is a key field of Target's class, its key, or none when unbound. Returns
	 * how many it set.
	 */
	VEYRASETTINGS_API int32 ApplyBindings(UObject& Target, const FVeyraSettingsStore& Store);

	/** Id's key now: the player's, or the developer's in Defaults; unset when Defaults has no such key field. */
	VEYRASETTINGS_API TOptional<FKey> BindingKey(const UObject& Defaults, const FVeyraSettingsStore& Store, const FVeyraContentId& Id);

	/** The key field of Class named Property; null when it has none. */
	VEYRASETTINGS_API FStructProperty* FindKeyProperty(const UClass& Class, const FString& Property);

	/**
	 * Problems the schema cannot catch, each a JSON pointer and a message; empty when consistent. A
	 * setting ID is used once across the kinds; a range's maximum is above its minimum and its default
	 * lies within them on a step; a choice's options are distinct and its default is one of them. The
	 * layout lists each category once and every setting once, in its own category's section.
	 */
	VEYRASETTINGS_API TArray<FString> Validate(const FVeyraSettingsRegistry& Registry);

	/** The setting Id describes, or nothing when the registry has none. */
	VEYRASETTINGS_API TOptional<FVeyraSettingInfo> Find(const FVeyraSettingsRegistry& Registry, const FVeyraContentId& Id);

	/** Every setting, toggles then ranges then choices, each kind in ID order. */
	VEYRASETTINGS_API TArray<FVeyraSettingInfo> All(const FVeyraSettingsRegistry& Registry);

	/** Whether Setting may change now: in a live match, only one available Anywhere (Settings Bible §6.2). */
	VEYRASETTINGS_API bool IsChangeable(const FVeyraSettingInfo& Setting, bool bInLiveMatch);

	/** The Settings screen's sections in the layout's order, each setting described; IDs the registry lacks are left out. */
	VEYRASETTINGS_API TArray<FVeyraSettingsSectionInfo> Sections(const FVeyraSettingsRegistry& Registry);

	/**
	 * Value as the setting's canonical text, or nothing when it is not a value the setting takes: a
	 * toggle takes On or Off; a range a number within its bounds, which snaps to the nearest step; a
	 * choice one of its options.
	 */
	VEYRASETTINGS_API TOptional<FString> Normalize(const FVeyraSettingInfo& Setting, FStringView Value);

	/** A range's number as the canonical text Normalize gives it. */
	VEYRASETTINGS_API FString NumberText(double Number);
}
