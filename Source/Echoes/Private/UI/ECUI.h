// ECHOES: menus, HUD (loop timer, loop pips, rewind), touch pads. (CLAUDE.md: UI)
#pragma once

#include "CoreMinimal.h"

class FECDraw;
class FECGame;
class UCanvas;
class UFont;

// On-screen touch pads. Shared by the controller (hit testing) and the UI (drawing).
struct FECTouchLayout
{
	FVector2D Left, Right, Jump;   // centres (pixels)
	double Radius = 0, JumpRadius = 0;
	double W = 0, H = 0;

	static FECTouchLayout Compute(double W, double H, const FVector4& Safe);
	bool HitLeft(const FVector2D& P) const;
	bool HitRight(const FVector2D& P) const;
	bool HitJump(const FVector2D& P) const;
};

struct FECUiContext
{
	UFont* Font = nullptr;
	bool bShowTouch = false;
	bool bTouchDevice = false;
	bool bLeftDown = false, bRightDown = false, bJumpDown = false;
	FVector4 Safe = FVector4(0, 0, 0, 0);   // left, top, right, bottom insets (pixels)
};

class FECUI
{
public:
	static void Draw(FECDraw& D, UCanvas* Canvas, FECGame& Game, const FECUiContext& Ctx);
};
