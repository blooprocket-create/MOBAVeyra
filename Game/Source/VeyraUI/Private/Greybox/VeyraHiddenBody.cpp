// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Greybox/VeyraHiddenBody.h"

#include "Greybox/VeyraGreyboxSettings.h"

EVeyraHiddenKind VeyraHiddenBody::KindOf(bool bShows, bool bInvisible, bool bCamouflaged, bool bInDenseFog)
{
	if (!bShows)
	{
		return EVeyraHiddenKind::None;
	}
	if (bInvisible)
	{
		return EVeyraHiddenKind::Invisible;
	}
	if (bCamouflaged)
	{
		return EVeyraHiddenKind::Camouflage;
	}
	return bInDenseFog ? EVeyraHiddenKind::DenseFog : EVeyraHiddenKind::None;
}

double VeyraHiddenBody::StepVeil(double Veil, bool bHidden, double DeltaSeconds, double FadeSeconds)
{
	const double Step = FadeSeconds > 0.0 ? DeltaSeconds / FadeSeconds : 1.0;
	return FMath::Clamp(Veil + (bHidden ? Step : -Step), 0.0, 1.0);
}

FLinearColor VeyraHiddenBody::TintOf(EVeyraHiddenKind Kind, const UVeyraGreyboxSettings& Settings)
{
	switch (Kind)
	{
	case EVeyraHiddenKind::Invisible:
		return Settings.InvisibleVeilColor;
	case EVeyraHiddenKind::Camouflage:
		return Settings.CamouflageVeilColor;
	case EVeyraHiddenKind::DenseFog:
	case EVeyraHiddenKind::None:
		break;
	}
	return Settings.FogVeilColor;
}
