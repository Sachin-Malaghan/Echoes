// ECHOES: game flow. (CLAUDE.md: Game flow)
#include "Game/ECGame.h"

#include "Game/ECSaveGame.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogEchoesGame, Log, All);

void FECGame::Init(UECSaveGame* InSave, IECAudioSink* InAudio)
{
	Save = InSave;
	Audio = InAudio;
	if (Save->Stars.Num() < NumLevels()) { Save->Stars.SetNumZeroed(NumLevels()); }
	if (Save->BestLoops.Num() < NumLevels()) { Save->BestLoops.SetNumZeroed(NumLevels()); }
	if (Save->BestTimes.Num() < NumLevels()) { Save->BestTimes.SetNumZeroed(NumLevels()); }
	Save->UnlockedLevels = FMath::Clamp(Save->UnlockedLevels, 1, NumLevels());
	Save->LastLevel = FMath::Clamp(Save->LastLevel, 0, Save->UnlockedLevels - 1);
	StartAttract(0);
	GoTo(EECScreen::Title);
}

void FECGame::SaveProgress()
{
	if (Save && !bNoSave) { UGameplayStatics::SaveGameToSlot(Save, UECSaveGame::SlotName, 0); }
}

void FECGame::LoadLevel(int32 Index)
{
	LevelIndex = FMath::Clamp(Index, 0, NumLevels() - 1);
	char Err[256];
	if (!Sim.Init(EC::GetLevelDef(LevelIndex), Err, sizeof Err)) { UE_LOG(LogEchoesGame, Error, TEXT("%hs"), Err); }
	Accumulator = 0;
	LevelTime = 0;
	Fx = EECFx::None;
	Shake = Flash = DoorShake = 0;
	for (double& F : PlateFlash) { F = 0; }
	NumParticles = 0;
	RunTime = 0;
	FailCount = 0;
	bHintActive = false;
	HintTrack.Length = 0;
	OnLoopReset();
}

void FECGame::StartLevel(int32 Index)
{
	LoadLevel(Index);
	bAttract = false;
	char Err[128];
	const EC::FLevelDef& Def = EC::GetLevelDef(LevelIndex);
	Pilot.Load(bPilotShardPlan ? Def.ShardPlan : Def.Plan, Err, sizeof Err);
	Save->LastLevel = LevelIndex;
	SaveProgress();
	GoTo(EECScreen::Playing);
}

void FECGame::StartAttract(int32 Index)
{
	LoadLevel(Index);
	bAttract = true;
	char Err[128];
	Pilot.Load(EC::GetLevelDef(LevelIndex).ShardPlan, Err, sizeof Err);
}

// Every loop starts here: the controls must be released before the first jump, the pilot restarts its run.
void FECGame::OnLoopReset()
{
	Sim.Player.PrevJump = true;
	PrevPX = Sim.Player.X;
	PrevPY = Sim.Player.Y;
	Pilot.BeginLoop(Sim.NumEchoes);
	if (bHintActive) { BuildHint(); }
}

// Plays the par solution in a private sim up to the run the player is on, and keeps that run as the hint.
void FECGame::BuildHint()
{
	HintTrack.Length = 0;
	if (!HintSim) { HintSim = MakeUnique<EC::FSim>(); }
	const EC::FLevelDef& Def = EC::GetLevelDef(LevelIndex);
	char Err[128];
	EC::FAutopilot Solver;
	if (!HintSim->Init(Def, Err, sizeof Err) || !Solver.Load(Def.Plan, Err, sizeof Err)) { return; }
	const int32 Target = FMath::Min(Sim.NumEchoes, Solver.NumRuns() - 1);
	for (int32 Guard = 0; Guard <= EC::MaxEchoes; ++Guard)
	{
		Solver.BeginLoop(HintSim->NumEchoes);
		while (HintSim->Phase == EC::EPhase::Playing) { HintSim->Step(Solver.Next(*HintSim)); }
		if (HintSim->NumEchoes == Target)
		{
			HintTrack = HintSim->Current;
			bHintEndsWithRewind = HintSim->Phase == EC::EPhase::LoopEnded;
			return;
		}
		if (HintSim->Phase != EC::EPhase::LoopEnded) { return; }
		HintSim->CommitLoop();
	}
}

FString FECGame::FormatTime(double Seconds)
{
	const int32 Ms = FMath::Max(0, FMath::RoundToInt(Seconds * 1000.0));
	return FString::Printf(TEXT("%d:%02d.%03d"), Ms / 60000, (Ms / 1000) % 60, Ms % 1000);
}

double FECGame::WorldTotalTime(int32 World, bool& bOutComplete) const
{
	double Sum = 0;
	bOutComplete = true;
	for (int32 I = EC::FirstLevelOfWorld(World); I < EC::FirstLevelOfWorld(World + 1); ++I)
	{
		const float T = Save && Save->BestTimes.IsValidIndex(I) ? Save->BestTimes[I] : 0.f;
		if (T <= 0) { bOutComplete = false; }
		Sum += T;
	}
	return Sum;
}

void FECGame::Burst(int32 Count, float X, float Y, float Speed, float Up, float Life, float Size, uint32 Color, bool bGlow, float Gravity, float SpreadX)
{
	for (int32 I = 0; I < Count; ++I)
	{
		if (NumParticles == MaxParticles) { Particles[FMath::RandHelper(MaxParticles)] = Particles[--NumParticles]; }
		FECParticle& P = Particles[NumParticles++];
		const float A = FMath::FRandRange(0.f, 2.f * PI);
		const float S = Speed * FMath::FRandRange(0.35f, 1.f);
		P.X = X + FMath::FRandRange(-SpreadX, SpreadX);
		P.Y = Y + FMath::FRandRange(0.f, 0.08f);
		P.VX = FMath::Cos(A) * S;
		P.VY = FMath::Abs(FMath::Sin(A)) * S * 0.6f + Up * FMath::FRandRange(0.5f, 1.f);
		P.MaxLife = P.Life = Life * FMath::FRandRange(0.6f, 1.f);
		P.Size = Size * FMath::FRandRange(0.6f, 1.2f);
		P.Gravity = Gravity;
		P.Drag = bGlow ? 1.5f : 3.5f;
		P.Color = Color;
		P.bGlow = bGlow;
	}
}

void FECGame::UpdateParticles(double Dt)
{
	const float D = (float)Dt;
	for (int32 I = 0; I < NumParticles; )
	{
		FECParticle& P = Particles[I];
		P.Life -= D;
		if (P.Life <= 0) { P = Particles[--NumParticles]; continue; }
		P.VY -= P.Gravity * D;
		P.VX *= FMath::Max(0.f, 1.f - P.Drag * D);
		P.VY *= FMath::Max(0.f, 1.f - P.Drag * D * 0.5f);
		P.X += P.VX * D;
		P.Y += P.VY * D;
		++I;
	}
}

void FECGame::Sound(EECSound S, float Strength, float Delay)
{
	if (!Audio || bAttract) { return; }
	const float W = (float)FMath::Max(1, Sim.Level.W);
	Audio->Play(S, Sim.Player.X / W * 2.f - 1.f, Strength, Delay);
}

void FECGame::UiSound(EECSound S)
{
	if (Audio) { Audio->Play(S, 0.f, 1.f, 0.f); }
}

void FECGame::GoTo(EECScreen NewScreen)
{
	Screen = NewScreen;
	ScreenTime = 0;
	Focus = 0;
	Buttons.Reset();
}

void FECGame::OnAppBackground()
{
	if (Screen == EECScreen::Playing && !bAttract) { GoTo(EECScreen::Paused); }
	SaveProgress();
}

double FECGame::FxProgress() const
{
	switch (Fx)
	{
	case EECFx::Rewind: return FMath::Clamp(FxTime / RewindSeconds, 0.0, 1.0);
	case EECFx::Death: return FMath::Clamp(FxTime / DeathSeconds, 0.0, 1.0);
	case EECFx::Paradox: return FMath::Clamp(FxTime / ParadoxSeconds, 0.0, 1.0);
	case EECFx::Solve: return FMath::Clamp(FxTime / SolveSeconds, 0.0, 1.0);
	case EECFx::Restart: return FMath::Clamp(FxTime / RestartSeconds, 0.0, 1.0);
	case EECFx::OutOfLoops: return FMath::Clamp(FxTime / 0.4, 0.0, 1.0);
	default: return 0.0;
	}
}

void FECGame::Tick(float DeltaSeconds, const FECControls& Controls, const FECMenuInput& Menu, double ScreenW, double ScreenH)
{
	const double Dt = FMath::Clamp((double)DeltaSeconds, 0.0, 0.1);
	RealTime += Dt;
	ScreenTime += Dt;

	HandleMenu(Menu);

	const bool bWorldRuns = Screen == EECScreen::Playing || Screen == EECScreen::Title || Screen == EECScreen::LevelSelect;
	if (bWorldRuns) { StepWorld(Dt, Controls); }

	UpdateParticles(Dt);
	if (Screen == EECScreen::Playing && !bAttract && Fx != EECFx::Solve && Sim.Phase != EC::EPhase::Solved) { RunTime += Dt; }
	Shake = FMath::Max(0.0, Shake - Dt * 10.0);
	Flash = FMath::Max(0.0, Flash - Dt * 2.5);
	DoorShake = FMath::Max(0.0, DoorShake - Dt * 6.0);
	for (double& F : PlateFlash) { F = FMath::Max(0.0, F - Dt * 2.0); }

	if (Audio && Save)
	{
		const bool bMenu = Screen != EECScreen::Playing || bAttract;
		const bool bHum = Sim.Level.NumEmitters > 0 && Sim.LaserOn() && Fx == EECFx::None && (Screen == EECScreen::Playing || Screen == EECScreen::Title);
		Audio->SetMix(Save->bMusic, Save->bSound, bMenu, bHum ? (bAttract ? 0.4f : 1.f) : 0.f);
	}
}

void FECGame::StepWorld(double Dt, const FECControls& Controls)
{
	LevelTime += Dt;
	const bool bPilot = bAttract || bAutopilotInPlay;

	if (Fx != EECFx::None)
	{
		FxTime += Dt;
		const bool bDone =
			(Fx == EECFx::Rewind && FxTime >= RewindSeconds) || (Fx == EECFx::Death && FxTime >= DeathSeconds) ||
			(Fx == EECFx::Paradox && FxTime >= ParadoxSeconds) || (Fx == EECFx::Solve && FxTime >= SolveSeconds) ||
			(Fx == EECFx::Restart && FxTime >= RestartSeconds) || (Fx == EECFx::OutOfLoops && bPilot && FxTime >= 1.5);
		if (bDone) { EndFx(); }
		return;
	}

	if (!bPilot && Screen == EECScreen::Playing && Controls.Restart)
	{
		BeginFx(EECFx::Restart);
		return;
	}

	Accumulator += Dt;
	bool bRewindQueued = !bPilot && Screen == EECScreen::Playing && Controls.Rewind;
	while (Accumulator >= EC::Dt)
	{
		Accumulator -= EC::Dt;
		EC::FInput In;
		if (bPilot) { In = Pilot.Next(Sim); }
		else if (Screen == EECScreen::Playing)
		{
			In.Left = Controls.Dir < 0;
			In.Right = Controls.Dir > 0;
			In.Jump = Controls.Jump;
			In.Rewind = bRewindQueued;
			bRewindQueued = false;
		}
		PrevPX = Sim.Player.X;
		PrevPY = Sim.Player.Y;
		const int32 T = Sim.Tick;
		const uint32 Ev = Sim.Step(In);

		if (T < EC::LoopTicks && Sim.Tick > T)
		{
			uint8 Plates = 0;
			for (int32 P = 0; P < Sim.Level.NumPlates; ++P) { Plates |= Sim.PlatePressed[P] ? (1 << P) : 0; }
			PlateHistory[T] = Plates;
			for (int32 D = 0; D < EC::MaxDoors; ++D) { DoorHistory[T][D] = Sim.DoorOpen[D]; }
		}
		if (Ev & EC::EV_PlateDown)
		{
			for (int32 P = 0; P < Sim.Level.NumPlates; ++P) { if (Sim.PlatePressed[P] && (PlateHistory[FMath::Max(0, T - 1)] & (1 << P)) == 0) { PlateFlash[P] = 1.0; } }
		}
		if (Ev & EC::EV_DoorOpen) { DoorShake = 1.0; Shake = FMath::Max(Shake, 0.6); }
		if (Ev & EC::EV_Shard) { Flash = FMath::Max(Flash, 0.35); }

		// Sound: events of this tick, footsteps, the laser clock, the last three seconds, the music beat.
		if (Ev & EC::EV_Jump) { Sound(EECSound::Jump); }
		if (Ev & EC::EV_Land) { Sound(EECSound::Land, FMath::Clamp(-PrevVY / 20.f, 0.f, 1.f)); }
		if (Ev & EC::EV_EchoJump) { Sound(EECSound::EchoJump); }
		if (Ev & EC::EV_EchoLand) { Sound(EECSound::EchoLand); }
		if (Ev & EC::EV_PlateDown) { Sound(EECSound::PlateDown); }
		if (Ev & EC::EV_PlateUp) { Sound(EECSound::PlateUp); }
		if (Ev & EC::EV_DoorOpen) { Sound(EECSound::DoorOpen); }
		if (Ev & EC::EV_DoorClose) { Sound(EECSound::DoorClose); }
		if (Ev & EC::EV_Shard) { Sound(EECSound::Shard); }
		{
			// Dust and sparks. Visual only, so attract mode gets them too.
			const float PX = Sim.Player.X, PY = Sim.Player.Y;
			const uint32 DustC = EC::GetLevelDef(LevelIndex).World == 2 ? 0xC9A58A : 0xA9C4DE;
			if (Ev & EC::EV_Jump) { Burst(6, PX, PY, 1.6f, 0.3f, 0.35f, 0.07f, DustC, false, 2.f); }
			if (Ev & EC::EV_Land)
			{
				const float Hard = FMath::Clamp(-PrevVY / 20.f, 0.f, 1.f);
				Burst(5 + (int32)(Hard * 12), PX, PY, 1.5f + 2.5f * Hard, 0.2f, 0.4f + 0.3f * Hard, 0.07f + 0.05f * Hard, DustC, false, 2.f, 0.3f);
				if (Hard > 0.7f) { Shake = FMath::Max(Shake, 0.5 + Hard * 0.6); }   // a long fall thumps the camera
			}
			if (Ev & EC::EV_PlateDown) { Burst(10, PX, PY + 0.1f, 2.2f, 1.2f, 0.5f, 0.035f, 0x2EE6C5, true, 3.f, 0.3f); }
			if (Ev & EC::EV_Shard) { Burst(26, Sim.Level.Shard.X, Sim.Level.Shard.Y, 3.5f, 0.5f, 0.8f, 0.04f, 0xCFFBFF, true, 2.f, 0.1f); }
			if (Ev & EC::EV_Died)
			{
				Burst(34, PX, PY + 0.8f, 5.f, 1.5f, 0.9f, 0.09f, 0x0A0F1A, false, 9.f, 0.2f);    // dark shards of the body
				Burst(18, PX, PY + 0.8f, 4.f, 1.f, 0.6f, 0.045f, 0xFF4D5E, true, 4.f, 0.2f);
			}
			if (Sim.Player.Grounded && FMath::Abs(Sim.Player.VX) > 4.f && (T % 6) == 0)
			{
				Burst(1, PX - (Sim.Player.VX > 0 ? 0.2f : -0.2f), PY, 0.5f, 0.5f, 0.35f, 0.06f, DustC, false, 0.5f, 0.05f);
			}
		}
		PrevVY = Sim.Player.VY;
		if (Sim.Player.Grounded && FMath::Abs(Sim.Player.VX) > 1.f)
		{
			StepDistance += FMath::Abs(Sim.Player.X - PrevPX);
			if (StepDistance > 0.85) { StepDistance = 0; Sound(EECSound::Step); }
		}
		if (Sim.Level.NumEmitters > 0)
		{
			const bool bOn = Sim.LaserOnAt(T), bWarn = Sim.LaserWarning();
			if (bOn && !bPrevLaserOn) { Sound(EECSound::LaserOn); }
			if (bWarn && !bPrevLaserWarn) { Sound(EECSound::LaserWarn); }
			bPrevLaserOn = bOn;
			bPrevLaserWarn = bWarn;
		}
		if (Sim.Phase == EC::EPhase::Playing && (T == 350 || T == 400 || T == 450)) { Sound(EECSound::TimerTick, T == 450 ? 1.f : 0.f); }
		if (Audio && T % 25 == 0 && Sim.Phase == EC::EPhase::Playing) { Audio->OnBeat(T / 25, Sim.NumEchoes, EC::GetLevelDef(LevelIndex).World); }

		switch (Sim.Phase)
		{
		case EC::EPhase::LoopEnded: BeginFx(EECFx::Rewind); break;
		case EC::EPhase::Died: BeginFx(EECFx::Death); break;
		case EC::EPhase::Paradox: BeginFx(EECFx::Paradox); break;
		case EC::EPhase::OutOfLoops: BeginFx(EECFx::OutOfLoops); break;
		case EC::EPhase::Solved: BeginFx(EECFx::Solve); break;
		default: break;
		}
		if (Fx != EECFx::None) { Accumulator = 0; break; }
	}
}

void FECGame::BeginFx(EECFx NewFx)
{
	Fx = NewFx;
	FxTime = 0;
	static const EECSound FxSounds[] = { EECSound::UiMove, EECSound::Rewind, EECSound::Died, EECSound::Paradox, EECSound::OutOfLoops, EECSound::Solve, EECSound::Restart };
	if (NewFx != EECFx::None) { Sound(FxSounds[(int32)NewFx]); }
	if (!bAttract && (NewFx == EECFx::Death || NewFx == EECFx::Paradox || NewFx == EECFx::OutOfLoops || NewFx == EECFx::Restart)) { ++FailCount; }
	if (NewFx == EECFx::Solve) { Burst(40, Sim.Level.Exit.X, Sim.Level.Exit.Y + 1.f, 4.5f, 1.f, 1.2f, 0.05f, 0xFFF4E0, true, 1.f, 0.2f); }
	if (NewFx == EECFx::Paradox) { Shake = 2.5; }
	if (NewFx == EECFx::Death) { Shake = 1.6; }
	if (NewFx == EECFx::Solve) { Flash = 1.0; }
	if (NewFx == EECFx::Restart && Sim.Phase == EC::EPhase::Playing)
	{
		// Restart scrubs the running loop backwards like a rewind, then clears every Echo.
		Sim.Current.Length = Sim.Tick;
	}
}

void FECGame::EndFx()
{
	const EECFx Was = Fx;
	Fx = EECFx::None;
	FxTime = 0;
	switch (Was)
	{
	case EECFx::Rewind:
		Sim.CommitLoop();
		OnLoopReset();
		break;
	case EECFx::Death:
		Sim.RetryLoop();
		OnLoopReset();
		break;
	case EECFx::Paradox:
	case EECFx::Restart:
	case EECFx::OutOfLoops:   // (only the autopilot gets here on its own; players choose on the panel)
		Sim.RestartLevel();
		RunTime = 0;
		OnLoopReset();
		break;
	case EECFx::Solve:
		if (bAttract) { StartAttract((LevelIndex + 1) % FMath::Max(1, Save->UnlockedLevels)); }
		else { CompleteLevel(); }
		break;
	default:
		break;
	}
}

void FECGame::DebugParadox()
{
	StartLevel(0);
	// An Echo recorded walking right through door A - which nobody holds open now.
	EC::FRecording& R = Sim.Echoes[0];
	R.Length = 150;
	for (int32 T = 0; T < R.Length; ++T)
	{
		const float X = FMath::Min(6.5f + T * 0.08f, 13.f);
		R.Frames[T] = { X, 1.f, T * 0.08f < 6.5f ? 4.f : 0.f, 0.f, 1, (uint8)(X < 13.f ? EC::EAnim::Run : EC::EAnim::Idle), EC::FF_Grounded, 0 };
	}
	Sim.NumEchoes = 1;
	Sim.RetryLoop();
	OnLoopReset();
	bAutopilotInPlay = false;
}

void FECGame::DebugOutOfLoops()
{
	StartLevel(0);
	bAutopilotInPlay = false;
	for (int32 Guard = 0; Guard < 20 && Sim.Phase != EC::EPhase::OutOfLoops; ++Guard)
	{
		while (Sim.Phase == EC::EPhase::Playing) { Sim.Step(EC::FInput()); }
		if (Sim.Phase == EC::EPhase::LoopEnded) { Sim.CommitLoop(); }
	}
	OnLoopReset();
	BeginFx(EECFx::OutOfLoops);
}

void FECGame::CompleteLevel()
{
	const EC::FLevelDef& Def = EC::GetLevelDef(LevelIndex);
	ResultLoops = Sim.LoopsUsed();
	ResultStars = 1 | (ResultLoops <= Def.ParLoops ? 2 : 0) | (Sim.ShardTaken() ? 4 : 0);
	uint8& Stars = Save->Stars[LevelIndex];
	NewStars = ResultStars & ~Stars;
	Stars |= ResultStars;
	int32& Best = Save->BestLoops[LevelIndex];
	if (Best == 0 || ResultLoops < Best) { Best = ResultLoops; }
	ResultTime = (float)RunTime;
	float& BestTime = Save->BestTimes[LevelIndex];
	bNewBestTime = BestTime <= 0 || ResultTime < BestTime;
	if (bNewBestTime) { BestTime = ResultTime; }
	Save->UnlockedLevels = FMath::Clamp(FMath::Max(Save->UnlockedLevels, LevelIndex + 2), 1, NumLevels());
	Save->LastLevel = FMath::Min(LevelIndex + 1, NumLevels() - 1);
	SaveProgress();
	GoTo(EECScreen::Complete);
	for (int32 S = 0; S < 3; ++S)
	{
		if (ResultStars & (1 << S)) { UiStar(S); }
	}
}

void FECGame::Back()
{
	switch (Screen)
	{
	case EECScreen::LevelSelect: GoTo(EECScreen::Title); break;
	case EECScreen::Paused: GoTo(EECScreen::Playing); break;
	case EECScreen::Playing: if (!bAttract) { GoTo(EECScreen::Paused); } break;
	case EECScreen::Complete: SelectWorld = EC::GetLevelDef(LevelIndex).World; GoTo(EECScreen::LevelSelect); break;
	case EECScreen::Title:
		// Android convention: Back on the first screen leaves the app.
#if PLATFORM_ANDROID
		SaveProgress();
		bQuitRequested = true;
#endif
		break;
	default: break;
	}
}

void FECGame::Activate(const FECButton& B)
{
	if (!B.bEnabled) { return; }
	if (B.Action != EECAction::Rewind && B.Action != EECAction::Restart && B.Action != EECAction::Pause)
	{
		UiSound(B.Action == EECAction::Back ? EECSound::UiBack : EECSound::UiSelect);
	}
	switch (B.Action)
	{
	case EECAction::Play: StartLevel(FMath::Clamp(Save->LastLevel, 0, Save->UnlockedLevels - 1)); break;
	case EECAction::Levels:
		SelectWorld = EC::GetLevelDef(bAttract ? FMath::Clamp(Save->LastLevel, 0, NumLevels() - 1) : LevelIndex).World;
		if (bAttract == false) { StartAttract(0); }
		GoTo(EECScreen::LevelSelect);
		break;
	case EECAction::WorldPrev:
	case EECAction::WorldNext:
		SelectWorld = FMath::Clamp(SelectWorld + (B.Action == EECAction::WorldNext ? 1 : -1), 1, EC::NumWorlds());
		Focus = 0;
		break;
	case EECAction::Back: Back(); break;
	case EECAction::SelectLevel: if (B.Param < Save->UnlockedLevels) { StartLevel(B.Param); } break;
	case EECAction::Resume: GoTo(EECScreen::Playing); break;
	case EECAction::Restart:
		if (Screen == EECScreen::Paused) { GoTo(EECScreen::Playing); }
		if (Fx == EECFx::OutOfLoops) { Fx = EECFx::None; Sim.RestartLevel(); RunTime = 0; OnLoopReset(); }
		else if (Fx == EECFx::None) { BeginFx(EECFx::Restart); }
		break;
	case EECAction::RetryLoop:
		if (Fx == EECFx::OutOfLoops) { Fx = EECFx::None; Sim.RetryLoop(); OnLoopReset(); }
		break;
	case EECAction::Rewind:
		// Same as the keyboard: queue a rewind for the next tick (the sim decides whether it is allowed).
		if (Fx == EECFx::None && Sim.Phase == EC::EPhase::Playing)
		{
			EC::FInput In;
			In.Rewind = true;
			PrevPX = Sim.Player.X; PrevPY = Sim.Player.Y;
			Sim.Step(In);
			if (Sim.Phase == EC::EPhase::LoopEnded) { BeginFx(EECFx::Rewind); }
			else if (Sim.Phase == EC::EPhase::OutOfLoops) { BeginFx(EECFx::OutOfLoops); }
		}
		break;
	case EECAction::Pause: GoTo(EECScreen::Paused); break;
	case EECAction::NextLevel: StartLevel(FMath::Min(LevelIndex + 1, NumLevels() - 1)); break;
	case EECAction::Replay: StartLevel(LevelIndex); break;
	case EECAction::ToTitle: StartAttract(0); GoTo(EECScreen::Title); break;
	case EECAction::CycleTouch:
		Save->TouchMode = (EECTouchMode)(((int32)Save->TouchMode + 1) % 3);
		SaveProgress();
		break;
	case EECAction::Quit: SaveProgress(); bQuitRequested = true; break;
	case EECAction::Hint:
		// The walkthrough starts from loop 1, so the plan of the ghost matches the Echoes on screen.
		bHintActive = true;
		if (Screen == EECScreen::Paused) { GoTo(EECScreen::Playing); }
		if (Fx == EECFx::OutOfLoops) { Fx = EECFx::None; Sim.RestartLevel(); RunTime = 0; OnLoopReset(); }
		else if (Sim.NumEchoes > 0 && Fx == EECFx::None) { BeginFx(EECFx::Restart); --FailCount; }
		else { BuildHint(); }
		break;
	case EECAction::Skip:
		if (SkipAvailable())
		{
			Save->UnlockedLevels = FMath::Clamp(FMath::Max(Save->UnlockedLevels, LevelIndex + 2), 1, NumLevels());
			StartLevel(LevelIndex + 1);
		}
		break;
	case EECAction::ToggleMusic: Save->bMusic = !Save->bMusic; SaveProgress(); break;
	case EECAction::ToggleSound: Save->bSound = !Save->bSound; SaveProgress(); break;
	default: break;
	}
}

void FECGame::HandleMenu(const FECMenuInput& Menu)
{
	if (Screen == EECScreen::Playing && !bAttract && (Menu.Pause || Menu.Back) && Fx != EECFx::OutOfLoops)
	{
		GoTo(EECScreen::Paused);
		return;
	}

	const int32 N = Buttons.Num();
	if (Menu.bPointerMoved || Menu.bClick) { bPointerActive = true; }

	int32 Hover = -1;
	if (Menu.bPointerValid)
	{
		for (int32 I = 0; I < N; ++I)
		{
			if (Buttons[I].bEnabled && Buttons[I].Box.IsInside(Menu.Pointer)) { Hover = I; break; }
		}
	}
	if (Menu.bPointerMoved && Hover >= 0 && Hover != Focus) { Focus = Hover; UiSound(EECSound::UiMove); }
	if (Menu.bClick && Hover >= 0)
	{
		const FECButton B = Buttons[Hover];
		Activate(B);
		return;
	}

	// Keyboard / gamepad navigation (during play only the out-of-loops panel takes it).
	if (N == 0 || (Screen == EECScreen::Playing && Fx != EECFx::OutOfLoops)) { return; }
	if (Menu.Up || Menu.Down || Menu.Left || Menu.Right || Menu.Confirm) { bPointerActive = false; }
	auto Move = [&](int32 Delta)
	{
		for (int32 Tries = 0; Tries < N; ++Tries)
		{
			Focus = (Focus + Delta + N) % N;
			if (Buttons[Focus].bEnabled) { break; }
		}
		UiSound(EECSound::UiMove);
	};
	if (Menu.Up || Menu.Left) { Move(-1); }
	if (Menu.Down || Menu.Right) { Move(1); }
	Focus = FMath::Clamp(Focus, 0, N - 1);
	if (Menu.Confirm && Buttons.IsValidIndex(Focus))
	{
		const FECButton B = Buttons[Focus];
		Activate(B);
		return;
	}
	if (Menu.Back) { Back(); }
}
