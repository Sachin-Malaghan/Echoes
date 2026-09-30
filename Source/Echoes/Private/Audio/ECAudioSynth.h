// ECHOES: all sound is synthesised live - effects, laser hum, and a score locked to the 10-second loop
// that gains one instrument per Echo. No audio files. (CLAUDE.md: Audio)
#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Containers/Queue.h"
#include "Game/ECGame.h"

#include <atomic>

#include "ECAudioSynth.generated.h"

UCLASS()
class UECAudioSynth : public USynthComponent, public IECAudioSink
{
	GENERATED_BODY()

public:
	UECAudioSynth(const FObjectInitializer& ObjectInitializer);

	// IECAudioSink (game thread)
	virtual void Play(EECSound Sound, float Pan, float Strength, float Delay) override;
	virtual void OnBeat(int32 Beat, int32 Layers, int32 World) override;
	virtual void SetMix(bool bMusic, bool bSound, bool bMenu, float LaserHum) override;

	// Automation tests render the synth offline (no audio device needed).
	void RenderOffline(float* OutStereo, int32 NumFrames, int32 SampleRate)
	{
		if (!bOfflineInit) { int32 R = SampleRate; Init(R); bOfflineInit = true; }
		OnGenerateAudio(OutStereo, NumFrames * 2);
	}

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

public:
	struct FVoiceSpec
	{
		float Delay = 0;        // seconds before it starts
		float Dur = 0.2f;       // seconds to -60 dB
		float Attack = 0.003f;
		float Amp = 0.1f;
		float Pan = 0;          // -1..1
		float F0 = 0, F1 = 0;   // tone sweep (Hz), 0 = no tone
		float Tone = 0;         // sine mix
		float Square = 0;       // soft square mix (chiptune-ish edge)
		float Noise = 0;        // band-limited noise mix
		float Cut1 = 0.2f;      // noise low-pass coefficient (higher = brighter)
		float Cut2 = 0.02f;     // noise high-pass coefficient
		float Wet = 0.15f;      // reverb send
		float Harm = 0;         // 2nd + 3rd harmonic amount (bells)
		float Vibrato = 0;      // pitch wobble depth (0..0.1)
		bool bMusicBus = false; // follows the music setting instead of the sound setting
	};

private:
	void Queue(const FVoiceSpec& Spec) { Commands.Enqueue(Spec); }

	struct FVoice
	{
		FVoiceSpec S;
		double T = 0;
		double Phase = 0;
		float Lp1 = 0, Lp2 = 0;
		bool bActive = false;
	};

	TQueue<FVoiceSpec, EQueueMode::Mpsc> Commands;

	// Mix targets written by the game thread, smoothed on the audio thread.
	std::atomic<float> TargetMusic{ 1.f };
	std::atomic<float> TargetSfx{ 1.f };
	std::atomic<float> TargetHum{ 0.f };
	std::atomic<int32> TargetWorld{ 1 };

	// Audio-thread state
	static constexpr int32 MaxVoices = 48;
	FVoice Voices[MaxVoices];
	float Rate = 48000.f;
	bool bOfflineInit = false;
	uint32 Rng = 0x9E3779B9u;
	float Music = 0, Sfx = 0, Hum = 0;
	double DronePhase[2] = { 0, 0 };
	double HumPhase = 0;
	float HumLp = 0;
	double Clock = 0;
	TArray<float> CombBuf[4];
	int32 CombPos[4] = { 0, 0, 0, 0 };
	float CombLp[4] = { 0, 0, 0, 0 };
	TArray<float> ApBuf[2];
	int32 ApPos[2] = { 0, 0 };

	float Noise();
	void StartVoice(const FVoiceSpec& Spec);
	float Reverb(float In);
};
