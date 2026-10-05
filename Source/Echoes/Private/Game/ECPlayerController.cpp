// ECHOES: input, lifecycle, console commands and the capture script. (CLAUDE.md: Game flow / Input)
#include "Game/ECPlayerController.h"

#include "Audio/ECAudioSynth.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Game/ECSaveGame.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UI/ECUI.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogEchoes, Log, All);

namespace
{
	AECPlayerController* FindController(UWorld* World)
	{
		return World ? Cast<AECPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdPlay(TEXT("ec.Play"), TEXT("ec.Play <level>  start a level (1-based)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AECPlayerController* PC = FindController(World)) { PC->Game->StartLevel(Args.Num() > 0 ? FCString::Atoi(*Args[0]) - 1 : 0); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAutopilot(TEXT("ec.Autopilot"), TEXT("ec.Autopilot 0|1|2  autopilot plays (2 = the shard solution)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AECPlayerController* PC = FindController(World))
			{
				const int32 Mode = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
				PC->Game->bAutopilotInPlay = Mode != 0;
				PC->Game->bPilotShardPlan = Mode == 2;
				PC->Game->StartLevel(PC->Game->LevelIndex);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdUnlock(TEXT("ec.UnlockAll"), TEXT("Unlock every level"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (AECPlayerController* PC = FindController(World)) { PC->Save->UnlockedLevels = PC->Game->NumLevels(); PC->Game->SaveProgress(); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdReset(TEXT("ec.ResetProgress"), TEXT("Forget all progress (keeps settings)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (AECPlayerController* PC = FindController(World))
			{
				PC->Save->UnlockedLevels = 1;
				PC->Save->LastLevel = 0;
				for (uint8& S : PC->Save->Stars) { S = 0; }
				for (int32& L : PC->Save->BestLoops) { L = 0; }
				PC->Game->SaveProgress();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdParadox(TEXT("ec.Debug.Paradox"), TEXT("Show a paradox (an Echo walks into a closed door)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (AECPlayerController* PC = FindController(World)) { PC->Game->DebugParadox(); }
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdHideUI(TEXT("ec.HideUI"), TEXT("ec.HideUI 0|1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AECPlayerController* PC = FindController(World)) { PC->Game->bHideUI = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0; }
		}));

	// Capture script: each step sets something up, waits for a moment worth seeing, and takes a shot.
	enum class ESetup : uint8 { None, Title, Levels, Level, Paused, Paradox, OutOfLoops, Hint };
	enum class EWait : uint8 { Time, Tick, Fx, Complete };

	struct FCaptureShot
	{
		ESetup Setup;
		int32 Level;
		bool bHideUI;
		EWait Wait;
		int32 Echoes;       // EWait::Tick: this many Echoes...
		int32 Tick;         // ...and at least this loop tick
		EECFx Fx;           // EWait::Fx: this effect, at least...
		double Value;       // ...this far through it (or seconds for EWait::Time / Complete)
		const TCHAR* Name;
	};

	const FCaptureShot CaptureScript[] = {
		{ ESetup::Title, 0, false, EWait::Time, 0, 0, EECFx::None, 3.0, TEXT("01_title") },
		{ ESetup::Levels, 0, false, EWait::Time, 0, 0, EECFx::None, 0.8, TEXT("02_levels") },
		{ ESetup::Levels, 1, false, EWait::Time, 0, 0, EECFx::None, 0.8, TEXT("03_levels_factory") },
		{ ESetup::Level, 0, false, EWait::Tick, 0, 20, EECFx::None, 0, TEXT("10_l1_intro") },
		{ ESetup::None, 0, false, EWait::Fx, 0, 0, EECFx::Rewind, 0.5, TEXT("11_l1_rewind") },
		{ ESetup::None, 0, false, EWait::Tick, 2, 40, EECFx::None, 0, TEXT("12_l1_on_echo") },
		{ ESetup::None, 0, false, EWait::Tick, 2, 120, EECFx::None, 0, TEXT("13_l1_door_held") },
		{ ESetup::None, 0, false, EWait::Fx, 0, 0, EECFx::Solve, 0.3, TEXT("14_l1_solve") },
		{ ESetup::None, 0, false, EWait::Complete, 0, 0, EECFx::None, 1.8, TEXT("15_complete") },
		{ ESetup::Level, 1, false, EWait::Tick, 2, 62, EECFx::None, 0, TEXT("20_l2_stack") },
		{ ESetup::Level, 2, false, EWait::Tick, 3, 80, EECFx::None, 0, TEXT("30_l3_echoes") },
		{ ESetup::None, 2, true, EWait::Tick, 3, 125, EECFx::None, 0, TEXT("31_l3_clean") },
		{ ESetup::Paused, 2, false, EWait::Time, 0, 0, EECFx::None, 0.5, TEXT("32_paused") },
		{ ESetup::Level, 9, false, EWait::Tick, 3, 150, EECFx::None, 0, TEXT("33_clockwork") },
		{ ESetup::Level, 10, false, EWait::Tick, 1, 112, EECFx::None, 0, TEXT("50_hotwire") },
		{ ESetup::Level, 11, false, EWait::Tick, 1, 30, EECFx::None, 0, TEXT("51_curtain") },
		{ ESetup::Level, 12, false, EWait::Tick, 2, 110, EECFx::None, 0, TEXT("52_crossfire") },
		{ ESetup::Level, 13, false, EWait::Tick, 2, 140, EECFx::None, 0, TEXT("53_stairwell") },
		{ ESetup::Level, 14, false, EWait::Tick, 2, 180, EECFx::None, 0, TEXT("54_walkway") },
		{ ESetup::Level, 15, false, EWait::Tick, 3, 110, EECFx::None, 0, TEXT("55_assembly") },
		{ ESetup::Level, 19, false, EWait::Tick, 3, 125, EECFx::None, 0, TEXT("56_core") },
		{ ESetup::Level, 15, true, EWait::Tick, 3, 110, EECFx::None, 0, TEXT("57_assembly_clean") },
		{ ESetup::Level, 19, true, EWait::Tick, 3, 125, EECFx::None, 0, TEXT("58_core_clean") },
		{ ESetup::Hint, 7, false, EWait::Tick, 0, 45, EECFx::None, 0, TEXT("60_hint") },
		{ ESetup::Paradox, 0, false, EWait::Fx, 0, 0, EECFx::Paradox, 0.3, TEXT("40_paradox") },
		{ ESetup::OutOfLoops, 0, false, EWait::Time, 0, 0, EECFx::None, 0.8, TEXT("41_out_of_loops") },
	};
}

AECPlayerController::AECPlayerController()
	: Game(MakeUnique<FECGame>())
{
	bShowMouseCursor = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	Audio = CreateDefaultSubobject<UECAudioSynth>(TEXT("Synth"));
	Audio->SetupAttachment(RootComponent);
}

void AECPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) { return; }

	ActivateTouchInterface(nullptr);   // remove any virtual joystick another path may have added

	Save = Cast<UECSaveGame>(UGameplayStatics::LoadGameFromSlot(UECSaveGame::SlotName, 0));
	if (!Save) { Save = Cast<UECSaveGame>(UGameplayStatics::CreateSaveGameObject(UECSaveGame::StaticClass())); }
	if (Audio) { Audio->Start(); }
	Game->Init(Save, Audio);

	// Nothing in the 3D world is drawn; the HUD paints everything.
	if (UGameViewportClient* VC = GetWorld()->GetGameViewport()) { VC->bDisableWorldRendering = true; }

	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &AECPlayerController::HandleBackground);
	DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &AECPlayerController::HandleBackground);

	const TCHAR* Cmd = FCommandLine::Get();
	bForceTouch = FParse::Param(Cmd, TEXT("ECForceTouch"));
	int32 StartLevelArg = 0;
	if (FParse::Value(Cmd, TEXT("ECLevel="), StartLevelArg)) { Game->StartLevel(StartLevelArg - 1); }
	if (FParse::Param(Cmd, TEXT("ECCapture")))
	{
		bCapture = true;
		Game->bNoSave = true;
		Game->bAutopilotInPlay = true;
		Game->bPilotShardPlan = true;
		FParse::Value(Cmd, TEXT("ECCaptureTag="), CaptureTag);
		Save->UnlockedLevels = Game->NumLevels();
		for (int32 I = 0; I < Save->Stars.Num(); ++I) { Save->Stars[I] = I == 0 ? 7 : (I == 1 ? 3 : 0); }
	}
}

void AECPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundHandle);
	FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
	if (Save) { Game->SaveProgress(); }
	Super::EndPlay(Reason);
}

void AECPlayerController::HandleBackground()
{
	Game->OnAppBackground();
}

bool AECPlayerController::IsTouchDevice() const
{
#if PLATFORM_ANDROID || PLATFORM_IOS
	return true;
#else
	return false;
#endif
}

bool AECPlayerController::ShouldShowTouch() const
{
	if (bForceTouch) { return true; }
	if (!Save) { return IsTouchDevice(); }
	switch (Save->TouchMode)
	{
	case EECTouchMode::On: return true;
	case EECTouchMode::Off: return false;
	default: return IsTouchDevice() || bTouchSeen;
	}
}

void AECPlayerController::UpdateSafeArea(double W, double H)
{
	SafeAreaTimer -= 1.0;
	if (SafeAreaTimer > 0) { return; }
	SafeAreaTimer = 60;
	FDisplayMetrics Metrics;
	FDisplayMetrics::RebuildDisplayMetrics(Metrics);
	const double Min = H * 0.02;
	SafeArea = FVector4(FMath::Max((double)Metrics.TitleSafePaddingSize.X, Min), FMath::Max((double)Metrics.TitleSafePaddingSize.Y, Min),
	                    FMath::Max((double)Metrics.TitleSafePaddingSize.Z, Min), FMath::Max((double)Metrics.TitleSafePaddingSize.W, Min));
}

void AECPlayerController::GatherInput(FECControls& Controls, FECMenuInput& Menu)
{
	auto Down = [&](const FKey& K) { return IsInputKeyDown(K); };
	auto Pressed = [&](const FKey& K) { return WasInputKeyJustPressed(K); };

	// Keyboard + gamepad
	const float StickX = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	const float StickY = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	const bool bLeft = Down(EKeys::Left) || Down(EKeys::A) || Down(EKeys::Gamepad_DPad_Left) || StickX < -0.35f;
	const bool bRight = Down(EKeys::Right) || Down(EKeys::D) || Down(EKeys::Gamepad_DPad_Right) || StickX > 0.35f;
	const bool bJump = Down(EKeys::SpaceBar) || Down(EKeys::W) || Down(EKeys::Up) || Down(EKeys::Gamepad_FaceButton_Bottom);
	Controls.Rewind = Pressed(EKeys::R) || Pressed(EKeys::Gamepad_FaceButton_Top) || Pressed(EKeys::Gamepad_RightShoulder);
	Controls.Restart = Pressed(EKeys::T) || Pressed(EKeys::Gamepad_Special_Left);

	Menu.Up = Pressed(EKeys::Up) || Pressed(EKeys::W) || Pressed(EKeys::Gamepad_DPad_Up) || (StickY > 0.5f && StickPrevY <= 0.5f);
	Menu.Down = Pressed(EKeys::Down) || Pressed(EKeys::S) || Pressed(EKeys::Gamepad_DPad_Down) || (StickY < -0.5f && StickPrevY >= -0.5f);
	Menu.Left = Pressed(EKeys::Left) || Pressed(EKeys::A) || Pressed(EKeys::Gamepad_DPad_Left) || (StickX < -0.5f && StickPrevX >= -0.5f);
	Menu.Right = Pressed(EKeys::Right) || Pressed(EKeys::D) || Pressed(EKeys::Gamepad_DPad_Right) || (StickX > 0.5f && StickPrevX <= 0.5f);
	Menu.Confirm = Pressed(EKeys::Enter) || Pressed(EKeys::SpaceBar) || Pressed(EKeys::Gamepad_FaceButton_Bottom);
	Menu.Back = Pressed(EKeys::Escape) || Pressed(EKeys::BackSpace) || Pressed(EKeys::Gamepad_FaceButton_Right) || Pressed(EKeys::Android_Back);
	Menu.Pause = Pressed(EKeys::P) || Pressed(EKeys::Gamepad_Special_Right);
	StickPrevX = StickX;
	StickPrevY = StickY;

	const bool bAnyKey = bLeft || bRight || bJump || Menu.Up || Menu.Down || Menu.Confirm || Menu.Back;
	if (bAnyKey && !IsTouchDevice()) { bTouchSeen = false; }   // Auto mode hides the pads again on keyboard use

	// Mouse (desktop only; on phones the "mouse" just mirrors the first finger)
	if (!IsTouchDevice())
	{
		double MX = 0, MY = 0;
		if (GetMousePosition(MX, MY))
		{
			const FVector2D M(MX, MY);
			Menu.bPointerValid = true;
			Menu.Pointer = M;
			Menu.bPointerMoved = LastMouse.X >= 0 && FVector2D::DistSquared(M, LastMouse) > 1.0;
			LastMouse = M;
			if (WasInputKeyJustReleased(EKeys::LeftMouseButton)) { Menu.bClick = true; }
		}
	}

	// Touch: the first contact decides whether a finger is a control (pads) or a tap on the UI.
	FVector2D Size(1280, 720);
	if (UGameViewportClient* VC = GetWorld()->GetGameViewport()) { VC->GetViewportSize(Size); }
	const FECTouchLayout Layout = FECTouchLayout::Compute(Size.X, Size.Y, SafeArea);
	const bool bControls = Game->IsGameplay() && Game->Fx != EECFx::OutOfLoops && ShouldShowTouch();
	bTouchLeft = bTouchRight = bTouchJump = false;
	for (int32 I = 0; I < MaxTouches; ++I)
	{
		double X = 0, Y = 0;
		bool bPressed = false;
		GetInputTouchState((ETouchIndex::Type)(ETouchIndex::Touch1 + I), X, Y, bPressed);
		const FVector2D P(X, Y);
		if (bPressed && !TouchDown[I])
		{
			bTouchSeen = true;
			TouchStart[I] = P;
			TouchIsControl[I] = bControls && (Layout.HitLeft(P) || Layout.HitRight(P) || Layout.HitJump(P));
			TouchStartedOnJump[I] = bControls && Layout.HitJump(P);
		}
		if (bPressed)
		{
			TouchLast[I] = P;
			if (TouchIsControl[I] && bControls)
			{
				if (TouchStartedOnJump[I]) { bTouchJump = true; }
				else if (Layout.HitLeft(P)) { bTouchLeft = true; }
				else if (Layout.HitRight(P)) { bTouchRight = true; }
			}
			else if (!TouchIsControl[I])
			{
				Menu.bPointerValid = true;
				Menu.Pointer = P;
				Menu.bPointerMoved = true;
			}
		}
		else if (TouchDown[I])
		{
			// A tap (not a control press) clicks whatever is under it.
			if (!TouchIsControl[I] && FVector2D::Distance(TouchStart[I], TouchLast[I]) < Size.Y * 0.06)
			{
				Menu.bPointerValid = true;
				Menu.Pointer = TouchLast[I];
				Menu.bClick = true;
			}
			TouchIsControl[I] = false;
			TouchStartedOnJump[I] = false;
		}
		TouchDown[I] = bPressed;
	}

	Controls.Dir = ((bRight || bTouchRight) ? 1 : 0) - ((bLeft || bTouchLeft) ? 1 : 0);
	Controls.Jump = bJump || bTouchJump;
}

void AECPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!Save) { return; }

	FVector2D Size(1280, 720);
	if (UGameViewportClient* VC = GetWorld()->GetGameViewport()) { VC->GetViewportSize(Size); }
	UpdateSafeArea(Size.X, Size.Y);

	FECControls Controls;
	FECMenuInput Menu;
	GatherInput(Controls, Menu);
	if (bCapture)
	{
		Menu = FECMenuInput();
		Controls = FECControls();
		TickCapture(DeltaTime);
	}
	Game->Tick(DeltaTime, Controls, Menu, Size.X, Size.Y);

	if (Game->bQuitRequested)
	{
		Game->bQuitRequested = false;
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
	}
}

bool AECPlayerController::CaptureConditionMet() const
{
	const FCaptureShot& S = CaptureScript[CaptureStep];
	switch (S.Wait)
	{
	case EWait::Time: return CaptureClock >= S.Value;
	case EWait::Tick: return Game->Fx == EECFx::None && Game->Sim.NumEchoes >= S.Echoes && Game->Sim.Tick >= S.Tick;
	case EWait::Fx: return Game->Fx == S.Fx && Game->FxProgress() >= S.Value;
	case EWait::Complete: return Game->Screen == EECScreen::Complete && Game->ScreenTime >= S.Value;
	}
	return true;
}

void AECPlayerController::TickCapture(float DeltaTime)
{
	constexpr int32 NumSteps = UE_ARRAY_COUNT(CaptureScript);
	CaptureClock += DeltaTime;
	if (CaptureStep >= NumSteps)
	{
		// Give the last screenshot a moment to be written, then quit.
		if (CaptureClock > 1.0 && CaptureStep == NumSteps)
		{
			++CaptureStep;
			ConsoleCommand(TEXT("quit"));
		}
		return;
	}

	if (CaptureStep >= 0 && !bCaptureShotTaken)
	{
		if (!CaptureConditionMet() && CaptureClock < 25.0) { return; }
		// Request the shot, then let this frame render before the next step changes anything.
		const FCaptureShot& S = CaptureScript[CaptureStep];
		const FString Name = CaptureTag.IsEmpty() ? FString(S.Name) : CaptureTag + TEXT("_") + S.Name;
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir() / (TEXT("Echoes_") + Name + TEXT(".png")));
		FScreenshotRequest::RequestScreenshot(Path, true, false);
		UE_LOG(LogEchoes, Display, TEXT("ECCapture: %s (level %d, loop %d, tick %d, fx %d)%s"), *Path, Game->LevelIndex + 1, Game->Sim.LoopsUsed(),
			Game->Sim.Tick, (int32)Game->Fx, CaptureClock >= 25.0 ? TEXT(" TIMED OUT") : TEXT(""));
		bCaptureShotTaken = true;
		CaptureClock = 0;
		return;
	}
	if (bCaptureShotTaken && CaptureClock < 0.3) { return; }
	bCaptureShotTaken = false;

	++CaptureStep;
	CaptureClock = 0;
	if (CaptureStep >= NumSteps) { return; }

	const FCaptureShot& S = CaptureScript[CaptureStep];
	Game->bHideUI = S.bHideUI;
	switch (S.Setup)
	{
	case ESetup::Title: Game->StartAttract(S.Level); Game->GoTo(EECScreen::Title); break;
	case ESetup::Levels: Game->SelectWorld = S.Level + 1; Game->GoTo(EECScreen::LevelSelect); break;
	case ESetup::Level: Game->bAutopilotInPlay = true; Game->StartLevel(S.Level); break;
	case ESetup::Paused: Game->GoTo(EECScreen::Paused); break;
	case ESetup::Hint:
	{
		Game->bAutopilotInPlay = false;
		Game->StartLevel(S.Level);
		FECButton B;
		B.Action = EECAction::Hint;
		Game->Activate(B);
		break;
	}
	case ESetup::Paradox: Game->DebugParadox(); break;
	case ESetup::OutOfLoops: Game->DebugOutOfLoops(); break;
	default: break;
	}
}
