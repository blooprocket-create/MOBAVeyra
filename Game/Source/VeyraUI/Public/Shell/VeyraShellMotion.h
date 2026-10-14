// Copyright © 2026 Wayfinder Studios. All rights reserved.

#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Slate/WidgetTransform.h"
#include "Widgets/Input/SButton.h"

class UWidget;

/** How a page's part arrives (ADR-082 §7), as the website's do: rising into place, or sliding in from the left with a lean. */
enum class EVeyraReveal : uint8
{
	Rise,
	Slide,
};

/**
 * A page's parts arriving (ADR-082 §7): each fades and rises or slides into place over the reveal's seconds, a stagger after
 * the one before, along the HUD's curves. Presentation only; under Reduce UI Animation every part shows at once.
 */
class VEYRAUI_API FVeyraShellReveal
{
public:
	FVeyraShellReveal() = default;
	FVeyraShellReveal(const FVeyraShellReveal&) = delete;
	FVeyraShellReveal& operator=(const FVeyraShellReveal&) = delete;
	~FVeyraShellReveal();

	/** Starts Parts arriving now, in their order, each by Kinds' entry or Rise; under a still look they show at once. */
	void Play(TConstArrayView<UWidget*> Parts, TConstArrayView<EVeyraReveal> Kinds = {});

	/** Carries a reveal under way over to Parts that replace its own, as a rebuild of the same page makes them, from where it has reached. */
	void Continue(TConstArrayView<UWidget*> Parts, TConstArrayView<EVeyraReveal> Kinds = {});

	bool IsPlaying() const { return Ticking.IsValid(); }

	/** Every part in place now. */
	void Finish();

	/** How a part at Index of the order stands After seconds into its reveal: its opacity and its transform. */
	static void Pose(EVeyraReveal Kind, int32 Index, double After, float& OutOpacity, FWidgetTransform& OutTransform);

	/** Seconds from a reveal's start until a part at Index is in place. */
	static double SecondsUntilPlaced(int32 Index);

private:
	struct FPart
	{
		TWeakObjectPtr<UWidget> Widget;
		EVeyraReveal Kind = EVeyraReveal::Rise;
	};

	void Take(TConstArrayView<UWidget*> Parts, TConstArrayView<EVeyraReveal> Kinds);
	bool Tick(float DeltaSeconds);
	void Apply(double Now);

	TArray<FPart> Parts;
	double StartedAt = 0.0;
	FTSTicker::FDelegateHandle Ticking;
};

/**
 * A shell button's Slate widget (ADR-082 §7): a button as the engine's, which paints the website's light over itself, a
 * sheen crossing it as the cursor arrives and a burst where it is pressed. Under Reduce UI Animation it paints neither.
 */
class VEYRAUI_API SVeyraShellButton : public SButton
{
public:
	/** Whether it takes the sheen; every button bursts where it is pressed. */
	void SetSheen(bool bInSheen) { bSheen = bInSheen; }

	/** Seconds since its sheen and its burst started; very large when they never have. For tests. */
	double SinceSheen() const;
	double SinceBurst() const;

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** Keeps it repainting while a light runs, then stops. */
	void Animate();
	EActiveTimerReturnType Step(double CurrentTime, float DeltaTime);

	bool bSheen = false;
	double SheenAt = -1.0e9;
	double BurstAt = -1.0e9;
	FVector2D BurstPoint = FVector2D::ZeroVector;
	uint32 BurstSeed = 0;
	TSharedPtr<FActiveTimerHandle> Timer;
};
