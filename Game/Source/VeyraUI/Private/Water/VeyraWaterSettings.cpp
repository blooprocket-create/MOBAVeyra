// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Water/VeyraWaterSettings.h"

#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"

TArray<FString> UVeyraWaterSettings::Validate() const
{
	TArray<FString> Problems;
	const auto Require = [&Problems](bool bValid, const TCHAR* Field, const FString& Message) {
		if (!bValid)
		{
			Problems.Add(FString::Printf(TEXT("%s: %s"), Field, *Message));
		}
	};
	Require(!RippleMaterial.IsNull() && !RiverTag.IsNone(), TEXT("RippleMaterial"), TEXT("the ripples need the material that steps them and the river's tag."));
	Require(WindowSize > 0.0f && Resolution >= 2, TEXT("WindowSize"), TEXT("the ripples' window needs a size above 0 and at least two texels across."));
	Require(StepsPerSecond > 0.0f && MaxStepsPerFrame >= 1, TEXT("StepsPerSecond"), TEXT("the ripples need steps a second above 0 and at least one step a frame."));
	Require(Damping > 0.0f && Damping < 1.0f, TEXT("Damping"), TEXT("a ripple must keep some of its height each step, and lose some."));
	Require(EdgeFade > 0.0f && EdgeFade <= 0.5f, TEXT("EdgeFade"), TEXT("the ripples fade over a share of the window above 0, at most half of it."));
	Require(MaxSources >= 1 && MaxSources <= SourceSlots, TEXT("MaxSources"),
		FString::Printf(TEXT("the water answers at least one unit and at most the %d its material takes."), SourceSlots));
	Require(SourceRadius > 0.0f && StillStrength >= 0.0f && MovingStrength >= StillStrength && FullSpeed > 0.0f && RingsPerSecond > 0.0f, TEXT("SourceRadius"),
		TEXT("a unit stirs the water across a radius above 0, standing no harder than moving, full at a speed above 0, sending rings a second above 0."));
	Require(NormalStrength > 0.0f, TEXT("NormalStrength"), TEXT("the ripples must bend the surface."));
	const FName Names[] = { StateParameter, TexelParameter, ShiftParameter, DampingParameter, EdgeFadeParameter, PhaseParameter, SourceParameterPrefix,
		HeightParameter, WindowParameter, NormalStrengthParameter };
	TSet<FName> Seen;
	bool bNamed = true;
	for (const FName Name : Names)
	{
		bool bAgain = false;
		Seen.Add(Name, &bAgain);
		bNamed &= !Name.IsNone() && !bAgain;
	}
	Require(bNamed, TEXT("StateParameter"), TEXT("every material parameter needs a name of its own."));
	Require(!WakeEffect.IsNull() && WakeScale > 0.0f && !WakeStrengthParameter.IsNone(), TEXT("WakeEffect"),
		TEXT("a wake needs its effect, a scale above 0 and the parameter its strength is set by."));
	Require(!SplashEffect.IsNull() && SplashScale > 0.0f && SplashSpeed > 0.0f, TEXT("SplashEffect"),
		TEXT("a splash needs its effect, a scale above 0 and the speed above 0 that throws it."));
	return Problems;
}

FName UVeyraWaterSettings::SourceParameter(int32 Slot) const
{
	return FName(*FString::Printf(TEXT("%s%d"), *SourceParameterPrefix.ToString(), Slot));
}
