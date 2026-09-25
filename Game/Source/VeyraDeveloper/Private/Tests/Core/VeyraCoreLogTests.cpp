// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "VeyraCoreLog.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCoreTests
{
	// Veyra.Core.Log.*: VeyraCore's log category is exported, so other modules can log to it.
	TEST_CLASS(Log, "Veyra.Core")
	{
		TEST_METHOD(CategoryIsReachableUnderItsName)
		{
			const FString Name = LogVeyraCore.GetCategoryName().ToString();
			ASSERT_THAT(IsTrue(Name.Equals(TEXT("LogVeyraCore"), ESearchCase::CaseSensitive),
				FString::Printf(TEXT("Expected the category LogVeyraCore but found %s."), *Name)));
		}
	};
}

#endif // WITH_AUTOMATION_WORKER
