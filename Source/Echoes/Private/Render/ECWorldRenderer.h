// ECHOES: draws the level, Subject 7, the Echoes and every loop effect with canvas triangles. (CLAUDE.md: Rendering / Art)
#pragma once

#include "CoreMinimal.h"
#include "Core/ECTypes.h"

class FECDraw;
class FECGame;

// World-to-screen mapping for the current frame (world y is up; OriginY is the world y at the top edge).
struct FECViewTransform
{
	double Scale = 1, OriginX = 0, OriginY = 0;
	FVector2D ToScreen(double X, double Y) const { return FVector2D((X - OriginX) * Scale, (OriginY - Y) * Scale); }
};

// Per-Echo look: colour plus its own time-stretched effect (so colour-blind players can tell them apart).
struct FECEchoStyle
{
	uint32 Color;
	int32 Effect;   // 0 soft trail, 1 chromatic split, 2 scanlines, 3 flicker, 4 wide afterimages, 5 blur
};

class FECWorldRenderer
{
public:
	static const FECEchoStyle& EchoStyle(int32 Index);
	static FLinearColor ChannelColor(int32 Channel, float Alpha = 1.f);

	// Draws the whole world into the screen. Returns the transform used (the UI anchors to it).
	FECViewTransform Draw(FECDraw& D, const FECGame& Game, bool bTouchMargin);

	struct FPose
	{
		double X = 0, Y = 0, VX = 0, VY = 0;
		int32 Facing = 1;
		EC::EAnim Anim = EC::EAnim::Idle;
	};

	// Subject 7 (Style < 0) or Echo n (Style = n). Alpha fades the whole figure.
	static void DrawCharacter(FECDraw& D, const FPose& P, int32 Style, double Time, double Alpha, double Pulse);

private:
	void DrawBackground(FECDraw& D, const FECGame& G, const FECViewTransform& V, double Time);
	void DrawTiles(FECDraw& D, const FECGame& G, double Time);
	void DrawPlatesAndLinks(FECDraw& D, const FECGame& G, const uint8 PlateBits, double Time);
	void DrawDoors(FECDraw& D, const FECGame& G, const float* DoorOpen, double Time);
	void DrawExit(FECDraw& D, const FECGame& G, double Time, double Solve);
	void DrawLasers(FECDraw& D, const FECGame& G, double Time);
	void DrawShard(FECDraw& D, const FECGame& G, double Time);
	void DrawActors(FECDraw& D, const FECGame& G, double Time);
	void DrawOverlays(FECDraw& D, const FECGame& G, double Time);
};
