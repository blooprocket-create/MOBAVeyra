// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Tags/VeyraDamageTags.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCoreTests
{
	// Veyra.Core.DamageTypeTags.*: the native tags for the three primary damage types match
	// Combat Bible §2 exactly, and no other damage type exists.
	TEST_CLASS(DamageTypeTags, "Veyra.Core")
	{
		struct FExpectedTag
		{
			FString CanonName;
			FGameplayTag Tag;
		};

		// The canon spelling of each primary damage type, beside the native tag that declares it.
		const TArray<FExpectedTag> ExpectedTags = {
			{ TEXT("Damage.Type.Physical"), VeyraTags::Damage_Type_Physical.GetTag() },
			{ TEXT("Damage.Type.Magic"), VeyraTags::Damage_Type_Magic.GetTag() },
			{ TEXT("Damage.Type.True"), VeyraTags::Damage_Type_True.GetTag() },
		};

		TEST_METHOD(AreValid)
		{
			for (const FExpectedTag& Expected : ExpectedTags)
			{
				ASSERT_THAT(IsTrue(Expected.Tag.IsValid(), FString::Printf(TEXT("%s is not a valid tag."), *Expected.CanonName)));
			}
		}

		TEST_METHOD(HaveTheCanonNames)
		{
			// FName and FString equality ignore case, so compare the spelling case-sensitively.
			for (const FExpectedTag& Expected : ExpectedTags)
			{
				const FString Actual = Expected.Tag.ToString();
				ASSERT_THAT(IsTrue(Actual.Equals(Expected.CanonName, ESearchCase::CaseSensitive),
					FString::Printf(TEXT("Expected the tag %s but found %s."), *Expected.CanonName, *Actual)));
			}
		}

		TEST_METHOD(ResolveByNameToTheNativeTags)
		{
			const UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();
			for (const FExpectedTag& Expected : ExpectedTags)
			{
				const FGameplayTag Requested = TagsManager.RequestGameplayTag(FName(*Expected.CanonName), /*ErrorIfNotFound*/ false);
				ASSERT_THAT(AreEqual(Expected.Tag, Requested));
			}
		}

		TEST_METHOD(AreDistinct)
		{
			for (int32 First = 0; First < ExpectedTags.Num(); ++First)
			{
				for (int32 Second = First + 1; Second < ExpectedTags.Num(); ++Second)
				{
					const FGameplayTag& A = ExpectedTags[First].Tag;
					const FGameplayTag& B = ExpectedTags[Second].Tag;
					// MatchesTag is true for the tag itself and its parents, so checking both
					// directions rules out equality and any ancestor relationship.
					ASSERT_THAT(IsFalse(A.MatchesTag(B), FString::Printf(TEXT("%s matches %s."), *A.ToString(), *B.ToString())));
					ASSERT_THAT(IsFalse(B.MatchesTag(A), FString::Printf(TEXT("%s matches %s."), *B.ToString(), *A.ToString())));
				}
			}
		}

		TEST_METHOD(AreTheOnlyDamageTypes)
		{
			const UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();
			const FGameplayTag DamageType = TagsManager.RequestGameplayTag(FName(TEXT("Damage.Type")), /*ErrorIfNotFound*/ false);
			ASSERT_THAT(IsTrue(DamageType.IsValid(), TEXT("The parent tag Damage.Type is not registered.")));

			const FGameplayTagContainer Descendants = TagsManager.RequestGameplayTagChildren(DamageType);
			ASSERT_THAT(AreEqual(ExpectedTags.Num(), Descendants.Num(),
				FString::Printf(TEXT("Damage.Type has unexpected descendants: %s"), *Descendants.ToStringSimple())));
			for (const FExpectedTag& Expected : ExpectedTags)
			{
				ASSERT_THAT(IsTrue(Descendants.HasTagExact(Expected.Tag),
					FString::Printf(TEXT("%s is not a descendant of Damage.Type."), *Expected.CanonName)));
			}
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
