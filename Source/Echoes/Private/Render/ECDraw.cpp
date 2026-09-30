// ECHOES: batched 2D triangle drawing on a UCanvas. (CLAUDE.md: Rendering)
#include "Render/ECDraw.h"

#include "CanvasItem.h"
#include "RenderUtils.h"

FLinearColor ECColor(uint32 RGB, float Alpha)
{
	FLinearColor C = FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xff, (RGB >> 8) & 0xff, RGB & 0xff, 255));
	C.A = Alpha;
	return C;
}

FECDraw::FECDraw(UCanvas* InCanvas)
	: Canvas(InCanvas)
{
	ScreenW = Canvas->ClipX;
	ScreenH = Canvas->ClipY;
	Batch.Reserve(8192);
}

FECDraw::~FECDraw()
{
	Flush();
}

void FECDraw::SetTransform(double InScale, double InOriginX, double InOriginY, bool bInFlipY)
{
	Scale = InScale;
	OriginX = InOriginX;
	OriginY = InOriginY;
	bFlipY = bInFlipY;
}

void FECDraw::SetBlend(ESimpleElementBlendMode Mode)
{
	if (Mode != Blend)
	{
		Flush();
		Blend = Mode;
	}
}

void FECDraw::Flush()
{
	if (Batch.Num() == 0 || !Canvas || !Canvas->Canvas) { Batch.Reset(); return; }
	FCanvasTriangleItem Item(Batch, GWhiteTexture);
	Item.BlendMode = Blend;
	Canvas->DrawItem(Item);
	Batch.Reset();
}

void FECDraw::Push(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC)
{
	FCanvasUVTri& T = Batch.AddDefaulted_GetRef();
	T.V0_Pos = A; T.V1_Pos = B; T.V2_Pos = C;
	T.V0_UV = T.V1_UV = T.V2_UV = FVector2D(0.5, 0.5);
	if (Blend == SE_BLEND_Additive)
	{
		// The additive blend is One/One: alpha is ignored, so fold it into the colour.
		T.V0_Color = FLinearColor(CA.R * CA.A, CA.G * CA.A, CA.B * CA.A, 1.f);
		T.V1_Color = FLinearColor(CB.R * CB.A, CB.G * CB.A, CB.B * CB.A, 1.f);
		T.V2_Color = FLinearColor(CC.R * CC.A, CC.G * CC.A, CC.B * CC.A, 1.f);
	}
	else
	{
		T.V0_Color = CA; T.V1_Color = CB; T.V2_Color = CC;
	}
	if (Batch.Num() >= 16000) { Flush(); }
}

void FECDraw::Tri(double AX, double AY, double BX, double BY, double CX, double CY, const FLinearColor& Color)
{
	Push(ToScreen(AX, AY), ToScreen(BX, BY), ToScreen(CX, CY), Color, Color, Color);
}

void FECDraw::TriColors(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& CA, const FLinearColor& CB, const FLinearColor& CC)
{
	Push(ToScreen(A.X, A.Y), ToScreen(B.X, B.Y), ToScreen(C.X, C.Y), CA, CB, CC);
}

void FECDraw::Quad(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, const FLinearColor& Color)
{
	const FVector2D SA = ToScreen(A.X, A.Y), SB = ToScreen(B.X, B.Y), SC = ToScreen(C.X, C.Y), SD = ToScreen(D.X, D.Y);
	Push(SA, SB, SC, Color, Color, Color);
	Push(SA, SC, SD, Color, Color, Color);
}

void FECDraw::Rect(double X0, double Y0, double X1, double Y1, const FLinearColor& Color)
{
	RectV(X0, Y0, X1, Y1, Color, Color);
}

void FECDraw::RectV(double X0, double Y0, double X1, double Y1, const FLinearColor& AtY0, const FLinearColor& AtY1)
{
	const FVector2D A = ToScreen(X0, Y0), B = ToScreen(X1, Y0), C = ToScreen(X1, Y1), D = ToScreen(X0, Y1);
	Push(A, B, C, AtY0, AtY0, AtY1);
	Push(A, C, D, AtY0, AtY1, AtY1);
}

void FECDraw::RectH(double X0, double Y0, double X1, double Y1, const FLinearColor& AtX0, const FLinearColor& AtX1)
{
	const FVector2D A = ToScreen(X0, Y0), B = ToScreen(X1, Y0), C = ToScreen(X1, Y1), D = ToScreen(X0, Y1);
	Push(A, B, C, AtX0, AtX1, AtX1);
	Push(A, C, D, AtX0, AtX1, AtX0);
}

void FECDraw::Circle(double CX, double CY, double R, const FLinearColor& Color, int Segments)
{
	Ellipse(CX, CY, R, R, Color, Segments, 0);
}

void FECDraw::Ellipse(double CX, double CY, double RX, double RY, const FLinearColor& Color, int Segments, double Rotation)
{
	const FVector2D C = ToScreen(CX, CY);
	const double CR = FMath::Cos(Rotation), SR = FMath::Sin(Rotation);
	auto P = [&](int I)
	{
		const double A = 2.0 * PI * I / Segments;
		const double LX = FMath::Cos(A) * RX, LY = FMath::Sin(A) * RY;
		return ToScreen(CX + LX * CR - LY * SR, CY + LX * SR + LY * CR);
	};
	FVector2D Prev = P(0);
	for (int I = 1; I <= Segments; ++I)
	{
		const FVector2D Next = P(I);
		Push(C, Prev, Next, Color, Color, Color);
		Prev = Next;
	}
}

void FECDraw::Glow(double CX, double CY, double R, const FLinearColor& Center, const FLinearColor& Edge, int Segments)
{
	// Concentric rings with a (1 - r)^2 falloff, so the edge fades out with no visible rim.
	static const double Radii[] = { 0.0, 0.1, 0.24, 0.42, 0.62, 0.82, 1.0 };
	constexpr int NumRings = UE_ARRAY_COUNT(Radii);
	FLinearColor Cols[NumRings];
	for (int K = 0; K < NumRings; ++K)
	{
		const float F = (float)FMath::Square(1.0 - Radii[K]);
		Cols[K] = Center * F + Edge * (1.f - F);
	}
	for (int I = 0; I < Segments; ++I)
	{
		const double A0 = 2.0 * PI * I / Segments, A1 = 2.0 * PI * (I + 1) / Segments;
		const double C0 = FMath::Cos(A0), S0 = FMath::Sin(A0), C1 = FMath::Cos(A1), S1 = FMath::Sin(A1);
		for (int K = 0; K + 1 < NumRings; ++K)
		{
			const double RA = R * Radii[K], RB = R * Radii[K + 1];
			const FVector2D In0 = ToScreen(CX + C0 * RA, CY + S0 * RA), In1 = ToScreen(CX + C1 * RA, CY + S1 * RA);
			const FVector2D Out0 = ToScreen(CX + C0 * RB, CY + S0 * RB), Out1 = ToScreen(CX + C1 * RB, CY + S1 * RB);
			if (K == 0) { Push(In0, Out0, Out1, Cols[0], Cols[1], Cols[1]); continue; }
			Push(In0, Out0, Out1, Cols[K], Cols[K + 1], Cols[K + 1]);
			Push(In0, Out1, In1, Cols[K], Cols[K + 1], Cols[K]);
		}
	}
}

void FECDraw::Ring(double CX, double CY, double R0, double R1, const FLinearColor& C0, const FLinearColor& C1, int Segments)
{
	for (int I = 0; I < Segments; ++I)
	{
		const double A0 = 2.0 * PI * I / Segments, A1 = 2.0 * PI * (I + 1) / Segments;
		const FVector2D In0 = ToScreen(CX + FMath::Cos(A0) * R0, CY + FMath::Sin(A0) * R0);
		const FVector2D In1 = ToScreen(CX + FMath::Cos(A1) * R0, CY + FMath::Sin(A1) * R0);
		const FVector2D Out0 = ToScreen(CX + FMath::Cos(A0) * R1, CY + FMath::Sin(A0) * R1);
		const FVector2D Out1 = ToScreen(CX + FMath::Cos(A1) * R1, CY + FMath::Sin(A1) * R1);
		Push(In0, Out0, Out1, C0, C1, C1);
		Push(In0, Out1, In1, C0, C1, C0);
	}
}

void FECDraw::Arc(double CX, double CY, double R0, double R1, double A0, double A1, const FLinearColor& Color, int Segments)
{
	const int N = FMath::Max(1, FMath::CeilToInt(Segments * FMath::Abs(A1 - A0) / (2.0 * PI)));
	for (int I = 0; I < N; ++I)
	{
		const double T0 = FMath::Lerp(A0, A1, double(I) / N), T1 = FMath::Lerp(A0, A1, double(I + 1) / N);
		const FVector2D In0 = ToScreen(CX + FMath::Cos(T0) * R0, CY + FMath::Sin(T0) * R0);
		const FVector2D In1 = ToScreen(CX + FMath::Cos(T1) * R0, CY + FMath::Sin(T1) * R0);
		const FVector2D Out0 = ToScreen(CX + FMath::Cos(T0) * R1, CY + FMath::Sin(T0) * R1);
		const FVector2D Out1 = ToScreen(CX + FMath::Cos(T1) * R1, CY + FMath::Sin(T1) * R1);
		Push(In0, Out0, Out1, Color, Color, Color);
		Push(In0, Out1, In1, Color, Color, Color);
	}
}

void FECDraw::Line(double AX, double AY, double BX, double BY, double Width, const FLinearColor& Color)
{
	TaperLine(AX, AY, BX, BY, Width, Width, Color);
}

void FECDraw::TaperLine(double AX, double AY, double BX, double BY, double WA, double WB, const FLinearColor& Color)
{
	const FVector2D D(BX - AX, BY - AY);
	const double Len = D.Size();
	if (Len < 1e-9) { return; }
	const FVector2D N(-D.Y / Len, D.X / Len);
	Quad(FVector2D(AX, AY) + N * (WA * 0.5), FVector2D(BX, BY) + N * (WB * 0.5),
	     FVector2D(BX, BY) - N * (WB * 0.5), FVector2D(AX, AY) - N * (WA * 0.5), Color);
}

void FECDraw::Poly(const FVector2D* Points, int32 Num, const FLinearColor& Color)
{
	if (Num < 3) { return; }
	const FVector2D P0 = ToScreen(Points[0].X, Points[0].Y);
	for (int32 I = 1; I + 1 < Num; ++I)
	{
		Push(P0, ToScreen(Points[I].X, Points[I].Y), ToScreen(Points[I + 1].X, Points[I + 1].Y), Color, Color, Color);
	}
}

void FECDraw::PolyOutline(const FVector2D* Points, int32 Num, double Width, const FLinearColor& Color)
{
	for (int32 I = 0; I < Num; ++I)
	{
		const FVector2D& A = Points[I];
		const FVector2D& B = Points[(I + 1) % Num];
		Line(A.X, A.Y, B.X, B.Y, Width, Color);
	}
}
