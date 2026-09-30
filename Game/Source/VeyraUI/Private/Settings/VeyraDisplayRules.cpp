// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Settings/VeyraDisplayRules.h"

#include "VeyraSettingsStore.h"

namespace VeyraDisplayRules
{
namespace
{
	FVeyraContentId IdOf(const TCHAR* Text)
	{
		// The registry's validation and the tests keep these real.
		return FVeyraContentId::FromText(Text).GetValue();
	}

	/** The option No cap offers in the frame caps. */
	const TCHAR* const Uncapped = TEXT("Uncapped");

	/** A quality option's scalability level: its place among the group's options, Low first. */
	int32 LevelOf(const FVeyraSettingsStore& Store, const FVeyraContentId& Group)
	{
		const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(Store.GetRegistry(), Group);
		if (!Setting.IsSet() || !Setting->Choice)
		{
			return 0;
		}
		return FMath::Max(0, Setting->Choice->Options.IndexOfByKey(Store.Get(Group)));
	}

	float CapOf(const FVeyraSettingsStore& Store, const FVeyraContentId& Cap)
	{
		const FString Value = Store.Get(Cap);
		return Value == Uncapped ? 0.0f : FCString::Atof(*Value);
	}

	TArray<FVeyraContentId> Groups()
	{
		return { TextureQuality(), ShadowQuality(), EffectsQuality() };
	}
}

const FVeyraContentId& FrameCap()
{
	static const FVeyraContentId Id = IdOf(TEXT("display_frame_cap"));
	return Id;
}

const FVeyraContentId& BackgroundFrameCap()
{
	static const FVeyraContentId Id = IdOf(TEXT("display_background_frame_cap"));
	return Id;
}

const FVeyraContentId& VSync()
{
	static const FVeyraContentId Id = IdOf(TEXT("display_vsync"));
	return Id;
}

const FVeyraContentId& RenderScale()
{
	static const FVeyraContentId Id = IdOf(TEXT("display_render_scale"));
	return Id;
}

const FVeyraContentId& Quality()
{
	static const FVeyraContentId Id = IdOf(TEXT("display_quality"));
	return Id;
}

const FVeyraContentId& TextureQuality()
{
	static const FVeyraContentId Id = IdOf(TEXT("display_texture_quality"));
	return Id;
}

const FVeyraContentId& ShadowQuality()
{
	static const FVeyraContentId Id = IdOf(TEXT("display_shadow_quality"));
	return Id;
}

const FVeyraContentId& EffectsQuality()
{
	static const FVeyraContentId Id = IdOf(TEXT("display_effects_quality"));
	return Id;
}

const FVeyraContentId& WindowSize()
{
	static const FVeyraContentId Id = IdOf(TEXT("display_window_size"));
	return Id;
}

const FVeyraContentId& MatchMode()
{
	static const FVeyraContentId Id = IdOf(TEXT("display_match_mode"));
	return Id;
}


TOptional<FIntPoint> ParseSize(const FString& Text)
{
	FString Width;
	FString Height;
	if (!Text.Split(TEXT("x"), &Width, &Height) || !Width.IsNumeric() || !Height.IsNumeric())
	{
		return {};
	}
	const FIntPoint Size(FCString::Atoi(*Width), FCString::Atoi(*Height));
	return Size.X > 0 && Size.Y > 0 ? TOptional<FIntPoint>(Size) : TOptional<FIntPoint>();
}

FVeyraDisplayState Resolve(const FVeyraSettingsStore& Store, bool bForeground)
{
	FVeyraDisplayState State;
	State.FrameCap = CapOf(Store, bForeground ? FrameCap() : BackgroundFrameCap());
	State.bVSync = Store.IsOn(VSync());
	State.RenderScale = static_cast<float>(Store.GetNumber(RenderScale()));
	State.TextureQuality = LevelOf(Store, TextureQuality());
	State.ShadowQuality = LevelOf(Store, ShadowQuality());
	State.EffectsQuality = LevelOf(Store, EffectsQuality());
	State.WindowSize = ParseSize(Store.Get(WindowSize())).Get(FIntPoint::ZeroValue);
	State.MatchMode = VeyraMatchDisplay::ParseDisplayMode(Store.Get(MatchMode())).Get(EVeyraDisplayMode::BorderlessFullscreen);
	return State;
}

void FollowQualityPreset(FVeyraSettingsStore& Store, const FVeyraContentId& Changed)
{
	const TArray<FVeyraContentId> Levels = Groups();
	if (Changed == Quality())
	{
		const FString Preset = Store.Get(Quality());
		// Custom leaves each group as it is.
		for (const FVeyraContentId& Group : Levels)
		{
			const TOptional<FVeyraSettingInfo> Setting = VeyraSettings::Find(Store.GetRegistry(), Group);
			if (Setting.IsSet() && Setting->Choice && Setting->Choice->Options.Contains(Preset))
			{
				Store.Follow(Group, Preset);
			}
		}
		return;
	}
	if (!Levels.Contains(Changed))
	{
		return;
	}
	const FString First = Store.Get(Levels[0]);
	const bool bShared = Levels.ContainsByPredicate([&Store, &First](const FVeyraContentId& Group) { return Store.Get(Group) != First; }) == false;
	const TOptional<FVeyraSettingInfo> Preset = VeyraSettings::Find(Store.GetRegistry(), Quality());
	const bool bNamed = Preset.IsSet() && Preset->Choice && Preset->Choice->Options.Contains(First);
	// The preset's last option names a mix of levels (Settings Bible §8: Low / Medium / High / Custom).
	const FString Custom = Preset.IsSet() && Preset->Choice && !Preset->Choice->Options.IsEmpty() ? Preset->Choice->Options.Last() : FString();
	Store.Follow(Quality(), bShared && bNamed ? First : Custom);
}
}

void FVeyraDisplayConfirmation::Await(const FVeyraContentId& Id, const FString& Previous, double Deadline)
{
	// A second change to the same setting keeps the value from before the first, which is the one that worked.
	if (Pending.IsSet() && Pending->Id == Id)
	{
		Pending->Deadline = Deadline;
		return;
	}
	Pending = FPending{ Id, Previous, Deadline };
}

double FVeyraDisplayConfirmation::SecondsLeft(double Now) const
{
	return Pending.IsSet() ? FMath::Max(0.0, Pending->Deadline - Now) : 0.0;
}

bool FVeyraDisplayConfirmation::Tick(double Now, FVeyraSettingsStore& Store, bool bInLiveMatch)
{
	if (!Pending.IsSet() || Now < Pending->Deadline)
	{
		return false;
	}
	Revert(Store, bInLiveMatch);
	return true;
}

void FVeyraDisplayConfirmation::Keep()
{
	Pending.Reset();
}

void FVeyraDisplayConfirmation::Revert(FVeyraSettingsStore& Store, bool bInLiveMatch)
{
	if (!Pending.IsSet())
	{
		return;
	}
	const FPending Reverting = Pending.GetValue();
	Pending.Reset();
	Store.Set(Reverting.Id, Reverting.Previous, bInLiveMatch);
}
