// ECHOES: batched 2D triangle drawing on a UCanvas. (CLAUDE.md: Rendering)
// Everything in the game is drawn from these primitives - no textures, no imported assets.
#pragma once

#include "CoreMinimal.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"

class FECDraw
{
public:
	explicit FECDraw(UCanvas* InCanvas);
	~FECDraw();

	// World -> screen. With bFlipY (world y up), Screen = ((X - OriginX) * Scale, (OriginY - Y) * Scale),
	// so (OriginX, OriginY) is the world point at the top-left of the screen. Without it, units are pixels.
	void SetTransform(double InScale, double InOriginX, double InOriginY, bool bInFlipY);
	void SetPixels() { SetTransform(1, 0, 0, false); }
	FVector2D ToScreen(double X, double Y) const { return FVector2D((X - OriginX) * Scale, bFlipY ? (OriginY - Y) * Scale : (Y - OriginY) * Scale); }
	double GetScale() const { return Scale; }

	void SetBlend(ESimpleElementBlendMode Mode);
	void Additive() { SetBlend(SE_BLEND_Additive); }
	void Translucent() { SetBlend(SE_BLEND_Translucent); }
	void Flush();

	// Primitives in the current transform's units.
	void Tri(double AX, double AY, double BX, double BY, double CX, double CY, const FLinearColor& Color);
	void TriColors(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC);
	void Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& Color);
	void Rect(double X0, double Y0, double X1, double Y1, const FLinearColor& Color);
	void RectV(double X0, double Y0, double X1, double Y1, const FLinearColor& AtY0, const FLinearColor& AtY1);
	void RectH(double X0, double Y0, double X1, double Y1, const FLinearColor& AtX0, const FLinearColor& AtX1);
	void Circle(double CX, double CY, double R, const FLinearColor& Color, int Segments = 20);
	void Ellipse(double CX, double CY, double RX, double RY, const FLinearColor& Color, int Segments = 20, double Rotation = 0);
	void Glow(double CX, double CY, double R, const FLinearColor& Center, const FLinearColor& Edge, int Segments = 28);
	void Ring(double CX, double CY, double R0, double R1, const FLinearColor& C0, const FLinearColor& C1, int Segments = 40);
	void Arc(double CX, double CY, double R0, double R1, double A0, double A1, const FLinearColor& Color, int Segments = 40);
	void Line(double AX, double AY, double BX, double BY, double Width, const FLinearColor& Color);
	void TaperLine(double AX, double AY, double BX, double BY, double WA, double WB, const FLinearColor& Color);
	void Poly(const FVector2D* Points, int32 Num, const FLinearColor& Color);          // convex, fan from point 0
	void PolyOutline(const FVector2D* Points, int32 Num, double Width, const FLinearColor& Color);

	UCanvas* Canvas;
	double ScreenW = 0, ScreenH = 0;

private:
	void Push(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC);

	TArray<FCanvasUVTri> Batch;
	ESimpleElementBlendMode Blend = SE_BLEND_Translucent;
	double Scale = 1, OriginX = 0, OriginY = 0;
	bool bFlipY = false;
};

// sRGB hex -> linear colour with alpha.
FLinearColor ECColor(uint32 RGB, float Alpha = 1.f);
inline FLinearColor ECAlpha(FLinearColor C, double A) { C.A *= (float)FMath::Clamp(A, 0.0, 1.0); return C; }
