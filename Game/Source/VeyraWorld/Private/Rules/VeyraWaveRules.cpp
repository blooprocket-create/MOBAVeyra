// Copyright © 2026 Wayfinder Studios. All rights reserved.

#include "Rules/VeyraWaveRules.h"

#include "Tuning/VeyraWorldTuning.h"

namespace VeyraWaveRules
{
namespace
{
	/** The last phase begun by Seconds; the first when none has (validation makes the first begin at 0). */
	const FVeyraWavePhaseTuning* PhaseAt(const FVeyraWavesTuning& Waves, double Seconds)
	{
		const FVeyraWavePhaseTuning* Current = Waves.Phases.IsEmpty() ? nullptr : &Waves.Phases[0];
		for (const FVeyraWavePhaseTuning& Phase : Waves.Phases)
		{
			if (Phase.FromSeconds <= Seconds)
			{
				Current = &Phase;
			}
		}
		return Current;
	}

	void Append(TArray<FVeyraContentId>& Out, const FVeyraWaveUnitTuning& Units)
	{
		for (int32 Index = 0; Index < Units.Count; ++Index)
		{
			Out.Add(Units.Unit);
		}
	}
}

double IntervalAt(const FVeyraWavesTuning& Waves, double Seconds)
{
	const FVeyraWavePhaseTuning* Phase = PhaseAt(Waves, Seconds);
	return Phase ? Phase->IntervalSeconds : 0.0;
}

double WaveTime(const FVeyraWavesTuning& Waves, int32 Index)
{
	double Seconds = Waves.FirstWaveSeconds;
	for (int32 Wave = 0; Wave < Index; ++Wave)
	{
		Seconds += IntervalAt(Waves, Seconds);
	}
	return Seconds;
}

bool HasSiege(const FVeyraWavesTuning& Waves, int32 Index, double Seconds)
{
	const FVeyraWavePhaseTuning* Phase = PhaseAt(Waves, Seconds);
	// Counting waves from 1, so the Nth, 2Nth and so on bring them.
	return Phase && Phase->SiegeEveryWaves > 0 && (Index + 1) % Phase->SiegeEveryWaves == 0;
}

TArray<FVeyraContentId> Composition(const FVeyraWavesTuning& Waves, bool bSiege, bool bInhibitorDown)
{
	TArray<FVeyraContentId> Out;
	if (bInhibitorDown)
	{
		for (const FVeyraWaveUnitTuning& Units : Waves.InhibitorDownUnits)
		{
			Append(Out, Units);
		}
	}
	for (int32 Index = 0; Index < Waves.Units.Num(); ++Index)
	{
		Append(Out, Waves.Units[Index]);
		// Siege units walk behind the front line (the first kind), before the rest.
		if (Index == 0 && bSiege)
		{
			for (const FVeyraWaveUnitTuning& Units : Waves.SiegeUnits)
			{
				Append(Out, Units);
			}
		}
	}
	return Out;
}
}
