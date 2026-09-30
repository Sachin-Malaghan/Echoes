// ECHOES: game flow - screens, the fixed-step loop, rewind / paradox / solve effects, progress. (CLAUDE.md: Game flow)
// Plain C++ owned by AECPlayerController; AECHUD draws it.
#pragma once

#include "CoreMinimal.h"
#include "Core/ECAutopilot.h"
#include "Core/ECSim.h"

class UECSaveGame;

enum class EECScreen : uint8 { Title, LevelSelect, Playing, Paused, Complete };

enum class EECAction : uint8
{
	None, Play, Levels, Back, SelectLevel, Resume, Restart, Rewind, Pause, NextLevel, Replay, ToTitle,
	RetryLoop, CycleTouch, Quit, WorldPrev, WorldNext, ToggleMusic, ToggleSound
};

enum class EECSound : uint8
{
	UiMove, UiSelect, UiBack, Jump, Land, Step, EchoJump, EchoLand, PlateDown, PlateUp, DoorOpen, DoorClose,
	Shard, Died, Rewind, Restart, Paradox, Solve, OutOfLoops, TimerTick, LaserOn, LaserWarn, Star
};

// Implemented by the audio synth; the game calls it on the game thread.
class IECAudioSink
{
public:
	virtual ~IECAudioSink() = default;
	virtual void Play(EECSound Sound, float Pan, float Strength, float Delay) = 0;
	virtual void OnBeat(int32 Beat, int32 Layers, int32 World) = 0;   // every 0.5 s of loop time
	virtual void SetMix(bool bMusic, bool bSound, bool bMenu, float LaserHum) = 0;
};

// A short full-screen moment between loops; the sim is frozen while one plays.
enum class EECFx : uint8 { None, Rewind, Death, Paradox, OutOfLoops, Solve, Restart };

struct FECControls
{
	int Dir = 0;
	bool Jump = false;
	bool Rewind = false;    // edge
	bool Restart = false;   // edge
};

struct FECMenuInput
{
	bool Up = false, Down = false, Left = false, Right = false;
	bool Confirm = false, Back = false, Pause = false;
	bool bPointerValid = false, bPointerMoved = false;
	bool bClick = false;    // mouse click or tap released this frame
	FVector2D Pointer = FVector2D::ZeroVector;
};

struct FECButton
{
	FBox2D Box;
	EECAction Action = EECAction::None;
	int32 Param = 0;
	bool bEnabled = true;
};

class FECGame
{
public:
	static constexpr double RewindSeconds = 0.6;
	static constexpr double DeathSeconds = 0.8;
	static constexpr double ParadoxSeconds = 1.1;
	static constexpr double SolveSeconds = 1.8;
	static constexpr double RestartSeconds = 0.45;

	void Init(UECSaveGame* InSave, IECAudioSink* InAudio);
	void Tick(float DeltaSeconds, const FECControls& Controls, const FECMenuInput& Menu, double ScreenW, double ScreenH);

	void StartLevel(int32 Index);
	void StartAttract(int32 Index);
	void GoTo(EECScreen NewScreen);
	void Activate(const FECButton& Button);
	void OnAppBackground();
	void SaveProgress();

	bool IsGameplay() const { return Screen == EECScreen::Playing && !bAttract; }
	int32 NumLevels() const { return EC::NumLevels(); }
	double FxProgress() const;      // 0..1 through the current effect
	float RenderAlpha() const { return (float)(Accumulator / EC::Dt); }

	// State (read by the HUD)
	EECScreen Screen = EECScreen::Title;
	int32 LevelIndex = 0;
	int32 SelectWorld = 1;       // the page shown on the level select
	bool bAttract = true;
	EC::FSim Sim;
	EC::FAutopilot Pilot;
	double Accumulator = 0;
	float PrevPX = 0, PrevPY = 0;
	double RealTime = 0, ScreenTime = 0, LevelTime = 0;
	EECFx Fx = EECFx::None;
	double FxTime = 0;
	double Shake = 0, Flash = 0;
	double PlateFlash[EC::MaxPlates] = {};
	double DoorShake = 0;

	// History of the running loop, so the rewind effect can scrub doors and plates backwards too.
	float DoorHistory[EC::LoopTicks][EC::MaxDoors] = {};
	uint8 PlateHistory[EC::LoopTicks] = {};

	// Result card
	int32 ResultLoops = 0;
	uint8 ResultStars = 0, NewStars = 0;

	TArray<FECButton> Buttons;   // rebuilt every frame by the UI, hit-tested the next
	int32 Focus = 0;
	bool bPointerActive = false;

	UECSaveGame* Save = nullptr;
	bool bAutopilotInPlay = false;   // validation / capture: the autopilot plays real levels
	bool bPilotShardPlan = false;    // ...using the level's shard solution (more Echoes on screen)
	void DebugParadox();             // an Echo walks through a closed door (captures / console)
	void DebugOutOfLoops();          // spends every loop idle
	bool bNoSave = false;
	bool bHideUI = false;
	bool bQuitRequested = false;
	static bool PlatformHasQuitButton() { return !(PLATFORM_ANDROID || PLATFORM_IOS); }

private:
	void LoadLevel(int32 Index);
	void StepWorld(double Dt, const FECControls& Controls);
	void BeginFx(EECFx NewFx);
	void EndFx();
	void OnLoopReset();
	void HandleMenu(const FECMenuInput& Menu);
	void Back();
	void CompleteLevel();
	void Sound(EECSound S, float Strength = 1.f, float Delay = 0.f);   // game sounds (silent in attract mode)
	void UiSound(EECSound S);

	IECAudioSink* Audio = nullptr;
	double StepDistance = 0;
	float PrevVY = 0;
	void UiStar(int32 Index) { if (Audio) { Audio->Play(EECSound::Star, 0.f, Index * 0.5f, 0.3f + Index * 0.25f); } }
	bool bPrevLaserOn = false, bPrevLaserWarn = false;
};
