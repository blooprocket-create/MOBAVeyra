// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "Content/VeyraContentId.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "VeyraShowcaseRecorderSubsystem.generated.h"

class AVeyraGameState;
class AVeyraPlayerController;
class AVeyraVanguardCharacter;
class FFrameGrabber;

/**
 * The skill showcase recorder (ADR-074 §3): a client started with -VeyraShowcase=<directory>, connected to the
 * showcase's stage, records each of its Vanguard's abilities in motion. It takes developer levels up to the first
 * ultimate rank and ranks every ability, hides the HUD and on-screen messages, and locks the camera on its Vanguard. With
 * the settings' rehearsal it casts the whole kit once unrecorded first. Then, for each ability in slot order, it:
 * - asks the server to reset the stage (Veyra.Dev.ResetShowcase) and waits for it to settle;
 * - steps toward the dummy if the dummy stands beyond the ability's reach;
 * - casts at the dummy, or at itself for an ability cast for an ally, capturing one frame in every FrameStride from the
 *   order until the cast has ended and the tail has passed, or the clip's limit.
 * Frames are written off the game thread as <directory>/<ability>/<index>.png, and manifest.json lists each clip's
 * frames with their server times. It logs "VeyraShowcase: DONE" or "VeyraShowcase: FAIL" and quits. Developer client
 * builds only; it does nothing on a client that is not connected to a server.
 */
UCLASS()
class UVeyraShowcaseRecorderSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	enum class EStep : uint8
	{
		WaitForLiveMatch,
		WaitForLevels,
		WaitForRanks,
		Reset,
		Settle,
		Approach,
		Cast,
		Prime,
		Attack,
		Record,
		Flush,
		Finished,
	};

	/** One clip: the ability, and each frame's file name and server time. */
	struct FClip
	{
		FVeyraContentId Ability;
		FString Slot;
		TArray<TPair<FString, double>> Frames;
		double OrderedAt = 0.0;
		double CommittedAt = -1.0;
		/** Why the recorder left it out, if it did. */
		FString Failure;
	};

	bool Tick(float DeltaSeconds);
	void Advance(EStep Next, const TCHAR* Description);
	void Finish(bool bDone, const FString& Reason);

	AVeyraPlayerController* GetController() const;
	AVeyraGameState* GetGameState() const;
	double ServerNow() const;

	/** Hides the HUD and locks the camera on the Vanguard, at the settings' distance. */
	void FrameTheStage(AVeyraPlayerController& Controller);

	/** Orders the cast of the current slot's ability, and starts its clip if it is the first try. */
	void OrderCast(AVeyraPlayerController& Controller, const AVeyraVanguardCharacter& Vanguard);

	/** One step of waiting for the ordered cast: its commit, a refusal to try again after, to prime for, or to skip. */
	void TickCast(AVeyraPlayerController& Controller);

	/** Leaves the current slot's ability out of the recording, for Reason, and goes on to the next. */
	void SkipAbility(const FVeyraContentId& Ability, const FString& Reason);

	/** Goes on to the next slot, or ends the rehearsal, or goes on to the basic attack after the last. */
	void NextSlot(const FString& Description);

	/** Orders a basic attack at Dummy and starts its clip (ADR-074 §3). */
	void OrderAttack(AVeyraPlayerController& Controller, AActor& Dummy);

	/** How far the Vanguard's basic attack reaches, from Vanguards.json; 0 when unknown. */
	double AttackReach(const AVeyraPlayerController& Controller) const;

	/** Lists the basic attack's clip as failed for Reason and ends the recording, or after a rehearsal starts it (ADR-074 §3). */
	void FailAttack(const FString& Reason);

	/** After the basic attack's clip: the recording from the first slot, after the rehearsal; the encoding, after the recording. */
	void AfterAttack(const FString& Description);

	void StartCapture();
	void CaptureFrame();
	/** Hands the frames read back so far to the image writer. */
	void WriteCapturedFrames();
	void StopCapture();
	/** Whether frames captured are still being read back or not yet handed to the writer. */
	bool HasOutstandingFrames() const;
	bool WriteManifest() const;

	EStep Step = EStep::WaitForLiveMatch;
	FString OutputDir;
	FTSTicker::FDelegateHandle TickHandle;
	double StartRealTime = 0.0;
	double StepSince = 0.0;
	int32 KitLevel = 0;
	int32 SlotIndex = 0;
	/** Whether the kit is being cast once unrecorded, so every effect is ready before it is recorded. */
	bool bRehearsing = false;
	/** The number of the latest reset asked for, which its reply carries; when this slot's first was asked for; and when the stage was set. */
	int32 ResetToken = 0;
	double ResetSince = 0.0;
	TOptional<double> SetAt;
	/** Whether the current slot's clip is open, and whether the dummy has been primed for it (VeyraShowcase::EAfterRefusal::Prime). */
	bool bClipForSlot = false;
	bool bPrimed = false;
	/** When the priming attack is ordered again, until one commits. */
	double PrimeRetryAt = 0.0;
	/** Whether the kit is recorded and its basic attack is being recorded, and whether that attack has been seen to commit. */
	bool bRecordingAttack = false;
	bool bAttackCommitted = false;
	bool bFirstSwingDone = false;

	/** The cast awaiting the server, and what it was asked for. */
	bool bCastPending = false;
	FVeyraContentId CastAbility;
	double ReadyAtBefore = 0.0;
	int32 RejectionsBefore = 0;
	int32 OrderRejectionsBefore = 0;
	TOptional<double> FirstRefusedAt;
	double NextTryAt = 0.0;
	/** When the pending order went, and when the clip's first did, in real seconds. */
	double PendingSince = 0.0;
	double CastSince = 0.0;
	/** The caster's cast before the pending order, so a new one for its ability shows the order was taken. */
	int32 CastIdBefore = 0;
	/** Whether an order for this clip went unanswered and was sent again. */
	bool bOrderedAgain = false;
	bool bAskedForResource = false;
	/** When the recorded cast's caster was last busy, in server time. */
	double LastBusyAt = 0.0;

	TSharedPtr<FFrameGrabber> Grabber;
	int32 FrameCount = 0;
	TArray<FClip> Clips;
};
