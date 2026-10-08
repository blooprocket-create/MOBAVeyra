// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Kit/VeyraSkillEffectsSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Cues/VeyraCombatCueSubsystem.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Kit/VeyraKitPresentationSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraUILog.h"

namespace
{
	FName SkillAbilityName(const FVeyraContentId& Id)
	{
		return FName(*Id.ToString());
	}

	/** The key a stage's system is loaded under. */
	FName SkillStageKey(FName Ability, const TCHAR* Stage)
	{
		return FName(*FString::Printf(TEXT("%s.%s"), *Ability.ToString().ToLower(), Stage));
	}

	/** Unit's cast state, beside its Ability System Component: on a Vanguard's participant, or on the unit itself. */
	const UVeyraCastStateComponent* SkillCasterState(const AActor& Unit)
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Unit);
		const AActor* Owner = AbilitySystem ? AbilitySystem->GetOwner() : nullptr;
		return Owner ? Owner->FindComponentByClass<UVeyraCastStateComponent>() : nullptr;
	}

	/** Whether a cast in Phase holds its caster: winding up or channelling. */
	bool SkillCastHolds(EVeyraCastPhase Phase)
	{
		return Phase == EVeyraCastPhase::Windup || Phase == EVeyraCastPhase::Channel;
	}

	/** Fades Component out: it stops emitting and lets what it emitted finish, then releases itself. */
	void FadeSkillEffect(const TWeakObjectPtr<UNiagaraComponent>& Component)
	{
		if (UNiagaraComponent* Effect = Component.Get())
		{
			Effect->Deactivate();
		}
	}
}

double VeyraSkillEffects::ReachOf(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability)
{
	if (const FVeyraAreaAbilityTuning* Area = Tuning.Area.Find(Ability); Area && !Area->Zones.IsEmpty())
	{
		// Innermost first, so the last zone is the reach.
		const FVeyraShape& Shape = Area->Zones.Last().Shape;
		return Shape.Kind == EVeyraShapeKind::Rectangle ? Shape.Length : Shape.Radius;
	}
	if (const FVeyraSkillshotAbilityTuning* Skillshot = Tuning.Skillshot.Find(Ability))
	{
		return Skillshot->Projectile.Range;
	}
	return 0.0;
}

double VeyraSkillEffects::LandingDelayOf(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability)
{
	const FVeyraAreaAbilityTuning* Area = Tuning.Area.Find(Ability);
	return Area ? FMath::Max(0.0, Area->DelaySeconds) : 0.0;
}

bool UVeyraSkillEffectsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Presentation is for worlds someone watches: game and play-in-editor worlds, never a dedicated server's.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

void UVeyraSkillEffectsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Side colours, bodies and the server's clock come from the grey-box presentation; the moments from the cues.
	Collection.InitializeDependency<UVeyraGreyboxSubsystem>();
	UVeyraCombatCueSubsystem* Cues = Collection.InitializeDependency<UVeyraCombatCueSubsystem>();
	const UVeyraKitPresentationSettings& Settings = *GetDefault<UVeyraKitPresentationSettings>();
	TArray<FString> Problems = Settings.Validate();
	if (Problems.IsEmpty())
	{
		for (const FVeyraAbilityEffects& Effects : Settings.AbilityEffects)
		{
			// A windup, channel or trail runs until its stage ends it; a commit or an impact is one burst that ends on its own,
			// as a pooled component releasing itself needs (the settings' contract: a continuous emitter for the one, a burst
			// for the other; Niagara's own looping flag does not tell a continuous emitter apart, so it is not checked).
			const TPair<const FVeyraSkillEffectStage*, const TCHAR*> Stages[] = { { &Effects.Windup, TEXT("Windup") }, { &Effects.Channel, TEXT("Channel") },
				{ &Effects.Commit, TEXT("Commit") }, { &Effects.Travel, TEXT("Travel") }, { &Effects.Impact, TEXT("Impact") } };
			for (const TPair<const FVeyraSkillEffectStage*, const TCHAR*>& Stage : Stages)
			{
				if (!Stage.Key->IsSet())
				{
					continue;
				}
				UNiagaraSystem* System = Stage.Key->Effect.LoadSynchronous();
				if (!System)
				{
					Problems.Add(FString::Printf(TEXT("AbilityEffects: %s's %s effect %s does not load; run BuildEffects.ps1."), *Effects.Ability.ToString(),
						Stage.Value, *Stage.Key->Effect.ToString()));
				}
				Systems.Add(SkillStageKey(Effects.Ability, Stage.Value), System);
			}
		}
	}
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogVeyraUI, Error, TEXT("Veyra skill effects: %s"), *Problem);
	}
	bReady = Problems.IsEmpty();
	if (Cues)
	{
		CueHandle = Cues->OnCue.AddUObject(this, &UVeyraSkillEffectsSubsystem::NoteCue);
	}
}

void UVeyraSkillEffectsSubsystem::Deinitialize()
{
	if (UVeyraCombatCueSubsystem* Cues = GetWorld()->GetSubsystem<UVeyraCombatCueSubsystem>())
	{
		Cues->OnCue.Remove(CueHandle);
	}
	for (TPair<TWeakObjectPtr<const AActor>, FShowing>& Shown : Showing)
	{
		FadeOut(Shown.Value);
	}
	Showing.Reset();
	Landings.Reset();
	Super::Deinitialize();
}

void UVeyraSkillEffectsSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Refresh();
}

TStatId UVeyraSkillEffectsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVeyraSkillEffectsSubsystem, STATGROUP_Tickables);
}

const FVeyraAbilityEffects* UVeyraSkillEffectsSubsystem::EffectsOf(const FVeyraContentId& Ability) const
{
	return bReady && Ability.IsValid() ? GetDefault<UVeyraKitPresentationSettings>()->EffectsOf(SkillAbilityName(Ability)) : nullptr;
}

UNiagaraSystem* UVeyraSkillEffectsSubsystem::Loaded(const FVeyraAbilityEffects& Effects, const TCHAR* Stage) const
{
	return Systems.FindRef(SkillStageKey(Effects.Ability, Stage));
}

UNiagaraComponent* UVeyraSkillEffectsSubsystem::PlayAt(UNiagaraSystem& System, const FVector& Where, const FRotator& Facing, float Scale, const FLinearColor& Color)
{
	UNiagaraComponent* Played = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), &System, Where, Facing, FVector::OneVector, /*bAutoDestroy*/ true,
		/*bAutoActivate*/ true, ENCPoolMethod::AutoRelease);
	if (Played)
	{
		const UVeyraGreyboxSettings& Greybox = *GetDefault<UVeyraGreyboxSettings>();
		Played->SetVariableLinearColor(Greybox.EffectColorParameter, Color);
		Played->SetVariableFloat(Greybox.EffectScaleParameter, Scale);
	}
	return Played;
}

void UVeyraSkillEffectsSubsystem::NoteCue(const FVeyraCombatCue& Cue)
{
	const AActor* Caster = Cue.Unit.Get();
	const FVeyraAbilityEffects* Effects = Caster ? EffectsOf(Cue.Ability) : nullptr;
	if (!Effects)
	{
		return;
	}
	if (Cue.Kind == EVeyraCombatCueKind::CastWindup)
	{
		BeginWindup(*Caster, Cue.Ability);
	}
	else if (Cue.Kind == EVeyraCombatCueKind::CastCommit)
	{
		// A channel that follows the commit runs while the cast holds its caster, which the refresh watches.
		if (Effects->Channel.IsSet())
		{
			FShowing& Shown = Showing.FindOrAdd(Caster);
			Shown.Ability = Cue.Ability;
		}
		// A delayed area lands where it was aimed, its delay from Abilities.json; one that lands at once shows its commit.
		const double Delay = VeyraSkillEffects::LandingDelayOf(UVeyraAbilitiesTuningSubsystem::Get(), Cue.Ability);
		const UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
		if (Effects->Impact.IsSet() && Delay > 0.0 && Greybox)
		{
			Landings.Add({ Cue.Ability, Cue.Location, Greybox->GetServerNow() + Delay, Greybox->SideColorOf(*Caster) });
		}
	}
}

void UVeyraSkillEffectsSubsystem::BeginWindup(const AActor& Caster, const FVeyraContentId& Ability)
{
	EndStages(Caster);
	const FVeyraAbilityEffects& Effects = *EffectsOf(Ability);
	FShowing& Shown = Showing.Add(&Caster);
	Shown.Ability = Ability;
	UNiagaraSystem* System = Effects.Windup.IsSet() ? Loaded(Effects, TEXT("Windup")) : nullptr;
	UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	if (!System || !Greybox)
	{
		return;
	}
	// On the body's bones as it moves; a Vanguard without a generated body pours it from its middle.
	USkeletalMeshComponent* Skin = Greybox->FindSkin(Caster);
	USceneComponent* Onto = Skin ? Skin : Caster.GetRootComponent();
	if (!Onto)
	{
		return;
	}
	const UVeyraGreyboxSettings& Settings = *GetDefault<UVeyraGreyboxSettings>();
	const FLinearColor Color = Greybox->SideColorOf(Caster);
	for (const FName Bone : Effects.WindupBones)
	{
		if (Skin && Skin->GetBoneIndex(Bone) == INDEX_NONE)
		{
			continue;
		}
		UNiagaraComponent* Poured = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Onto, Skin ? Bone : NAME_None, FVector::ZeroVector,
			FRotator::ZeroRotator, EAttachLocation::SnapToTarget, /*bAutoDestroy*/ true);
		if (Poured)
		{
			Poured->SetUsingAbsoluteScale(true);
			Poured->SetVariableLinearColor(Settings.EffectColorParameter, Color);
			Poured->SetVariableFloat(Settings.EffectScaleParameter, Effects.Windup.Scale);
			Shown.Windup.Add(Poured);
		}
		if (!Skin)
		{
			break;
		}
	}
}

void UVeyraSkillEffectsSubsystem::RefreshChannel(const AActor& Caster, const FVeyraContentId& Ability, const FVector& Direction)
{
	const FVeyraAbilityEffects* Effects = EffectsOf(Ability);
	UNiagaraSystem* System = Effects && Effects->Channel.IsSet() ? Loaded(*Effects, TEXT("Channel")) : nullptr;
	FShowing* Shown = Showing.Find(&Caster);
	const UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	if (!System || !Shown || !Greybox)
	{
		return;
	}
	// The windup's charge gives way to the channel.
	for (const TWeakObjectPtr<UNiagaraComponent>& Effect : Shown->Windup)
	{
		FadeSkillEffect(Effect);
	}
	Shown->Windup.Reset();
	const FVector From = Caster.GetActorLocation();
	const FRotator Facing = (Direction.IsNearlyZero() ? Caster.GetActorForwardVector() : Direction.GetSafeNormal2D()).Rotation();
	UNiagaraComponent* Channel = Shown->Channel.Get();
	if (!Channel)
	{
		Channel = PlayAt(*System, From, Facing, Effects->Channel.Scale, Greybox->SideColorOf(Caster));
		if (Channel)
		{
			Channel->SetVariableFloat(GetDefault<UVeyraKitPresentationSettings>()->ChannelLengthParameter,
				static_cast<float>(VeyraSkillEffects::ReachOf(UVeyraAbilitiesTuningSubsystem::Get(), Ability)));
		}
		Shown->Channel = Channel;
	}
	if (Channel)
	{
		Channel->SetWorldLocationAndRotation(From, Facing);
	}
}

void UVeyraSkillEffectsSubsystem::FadeOut(FShowing& Shown)
{
	for (const TWeakObjectPtr<UNiagaraComponent>& Effect : Shown.Windup)
	{
		FadeSkillEffect(Effect);
	}
	Shown.Windup.Reset();
	FadeSkillEffect(Shown.Channel);
	Shown.Channel.Reset();
}

void UVeyraSkillEffectsSubsystem::EndStages(const AActor& Caster)
{
	if (FShowing* Shown = Showing.Find(&Caster))
	{
		FadeOut(*Shown);
		Showing.Remove(&Caster);
	}
}

void UVeyraSkillEffectsSubsystem::Refresh()
{
	if (!bReady)
	{
		return;
	}
	// A stage lasts while its cast holds its caster: a commit with nothing after it, a cancel, or a death ends it.
	TArray<TWeakObjectPtr<const AActor>> Ended;
	for (TPair<TWeakObjectPtr<const AActor>, FShowing>& Shown : Showing)
	{
		const AActor* Caster = Shown.Key.Get();
		const UVeyraCastStateComponent* Casts = Caster ? SkillCasterState(*Caster) : nullptr;
		const FVeyraCastState* State = Casts ? &Casts->GetState() : nullptr;
		if (!State || !SkillCastHolds(State->Phase) || !(State->Ability == Shown.Value.Ability))
		{
			Ended.Add(Shown.Key);
			continue;
		}
		if (State->Phase == EVeyraCastPhase::Channel)
		{
			RefreshChannel(*Caster, State->Ability, State->Direction);
		}
	}
	for (const TWeakObjectPtr<const AActor>& Caster : Ended)
	{
		FadeOut(Showing.FindChecked(Caster));
		Showing.Remove(Caster);
	}
	// The delayed areas whose time has come land.
	const UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	const double Now = Greybox ? Greybox->GetServerNow() : 0.0;
	for (int32 Index = Landings.Num() - 1; Index >= 0; --Index)
	{
		const FLanding& Landing = Landings[Index];
		if (Now < Landing.At)
		{
			continue;
		}
		const FVeyraAbilityEffects* Effects = EffectsOf(Landing.Ability);
		if (UNiagaraSystem* System = Effects ? Loaded(*Effects, TEXT("Impact")) : nullptr)
		{
			PlayAt(*System, Landing.Where, FRotator::ZeroRotator, Effects->Impact.Scale, Landing.Color);
		}
		Landings.RemoveAtSwap(Index);
	}
}

UNiagaraSystem* UVeyraSkillEffectsSubsystem::CommitEffectOf(const FVeyraContentId& Ability, float& OutScale, bool& bOutAtTarget) const
{
	const FVeyraAbilityEffects* Effects = EffectsOf(Ability);
	if (!Effects || !Effects->Commit.IsSet())
	{
		return nullptr;
	}
	OutScale = Effects->Commit.Scale;
	bOutAtTarget = Effects->bCommitAtTarget;
	return Loaded(*Effects, TEXT("Commit"));
}

UNiagaraSystem* UVeyraSkillEffectsSubsystem::TravelEffectOf(const FVeyraContentId& Ability, float& OutScale) const
{
	const FVeyraAbilityEffects* Effects = EffectsOf(Ability);
	if (!Effects || !Effects->Travel.IsSet())
	{
		return nullptr;
	}
	OutScale = Effects->Travel.Scale;
	return Loaded(*Effects, TEXT("Travel"));
}

void UVeyraSkillEffectsSubsystem::NoteProjectileEnded(const FVeyraContentId& Ability, const FVector& Where, const FLinearColor& Color)
{
	const FVeyraAbilityEffects* Effects = EffectsOf(Ability);
	if (UNiagaraSystem* System = Effects && Effects->Impact.IsSet() ? Loaded(*Effects, TEXT("Impact")) : nullptr)
	{
		PlayAt(*System, Where, FRotator::ZeroRotator, Effects->Impact.Scale, Color);
	}
}

TArray<UNiagaraComponent*> UVeyraSkillEffectsSubsystem::FindWindupEffects(const AActor& Caster) const
{
	TArray<UNiagaraComponent*> Found;
	if (const FShowing* Shown = Showing.Find(&Caster))
	{
		for (const TWeakObjectPtr<UNiagaraComponent>& Effect : Shown->Windup)
		{
			if (UNiagaraComponent* Live = Effect.Get(); Live && Live->IsActive())
			{
				Found.Add(Live);
			}
		}
	}
	return Found;
}

UNiagaraComponent* UVeyraSkillEffectsSubsystem::FindChannelEffect(const AActor& Caster) const
{
	const FShowing* Shown = Showing.Find(&Caster);
	UNiagaraComponent* Channel = Shown ? Shown->Channel.Get() : nullptr;
	return Channel && Channel->IsActive() ? Channel : nullptr;
}
