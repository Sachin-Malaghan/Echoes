// ECHOES automation test: renders every sound effect and the layered score offline, checks each is
// audible and never clips, and writes them to Saved/AudioPreview/*.wav to listen to. (CLAUDE.md: Audio)
#include "Audio/ECAudioSynth.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 TestRate = 48000;

	void WriteWav(const FString& Path, const TArray<float>& Stereo)
	{
		TArray<uint8> B;
		auto U32 = [&B](uint32 V) { for (int32 I = 0; I < 4; ++I) { B.Add((V >> (8 * I)) & 255); } };
		auto U16 = [&B](uint16 V) { B.Add(V & 255); B.Add(V >> 8); };
		const uint32 DataBytes = Stereo.Num() * 2;
		B.Append((const uint8*)"RIFF", 4); U32(36 + DataBytes); B.Append((const uint8*)"WAVEfmt ", 8);
		U32(16); U16(1); U16(2); U32(TestRate); U32(TestRate * 4); U16(4); U16(16);
		B.Append((const uint8*)"data", 4); U32(DataBytes);
		for (float S : Stereo) { U16((uint16)(int16)FMath::Clamp(FMath::RoundToInt(S * 32767.f), -32768, 32767)); }
		FFileHelper::SaveArrayToFile(B, *Path);
	}

	struct FStats { float Peak = 0, Rms = 0; };

	FStats Measure(const TArray<float>& Stereo)
	{
		FStats S;
		double Sum = 0;
		for (float V : Stereo) { S.Peak = FMath::Max(S.Peak, FMath::Abs(V)); Sum += V * V; }
		S.Rms = (float)FMath::Sqrt(Sum / FMath::Max(1, Stereo.Num()));
		return S;
	}

	// Renders Seconds of audio; Tick(time) is called every 0.5 s of rendered time (for music beats).
	TArray<float> Render(UECAudioSynth* Synth, double Seconds, TFunctionRef<void(int32)> OnHalfSecond)
	{
		TArray<float> Out;
		const int32 Block = TestRate / 100;   // 10 ms
		const int32 Blocks = (int32)(Seconds * 100);
		Out.SetNumZeroed(Blocks * Block * 2);
		for (int32 K = 0; K < Blocks; ++K)
		{
			if (K % 50 == 0) { OnHalfSecond(K / 50); }
			Synth->RenderOffline(Out.GetData() + K * Block * 2, Block, TestRate);
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FECAudioTest, "Echoes.Audio.EverySoundAudibleNoClipping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FECAudioTest::RunTest(const FString&)
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("AudioPreview");
	IFileManager::Get().MakeDirectory(*Dir, true);
	static const TCHAR* Names[] = { TEXT("ui_move"), TEXT("ui_select"), TEXT("ui_back"), TEXT("jump"), TEXT("land"), TEXT("step"),
		TEXT("echo_jump"), TEXT("echo_land"), TEXT("plate_down"), TEXT("plate_up"), TEXT("door_open"), TEXT("door_close"), TEXT("shard"),
		TEXT("died"), TEXT("rewind"), TEXT("restart"), TEXT("paradox"), TEXT("solve"), TEXT("out_of_loops"), TEXT("timer_tick"),
		TEXT("laser_on"), TEXT("laser_warn"), TEXT("star") };
	constexpr int32 NumSounds = UE_ARRAY_COUNT(Names);
	static_assert(NumSounds == (int32)EECSound::Star + 1, "every sound needs a name");

	for (int32 I = 0; I < NumSounds; ++I)
	{
		UECAudioSynth* Synth = NewObject<UECAudioSynth>();
		Synth->SetMix(false, true, false, 0.f);
		Render(Synth, 1.5, [](int32) {});   // let the mix settle, as it has in a running game
		Synth->Play((EECSound)I, 0.f, 1.f, 0.f);
		const TArray<float> Wav = Render(Synth, 2.5, [](int32) {});
		const FStats S = Measure(Wav);
		WriteWav(Dir / FString::Printf(TEXT("sfx_%02d_%s.wav"), I, Names[I]), Wav);
		AddInfo(FString::Printf(TEXT("%-14s peak %.3f  rms %.4f"), Names[I], S.Peak, S.Rms));
		TestTrue(FString::Printf(TEXT("%s is audible"), Names[I]), S.Peak > 0.01f);
		TestTrue(FString::Printf(TEXT("%s does not clip"), Names[I]), S.Peak < 0.95f);
	}

	// The score: one loop (20 beats) with 0..6 Echoes, per world. Louder with each layer, never clipping.
	for (int32 World = 1; World <= 2; ++World)
	{
		float PrevRms = 0;
		for (int32 Layers = 0; Layers <= 6; ++Layers)
		{
			UECAudioSynth* Synth = NewObject<UECAudioSynth>();
			Synth->SetMix(true, true, false, 0.f);
			const TArray<float> Wav = Render(Synth, 10.0, [&](int32 Beat) { if (Beat < 20) { Synth->OnBeat(Beat, Layers, World); } });
			const FStats S = Measure(Wav);
			WriteWav(Dir / FString::Printf(TEXT("music_world%d_echoes%d.wav"), World, Layers), Wav);
			AddInfo(FString::Printf(TEXT("music world %d, %d Echoes: peak %.3f  rms %.4f"), World, Layers, S.Peak, S.Rms));
			TestTrue(TEXT("music is audible"), S.Rms > 0.004f);
			TestTrue(TEXT("music does not clip"), S.Peak < 0.95f);
			TestTrue(TEXT("each Echo adds to the music"), S.Rms >= PrevRms * 0.98f);
			PrevRms = S.Rms;
		}
	}

	// Laser hum
	{
		UECAudioSynth* Synth = NewObject<UECAudioSynth>();
		Synth->SetMix(false, true, false, 1.f);
		const TArray<float> Wav = Render(Synth, 2.0, [](int32) {});
		WriteWav(Dir / TEXT("laser_hum.wav"), Wav);
		TestTrue(TEXT("laser hum is audible"), Measure(Wav).Peak > 0.005f);
	}
	return true;
}

#endif
