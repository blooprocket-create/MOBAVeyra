// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Greybox/VeyraBodyFeedback.h"
#include "Greybox/VeyraOrderMarks.h"
#include "Hud/VeyraCombatTextModel.h"
#include "Shapes/VeyraShapes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Teams/VeyraTeam.h"

#include "Hud/VeyraHudWarnings.h"
#include "Settings/VeyraInterfacePreferences.h"
#include "VeyraGreyboxSubsystem.generated.h"

class AHUD;
class AVeyraProjectile;
class UFont;
class ULineBatchComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** What a telegraph warns of. */
UENUM()
enum class EVeyraTelegraphSource : uint8
{
	/** A cast's windup: where it lands at Commit. */
	Windup,
	/** A channel: where each tick lands. */
	Channel,
	/** A delayed area waiting to hit. */
	DelayedArea,
	/** A lingering area, until it ends (ADR-018 §5). */
	LingeringArea,
	/** A lingering area whose end is near and hits (ADR-026 §4). */
	LingeringAreaEnding,
	/** The local player's indicator: where an ability would land, before it is cast (ADR-041 §2). */
	Indicator,
};

/** A melee attack's swing as this machine draws it: an arc from its attacker toward its target, fading (ADR-063 §2). */
struct FVeyraSwingArc
{
	TWeakObjectPtr<const AActor> Attacker;

	/** Toward the target on the ground, and how far the arc reaches: to the target's far edge. */
	FVector Direction = FVector::ForwardVector;
	double Radius = 0.0;

	/** When it was struck, in this machine's real seconds, and in whose colour. */
	double At = 0.0;
	FLinearColor Color = FLinearColor::Transparent;
};

/** One telegraphed shape, as this machine draws it. */
struct FVeyraTelegraph
{
	FVeyraPlacedShape Placed;
	EVeyraTelegraphSource Source = EVeyraTelegraphSource::Windup;

	/** The side whose cast it is. */
	EVeyraTeam Team = EVeyraTeam::None;

	/** Seconds until it hits or its phase ends, in server gameplay time. */
	double RemainingSeconds = 0.0;
};

/**
 * The grey-box presentation of one client world (ADR-008 §1). Engine shapes show replicated state
 * and decide nothing:
 * - the battleground's lanes, river and bases are drawn on its floor from its layout;
 * - every unit gets a body in its side's colour, tinted while crowd controlled;
 * - projectiles are drawn from their launch data and the server's clock (ADR-009 §4);
 * - casts, channels and delayed areas are telegraphed from the ability tuning (VeyraCastTelegraphs);
 * - bars and the player's panel are drawn over whichever HUD the player has (VeyraGreyboxHud).
 * No gameplay class knows it. It is not created on a dedicated server, which never loads this module.
 */
UCLASS()
class VEYRAUI_API UVeyraGreyboxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** A pause stops the server's clock, not the drawing. */
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Brings the presentation up to date with the world. Its tick calls this; tests call it directly. */
	void Refresh();

	/** The body drawn for Unit, once it has one. */
	UStaticMeshComponent* FindBody(const AActor& Unit) const;

	/** The hit flash's strength over Unit's body now, from 0 to 1 (ADR-063 §2). */
	float GetFlashOf(const AActor& Unit) const;

	/**
	 * Outlines Hovered, and no other unit, in its side's colour (ADR-063 §3): its drawn body and art write their side's
	 * stencil to custom depth, and the outline pass shows while anything is hovered. The refresh calls this with the
	 * unit under the local player's cursor; tests call it directly.
	 */
	void ShowHover(const AActor* Hovered);

	/** The custom-depth stencil Unit's outline is drawn with: its side's as the viewer sees it, enemy, ally or neutral. */
	int32 HoverStencilOf(const AActor& Unit) const;

	/** The effect a cue of Kind plays (ADR-063 §4): a hit's impact, a cast's flash, a death's burst; null for the rest. */
	class UNiagaraSystem* EffectFor(EVeyraCombatCueKind Kind) const;

	/** The sound a cue of Kind plays (ADR-063 §5): a hit's impact, an attack's swing, a cast's, a death's; null for the rest. */
	class USoundBase* SoundFor(EVeyraCombatCueKind Kind) const;

	/** The shops drawn at the fountains, one by each side's team start, once the world has them (ADR-063 §6). */
	TArray<class AVeyraFountainShop*> GetShops() const;

	/**
	 * Takes Under, the shop under the local player's cursor or null, for a player on side Viewer: their own side's is
	 * outlined, and opens the shop when bClicked. Returns whether Under is their own. The refresh calls this with the
	 * viewer's side; tests call it directly.
	 */
	bool HoverShop(class AVeyraFountainShop* Under, EVeyraTeam Viewer, bool bClicked);

	/**
	 * Plays Cue's effect where it happens, in its unit's side colour; a cast's flashes toward its aim. Niagara skips one
	 * no viewer could see, as most of a battleground's hits are, and plays none where nothing renders.
	 */
	class UNiagaraComponent* PlayEffect(const struct FVeyraCombatCue& Cue);

	/** A structure's art, once drawn; null for any other unit. */
	UStaticMeshComponent* FindArt(const AActor& Unit) const;

	/** The sphere drawn for Projectile, once it has one. */
	UStaticMeshComponent* FindProjectileVisual(const AVeyraProjectile& Projectile) const;

	/** The actor holding the battleground's drawn ground, once the world shows the battleground. */
	AActor* GetGround() const { return GroundMarkings.Get(); }

	/** What the last refresh telegraphed. */
	const TArray<FVeyraTelegraph>& GetTelegraphs() const { return Telegraphs; }

	/** The ring the last refresh drew for the local player's last order, while it shows (ADR-062 §6). */
	const TOptional<FVeyraOrderMarkRing>& GetOrderMarkRing() const { return OrderMarkRing; }

	/** The melee swings still showing (ADR-063 §2). */
	const TArray<FVeyraSwingArc>& GetSwingArcs() const { return SwingArcs; }

	/** The server's gameplay time as this machine knows it; it stands still while the match is paused. */
	double GetServerNow() const;

	/**
	 * The HUD's font: the engine's Roboto family, every weight of it, as a font asset, which canvas text
	 * needs (a canvas draws no text from a font that is not an asset).
	 */
	UFont* GetHudFont() const { return HudFont; }

	/** The side of this machine's player; None for a spectator or a world without one. */
	EVeyraTeam GetViewerTeam() const;

	/** The combat text this machine's player received and still shows, oldest first, by this machine's clock (ADR-052 §1). */
	const TArray<FVeyraCombatTextArrival>& GetCombatText() const { return CombatText; }

	/** The colour of Team as the viewer sees it: ally, enemy or neutral. */
	FLinearColor ColorOfSide(EVeyraTeam Team) const;

	/** Whether the connection and frame-rate warnings show now (ADR-055 §5). */
	bool IsShowingConnectionWarning() const { return ConnectionWarning.bShowing; }
	bool IsShowingPerformanceWarning() const { return PerformanceWarning.bShowing; }

	/** The player's side colours this frame, from their colour vision (ADR-055 §1). */
	const FVeyraSideColors& GetSideColors() const;

	/** The colour of Unit as the viewer sees it: the viewer's own Vanguard, or its side's. */
	FLinearColor SideColorOf(const AActor& Unit) const;

	/** The colour Unit's body shows: its side's, tinted while it is stunned or slowed. */
	FLinearColor BodyColorOf(const AActor& Unit) const;

private:
	/** Listens to the local player's combat text once its controller exists, and forgets the numbers done showing. */
	void RefreshCombatText();

	/** Darkens the ground the viewer's side does not see, redrawn only when its seen ground changes (ADR-054 §3). */
	void RefreshFogOfWar();
	void OnCombatText(const FVeyraCombatTextLine& Line);

	TArray<FVeyraCombatTextArrival> CombatText;
	TWeakObjectPtr<class AVeyraPlayerController> CombatTextSource;
	FDelegateHandle CombatTextHandle;

	struct FBody
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		FLinearColor Shown = FLinearColor::Transparent;

		/** A unit's art, which stands in for its body: the component, the mesh it shows, and its Flux's colour. */
		TWeakObjectPtr<UStaticMeshComponent> Art;
		TWeakObjectPtr<UStaticMesh> ArtMesh;
		TWeakObjectPtr<UMaterialInstanceDynamic> ArtFlux;
		FLinearColor ArtShown = FLinearColor::Transparent;

		/** What its cues have it doing, and the hit flash's overlay, once it has flashed (ADR-063 §2). */
		FVeyraBodyFeedbackState Feedback;
		TWeakObjectPtr<UMaterialInstanceDynamic> Flash;
		TWeakObjectPtr<UStaticMeshComponent> Flashing;
	};

	/** Notes a cue in its unit's body, and plays its effect. */
	void OnCombatCue(const struct FVeyraCombatCue& Cue);

	/**
	 * Draws Unit's body in its pose: leaning, snapping, squashed or collapsed, and flashing over whichever of its body
	 * and art shows. Structures only flash. After the body and art are placed for the frame.
	 */
	void ApplyBodyPose(const APawn& Unit, FBody& Body, bool bReduceFlashing);

	FDelegateHandle CueHandle;

	/** Shows the outline pass on the local camera, in the player's side colours, while anything is hovered. */
	void RefreshHoverPass();

	/** Turns Unit's drawn body and art's outline stencil on or off. */
	void SetOutlined(const AActor& Unit, bool bOutlined) const;

	TWeakObjectPtr<const AActor> Hovered;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> HoverOutlineMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HoverOutline;

	TWeakObjectPtr<class UCameraComponent> HoverCamera;

	/** Each side's stencil, as the generated outline material's defaults give them. */
	int32 EnemyStencil = 0;
	int32 AllyStencil = 0;
	int32 NeutralStencil = 0;

	struct FProjectileVisual
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;

		/** A homing projectile's drawn position, stepped toward its target in server time. */
		FVector Position = FVector::ZeroVector;
		double PresentedAt = 0.0;

		/** Its trail, following the sphere (ADR-063 §4). */
		TWeakObjectPtr<class UNiagaraComponent> Trail;
	};

	void RefreshBodies();

	/** Dresses Structure in its kind's art, standing or wrecked as it is, its Flux in its side's colour; its body hides behind it. */
	void RefreshStructureArt(const class AVeyraStructure& Structure, FBody& Body);

	/** Dresses Unit in its kind's art, active or collapsed as it is, its Flux in its body's colour, once its kind is known and has art. */
	void RefreshFluxbornArt(const class AVeyraFluxborn& Unit, FBody& Body);

	/**
	 * Draws Mesh over Unit in place of its body, its pivot at the capsule's foot, and colours the art set's Flux
	 * slot Color. Visual only, as a body is: it blocks nothing and shapes no navigation.
	 */
	void ShowArt(const APawn& Unit, FBody& Body, UStaticMesh& Mesh, const class UVeyraUnitArtSet& Set, const FLinearColor& Color);

	/**
	 * Once the world shows the battleground (its structures have arrived), draws its ground from
	 * World.json's layout: the river, the lanes' road and each base's pad. It decides nothing:
	 * nothing collides with it or shapes navigation.
	 */
	void RefreshBattleground();

	/**
	 * A flat marking of Mesh in Color, centred on Centre, turned by Yaw, with half extents HalfSize on
	 * the ground, Layer lifts up. Returns its material instance, or null.
	 */
	UMaterialInstanceDynamic* AddGroundMarking(AActor& Owner, UStaticMesh& Mesh, const FLinearColor& Color, const FVector2D& Centre, double Yaw,
		const FVector2D& HalfSize, int32 Layer) const;

	/** The colour of Team's base as the viewer sees it. */
	FLinearColor BaseColorOf(EVeyraTeam Team) const;

	void RefreshProjectiles();
	void RefreshTelegraphs();

	/** Adds the local player's indicator, while it holds a cast ready or previews one, aimed at the ground under its cursor. */
	void AddIndicator(const class AVeyraPlayerController& Local, const struct FVeyraAbilitiesTuning& Tuning);

	/** Adds the ring of the local player's basic attack reach around the body it commands, while Show Attack Range is held. */
	void AddAttackRange(const class AVeyraPlayerController& Local);
	void DrawTelegraphs();

	/** The viewer's side's presence pings and outlines, drawn on the ground with the telegraphs (ADR-016 §8). */
	void DrawVisionMarks();

	/** The chains between companions and their owners, joining the telegraphs' lines (ADR-034 §7). */
	void DrawChains();

	/** The local player's last order's mark, joining the telegraphs' lines while it shows. */
	void DrawOrderMark();
	TOptional<FVeyraOrderMarkRing> OrderMarkRing;

	/** Notes a melee swing at an attack's commit; draws and forgets the swings, joining the telegraphs' lines. */
	void NoteSwing(const struct FVeyraCombatCue& Cue);
	void DrawSwingArcs();
	TArray<FVeyraSwingArc> SwingArcs;

	/** A projected Echo's tether circle and stream (ADR-050 §7). */
	void DrawEchoTethers();

	/**
	 * Has the local player's HUD draw the grey-box HUD, through an overlay actor it renders for
	 * (AVeyraHudOverlay), once per HUD. That puts the HUD on the HUD's own canvas, under the menus.
	 */
	void AttachHudOverlay();

	/** A shape of Mesh attached to Owner, with its own material instance, or null. */
	UStaticMeshComponent* AddShape(AActor& Owner, UStaticMesh& Mesh, UMaterialInstanceDynamic*& OutMaterial) const;

	/** The ground under Location, lifted for drawing; Location itself where there is none. */
	FVector GroundUnder(const FVector& Location) const;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> BodyMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> ProjectileMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> HitFlashMaterial;

	/** The fight's effects (ADR-063 §4). */
	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraSystem> ImpactEffect;

	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraSystem> CastEffect;

	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraSystem> DeathEffect;

	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraSystem> TrailEffect;

	/** The fight's sounds (ADR-063 §5), how far they carry and how many play at once. */
	UPROPERTY(Transient)
	TObjectPtr<class USoundBase> ImpactSound;

	UPROPERTY(Transient)
	TObjectPtr<class USoundBase> SwingSound;

	UPROPERTY(Transient)
	TObjectPtr<class USoundBase> CastSound;

	UPROPERTY(Transient)
	TObjectPtr<class USoundBase> DeathSound;

	UPROPERTY(Transient)
	TObjectPtr<class USoundBase> ClickSound;

	UPROPERTY(Transient)
	TObjectPtr<class USoundAttenuation> CueAttenuation;

	UPROPERTY(Transient)
	TObjectPtr<class USoundConcurrency> CueConcurrency;

	/** Plays Cue's sound where it happens, at the player's gameplay effects volume. */
	void PlaySound(const struct FVeyraCombatCue& Cue);

	/** Hears from the camera's focus on the ground, not from the camera above it, and clicks for each new order of the player's. */
	void RefreshSound();

	TWeakObjectPtr<class APlayerController> ListeningFrom;
	double ClickedFor = -1.0;

	/** Stands a shop by each team start the world has, once. */
	void RefreshShops();

	TArray<TWeakObjectPtr<class AVeyraFountainShop>> Shops;
	TWeakObjectPtr<class AVeyraFountainShop> HoveredShop;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> GroundMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PadMesh;

	/** The structure kit's and the Fluxborn kit's art sets, loaded with the settings. */
	UPROPERTY(Transient)
	TObjectPtr<class UVeyraUnitArtSet> StructureArt;

	UPROPERTY(Transient)
	TObjectPtr<class UVeyraUnitArtSet> FluxbornArt;

	/** The actor holding the battleground's ground markings, once drawn. */
	TWeakObjectPtr<AActor> GroundMarkings;

	/** The fog banks drawn on the ground already: each carries its markings, and takes them as it goes (ADR-036 §1). */
	TSet<TWeakObjectPtr<const AActor>> DrawnFogBanks;

	/** Each base's pad, and the viewer's side they were coloured for. */
	TMap<EVeyraTeam, TWeakObjectPtr<UMaterialInstanceDynamic>> PadMaterials;
	EVeyraTeam PadsDrawnFor = EVeyraTeam::None;

	UPROPERTY(Transient)
	TObjectPtr<ULineBatchComponent> TelegraphLines;

	/** The fog of war's sheet, and the seen ground it was drawn from (VeyraFogOfWar::SignatureOf). */
	UPROPERTY(Transient)
	TObjectPtr<ULineBatchComponent> FogOfWarSheet;
	uint32 FogOfWarDrawn = 0;

	/** Measures the connection and the frame rate for the HUD's warnings, as the player allows them. */
	void RefreshWarnings();

	FVeyraWarningState ConnectionWarning;
	FVeyraWarningState PerformanceWarning;

	/** The side colours, resolved once a frame. */
	mutable FVeyraSideColors SideColors;
	mutable uint64 SideColorsFrame = MAX_uint64;

	UPROPERTY(Transient)
	TObjectPtr<UFont> HudFont;

	TMap<TWeakObjectPtr<const AActor>, FBody> Bodies;
	TMap<TWeakObjectPtr<const AVeyraProjectile>, FProjectileVisual> Projectiles;
	TArray<FVeyraTelegraph> Telegraphs;

	/** The overlay actor, which the world owns, and the HUD it draws for. */
	TWeakObjectPtr<AActor> HudOverlay;
	TWeakObjectPtr<AHUD> OverlayHud;

	/** Whether the settings and their assets are usable; nothing is drawn otherwise. */
	bool bReady = false;
};
