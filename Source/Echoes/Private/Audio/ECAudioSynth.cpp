// ECHOES: live sound synthesis. (CLAUDE.md: Audio)
#include "Audio/ECAudioSynth.h"

namespace
{
	constexpr float SfxGain = 1.8f;
	constexpr float MasterGain = 1.8f;   // phone speakers are small; the soft clip keeps peaks round   // effects sit above the music, even on phone speakers

	const int32 CombDelay48k[4] = { 1687, 1601, 2053, 2251 };
	const int32 ApDelay48k[2] = { 556, 441 };

	float Semi(float Root, int32 N) { return Root * FMath::Pow(2.f, N / 12.f); }

	// The loop is 20 beats at 120 BPM = 5 bars. One chord per bar; the fifth bar returns home.
	struct FWorldMusic
	{
		float Roots[5];      // bass roots per bar (Hz)
		int32 Minor[5];      // 1 = minor triad, 0 = major
		float Melody[5];     // pentatonic for the bell line
		float BassSquare;    // grittier bass in the Factory
		float BellHarm;      // more metallic bells in the Factory
	};
	const FWorldMusic GMusic[] =
	{
		// The Lab: A minor - F - C - G - Am. Glassy.
		{ { 110.f, 87.31f, 130.81f, 98.f, 110.f }, { 1, 0, 0, 0, 1 }, { 440.f, 523.25f, 587.33f, 659.25f, 783.99f }, 0.15f, 0.45f },
		// The Factory: D minor - Bb - F - C - Dm. Lower, heavier.
		{ { 73.42f, 58.27f, 87.31f, 65.41f, 73.42f }, { 1, 0, 0, 0, 1 }, { 293.66f, 349.23f, 392.f, 440.f, 523.25f }, 0.45f, 0.9f },
	};

	uint32 HashBeat(uint32 X)
	{
		X ^= X >> 16; X *= 0x7feb352dU; X ^= X >> 15; X *= 0x846ca68bU; X ^= X >> 16;
		return X;
	}
}

UECAudioSynth::UECAudioSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 2;
	bAutoActivate = true;
	bIsUISound = true;          // keeps playing regardless of world pause
	bAllowSpatialization = false;
}

bool UECAudioSynth::Init(int32& SampleRate)
{
	NumChannels = 2;
	Rate = (float)SampleRate;
	for (int32 I = 0; I < 4; ++I)
	{
		CombBuf[I].SetNumZeroed(FMath::Max(64, (int32)(CombDelay48k[I] * Rate / 48000.f)));
		CombPos[I] = 0;
	}
	for (int32 I = 0; I < 2; ++I)
	{
		ApBuf[I].SetNumZeroed(FMath::Max(32, (int32)(ApDelay48k[I] * Rate / 48000.f)));
		ApPos[I] = 0;
	}
	return true;
}

// -------------------------------------------------------------------------------------------------
// Game thread

void UECAudioSynth::SetMix(bool bMusic, bool bSound, bool bMenu, float LaserHum)
{
	TargetMusic = bMusic ? (bMenu ? 0.6f : 1.f) : 0.f;
	TargetSfx = bSound ? 1.f : 0.f;
	TargetHum = bSound ? FMath::Clamp(LaserHum, 0.f, 1.f) : 0.f;
}

void UECAudioSynth::OnBeat(int32 Beat, int32 Layers, int32 World)
{
	TargetWorld = World;
	const FWorldMusic& M = GMusic[FMath::Clamp(World, 1, (int32)UE_ARRAY_COUNT(GMusic)) - 1];
	const int32 Bar = FMath::Clamp(Beat / 4, 0, 4), InBar = Beat % 4;
	const float Root = M.Roots[Bar];
	const int32 Third = M.Minor[Bar] ? 3 : 4;
	const float Chord[3] = { Root, Semi(Root, Third), Semi(Root, 7) };
	const uint32 H = HashBeat((uint32)Beat * 7919u + (uint32)World * 104729u);

	// Base (always): the clock - a tick every beat, a soft low pulse on each bar.
	{
		FVoiceSpec Tick; Tick.Noise = 1; Tick.Cut1 = 0.75f; Tick.Cut2 = 0.35f; Tick.Dur = 0.025f; Tick.Amp = InBar == 0 ? 0.03f : 0.018f; Tick.Wet = 0.2f; Tick.bMusicBus = true;
		Queue(Tick);
		if (InBar == 0)
		{
			FVoiceSpec Pulse; Pulse.Tone = 1; Pulse.F0 = 70; Pulse.F1 = 42; Pulse.Dur = 0.35f; Pulse.Amp = 0.09f; Pulse.Wet = 0.1f; Pulse.bMusicBus = true;
			Queue(Pulse);
		}
		if (World == 2 && (InBar == 1 || InBar == 3))
		{
			// The Factory's anvil: a dull metal clank.
			FVoiceSpec Clank; Clank.Noise = 0.7f; Clank.Cut1 = 0.18f; Clank.Cut2 = 0.06f; Clank.Tone = 0.4f; Clank.F0 = Clank.F1 = 310.f; Clank.Harm = 1.f;
			Clank.Dur = 0.12f; Clank.Amp = 0.035f; Clank.Wet = 0.35f; Clank.bMusicBus = true;
			Queue(Clank);
		}
	}
	// Echo 1: bass on beats 1 and 3.
	if (Layers >= 1 && (InBar == 0 || InBar == 2))
	{
		FVoiceSpec B; B.Tone = 1; B.Square = M.BassSquare; B.F0 = B.F1 = Root; B.Attack = 0.01f; B.Dur = 0.55f; B.Amp = 0.075f; B.Wet = 0.08f; B.bMusicBus = true;
		Queue(B);
	}
	// Echo 2: an arpeggio in eighth notes, two octaves up.
	if (Layers >= 2)
	{
		for (int32 K = 0; K < 2; ++K)
		{
			FVoiceSpec A; A.Delay = K * 0.25f; A.Tone = 1; A.Harm = 0.2f; A.F0 = A.F1 = Chord[(InBar * 2 + K) % 3] * 4.f;
			A.Dur = 0.3f; A.Amp = 0.026f; A.Wet = 0.3f; A.Pan = K ? 0.25f : -0.25f; A.bMusicBus = true;
			Queue(A);
		}
	}
	// Echo 3: a bell melody (the same notes every loop, so it becomes a tune).
	if (Layers >= 3 && (H & 3) != 3)
	{
		FVoiceSpec Bell; Bell.Tone = 1; Bell.Harm = M.BellHarm; Bell.F0 = Bell.F1 = M.Melody[(H >> 4) % 5]; Bell.Delay = (H & 16) ? 0.25f : 0.f;
		Bell.Dur = 1.3f; Bell.Amp = 0.032f; Bell.Wet = 0.5f; Bell.bMusicBus = true;
		Queue(Bell);
	}
	// Echo 4: shimmer - high chord tones with a long tail.
	if (Layers >= 4)
	{
		FVoiceSpec S; S.Tone = 1; S.F0 = S.F1 = Chord[(InBar + 2) % 3] * 8.f; S.Delay = 0.125f; S.Attack = 0.05f; S.Dur = 1.6f; S.Amp = 0.011f; S.Wet = 0.85f;
		S.Vibrato = 0.004f; S.Pan = (InBar % 2) ? 0.5f : -0.5f; S.bMusicBus = true;
		Queue(S);
	}
	// Echo 5: hats on the off-beats.
	if (Layers >= 5)
	{
		FVoiceSpec Hat; Hat.Noise = 1; Hat.Cut1 = 0.95f; Hat.Cut2 = 0.55f; Hat.Delay = 0.25f; Hat.Dur = 0.04f; Hat.Amp = 0.04f; Hat.Wet = 0.1f; Hat.bMusicBus = true;
		Queue(Hat);
	}
	// Echo 6: a swelling pad chord on each bar.
	if (Layers >= 6 && InBar == 0)
	{
		for (int32 K = 0; K < 3; ++K)
		{
			FVoiceSpec P; P.Tone = 1; P.F0 = P.F1 = Chord[K] * 2.f; P.Attack = 0.6f; P.Dur = 2.4f; P.Amp = 0.018f; P.Wet = 0.6f; P.Vibrato = 0.003f;
			P.Pan = (K - 1) * 0.4f; P.bMusicBus = true;
			Queue(P);
		}
	}
}

void UECAudioSynth::Play(EECSound Sound, float Pan, float S, float Delay)
{
	FVoiceSpec V;
	V.Pan = FMath::Clamp(Pan, -1.f, 1.f) * 0.6f;
	V.Delay = Delay;
	switch (Sound)
	{
	case EECSound::UiMove: V.Tone = 1; V.F0 = V.F1 = 1480; V.Dur = 0.04f; V.Amp = 0.02f; V.Wet = 0.2f; Queue(V); break;
	case EECSound::UiSelect: V.Tone = 1; V.F0 = 880; V.F1 = 1320; V.Dur = 0.2f; V.Amp = 0.045f; V.Harm = 0.3f; V.Wet = 0.25f; Queue(V); break;
	case EECSound::UiBack: V.Tone = 1; V.F0 = 740; V.F1 = 520; V.Dur = 0.15f; V.Amp = 0.035f; V.Wet = 0.2f; Queue(V); break;
	case EECSound::Jump:
		V.Tone = 1; V.Square = 0.25f; V.F0 = 330; V.F1 = 640; V.Attack = 0.005f; V.Dur = 0.13f; V.Amp = 0.05f; V.Wet = 0.1f; Queue(V);
		V.Tone = 0; V.Square = 0; V.Noise = 1; V.Cut1 = 0.2f; V.Cut2 = 0.06f; V.Dur = 0.08f; V.Amp = 0.03f; Queue(V);
		break;
	case EECSound::Land:
		V.Tone = 1; V.F0 = 150; V.F1 = 60; V.Dur = 0.11f; V.Amp = 0.08f + 0.06f * S; V.Wet = 0.05f; Queue(V);
		V.Tone = 0; V.Noise = 1; V.Cut1 = 0.12f; V.Cut2 = 0.03f; V.Dur = 0.06f; V.Amp = 0.05f; Queue(V);
		break;
	case EECSound::Step:
		V.Noise = 1; V.Cut1 = 0.35f; V.Cut2 = 0.1f; V.Dur = 0.035f; V.Amp = 0.03f; V.Wet = 0.05f; Queue(V);
		V.Noise = 0; V.Tone = 1; V.F0 = 190; V.F1 = 130; V.Dur = 0.03f; V.Amp = 0.018f; Queue(V);
		break;
	case EECSound::EchoJump:
		V.Tone = 1; V.F0 = 560; V.F1 = 1040; V.Dur = 0.16f; V.Amp = 0.016f; V.Wet = 0.55f; V.Vibrato = 0.02f; Queue(V);
		break;
	case EECSound::EchoLand:
		V.Tone = 1; V.F0 = 300; V.F1 = 190; V.Dur = 0.12f; V.Amp = 0.022f; V.Wet = 0.45f; Queue(V);
		break;
	case EECSound::PlateDown:
		V.Noise = 1; V.Cut1 = 0.6f; V.Cut2 = 0.2f; V.Dur = 0.03f; V.Amp = 0.07f; Queue(V);
		V.Noise = 0; V.Tone = 1; V.Harm = 0.5f; V.F0 = V.F1 = 1046.5f; V.Dur = 0.55f; V.Amp = 0.045f; V.Wet = 0.45f; Queue(V);
		V.Delay = Delay + 0.06f; V.F0 = V.F1 = 1568.f; V.Amp = 0.03f; Queue(V);
		break;
	case EECSound::PlateUp:
		V.Tone = 1; V.F0 = 784; V.F1 = 523; V.Dur = 0.15f; V.Amp = 0.025f; V.Wet = 0.3f; Queue(V);
		break;
	case EECSound::DoorOpen:
		V.Noise = 1; V.Cut1 = 0.09f; V.Cut2 = 0.02f; V.Attack = 0.04f; V.Dur = 0.35f; V.Amp = 0.08f; V.Wet = 0.2f; Queue(V);
		V.Noise = 0; V.Tone = 0.6f; V.Square = 0.4f; V.F0 = 170; V.F1 = 380; V.Dur = 0.3f; V.Amp = 0.035f; Queue(V);
		V.Tone = 1; V.Square = 0; V.F0 = 75; V.F1 = 48; V.Delay = Delay + 0.2f; V.Dur = 0.25f; V.Amp = 0.12f; Queue(V);
		break;
	case EECSound::DoorClose:
		V.Tone = 1; V.F0 = 95; V.F1 = 45; V.Dur = 0.3f; V.Amp = 0.13f; V.Wet = 0.2f; Queue(V);
		V.Tone = 0; V.Noise = 1; V.Cut1 = 0.07f; V.Cut2 = 0.02f; V.Dur = 0.12f; V.Amp = 0.07f; Queue(V);
		break;
	case EECSound::Shard:
	{
		const float Notes[4] = { 1318.5f, 1568.f, 1975.5f, 2637.f };
		for (int32 I = 0; I < 4; ++I)
		{
			FVoiceSpec N = V; N.Delay = Delay + I * 0.06f; N.Tone = 1; N.Harm = 0.4f; N.F0 = N.F1 = Notes[I]; N.Dur = 0.7f; N.Amp = 0.035f; N.Wet = 0.6f;
			Queue(N);
		}
		break;
	}
	case EECSound::Died:
		V.Noise = 1; V.Cut1 = 0.85f; V.Cut2 = 0.3f; V.Dur = 0.14f; V.Amp = 0.14f; V.Wet = 0.2f; Queue(V);
		V.Noise = 0; V.Tone = 0.6f; V.Square = 0.5f; V.F0 = 900; V.F1 = 70; V.Dur = 0.55f; V.Amp = 0.06f; V.Wet = 0.4f; Queue(V);
		break;
	case EECSound::Rewind:
		// Tape rewind: a pitch that dives, wobbling, over hiss.
		V.Tone = 0.7f; V.Square = 0.5f; V.F0 = 1300; V.F1 = 90; V.Attack = 0.02f; V.Dur = 0.7f; V.Amp = 0.05f; V.Vibrato = 0.06f; V.Wet = 0.3f; Queue(V);
		V.Tone = 0; V.Square = 0; V.Noise = 1; V.Cut1 = 0.5f; V.Cut2 = 0.08f; V.Attack = 0.05f; V.Dur = 0.6f; V.Amp = 0.05f; Queue(V);
		V.Noise = 0; V.Tone = 1; V.F0 = 55; V.F1 = 110; V.Delay = Delay + 0.5f; V.Dur = 0.35f; V.Amp = 0.08f; V.Vibrato = 0; Queue(V);
		break;
	case EECSound::Restart:
		V.Tone = 0.7f; V.Square = 0.3f; V.F0 = 620; V.F1 = 120; V.Dur = 0.4f; V.Amp = 0.04f; V.Vibrato = 0.04f; V.Wet = 0.3f; Queue(V);
		break;
	case EECSound::Paradox:
		for (int32 I = 0; I < 7; ++I)
		{
			FVoiceSpec G = V; G.Delay = Delay + I * 0.07f; G.Square = 0.9f; G.Tone = 0.2f;
			G.F0 = 200.f + 1400.f * FMath::FRand(); G.F1 = G.F0 * (0.5f + FMath::FRand()); G.Dur = 0.06f; G.Amp = 0.055f; G.Wet = 0.25f; G.Pan = FMath::FRandRange(-0.8f, 0.8f);
			Queue(G);
		}
		V.Tone = 1; V.F0 = 62; V.F1 = 30; V.Dur = 1.2f; V.Amp = 0.16f; V.Wet = 0.4f; Queue(V);
		break;
	case EECSound::Solve:
	{
		const float Chord[5] = { 440.f, 554.37f, 659.25f, 880.f, 1108.7f };
		for (int32 I = 0; I < 5; ++I)
		{
			FVoiceSpec N = V; N.Delay = Delay + I * 0.07f; N.Tone = 1; N.Harm = 0.3f; N.F0 = N.F1 = Chord[I]; N.Attack = 0.02f; N.Dur = 2.4f; N.Amp = 0.04f;
			N.Wet = 0.65f; N.Pan = (I - 2) * 0.2f;
			Queue(N);
		}
		V.Tone = 1; V.F0 = 110; V.F1 = 110; V.Attack = 0.05f; V.Dur = 2.f; V.Amp = 0.08f; V.Wet = 0.4f; Queue(V);
		break;
	}
	case EECSound::OutOfLoops:
	{
		const float Notes[3] = { 523.25f, 392.f, 311.13f };
		for (int32 I = 0; I < 3; ++I)
		{
			FVoiceSpec N = V; N.Delay = Delay + I * 0.2f; N.Tone = 1; N.Harm = 0.3f; N.F0 = N.F1 = Notes[I]; N.Dur = 0.7f; N.Amp = 0.045f; N.Wet = 0.5f;
			Queue(N);
		}
		break;
	}
	case EECSound::TimerTick:
		V.Tone = 1; V.F0 = V.F1 = S > 0.5f ? 2400.f : 1900.f; V.Dur = 0.04f; V.Amp = 0.035f; V.Wet = 0.15f; Queue(V);
		V.Tone = 0; V.Noise = 1; V.Cut1 = 0.8f; V.Cut2 = 0.4f; V.Dur = 0.02f; V.Amp = 0.04f; Queue(V);
		break;
	case EECSound::LaserOn:
		V.Noise = 1; V.Cut1 = 0.9f; V.Cut2 = 0.2f; V.Dur = 0.1f; V.Amp = 0.06f; V.Wet = 0.2f; Queue(V);
		V.Noise = 0; V.Tone = 0.5f; V.Square = 0.5f; V.F0 = 2400; V.F1 = 900; V.Dur = 0.16f; V.Amp = 0.03f; Queue(V);
		break;
	case EECSound::LaserWarn:
		V.Tone = 0.5f; V.Square = 0.5f; V.F0 = 380; V.F1 = 1150; V.Attack = 0.22f; V.Dur = 0.12f; V.Amp = 0.02f; V.Wet = 0.2f; Queue(V);
		break;
	case EECSound::Star:
		V.Tone = 1; V.Harm = 0.5f; V.F0 = V.F1 = 1318.5f * FMath::Pow(2.f, (S * 2.f) * 4.f / 12.f); V.Dur = 0.9f; V.Amp = 0.05f; V.Wet = 0.55f; Queue(V);
		break;
	}
}

// -------------------------------------------------------------------------------------------------
// Audio thread

float UECAudioSynth::Noise()
{
	Rng ^= Rng << 13;
	Rng ^= Rng >> 17;
	Rng ^= Rng << 5;
	return (Rng & 0xffffff) / float(0x800000) - 1.f;
}

void UECAudioSynth::StartVoice(const FVoiceSpec& Spec)
{
	int32 Slot = -1;
	double Oldest = -1;
	for (int32 I = 0; I < MaxVoices; ++I)
	{
		if (!Voices[I].bActive) { Slot = I; break; }
		if (Voices[I].T > Oldest) { Oldest = Voices[I].T; Slot = I; }
	}
	FVoice& V = Voices[Slot];
	V = FVoice();
	V.S = Spec;
	V.T = -Spec.Delay;
	V.bActive = true;
}

float UECAudioSynth::Reverb(float In)
{
	float Sum = 0;
	for (int32 I = 0; I < 4; ++I)
	{
		TArray<float>& B = CombBuf[I];
		const float Out = B[CombPos[I]];
		CombLp[I] += 0.35f * (Out - CombLp[I]);
		B[CombPos[I]] = In + CombLp[I] * 0.84f;
		CombPos[I] = (CombPos[I] + 1) % B.Num();
		Sum += Out;
	}
	Sum *= 0.25f;
	for (int32 I = 0; I < 2; ++I)
	{
		TArray<float>& B = ApBuf[I];
		const float Buf = B[ApPos[I]];
		const float Out = -Sum + Buf;
		B[ApPos[I]] = Sum + Buf * 0.5f;
		ApPos[I] = (ApPos[I] + 1) % B.Num();
		Sum = Out;
	}
	return Sum;
}

int32 UECAudioSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	FVoiceSpec Cmd;
	while (Commands.Dequeue(Cmd)) { StartVoice(Cmd); }

	const int32 Frames = NumSamples / 2;
	const float Dt = 1.f / Rate;
	const float Smooth = 1.f - FMath::Exp(-Dt * 4.f);
	const float HumSmooth = 1.f - FMath::Exp(-Dt * 25.f);
	const float TMusic = TargetMusic.load(), TSfx = TargetSfx.load(), THum = TargetHum.load();
	const bool bFactory = TargetWorld.load() == 2;
	const float DroneHz[2] = { bFactory ? 36.71f : 55.f, bFactory ? 55.f : 82.41f };

	for (int32 F = 0; F < Frames; ++F)
	{
		Music += (TMusic - Music) * Smooth;
		Sfx += (TSfx - Sfx) * Smooth;
		Hum += (THum - Hum) * HumSmooth;
		Clock += Dt;

		float L = 0, R = 0, Wet = 0;

		// A low drone under everything, breathing slowly.
		if (Music > 0.001f)
		{
			float Drone = 0;
			for (int32 I = 0; I < 2; ++I)
			{
				DronePhase[I] += DroneHz[I] * Dt;
				if (DronePhase[I] > 1.0) { DronePhase[I] -= 1.0; }
				const float Swell = 0.6f + 0.4f * FMath::Sin((float)(Clock * (0.07 + I * 0.043) * 2.0 * PI + I));
				Drone += FMath::Sin((float)(DronePhase[I] * 2.0 * PI)) * Swell * (I == 0 ? 0.6f : 0.35f);
			}
			Drone *= 0.022f * Music;
			L += Drone; R += Drone; Wet += Drone * 0.3f;
		}

		// Laser hum: a buzzing mains tone with a little crackle.
		if (Hum > 0.001f)
		{
			HumPhase += 110.0 * Dt;
			if (HumPhase > 1.0) { HumPhase -= 1.0; }
			const float P = (float)(HumPhase * 2.0 * PI);
			HumLp += 0.3f * (Noise() - HumLp);
			const float Buzz = (FMath::Sin(P) + 0.5f * FMath::Sin(2.f * P) + 0.33f * FMath::Sin(3.f * P) + 0.2f * FMath::Sin(5.f * P)) * 0.5f + HumLp * 0.15f;
			const float Sample = Buzz * 0.022f * Hum * Sfx;
			L += Sample; R += Sample;
		}

		// Voices
		for (int32 I = 0; I < MaxVoices; ++I)
		{
			FVoice& V = Voices[I];
			if (!V.bActive) { continue; }
			V.T += Dt;
			if (V.T < 0) { continue; }
			const FVoiceSpec& S = V.S;
			if (V.T > S.Dur + S.Attack) { V.bActive = false; continue; }
			const float T = (float)V.T;
			const float Env = T < S.Attack ? T / S.Attack : FMath::Exp(-(T - S.Attack) * 6.9f / S.Dur);
			float Sample = 0;
			if (S.Tone > 0 || S.Square > 0)
			{
				const float K = FMath::Clamp(T / (S.Dur + S.Attack), 0.f, 1.f);
				float Hz = S.F0 * FMath::Pow(FMath::Max(1.f, S.F1) / FMath::Max(1.f, S.F0), K);
				if (S.Vibrato > 0) { Hz *= 1.f + S.Vibrato * FMath::Sin(T * 2.f * PI * 11.f); }
				V.Phase += Hz * Dt;
				if (V.Phase > 1.0) { V.Phase -= 1.0; }
				const float P = (float)(V.Phase * 2.0 * PI);
				const float Sine = FMath::Sin(P);
				Sample += S.Tone * (Sine + S.Harm * (0.6f * FMath::Sin(P * 2.f) + 0.3f * FMath::Sin(P * 3.f)) * FMath::Exp(-T * 3.f));
				if (S.Square > 0) { Sample += S.Square * FMath::Clamp(Sine * 4.f, -1.f, 1.f) * 0.5f; }
			}
			if (S.Noise > 0)
			{
				V.Lp1 += S.Cut1 * (Noise() - V.Lp1);
				V.Lp2 += S.Cut2 * (V.Lp1 - V.Lp2);
				Sample += S.Noise * (V.Lp1 - V.Lp2) * 2.f;
			}
			Sample *= Env * S.Amp * (S.bMusicBus ? Music : Sfx * SfxGain);
			const float PanR = 0.5f + 0.5f * S.Pan;
			L += Sample * FMath::Sqrt(1.f - PanR);
			R += Sample * FMath::Sqrt(PanR);
			Wet += Sample * S.Wet;
		}

		const float Verb = Reverb(Wet);
		L += Verb * 0.5f;
		R += Verb * 0.5f;
		L *= MasterGain;
		R *= MasterGain;
		// gentle soft clip
		OutAudio[F * 2 + 0] = FMath::Clamp(L / (1.f + FMath::Abs(L) * 0.5f), -1.f, 1.f);
		OutAudio[F * 2 + 1] = FMath::Clamp(R / (1.f + FMath::Abs(R) * 0.5f), -1.f, 1.f);
	}
	return NumSamples;
}
