// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Kit/VeyraSkillEffectsSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Casting/VeyraCastStateComponent.h"
#include "Casting/VeyraCastTelegraphs.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Cues/VeyraCombatCueSubsystem.h"
#include "Delivery/VeyraLingeringArea.h"
#include "Delivery/VeyraDelayedArea.h"
#include "Delivery/VeyraProjectile.h"
#include "Greybox/VeyraVanguardAnimInstance.h"
#include "Greybox/VeyraGreyboxSettings.h"
#include "Greybox/VeyraGreyboxSubsystem.h"
#include "Greybox/VeyraTelegraphFill.h"
#include "Kit/VeyraKitPresentationSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Tuning/VeyraAbilitiesTuningSubsystem.h"
#include "VeyraPlayerState.h"
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

	/** The first index a lane's areas take beside its caster's windup zones, so the two never share an effect. */
	constexpr int32 LaneAreaIndex = 1000;

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

FVector VeyraSkillEffects::CommitPlacement(const FVector& From, const FVector& Aimed, bool bAtTarget)
{
	// The aimed point is on the ground where it was aimed (FVeyraCastCommit::Location), whatever the caster's height.
	return bAtTarget ? Aimed : From;
}

double VeyraSkillEffects::RetractSpeedOf(const FVeyraAbilitiesTuning& Tuning, const FVeyraContentId& Ability)
{
	const FVeyraSkillshotAbilityTuning* Skillshot = Tuning.Skillshot.Find(Ability);
	if (!Skillshot)
	{
		return 0.0;
	}
	for (const FVeyraDisplacementTuning& Displacement : Skillshot->Effects.Displacement)
	{
		if (Displacement.Direction == EVeyraDisplacementDirection::TowardOrigin && Displacement.Speed > 0.0)
		{
			return Displacement.Speed;
		}
	}
	return Skillshot->Projectile.Speed;
}

FVector VeyraSkillEffects::ArcPoint(const FVector& From, const FVector& To, double Share, double Height)
{
	const double Along = FMath::Clamp(Share, 0.0, 1.0);
	// A parabola through both ends, highest (by Height) halfway.
	return FMath::Lerp(From, To, Along) + FVector(0.0, 0.0, 4.0 * Height * Along * (1.0 - Along));
}

FVector VeyraSkillEffects::RetractPoint(const FVector& From, const FVector& To, double Seconds, double Speed, bool& bOutHome)
{
	const double Span = FVector::Dist(From, To);
	const double Covered = FMath::Max(0.0, Seconds) * FMath::Max(0.0, Speed);
	bOutHome = Covered >= Span;
	return bOutHome || Span <= UE_KINDA_SMALL_NUMBER ? To : FMath::Lerp(From, To, Covered / Span);
}

bool VeyraSkillEffects::SawItEnd(const FVeyraSeenProjectile& Seen, const FVector& EndedAt, double Now, double WindowSeconds)
{
	return Now - Seen.SeenAt <= WindowSeconds && FVector::Dist2D(Seen.At, EndedAt) <= Seen.Speed * WindowSeconds;
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
		// Basic attacks in flight (ADR-072 §9): the shared look, and each Vanguard's own.
		TArray<TPair<FName, const FVeyraSkillEffectStage*>> Attacks;
		if (Settings.AttackTravel.IsSet())
		{
			Attacks.Emplace(NAME_None, &Settings.AttackTravel);
		}
		for (const FVeyraVanguardAttack& Attack : Settings.VanguardAttacks)
		{
			Attacks.Emplace(Attack.Vanguard, &Attack.Travel);
		}
		for (const TPair<FName, const FVeyraSkillEffectStage*>& Attack : Attacks)
		{
			UNiagaraSystem* System = Attack.Value->Effect.LoadSynchronous();
			if (!System)
			{
				Problems.Add(FString::Printf(TEXT("Attacks: %s's effect in flight %s does not load; run BuildEffects.ps1."),
					Attack.Key.IsNone() ? TEXT("AttackTravel") : *Attack.Key.ToString(), *Attack.Value->Effect.ToString()));
			}
			Systems.Add(SkillStageKey(Attack.Key, TEXT("Attack")), System);
		}
		for (const FVeyraAbilityEffects& Effects : Settings.AbilityEffects)
		{
			// A windup, channel or trail runs until its stage ends it; a commit or an impact is one burst that ends on its own,
			// as a pooled component releasing itself needs (the settings' contract: a continuous emitter for the one, a burst
			// for the other; Niagara's own looping flag does not tell a continuous emitter apart, so it is not checked).
			const TPair<const FVeyraSkillEffectStage*, const TCHAR*> Stages[] = { { &Effects.Windup, TEXT("Windup") }, { &Effects.Channel, TEXT("Channel") },
				{ &Effects.Commit, TEXT("Commit") }, { &Effects.Travel, TEXT("Travel") }, { &Effects.Impact, TEXT("Impact") }, { &Effects.Tether, TEXT("Tether") },
				{ &Effects.Mark, TEXT("Mark") }, { &Effects.Arc, TEXT("Arc") }, { &Effects.Linger, TEXT("Linger") }, { &Effects.Area, TEXT("Area") } };
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
	for (const FLanding& Landing : Landings)
	{
		FadeSkillEffect(Landing.Mark);
		FadeSkillEffect(Landing.Arc);
	}
	Landings.Reset();
	Seen.Reset();
	for (const TPair<TPair<FName, int32>, FTether>& Tether : Tethers)
	{
		FadeSkillEffect(Tether.Value.Chain);
	}
	Tethers.Reset();
	for (const TPair<TWeakObjectPtr<const AVeyraLingeringArea>, TWeakObjectPtr<UNiagaraComponent>>& Area : Lingering)
	{
		FadeSkillEffect(Area.Value);
	}
	Lingering.Reset();
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
	if (Cue.Kind == EVeyraCombatCueKind::ProjectileEnd)
	{
		NoteProjectileEnd(Cue);
		return;
	}
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
		// Its mark shows there until then, and what it throws flies there from the caster's hand.
		const double Delay = VeyraSkillEffects::LandingDelayOf(UVeyraAbilitiesTuningSubsystem::Get(), Cue.Ability);
		const UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
		if ((Effects->Impact.IsSet() || Effects->Mark.IsSet() || Effects->Arc.IsSet()) && Delay > 0.0 && Greybox)
		{
			FLanding& Landing = Landings.Add_GetRef({ Cue.Ability, Cue.CastId, Cue.Location, Greybox->GetServerNow() + Delay, false, Greybox->SideColorOf(*Caster) });
			if (UNiagaraSystem* Mark = Effects->Mark.IsSet() ? Loaded(*Effects, TEXT("Mark")) : nullptr)
			{
				// Across the area it marks, as Abilities.json shapes it.
				UNiagaraComponent* Marked = PlayAt(*Mark, Cue.Location, FRotator::ZeroRotator, Effects->Mark.Scale, Landing.Color);
				if (Marked)
				{
					Marked->SetVariableFloat(GetDefault<UVeyraKitPresentationSettings>()->LengthParameter,
						static_cast<float>(VeyraSkillEffects::ReachOf(UVeyraAbilitiesTuningSubsystem::Get(), Cue.Ability) * 2.0));
				}
				Landing.Mark = Marked;
			}
			if (UNiagaraSystem* Arc = Effects->Arc.IsSet() ? Loaded(*Effects, TEXT("Arc")) : nullptr)
			{
				const USkeletalMeshComponent* Skin = Greybox->FindSkin(*Caster);
				Landing.ArcFrom = Skin && Skin->GetBoneIndex(Effects->ArcBone) != INDEX_NONE ? Skin->GetBoneLocation(Effects->ArcBone) : Caster->GetActorLocation();
				Landing.ArcSince = Greybox->GetServerNow();
				Landing.Arc = PlayAt(*Arc, Landing.ArcFrom, FRotator::ZeroRotator, Effects->Arc.Scale, Landing.Color);
			}
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
	// From the hands it pours from, as they are drawn this frame; from its middle on a body without them.
	const USkeletalMeshComponent* Skin = Greybox->FindSkin(Caster);
	const bool bBone = Skin && !Effects->ChannelBone.IsNone() && Skin->GetBoneIndex(Effects->ChannelBone) != INDEX_NONE;
	const FVector From = bBone ? Skin->GetBoneLocation(Effects->ChannelBone) : Caster.GetActorLocation();
	const FRotator Facing = (Direction.IsNearlyZero() ? Caster.GetActorForwardVector() : Direction.GetSafeNormal2D()).Rotation();
	UNiagaraComponent* Channel = Shown->Channel.Get();
	if (!Channel)
	{
		Channel = PlayAt(*System, From, Facing, Effects->Channel.Scale, Greybox->SideColorOf(Caster));
		if (Channel)
		{
			Channel->SetVariableFloat(GetDefault<UVeyraKitPresentationSettings>()->LengthParameter,
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

void UVeyraSkillEffectsSubsystem::SeedHeldCasts()
{
	// A caster first seen mid-cast (out of the fog, or as this machine joins) raised no cue for it: its stages take up
	// what it is seen doing, as its body's clip does.
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn& Caster = **It;
		const UVeyraCastStateComponent* Casts = Caster.IsHidden() || Showing.Contains(&Caster) ? nullptr : SkillCasterState(Caster);
		const FVeyraCastState* State = Casts ? &Casts->GetState() : nullptr;
		const FVeyraAbilityEffects* Effects = State && SkillCastHolds(State->Phase) ? EffectsOf(State->Ability) : nullptr;
		if (!Effects)
		{
			continue;
		}
		if (State->Phase == EVeyraCastPhase::Windup && Effects->Windup.IsSet())
		{
			BeginWindup(Caster, State->Ability);
		}
		else if (State->Phase == EVeyraCastPhase::Channel && Effects->Channel.IsSet())
		{
			Showing.Add(&Caster).Ability = State->Ability;
		}
	}
}

void UVeyraSkillEffectsSubsystem::NoteProjectileDrawn(const AVeyraProjectile& Projectile, const FVector& At, const FLinearColor& Color)
{
	const FVeyraAbilityEffects* Effects = EffectsOf(Projectile.GetAbility());
	const UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	if (!Effects || !Greybox)
	{
		return;
	}
	const double Now = Greybox->GetServerNow();
	const TPair<FName, int32> Key{ SkillAbilityName(Projectile.GetAbility()), Projectile.GetCastId() };
	if (Effects->Impact.IsSet())
	{
		Seen.Add(Key, FVeyraSeenProjectile{ Now, At, Projectile.GetSpeed(), Color });
	}
	if (Effects->TetherBone.IsNone())
	{
		return;
	}
	FTether* Tether = Tethers.Find(Key);
	if (!Tether)
	{
		const AActor* Caster = CasterOf(Projectile);
		if (!Caster)
		{
			return;
		}
		Tether = &Tethers.Add(Key);
		Tether->Caster = Caster;
		Tether->Bone = Effects->TetherBone;
		Tether->FromBone = Effects->TetherFromBone;
		Tether->ReturnSpeed = VeyraSkillEffects::RetractSpeedOf(UVeyraAbilitiesTuningSubsystem::Get(), Projectile.GetAbility());
		Tether->Color = Color;
		if (UNiagaraSystem* Chain = Effects->Tether.IsSet() ? Loaded(*Effects, TEXT("Tether")) : nullptr)
		{
			Tether->Chain = PlayAt(*Chain, At, FRotator::ZeroRotator, Effects->Tether.Scale, Color);
		}
	}
	if (!Tether->bReturning)
	{
		Tether->At = At;
		Tether->SeenAt = Now;
	}
}

bool UVeyraSkillEffectsSubsystem::HidesProjectileBody(const AVeyraProjectile& Projectile) const
{
	// A basic attack hides its body when an effect flies in its place.
	if (!Projectile.GetAbility().IsValid())
	{
		float Scale = 0.0f;
		return ProjectileTravelOf(Projectile, Scale) != nullptr;
	}
	// A hook drawn on its chain only from a caster seen here; one whose caster is not shows its own body.
	const FVeyraAbilityEffects* Effects = EffectsOf(Projectile.GetAbility());
	return Effects && Effects->bHideProjectileBody && (Effects->TetherBone.IsNone() || CasterOf(Projectile));
}

TArray<double> UVeyraSkillEffectsSubsystem::GetLandingTimes() const
{
	TArray<double> Times;
	for (const FLanding& Landing : Landings)
	{
		Times.Add(Landing.At);
	}
	return Times;
}

TOptional<FVector> UVeyraSkillEffectsSubsystem::FindTether(const AActor& Caster) const
{
	for (const TPair<TPair<FName, int32>, FTether>& Tether : Tethers)
	{
		if (Tether.Value.Caster.Get() == &Caster)
		{
			return Tether.Value.At;
		}
	}
	return {};
}

const AActor* UVeyraSkillEffectsSubsystem::CasterOf(const AVeyraProjectile& Projectile) const
{
	// Who launched it, as the server named it; none while its caster is not seen here.
	return Projectile.GetInstigator();
}

void UVeyraSkillEffectsSubsystem::NoteProjectileEnd(const FVeyraCombatCue& Cue)
{
	const FVeyraAbilityEffects* Effects = EffectsOf(Cue.Ability);
	const UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	const TPair<FName, int32> Key{ SkillAbilityName(Cue.Ability), Cue.CastId };
	const FVeyraSeenProjectile* Drawn = Seen.Find(Key);
	UNiagaraSystem* System = Effects && Effects->Impact.IsSet() ? Loaded(*Effects, TEXT("Impact")) : nullptr;
	if (Drawn && System && Greybox
		&& VeyraSkillEffects::SawItEnd(*Drawn, Cue.Location, Greybox->GetServerNow(), GetDefault<UVeyraKitPresentationSettings>()->ProjectileEndWindowSeconds))
	{
		PlayAt(*System, Cue.Location, FRotator::ZeroRotator, Effects->Impact.Scale, Drawn->Color);
	}
	Seen.Remove(Key);
	// Its tether comes home from where the server ended its flight.
	if (FTether* Tether = Tethers.Find(Key); Tether && !Tether->bReturning && Greybox)
	{
		Tether->bReturning = true;
		Tether->ReturnFrom = Cue.Location;
		Tether->ReturnSince = Greybox->GetServerNow();
	}
}

void UVeyraSkillEffectsSubsystem::RefreshTethers(double Now)
{
	const UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	const UVeyraKitPresentationSettings& Settings = *GetDefault<UVeyraKitPresentationSettings>();
	for (auto It = Tethers.CreateIterator(); It; ++It)
	{
		FTether& Tether = It.Value();
		const AActor* Caster = Tether.Caster.Get();
		USkeletalMeshComponent* Skin = Caster && Greybox ? Greybox->FindSkin(*Caster) : nullptr;
		UVeyraVanguardAnimInstance* Animation = Skin ? Cast<UVeyraVanguardAnimInstance>(Skin->GetAnimInstance()) : nullptr;
		if (!Animation || Skin->GetBoneIndex(Tether.FromBone) == INDEX_NONE)
		{
			FadeSkillEffect(Tether.Chain);
			It.RemoveCurrent();
			continue;
		}
		// A flight drawn no more (it left sight with no end come) comes home from where it was last drawn.
		if (!Tether.bReturning && Now - Tether.SeenAt > Settings.ProjectileEndWindowSeconds)
		{
			Tether.bReturning = true;
			Tether.ReturnFrom = Tether.At;
			Tether.ReturnSince = Now;
		}
		const FVector Hand = Skin->GetBoneLocation(Tether.FromBone);
		bool bHome = false;
		if (Tether.bReturning)
		{
			Tether.At = VeyraSkillEffects::RetractPoint(Tether.ReturnFrom, Hand, Now - Tether.ReturnSince, Tether.ReturnSpeed, bHome);
		}
		if (bHome)
		{
			Animation->ClearBoneTarget(Tether.Bone);
			FadeSkillEffect(Tether.Chain);
			It.RemoveCurrent();
			continue;
		}
		Animation->SetBoneTarget(Tether.Bone, Tether.At);
		if (UNiagaraComponent* Chain = Tether.Chain.Get())
		{
			// The chain runs from the hand to the hook, along its length.
			const FVector Span = Tether.At - Hand;
			Chain->SetWorldLocationAndRotation(Hand, Span.IsNearlyZero() ? FRotator::ZeroRotator : Span.Rotation());
			Chain->SetVariableFloat(Settings.LengthParameter, static_cast<float>(Span.Size()));
		}
	}
}

void UVeyraSkillEffectsSubsystem::RefreshLandings(double Now)
{
	for (int32 Index = Landings.Num() - 1; Index >= 0; --Index)
	{
		FLanding& Landing = Landings[Index];
		const FVeyraAbilityEffects* Effects = EffectsOf(Landing.Ability);
		// It lands when its area resolves, by the server's time, once that area is seen here.
		if (!Landing.bServerTime)
		{
			const AVeyraDelayedArea* Nearest = nullptr;
			for (TActorIterator<AVeyraDelayedArea> Area(GetWorld()); Area; ++Area)
			{
				if (Area->GetAbility() == Landing.Ability && Area->GetCastId() == Landing.CastId
					&& (!Nearest || FVector::DistSquared(Area->GetActorLocation(), Landing.Where) < FVector::DistSquared(Nearest->GetActorLocation(), Landing.Where)))
				{
					Nearest = *Area;
				}
			}
			if (Nearest)
			{
				Landing.At = Nearest->GetResolvesAt();
				Landing.bServerTime = true;
			}
		}
		if (Now < Landing.At)
		{
			// What it throws flies on toward where it lands.
			if (UNiagaraComponent* Arc = Landing.Arc.Get(); Arc && Effects)
			{
				const double Share = (Now - Landing.ArcSince) / FMath::Max(Landing.At - Landing.ArcSince, UE_KINDA_SMALL_NUMBER);
				Arc->SetWorldLocation(VeyraSkillEffects::ArcPoint(Landing.ArcFrom, Landing.Where, Share, Effects->ArcHeight));
			}
			continue;
		}
		FadeSkillEffect(Landing.Mark);
		FadeSkillEffect(Landing.Arc);
		if (UNiagaraSystem* System = Effects && Effects->Impact.IsSet() ? Loaded(*Effects, TEXT("Impact")) : nullptr)
		{
			PlayAt(*System, Landing.Where, FRotator::ZeroRotator, Effects->Impact.Scale, Landing.Color);
		}
		Landings.RemoveAtSwap(Index);
	}
}

void UVeyraSkillEffectsSubsystem::RefreshLingering(double Now)
{
	const UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	const UVeyraKitPresentationSettings& Settings = *GetDefault<UVeyraKitPresentationSettings>();
	for (TActorIterator<AVeyraLingeringArea> It(GetWorld()); It; ++It)
	{
		const AVeyraLingeringArea& Area = **It;
		const FVeyraAbilityEffects* Effects = Greybox && !Lingering.Contains(&Area) && !Area.IsHidden() && Now < Area.GetEndsAt() ? EffectsOf(Area.GetAbility()) : nullptr;
		UNiagaraSystem* System = Effects && Effects->Linger.IsSet() ? Loaded(*Effects, TEXT("Linger")) : nullptr;
		if (!System)
		{
			continue;
		}
		// Over its shape, turned its way: a rectangle's length and width, or a circle's or sector's breadth both ways.
		const FVeyraPlacedShape Placed = Area.GetPlacedShape();
		const bool bRectangle = Placed.Shape.Kind == EVeyraShapeKind::Rectangle;
		UNiagaraComponent* Poured = PlayAt(*System, Placed.Origin, Placed.Direction.Rotation(), Effects->Linger.Scale, Greybox->ColorOfSide(Area.GetVeyraTeam()));
		if (Poured)
		{
			Poured->SetVariableFloat(Settings.LengthParameter, static_cast<float>(bRectangle ? Placed.Shape.Length : Placed.Shape.Radius * 2.0));
			Poured->SetVariableFloat(Settings.WidthParameter, static_cast<float>(bRectangle ? Placed.Shape.Width : Placed.Shape.Radius * 2.0));
			// And its true shape whole, so an Area effect can pour over it too (ADR-075 §3).
			const FVeyraAreaParameters Drawn = VeyraSkillEffects::AreaParametersOf(Placed.Shape, 0.0);
			if (!Settings.ShapeParameter.IsNone() && !Settings.RadiusParameter.IsNone() && !Settings.HalfArcParameter.IsNone())
			{
				Poured->SetVariableFloat(Settings.ShapeParameter, static_cast<float>(Drawn.Shape));
				Poured->SetVariableFloat(Settings.RadiusParameter, static_cast<float>(Drawn.Radius));
				Poured->SetVariableFloat(Settings.HalfArcParameter, static_cast<float>(Drawn.HalfArc));
			}
		}
		Lingering.Add(&Area, Poured);
	}
	// Each ends with its area, or as its time runs out.
	for (auto It = Lingering.CreateIterator(); It; ++It)
	{
		const AVeyraLingeringArea* Area = It.Key().Get();
		if (!Area || Now >= Area->GetEndsAt())
		{
			FadeSkillEffect(It.Value());
			It.RemoveCurrent();
		}
	}
}

void UVeyraSkillEffectsSubsystem::Refresh()
{
	if (!bReady)
	{
		return;
	}
	SeedHeldCasts();
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
	// A drawn projectile whose end has not come within its window has left sight, or ended where this machine did not see.
	const double Window = GetDefault<UVeyraKitPresentationSettings>()->ProjectileEndWindowSeconds;
	for (auto It = Seen.CreateIterator(); It; ++It)
	{
		if (Now - It.Value().SeenAt > Window)
		{
			It.RemoveCurrent();
		}
	}
	RefreshLandings(Now);
	RefreshTethers(Now);
	RefreshLingering(Now);
	RefreshAreas(Now);
}

void UVeyraSkillEffectsSubsystem::RefreshAreas(double Now)
{
	const UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	if (!Greybox)
	{
		return;
	}
	for (TPair<TPair<TWeakObjectPtr<const AActor>, int32>, FAreaShown>& Shown : Areas)
	{
		Shown.Value.bWanted = false;
	}
	// Each cast winding up or channelling: the areas it lands in, as its telegraph would show them (one view, from
	// Abilities.json), from where its caster stands now.
	const FVeyraAbilitiesTuning& Tuning = UVeyraAbilitiesTuningSubsystem::Get();
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn& Caster = **It;
		const UVeyraCastStateComponent* Casts = SkillCasterState(Caster);
		const FVeyraCastState* State = Casts ? &Casts->GetState() : nullptr;
		const FVeyraAbilityEffects* Effects = State && SkillCastHolds(State->Phase) && !Caster.IsHidden() ? EffectsOf(State->Ability) : nullptr;
		if (!Effects || !Effects->Area.IsSet())
		{
			continue;
		}
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Caster.GetSimpleCollisionCylinder(Radius, HalfHeight);
		const TArray<FVeyraPlacedShape> Placed = VeyraCastTelegraphs::ForCast(Tuning, *State, Caster.GetActorLocation(), Radius);
		const EVeyraTelegraphSource Source = State->Phase == EVeyraCastPhase::Channel ? EVeyraTelegraphSource::Channel : EVeyraTelegraphSource::Windup;
		for (int32 Index = 0; Index < Placed.Num(); ++Index)
		{
			ShowArea(Caster, Index, *Effects, Placed[Index], FMath::Max(0.0, State->PhaseEndsAt - Now), Source, Greybox->SideColorOf(Caster));
		}
	}
	// Each volley lane held open, where its shots can go, until it closes (ADR-075 §5). Its areas follow a windup's.
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn& Caster = **It;
		const UVeyraCastStateComponent* Casts = SkillCasterState(Caster);
		const FVeyraVolleyLaneState* Lane = Casts ? &Casts->GetLane() : nullptr;
		const FVeyraAbilityEffects* Effects = Lane && Lane->Ability.IsValid() && Now < Lane->ClosesAt && !Caster.IsHidden() ? EffectsOf(Lane->Ability) : nullptr;
		if (!Effects || !Effects->Area.IsSet())
		{
			continue;
		}
		const TArray<FVeyraPlacedShape> Placed = VeyraCastTelegraphs::ForLane(Tuning, *Lane, Caster.GetActorLocation());
		for (int32 Index = 0; Index < Placed.Num(); ++Index)
		{
			ShowArea(Caster, LaneAreaIndex + Index, *Effects, Placed[Index], Lane->ClosesAt - Now, EVeyraTelegraphSource::VolleyLane, Greybox->SideColorOf(Caster));
		}
	}
	// Each delayed area, until it lands.
	for (TActorIterator<AVeyraDelayedArea> It(GetWorld()); It; ++It)
	{
		const AVeyraDelayedArea& Area = **It;
		const FVeyraAbilityEffects* Effects = !Area.IsHidden() && Now < Area.GetResolvesAt() ? EffectsOf(Area.GetAbility()) : nullptr;
		if (!Effects || !Effects->Area.IsSet())
		{
			continue;
		}
		const TArray<FVeyraShape>& Shapes = Area.GetShapes();
		for (int32 Index = 0; Index < Shapes.Num(); ++Index)
		{
			ShowArea(Area, Index, *Effects, FVeyraPlacedShape{ Shapes[Index], Area.GetActorLocation(), Area.GetDirection() }, Area.GetResolvesAt() - Now,
				EVeyraTelegraphSource::DelayedArea, Greybox->ColorOfSide(Area.GetVeyraTeam()));
		}
	}
	// What no longer lands there fades.
	for (auto It = Areas.CreateIterator(); It; ++It)
	{
		if (!It.Value().bWanted)
		{
			FadeSkillEffect(It.Value().Effect);
			It.RemoveCurrent();
		}
	}
}

void UVeyraSkillEffectsSubsystem::ShowArea(const AActor& Owner, int32 Index, const FVeyraAbilityEffects& Effects, const FVeyraPlacedShape& Placed,
	double Remaining, EVeyraTelegraphSource Source, const FLinearColor& Color)
{
	const UVeyraGreyboxSubsystem* Greybox = GetWorld()->GetSubsystem<UVeyraGreyboxSubsystem>();
	FAreaShown& Shown = Areas.FindOrAdd({ &Owner, Index });
	Shown.bWanted = true;
	// A different recipe landing where another was drawn (a second cast seen without the idle between) starts its own.
	if (Shown.Parameters.Recipe != Effects.Ability || Shown.Parameters.Scale != Effects.Area.Scale)
	{
		FadeSkillEffect(Shown.Effect);
		Shown.Effect.Reset();
	}
	Shown.Parameters = VeyraSkillEffects::AreaParametersOf(Placed.Shape,
		VeyraTelegraphFill::LandingOf(Source, Remaining, GetDefault<UVeyraGreyboxSettings>()->TelegraphLandingSeconds));
	Shown.Parameters.Recipe = Effects.Ability;
	Shown.Parameters.Scale = Effects.Area.Scale;
	// Drawn on the plane that best fits the ground under the whole footprint, not level at its origin's height.
	const FVeyraGroundFit Fit = VeyraTelegraphFill::FitGround(Placed, [Greybox, &Placed](const FVector2D& Point) {
		return Greybox ? Greybox->GroundUnder(FVector(Point, Placed.Origin.Z)).Z : Placed.Origin.Z;
	});
	Shown.Parameters.Origin = Fit.Origin;
	Shown.Parameters.Normal = Fit.Normal;
	UNiagaraComponent* Effect = Shown.Effect.Get();
	const FVector Ground = Fit.Origin;
	const FVector Along = Placed.Direction.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Placed.Direction.GetSafeNormal2D();
	const FRotator Facing = FRotationMatrix::MakeFromXZ(Along, Fit.Normal).Rotator();
	if (!Effect)
	{
		UNiagaraSystem* System = Loaded(Effects, TEXT("Area"));
		Effect = System ? PlayAt(*System, Ground, Facing, Effects.Area.Scale, Color) : nullptr;
		Shown.Effect = Effect;
		if (!Effect)
		{
			return;
		}
	}
	// It follows a caster who turns or steps while it winds up, at the area's true shape.
	Effect->SetWorldLocationAndRotation(Ground, Facing);
	const UVeyraKitPresentationSettings& Settings = *GetDefault<UVeyraKitPresentationSettings>();
	const FVeyraAreaParameters& Drawn = Shown.Parameters;
	Effect->SetVariableFloat(Settings.ShapeParameter, static_cast<float>(Drawn.Shape));
	Effect->SetVariableFloat(Settings.RadiusParameter, static_cast<float>(Drawn.Radius));
	Effect->SetVariableFloat(Settings.HalfArcParameter, static_cast<float>(Drawn.HalfArc));
	Effect->SetVariableFloat(Settings.LengthParameter, static_cast<float>(Drawn.Length));
	Effect->SetVariableFloat(Settings.WidthParameter, static_cast<float>(Drawn.Width));
	Effect->SetVariableFloat(Settings.LandingParameter, static_cast<float>(Drawn.Landing));
}

FVeyraAreaParameters VeyraSkillEffects::AreaParametersOf(const FVeyraShape& Shape, double Landing)
{
	FVeyraAreaParameters Parameters;
	Parameters.Shape = Shape.Kind == EVeyraShapeKind::Sector ? 1 : Shape.Kind == EVeyraShapeKind::Rectangle ? 2 : 0;
	Parameters.Radius = Shape.Radius;
	Parameters.HalfArc = Parameters.Shape == 1 ? Shape.ArcDegrees / 2.0 : 180.0;
	Parameters.Length = Parameters.Shape == 2 ? Shape.Length : Shape.Radius * 2.0;
	Parameters.Width = Parameters.Shape == 2 ? Shape.Width : Shape.Radius * 2.0;
	Parameters.Landing = Landing;
	return Parameters;
}

TArray<FVeyraAreaParameters> UVeyraSkillEffectsSubsystem::FindAreas(const AActor& Owner) const
{
	TArray<TPair<int32, FVeyraAreaParameters>> Found;
	for (const TPair<TPair<TWeakObjectPtr<const AActor>, int32>, FAreaShown>& Shown : Areas)
	{
		if (Shown.Key.Key.Get() == &Owner)
		{
			Found.Emplace(Shown.Key.Value, Shown.Value.Parameters);
		}
	}
	Found.Sort([](const TPair<int32, FVeyraAreaParameters>& A, const TPair<int32, FVeyraAreaParameters>& B) { return A.Key < B.Key; });
	TArray<FVeyraAreaParameters> Drawn;
	for (const TPair<int32, FVeyraAreaParameters>& Each : Found)
	{
		Drawn.Add(Each.Value);
	}
	return Drawn;
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

UNiagaraSystem* UVeyraSkillEffectsSubsystem::ProjectileTravelOf(const AVeyraProjectile& Projectile, float& OutScale) const
{
	if (Projectile.GetAbility().IsValid())
	{
		return TravelEffectOf(Projectile.GetAbility(), OutScale);
	}
	// A basic attack's: its Vanguard's own, found through who launched it, or the shared one. Another of no ability (an
	// ability's return) keeps its own body.
	if (!Projectile.IsBasicAttack())
	{
		return nullptr;
	}
	const UVeyraKitPresentationSettings& Settings = *GetDefault<UVeyraKitPresentationSettings>();
	const APawn* Attacker = Projectile.GetInstigator();
	const AVeyraPlayerState* Participant = Attacker ? Attacker->GetPlayerState<AVeyraPlayerState>() : nullptr;
	const FName Vanguard = Participant ? FName(*Participant->GetVanguardId().ToString()) : NAME_None;
	if (const FVeyraVanguardAttack* Own = Vanguard.IsNone() ? nullptr : Settings.VanguardAttackOf(Vanguard))
	{
		OutScale = Own->Travel.Scale;
		return Systems.FindRef(SkillStageKey(Own->Vanguard, TEXT("Attack")));
	}
	if (!Settings.AttackTravel.IsSet())
	{
		return nullptr;
	}
	OutScale = Settings.AttackTravel.Scale;
	return Systems.FindRef(SkillStageKey(NAME_None, TEXT("Attack")));
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
