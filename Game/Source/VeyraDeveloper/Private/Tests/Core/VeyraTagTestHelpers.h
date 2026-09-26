// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"

namespace VeyraCoreTests
{
	/**
	 * Why a native tag is not exactly its canon spelling, registered with the tag manager, or an
	 * empty string when it is. FName and FString equality ignore case, so the spelling is compared
	 * case-sensitively.
	 */
	inline FString DescribeNativeTagProblem(const FString& CanonName, const FGameplayTag& Tag)
	{
		if (!Tag.IsValid())
		{
			return FString::Printf(TEXT("%s is not a valid tag."), *CanonName);
		}
		if (!Tag.ToString().Equals(CanonName, ESearchCase::CaseSensitive))
		{
			return FString::Printf(TEXT("Expected the tag %s but found %s."), *CanonName, *Tag.ToString());
		}
		if (UGameplayTagsManager::Get().RequestGameplayTag(FName(*CanonName), /*ErrorIfNotFound*/ false) != Tag)
		{
			return FString::Printf(TEXT("%s does not resolve to its native tag."), *CanonName);
		}
		return FString();
	}
}
