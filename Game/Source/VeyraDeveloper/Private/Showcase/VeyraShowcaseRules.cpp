// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Showcase/VeyraShowcaseRules.h"

#include "Attacks/VeyraBasicAttackComponent.h"
#include "Kit/VeyraKitOrders.h"
#include "Movement/VeyraForcedMovementTypes.h"
#include "VeyraCombatVerbs.h"

namespace VeyraShowcase
{
	namespace
	{
		const TCHAR* const SetWord = TEXT("set");
		const TCHAR* const NotSetWord = TEXT("not set");

		FString Prefix(int32 Token)
		{
			return FString::Printf(TEXT("Showcase reset %d: "), Token);
		}
	}

	FString ResetReply(int32 Token, bool bSet, const FString& Done)
	{
		return Prefix(Token) + (bSet ? SetWord : NotSetWord) + TEXT(". ") + Done;
	}

	EResetAnswer ReadResetReply(const FString& Reply, int32 Token)
	{
		const FString Answering = Prefix(Token);
		if (!Reply.StartsWith(Answering, ESearchCase::CaseSensitive))
		{
			return EResetAnswer::None;
		}
		const FString Answer = Reply.RightChop(Answering.Len());
		return Answer.StartsWith(FString(SetWord) + TEXT("."), ESearchCase::CaseSensitive) ? EResetAnswer::Set : EResetAnswer::NotSet;
	}

	EAfterRefusal AfterRefusal(EVeyraCastRejection Rejection, bool bPrimed)
	{
		if (VeyraKitOrders::IsPassingRejection(Rejection))
		{
			return EAfterRefusal::Retry;
		}
		return Rejection == EVeyraCastRejection::InvalidTarget && !bPrimed ? EAfterRefusal::Prime : EAfterRefusal::Skip;
	}

	bool MayStartAttackClip(EVeyraAttackPhase Phase)
	{
		return Phase == EVeyraAttackPhase::None;
	}

	EAfterAttack AfterAttack(bool bRehearsing)
	{
		return bRehearsing ? EAfterAttack::Record : EAfterAttack::Flush;
	}

	FVector MarkOf(const FVector& Centre, const FVector& OwnStart, double Stand, double YawDegrees)
	{
		const FVector Outward = FRotator(0.0, YawDegrees, 0.0).RotateVector((OwnStart - Centre).GetSafeNormal2D());
		return Centre + Outward * Stand;
	}

	void ReleaseHolds(UAbilitySystemComponent& Unit)
	{
		if (VeyraCombat::IsRiding(Unit))
		{
			VeyraCombat::EndRide(Unit, EVeyraRideEndReason::Expired);
		}
		if (VeyraCombat::GetAttachHost(Unit))
		{
			VeyraCombat::Detach(Unit);
		}
	}
}
