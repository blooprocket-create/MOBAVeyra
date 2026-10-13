// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "VeyraShowcaseSettings.generated.h"

/**
 * How the skill showcase recorder records (ADR-074 §4): harness settings, not gameplay tuning, read by the stage's
 * server and by the recording client. Stored in Config/DefaultGame.ini; neither runs while Validate finds problems.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Veyra Skill Showcase"))
class UVeyraShowcaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Every problem with these settings, as "Field: message"; empty when the recorder can use them. */
	TArray<FString> Validate() const;

	/** One frame in this many is captured: 2 captures every other frame. */
	UPROPERTY(Config, EditAnywhere, Category = "Capture")
	int32 FrameStride = 0;

	/** The recording client's frame rate limit, so one frame in FrameStride comes at a steady rate. */
	UPROPERTY(Config, EditAnywhere, Category = "Capture")
	int32 FrameRateLimit = 0;

	/** The size each captured frame is saved at, in pixels; the recording client's window is opened at it. */
	UPROPERTY(Config, EditAnywhere, Category = "Capture")
	FIntPoint CaptureSize = FIntPoint::ZeroValue;

	/** After the stage is reset, how long to wait before the next cast: long enough for statuses on either body to run out. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing")
	double SettleSeconds = 0.0;

	/** How long a clip runs on after its cast has ended, for what lands and what lingers. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing")
	double TailSeconds = 0.0;

	/** The longest a clip may run, from its cast order. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing")
	double MaxClipSeconds = 0.0;

	/** How long one ability may be refused for a passing reason before the recording fails. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing")
	double CastPatienceSeconds = 0.0;

	/**
	 * How long after the settle the recorder waits for the statuses the last clip left to end through their owners (a
	 * stance, a mode), before casting anyway: longer than the longest such status the roster holds.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Timing")
	double StatusPatienceSeconds = 0.0;

	/** How long the whole recording may take, from the client's start. */
	UPROPERTY(Config, EditAnywhere, Category = "Timing")
	double TimeoutSeconds = 0.0;

	/**
	 * How far from the stage's centre each body stands after a reset, on the line between the sides' starts: inside the
	 * starts, so the start markers stand behind the bodies, out of the shot's middle.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Stage")
	double StandDistanceFromCenter = 0.0;

	/**
	 * How far the line the bodies stand on is turned from the line between the starts, in degrees about the stage's
	 * centre. The camera looks along that line, so 0 stands them top and bottom on the screen and 90 left and right.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Stage")
	double MarkYawDegrees = 0.0;

	/** An ability cast at the dummy from further than its reach steps in to this share of it first. */
	UPROPERTY(Config, EditAnywhere, Category = "Stage")
	double ApproachShareOfReach = 0.0;

	/**
	 * Whether the recorder casts the whole kit once unrecorded before recording it: an effect a client first plays may
	 * still be compiling, and would be missing from its first clip.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Capture")
	bool bRehearse = false;

	/** Whether the HUD shows in the clips: health bars, numbers and the action bar over the fight. */
	UPROPERTY(Config, EditAnywhere, Category = "Stage")
	bool bShowHud = false;

	/** The Vanguard the dummy plays unless the stage's URL names another. */
	UPROPERTY(Config, EditAnywhere, Category = "Stage")
	FString DefaultDummy;
};
