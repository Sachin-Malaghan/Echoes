// ECHOES: owns the game flow; gathers keyboard, gamepad, mouse and multi-touch input. (CLAUDE.md: Game flow / Input)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/ECGame.h"
#include "ECPlayerController.generated.h"

class UECSaveGame;
class UECAudioSynth;

UCLASS()
class AECPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AECPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void PlayerTick(float DeltaTime) override;
	// The game draws its own touch pads; never create Unreal's default virtual joysticks (they would
	// sit on top and swallow the touches meant for JUMP and the HUD buttons).
	virtual void CreateTouchInterface() override {}

	bool ShouldShowTouch() const;
	bool IsTouchDevice() const;

	// On the heap: the sim holds every Echo's recording (~80 KB), more than a UObject may embed.
	TUniquePtr<FECGame> Game;

	UPROPERTY() TObjectPtr<UECSaveGame> Save;
	UPROPERTY() TObjectPtr<UECAudioSynth> Audio;

	// Read by the HUD.
	bool bTouchLeft = false, bTouchRight = false, bTouchJump = false;
	FVector4 SafeArea = FVector4(0, 0, 0, 0);
	bool bForceTouch = false;

private:
	void GatherInput(FECControls& Controls, FECMenuInput& Menu);
	void UpdateSafeArea(double W, double H);
	void TickCapture(float DeltaTime);
	bool CaptureConditionMet() const;
	void HandleBackground();

	static constexpr int32 MaxTouches = 10;
	bool TouchDown[MaxTouches] = {};
	bool TouchIsControl[MaxTouches] = {};
	bool TouchStartedOnJump[MaxTouches] = {};
	FVector2D TouchStart[MaxTouches];
	FVector2D TouchLast[MaxTouches];
	bool bTouchSeen = false;
	FVector2D LastMouse = FVector2D(-1, -1);
	float StickPrevY = 0, StickPrevX = 0;
	double SafeAreaTimer = 0;
	FDelegateHandle BackgroundHandle, DeactivateHandle;

	// Capture script (-ECCapture): screenshots of every screen and effect, then quit.
	bool bCapture = false;
	int32 CaptureStep = -1;
	bool bCaptureShotTaken = false;
	double CaptureClock = 0;
	FString CaptureTag;
};
