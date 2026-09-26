// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Tags/VeyraHealthTags.h"
#include "Tags/VeyraShieldTags.h"
#include "Tags/VeyraStatusTags.h"
#include "Tests/Core/VeyraTagTestHelpers.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCoreTests
{
	// Veyra.Core.ShieldTypeTags.*: the three shield categories of Combat Bible §7, and no others.
	TEST_CLASS(ShieldTypeTags, "Veyra.Core")
	{
		TEST_METHOD(HaveTheCanonNamesAndResolve)
		{
			ASSERT_THAT(IsTrue(DescribeNativeTagProblem(TEXT("Shield.Type.Physical"), VeyraTags::Shield_Type_Physical).IsEmpty()));
			ASSERT_THAT(IsTrue(DescribeNativeTagProblem(TEXT("Shield.Type.Magic"), VeyraTags::Shield_Type_Magic).IsEmpty()));
			ASSERT_THAT(IsTrue(DescribeNativeTagProblem(TEXT("Shield.Type.Universal"), VeyraTags::Shield_Type_Universal).IsEmpty()));
		}

		TEST_METHOD(AreTheOnlyShieldTypes)
		{
			const FGameplayTag ShieldType = UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Shield.Type")), /*ErrorIfNotFound*/ false);
			ASSERT_THAT(IsTrue(ShieldType.IsValid(), TEXT("The parent tag Shield.Type is not registered.")));
			const FGameplayTagContainer Descendants = UGameplayTagsManager::Get().RequestGameplayTagChildren(ShieldType);
			ASSERT_THAT(AreEqual(3, Descendants.Num(), FString::Printf(TEXT("Shield.Type has: %s"), *Descendants.ToStringSimple())));
			ASSERT_THAT(IsTrue(Descendants.HasTagExact(VeyraTags::Shield_Type_Physical)
				&& Descendants.HasTagExact(VeyraTags::Shield_Type_Magic)
				&& Descendants.HasTagExact(VeyraTags::Shield_Type_Universal)));
		}
	};

	// Veyra.Core.HealthTags.*: Temporary Health (Combat Bible §7).
	TEST_CLASS(HealthTags, "Veyra.Core")
	{
		TEST_METHOD(TemporaryHealthHasItsCanonName)
		{
			const FString Problem = DescribeNativeTagProblem(TEXT("TemporaryHealth"), VeyraTags::TemporaryHealth);
			ASSERT_THAT(IsTrue(Problem.IsEmpty(), Problem));
		}
	};

	// Veyra.Core.StatusTags.*: combat states (Combat Bible §10).
	TEST_CLASS(StatusTags, "Veyra.Core")
	{
		TEST_METHOD(InvulnerableHasItsCanonName)
		{
			const FString Problem = DescribeNativeTagProblem(TEXT("Status.Invulnerable"), VeyraTags::Status_Invulnerable);
			ASSERT_THAT(IsTrue(Problem.IsEmpty(), Problem));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
