// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Gold/VeyraGoldComponent.h"
#include "Join/VeyraMatchAssignment.h"
#include "Statistics/VeyraMatchStatistics.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"

#include "VeyraMatchStatisticsSubsystem.generated.h"

class APlayerState;
class AVeyraPlayerState;
class AVeyraWard;
class UAbilitySystemComponent;
struct FVeyraDamageResolution;
struct FVeyraDeathEvent;
struct FVeyraFluxWellSecuredEvent;
struct FVeyraHealthRestored;
struct FVeyraStatusApplied;

/**
 * The match's one statistics service (Match Statistics Bible §1; ADR-017 §3), on the server. It keeps
 * a record for every participant, bots too, from what Combat, Economy, World and Vision report as it
 * happens, and publishes each one's K/D/A and last hits on its PlayerState's score. It computes nothing
 * those systems decide: it only counts what they say happened.
 *
 * Match starts it as the match loads, adds each participant as it is prepared, and stops it as the
 * match ends. A client's instance records nothing.
 */
UCLASS()
class VEYRAMATCH_API UVeyraMatchStatisticsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Server: begins recording what is reported. */
	void Start();

	/** Server: stops recording, as the match ends; crowd control still running counts until now. */
	void Stop();

	bool IsRecording() const { return bRecording; }

	/**
	 * Server: keeps a record for Participant from now, its Gold grants included. Match adds each one
	 * before its starting Gold, so that counts (ADR-017 §9.1). Adding one twice keeps one record.
	 */
	void AddParticipant(AVeyraPlayerState& Participant);

	/**
	 * Server: Participant's statistics as they stand, with its level and equipment now; unset if it has
	 * no record.
	 */
	TOptional<FVeyraPlayerStatistics> Snapshot(const AVeyraPlayerState& Participant) const;

	/**
	 * Server: Participant disconnects: its record keeps who it was and what it had as it left, so the
	 * scoreboard has its line even if its PlayerState goes (ADR-017 §5). If it returns, the record
	 * follows its PlayerState again (ADR-019 §1).
	 */
	void NoteLeaving(const AVeyraPlayerState& Participant);

	/**
	 * Server: the scoreboard (ADR-017 §5): a line for every participant recorded, one who left with
	 * what it had as it left; side A first, then in the order they joined.
	 */
	TArray<FVeyraPlayerResult> BuildScoreboard() const;

	/** Server: every Flux Well secured while recording, in order (Match Statistics Bible §5). */
	const TArray<FVeyraWellCapture>& GetWellCaptures() const { return WellCaptures; }

	virtual void Deinitialize() override;

private:
	struct FRecord
	{
		TWeakObjectPtr<AVeyraPlayerState> Participant;

		/** Its PlayerId, for the scoreboard's order. */
		int32 PlayerId = 0;

		/** Its scoreboard line as it left, once it has: its PlayerState goes with it. */
		TOptional<FVeyraPlayerResult> Left;
		/** Its Vanguard's Ability System Component, which every combat event names. */
		TWeakObjectPtr<const UAbilitySystemComponent> Unit;
		FVeyraPlayerStatistics Statistics;
		/** Crowd control it applied to enemy Vanguards, by target and kind, for the union of each (§4). */
		TMap<TPair<FObjectKey, uint8>, TArray<FVeyraSpan>> CrowdControl;
		FDelegateHandle GoldHandle;
		FDelegateHandle BuybackHandle;
	};

	FRecord* Find(const UAbilitySystemComponent* Unit);
	FRecord* Find(const APlayerState* Participant);
	const FRecord* Find(const AVeyraPlayerState& Participant) const;

	void OnDeath(const FVeyraDeathEvent& Death);
	void OnDamageResolved(const FVeyraDamageResolution& Event);
	void OnHealthRestored(const FVeyraHealthRestored& Event);
	void OnStatusApplied(const FVeyraStatusApplied& Event);
	void OnWardPlaced(const AVeyraWard& Ward, APlayerState& Placer);
	void OnFluxWellSecured(const FVeyraFluxWellSecuredEvent& Event);
	void OnGoldGranted(double Amount, EVeyraGoldReason Reason, const FVeyraGoldSource& From, TWeakObjectPtr<AVeyraPlayerState> Participant);
	void OnBoughtBack(double Cost, TWeakObjectPtr<AVeyraPlayerState> Participant);

	/** Record's scoreboard line: as it stands, or as it was when its participant left. Unset if neither. */
	TOptional<FVeyraPlayerResult> LineOf(const FRecord& Record) const;

	/** Publishes Record's public part on its PlayerState's score. */
	static void Publish(const FRecord& Record);

	/** Crowd control ends when its target dies (Combat Bible §18): every span on Target ends by At. */
	void EndCrowdControlOn(const UAbilitySystemComponent& Target, double At);

	/** Now, or when recording stopped. */
	double RecordedUntil() const;

	TArray<FRecord> Records;
	TArray<FVeyraWellCapture> WellCaptures;
	double StoppedAt = 0.0;
	bool bRecording = false;
	bool bStopped = false;
	FDelegateHandle DeathHandle;
	FDelegateHandle DamageHandle;
	FDelegateHandle HealingHandle;
	FDelegateHandle StatusHandle;
	FDelegateHandle WardHandle;
	FDelegateHandle WellHandle;
};
