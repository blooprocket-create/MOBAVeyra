// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Hash/Blake3.h"
#include "Misc/Optional.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tuning/VeyraAbilitiesTuning.h"
#include "Tuning/VeyraTuning.h"

#include "VeyraAbilitiesTuningSubsystem.generated.h"

/**
 * Owns the Abilities domain's tuning: loads and validates Game/Tuning/Abilities.json once at startup
 * (ADR-006 §6). Ability code reads it through Get(); nothing else keeps a copy.
 */
UCLASS()
class VEYRAABILITIES_API UVeyraAbilitiesTuningSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/** The domain's file name in Game/Tuning. */
	static constexpr const TCHAR* Domain = TEXT("Abilities");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The loaded Abilities tuning. Fails a check if it did not load: abilities never run on defaults. */
	static const FVeyraAbilitiesTuning& Get();

	/** A targeted damage ability's tuning, or null if no such ability exists. */
	static const FVeyraTargetedDamageAbilityTuning* FindTargetedDamage(const FVeyraContentId& Ability);

	/** An area ability's tuning, or null. */
	static const FVeyraAreaAbilityTuning* FindArea(const FVeyraContentId& Ability);

	/** A self-buff ability's tuning, or null. */
	static const FVeyraSelfBuffAbilityTuning* FindSelfBuff(const FVeyraContentId& Ability);

	/** A skillshot ability's tuning, or null. */
	static const FVeyraSkillshotAbilityTuning* FindSkillshot(const FVeyraContentId& Ability);

	/** A dash ability's tuning, or null. */
	static const FVeyraDashAbilityTuning* FindDash(const FVeyraContentId& Ability);

	/** An empowered-attack ability's tuning, or null. */
	static const FVeyraEmpoweredAttackAbilityTuning* FindEmpoweredAttack(const FVeyraContentId& Ability);
	static const FVeyraVolleyAbilityTuning* FindVolley(const FVeyraContentId& Ability);
	static const FVeyraTetherAbilityTuning* FindTether(const FVeyraContentId& Ability);
	static const FVeyraAttachAbilityTuning* FindAttach(const FVeyraContentId& Ability);
	static const FVeyraRideAbilityTuning* FindRide(const FVeyraContentId& Ability);
	static const FVeyraAmbushAbilityTuning* FindAmbush(const FVeyraContentId& Ability);
	static const FVeyraStanceAbilityTuning* FindStance(const FVeyraContentId& Ability);
	static const FVeyraPlacementAbilityTuning* FindPlacement(const FVeyraContentId& Ability);
	static const FVeyraBlinkAbilityTuning* FindBlink(const FVeyraContentId& Ability);

	/** The companion Id defines (ADR-034 §3), or null. */
	static const FVeyraCompanionTuning* FindCompanion(const FVeyraContentId& Id);

	/**
	 * Status Id as Combat applies it from a source at SourceLevel, or nothing if the statuses map has
	 * no such status. Only a damage-over-time status's ticks read the Level.
	 */
	static TOptional<FVeyraStatusSpec> FindStatus(const FVeyraContentId& Id, int32 SourceLevel = 1);

	/** Whether any archetype defines Ability. Other domains check references against it. */
	static bool Defines(const FVeyraContentId& Ability);

	/** Reads and validates the file again, replacing the loaded tuning only when it is valid. */
	VeyraTuning::FErrors Reload();

	bool IsLoaded() const { return Tuning.IsSet(); }

	/** BLAKE3 of Abilities.json's exact bytes, for comparing tuning between builds. */
	const FBlake3Hash& GetDocumentHash() const { return DocumentHash; }

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests only: Get() returns this until it is cleared with nullptr. The caller keeps it alive. */
	static void SetTestOverride(const FVeyraAbilitiesTuning* Override);
#endif

private:
	TOptional<FVeyraAbilitiesTuning> Tuning;
	FBlake3Hash DocumentHash;
};
