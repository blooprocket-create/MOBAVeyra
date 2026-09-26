// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/StringView.h"
#include "Misc/Optional.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"

#include "VeyraContentId.generated.h"

/**
 * The stable identifier of a piece of content, such as an ability or a Vanguard: lowercase ASCII
 * snake_case, matching the Vanguard and backend identifiers (ADR-006 §6). Tuning keys content by
 * it, and code compares it; it is never shown to players.
 */
USTRUCT()
struct VEYRACORE_API FVeyraContentId
{
	GENERATED_BODY()

	/** The format, as the tuning schemas spell it. */
	static constexpr const TCHAR* Pattern = TEXT("^[a-z][a-z0-9]*(_[a-z0-9]+)*$");

	FVeyraContentId() = default;

	/** Whether Text follows the format: a lowercase letter first, then lowercase letters, digits and single underscores between them. */
	static bool IsValidText(FStringView Text);

	/** The identifier for Text, or nothing when Text does not follow the format. */
	static TOptional<FVeyraContentId> FromText(FStringView Text);

	/** False only for a default-constructed identifier. */
	bool IsValid() const { return !Name.IsNone(); }

	FString ToString() const { return Name.ToString(); }

	bool operator==(const FVeyraContentId& Other) const { return Name == Other.Name; }

	friend uint32 GetTypeHash(const FVeyraContentId& Id) { return GetTypeHash(Id.Name); }

private:
	/** Case-insensitive as an FName, which is safe because the format allows only lowercase. */
	UPROPERTY()
	FName Name;
};

template <>
struct TStructOpsTypeTraits<FVeyraContentId> : public TStructOpsTypeTraitsBase2<FVeyraContentId>
{
	enum
	{
		WithIdenticalViaEquality = true,
	};
};

inline FString LexToString(const FVeyraContentId& Id)
{
	return Id.ToString();
}
