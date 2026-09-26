// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "CQTest.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Tags/VeyraDamageTags.h"

#if WITH_AUTOMATION_WORKER

namespace VeyraCoreTests
{
	// PROJECT_STRUCTURE.md §5, "Gameplay Tag vocabulary": a tag is a dotted path of PascalCase
	// ASCII segments. Each segment starts with an upper-case letter and continues with letters or
	// digits.
	static bool IsConventionalTagName(FStringView TagName)
	{
		bool bAtSegmentStart = true;
		for (const TCHAR Character : TagName)
		{
			const bool bIsUpper = Character >= TEXT('A') && Character <= TEXT('Z');
			const bool bIsLower = Character >= TEXT('a') && Character <= TEXT('z');
			const bool bIsDigit = Character >= TEXT('0') && Character <= TEXT('9');
			if (Character == TEXT('.'))
			{
				if (bAtSegmentStart)
				{
					return false; // A leading dot or an empty segment.
				}
				bAtSegmentStart = true;
			}
			else if (bAtSegmentStart)
			{
				if (!bIsUpper)
				{
					return false;
				}
				bAtSegmentStart = false;
			}
			else if (!bIsUpper && !bIsLower && !bIsDigit)
			{
				return false;
			}
		}
		return !bAtSegmentStart; // Rejects an empty name and a trailing dot.
	}

	// Veyra.Core.TagConvention.*: the convention checker itself, then every tag VeyraCore registers.
	TEST_CLASS(TagConvention, "Veyra.Core")
	{
		TEST_METHOD(AcceptsConventionalNames)
		{
			const TCHAR* const Names[] = {
				TEXT("Alpha"),
				TEXT("Alpha.Beta"),
				TEXT("Alpha2.Beta3.Gamma"),
				TEXT("AoE.DoT"),
				TEXT("Damage.Type.Physical"),
			};
			for (const TCHAR* Name : Names)
			{
				ASSERT_THAT(IsTrue(IsConventionalTagName(Name), FString::Printf(TEXT("'%s' should be accepted."), Name)));
			}
		}

		TEST_METHOD(RejectsUnconventionalNames)
		{
			const TCHAR* const Names[] = {
				TEXT(""),
				TEXT("."),
				TEXT(".Alpha"),
				TEXT("Alpha."),
				TEXT("Alpha..Beta"),
				TEXT("alpha.Beta"),
				TEXT("Alpha.beta"),
				TEXT("Alpha.2Beta"),
				TEXT("Alpha_Beta"),
				TEXT("Alpha Beta"),
				TEXT("Alpha-Beta"),
				TEXT("Alpha.Bêta"),
			};
			for (const TCHAR* Name : Names)
			{
				ASSERT_THAT(IsFalse(IsConventionalTagName(Name), FString::Printf(TEXT("'%s' should be rejected."), Name)));
			}
		}

#if WITH_EDITOR
		// Tag sources, explicit-tag flags and developer comments exist only in editor builds.

		static TArray<TSharedPtr<FGameplayTagNode>> GetVeyraCoreTagNodes()
		{
			// In editor builds a native tag's source is the name of the module that defines it.
			TArray<TSharedPtr<FGameplayTagNode>> Nodes;
			UGameplayTagsManager::Get().GetAllTagsFromSource(FName(TEXT("VeyraCore")), Nodes);
			return Nodes;
		}

		TEST_METHOD(FindsTheVeyraCoreTags)
		{
			// Guards the tests below against passing on an empty source list.
			const TArray<TSharedPtr<FGameplayTagNode>> Nodes = GetVeyraCoreTagNodes();
			const bool bFoundDamageTag = Nodes.ContainsByPredicate([](const TSharedPtr<FGameplayTagNode>& Node)
			{
				return Node->GetCompleteTag() == VeyraTags::Damage_Type_Physical.GetTag();
			});
			ASSERT_THAT(IsTrue(bFoundDamageTag, TEXT("VeyraCore's tags were not found under the source name VeyraCore.")));
		}

		TEST_METHOD(EveryVeyraCoreTagFollowsIt)
		{
			for (const TSharedPtr<FGameplayTagNode>& Node : GetVeyraCoreTagNodes())
			{
				const FString Name = Node->GetCompleteTagString();
				ASSERT_THAT(IsTrue(IsConventionalTagName(Name), FString::Printf(TEXT("'%s' breaks the tag naming convention."), *Name)));
			}
		}

		TEST_METHOD(EveryExplicitVeyraCoreTagHasAComment)
		{
			for (const TSharedPtr<FGameplayTagNode>& Node : GetVeyraCoreTagNodes())
			{
				if (Node->IsExplicitTag())
				{
					ASSERT_THAT(IsFalse(Node->GetDevComment().IsEmpty(),
						FString::Printf(TEXT("'%s' has no comment citing its canon section."), *Node->GetCompleteTagString())));
				}
			}
		}
#endif // WITH_EDITOR
	};
}

#endif // WITH_AUTOMATION_WORKER
