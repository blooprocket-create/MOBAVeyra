// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Life/VeyraKillCredit.h"

#include "AbilitySystemComponent.h"

namespace VeyraKillCredit
{
UAbilitySystemComponent* Resolve(UAbilitySystemComponent* LethalSource, bool bLethalSourceIsEnemyVanguard,
	TConstArrayView<FVeyraContribution> Contributions, double Now, double WindowSeconds)
{
	if (LethalSource && bLethalSourceIsEnemyVanguard)
	{
		return LethalSource;
	}
	UAbilitySystemComponent* Credited = nullptr;
	double CreditedAt = 0.0;
	for (const FVeyraContribution& Contribution : Contributions)
	{
		UAbilitySystemComponent* Contributor = Contribution.Contributor.Get();
		if (!Contributor || Now - Contribution.AtSeconds > WindowSeconds)
		{
			continue;
		}
		const bool bLater = !Credited || Contribution.AtSeconds > CreditedAt;
		const bool bTieBreak = Credited && Contribution.AtSeconds == CreditedAt && Contributor->GetUniqueID() < Credited->GetUniqueID();
		if (bLater || bTieBreak)
		{
			Credited = Contributor;
			CreditedAt = Contribution.AtSeconds;
		}
	}
	return Credited;
}
}
