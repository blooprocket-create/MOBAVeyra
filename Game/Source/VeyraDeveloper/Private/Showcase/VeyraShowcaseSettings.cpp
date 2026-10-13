// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Showcase/VeyraShowcaseSettings.h"

TArray<FString> UVeyraShowcaseSettings::Validate() const
{
	TArray<FString> Problems;
	const auto Require = [&Problems](bool bValid, const TCHAR* Field, const TCHAR* Message) {
		if (!bValid)
		{
			Problems.Add(FString::Printf(TEXT("%s: %s"), Field, Message));
		}
	};
	Require(FrameStride >= 1, TEXT("FrameStride"), TEXT("one frame in at least every one is captured."));
	Require(FrameRateLimit > 0, TEXT("FrameRateLimit"), TEXT("the recording client runs at a frame rate above 0."));
	Require(MarkYawDegrees >= -180.0 && MarkYawDegrees <= 180.0, TEXT("MarkYawDegrees"), TEXT("the marks' line turns between -180 and 180 degrees."));
	Require(StandDistanceFromCenter > 0.0, TEXT("StandDistanceFromCenter"), TEXT("each body stands a distance above 0 from the centre."));
	Require(CaptureSize.X > 0 && CaptureSize.Y > 0, TEXT("CaptureSize"), TEXT("frames need a width and a height above 0."));
	Require(SettleSeconds >= 0.0 && TailSeconds >= 0.0, TEXT("SettleSeconds"), TEXT("the settle and tail times are 0 or more."));
	Require(MaxClipSeconds > TailSeconds, TEXT("MaxClipSeconds"), TEXT("a clip may run longer than its tail."));
	Require(CastPatienceSeconds > 0.0, TEXT("CastPatienceSeconds"), TEXT("a refused cast is retried for a time above 0."));
	Require(StatusPatienceSeconds > 0.0 && StatusPatienceSeconds < TimeoutSeconds, TEXT("StatusPatienceSeconds"),
		TEXT("the wait for a clip's statuses to end is above 0 and inside the recording's timeout."));
	Require(TimeoutSeconds > 0.0, TEXT("TimeoutSeconds"), TEXT("the recording needs a time above 0 to finish in."));
	Require(ApproachShareOfReach > 0.0 && ApproachShareOfReach <= 1.0, TEXT("ApproachShareOfReach"),
		TEXT("a cast steps in to a share of its reach above 0 and at most all of it."));
	Require(!DefaultDummy.IsEmpty(), TEXT("DefaultDummy"), TEXT("the dummy needs a Vanguard to play."));
	return Problems;
}
