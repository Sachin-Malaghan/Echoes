// ECHOES: world rendering. World 1 "The Lab": near-black blue glass, cyan accent, warm-white exit.
// Order: background -> warm exit light -> plates, doors, walls -> props -> Echoes -> player -> effects.
// (CLAUDE.md: Rendering / Art direction)
#include "Render/ECWorldRenderer.h"

#include "Core/ECSim.h"
#include "Game/ECGame.h"
#include "Render/ECDraw.h"

using namespace EC;

namespace
{
	// ---- Palette: World 1, The Lab -------------------------------------------------------------
	// One palette per world: near-black background, one saturated accent, one danger colour.
	struct FPalette { uint32 BgTop, BgBottom, Mid, WallFill, WallPanel, WallEdge, Accent, Danger, Lamp, Dust, Screen; };
	const FPalette GPalettes[] =
	{
		// World 1, The Lab: cold glass, cyan.
		{ 0x0A1220, 0x14243D, 0x1E3A5F, 0x0B1526, 0x0F1C31, 0x24466F, 0x3DF2FF, 0xFF4D5E, 0xBFF8FF, 0xCFF6FF, 0x0E2038 },
		// World 2, The Factory: furnace glow, orange, hot pink lasers.
		{ 0x160D0A, 0x2A1610, 0x4A2A1C, 0x120A07, 0x1D110B, 0x6A3A1E, 0xFF8A2B, 0xFF2E63, 0xFFD9A8, 0xFFD2A0, 0x2A160E },
	};
	// The current world's colours (set at the start of every frame).
	uint32 BgTop, BgBottom, Midground, WallFill, WallPanel, WallEdge, Accent, Danger, Lamp, Dust, Screen;
	int32 GPaletteWorld = 1;
	void UsePalette(int32 World)
	{
		GPaletteWorld = World;
		const FPalette& P = GPalettes[FMath::Clamp(World, 1, (int32)UE_ARRAY_COUNT(GPalettes)) - 1];
		BgTop = P.BgTop; BgBottom = P.BgBottom; Midground = P.Mid; WallFill = P.WallFill; WallPanel = P.WallPanel;
		WallEdge = P.WallEdge; Accent = P.Accent; Danger = P.Danger; Lamp = P.Lamp; Dust = P.Dust; Screen = P.Screen;
	}
	const uint32 PlayerCyan = 0x3DF2FF;   // Subject 7 and the shard are cyan in every world
	const uint32 ExitWarm = 0xFFF4E0, ExitGlow = 0xFFB070;
	const uint32 PlayerBody = 0xF4F7FF, PlayerCoat = 0xE6EDF8, PlayerShade = 0xB8C6DC, PlayerFace = 0x060B16;

	const FECEchoStyle GEchoStyles[] =
	{
		{ 0xFFB020, 0 },   // amber: soft glow, slow trail
		{ 0xFF3D9A, 1 },   // magenta: chromatic split
		{ 0x9B6BFF, 2 },   // violet: scanline shimmer
		{ 0xB6FF3D, 3 },   // lime: sparse flicker
		{ 0x7FD4FF, 4 },   // ice blue: wide afterimages
		{ 0xFFC2A8, 5 },   // rose gold: blur
	};
	const uint32 GChannels[] = { 0x2EE6C5, 0xA77BFF, 0xFFC857, 0xFF6FA0 };

	double Hash(int32 N)
	{
		uint32 X = (uint32)N * 747796405u + 2891336453u;
		X = ((X >> ((X >> 28u) + 4u)) ^ X) * 277803737u;
		X = (X >> 22u) ^ X;
		return (X & 0xFFFFFF) / double(0x1000000);
	}

	double Smooth(double T) { T = FMath::Clamp(T, 0.0, 1.0); return T * T * (3.0 - 2.0 * T); }

	void EllipseOutline(FECDraw& D, double CX, double CY, double RX, double RY, double W, const FLinearColor& C, int32 N = 22)
	{
		FVector2D Pts[32];
		N = FMath::Min(N, 32);
		for (int32 I = 0; I < N; ++I)
		{
			const double A = 2.0 * PI * I / N;
			Pts[I] = FVector2D(CX + FMath::Cos(A) * RX, CY + FMath::Sin(A) * RY);
		}
		D.PolyOutline(Pts, N, W, C);
	}

	// Vertical-gradient quad given as corner colours (used for light shafts).
	void Shaft(FECDraw& D, double TopX, double TopY, double TopHalf, double BotY, double BotHalf, const FLinearColor& Top, const FLinearColor& Bot)
	{
		const FVector2D A(TopX - TopHalf, TopY), B(TopX + TopHalf, TopY), C(TopX + BotHalf, BotY), E(TopX - BotHalf, BotY);
		D.TriColors(A, B, C, Top, Top, Bot);
		D.TriColors(A, C, E, Top, Bot, Bot);
	}
}

const FECEchoStyle& FECWorldRenderer::EchoStyle(int32 Index)
{
	return GEchoStyles[FMath::Clamp(Index, 0, (int32)UE_ARRAY_COUNT(GEchoStyles) - 1)];
}

FLinearColor FECWorldRenderer::ChannelColor(int32 Channel, float Alpha)
{
	return ECColor(GChannels[FMath::Clamp(Channel, 0, 3)], Alpha);
}

// =================================================================================================
// Subject 7 and the Echoes
// =================================================================================================

void FECWorldRenderer::DrawCharacter(FECDraw& D, const FPose& P, int32 Style, double Time, double Alpha, double Pulse)
{
	if (Alpha <= 0.01) { return; }
	const bool bEcho = Style >= 0;
	const FECEchoStyle* ES = bEcho ? &EchoStyle(Style) : nullptr;
	const double F = P.Facing ? 1.0 : -1.0;

	// Squash and stretch follow the motion: stretch with vertical speed, squash on landing.
	double SX = 1, SY = 1;
	if (P.Anim == EAnim::Land) { SX = 1.12; SY = 0.86; }
	else if (P.Anim == EAnim::Jump) { const double K = FMath::Clamp(P.VY / JumpSpeed, 0.0, 1.0); SY = 1.0 + 0.13 * K; SX = 1.0 - 0.08 * K; }
	else if (P.Anim == EAnim::Fall) { const double K = FMath::Clamp(-P.VY / MaxFall, 0.0, 1.0); SY = 1.0 + 0.11 * K; SX = 1.0 - 0.07 * K; }
	const double Speed = FMath::Clamp(FMath::Abs(P.VX) / RunSpeed, 0.0, 1.0);
	const bool bRun = P.Anim == EAnim::Run;
	// Lean into the run; in the air lean with the drift.
	const double Lean = bRun ? 0.075 * Speed : ((P.Anim == EAnim::Jump || P.Anim == EAnim::Fall) ? 0.045 * FMath::Clamp(P.VX / RunSpeed, -1.0, 1.0) * (P.Facing ? 1.0 : -1.0) : 0.0);
	const double Ph = P.X / 1.15 * 2.0 * PI * F;   // stride phase follows distance, so Echoes animate exactly
	const double Bob = bRun ? FMath::Abs(FMath::Sin(Ph)) * 0.035 * Speed : FMath::Sin(Time * 2.2) * 0.008;
	auto W = [&](double U, double V) { return FVector2D(P.X + (U + V * Lean) * F * SX, P.Y + V * SY); };

	// Limbs
	FVector2D Foot[2], Knee[2], Hand[2], Elbow[2];
	const FVector2D Hip = W(0.0, 0.62 + Bob);
	for (int32 K = 0; K < 2; ++K)
	{
		double U = 0, V = 0;
		const double LPh = Ph + K * PI;
		switch (P.Anim)
		{
		case EAnim::Run:  U = FMath::Sin(LPh) * 0.25 * Speed; V = FMath::Max(0.0, FMath::Cos(LPh)) * 0.17 * Speed; break;
		case EAnim::Jump: U = K ? -0.09 : 0.15; V = K ? 0.06 : 0.26; break;
		case EAnim::Fall: U = K ? -0.13 : 0.10; V = K ? 0.18 : 0.08; break;
		case EAnim::Land: U = K ? -0.13 : 0.13; V = 0; break;
		default:          U = K ? -0.07 : 0.09; V = 0; break;
		}
		Foot[K] = W(U, V + 0.02);
		Knee[K] = W(U * 0.5 + 0.07, 0.33 + V * 0.6 + Bob * 0.5);
		double A = 0;   // arm angle from straight down, positive = forward
		switch (P.Anim)
		{
		case EAnim::Run:  A = -FMath::Sin(LPh) * 0.75 * Speed + 0.1; break;
		case EAnim::Jump: A = K ? -0.7 : 2.3; break;
		case EAnim::Fall: A = K ? -1.1 : 1.5; break;
		default:          A = (K ? -0.08 : 0.14) + FMath::Sin(Time * 2.2 + K) * 0.03; break;
		}
		const FVector2D Sh = W(0.0, 1.06 + Bob);
		const FVector2D Dir(FMath::Sin(A) * F * SX, -FMath::Cos(A) * SY);
		Hand[K] = Sh + Dir * 0.34;
		Elbow[K] = Sh + Dir * 0.17 + FVector2D(0.03 * F, -0.01);
	}

	// Coat with a torn hem that trails behind when running and flutters in the air.
	const double Trail = FMath::Clamp(P.VX * F * 0.022, -0.04, 0.13) + FMath::Sin(Time * 9.0 + P.X * 3.0) * 0.014 + FMath::Clamp(-P.VY * 0.006, -0.05, 0.06);
	const FVector2D Coat[6] = { W(-0.19, 1.14 + Bob), W(0.17, 1.14 + Bob), W(0.2, 0.74 + Bob), W(0.25, 0.41), W(-0.27 - Trail, 0.38 + Trail * 0.4), W(-0.21, 0.74 + Bob) };
	FVector2D Teeth[5][3];
	for (int32 I = 0; I < 5; ++I)
	{
		const FVector2D A = FMath::Lerp(Coat[3], Coat[4], I / 5.0), B = FMath::Lerp(Coat[3], Coat[4], (I + 1) / 5.0);
		const double Len = 0.05 + 0.06 * Hash(I * 7 + 3) + (I >= 3 ? Trail * 0.25 : 0);
		Teeth[I][0] = A; Teeth[I][1] = B; Teeth[I][2] = (A + B) * 0.5 + FVector2D(-Trail * 0.35 * F, -Len * SY);
	}
	const FVector2D HoodC = W(0.0, 1.355 + Bob);
	const double HoodRX = 0.205 * SX, HoodRY = 0.215 * SY;
	const double Flap = FMath::Sin(Time * 7.0 + P.X) * 0.02 + Trail * 0.3;
	const FVector2D Peak[3] = { W(-0.13, 1.51 + Bob), W(-0.31 - Flap, 1.37 + Bob + Flap * 0.5), W(-0.11, 1.22 + Bob) };
	const FVector2D FaceC = W(0.085, 1.33 + Bob);
	const FVector2D Eye0 = W(0.075, 1.335 + Bob), Eye1 = W(0.155, 1.335 + Bob);
	const FVector2D Core = W(0.07, 0.97 + Bob);

	const FLinearColor Glow = bEcho ? ECColor(ES->Color) : ECColor(PlayerCyan);

	if (!bEcho)
	{
		// Rim light: the silhouette slightly enlarged in cyan, then the body on top.
		const FLinearColor Rim = ECColor(PlayerCyan, 0.55f * (float)Alpha);
		FVector2D Big[6];
		const FVector2D C0 = W(0.0, 0.78);
		for (int32 I = 0; I < 6; ++I) { Big[I] = C0 + (Coat[I] - C0) * 1.09; }
		D.Poly(Big, 6, Rim);
		D.Ellipse(HoodC.X, HoodC.Y, HoodRX + 0.035, HoodRY + 0.035, Rim, 22);
		for (int32 K = 0; K < 2; ++K)
		{
			D.TaperLine(Hip.X, Hip.Y, Knee[K].X, Knee[K].Y, 0.19, 0.16, Rim);
			D.TaperLine(Knee[K].X, Knee[K].Y, Foot[K].X, Foot[K].Y, 0.16, 0.14, Rim);
		}

		const FLinearColor Shade = ECColor(PlayerShade, (float)Alpha), Body = ECColor(PlayerBody, (float)Alpha), CoatC = ECColor(PlayerCoat, (float)Alpha);
		// Back limbs (in shade)
		D.TaperLine(Hip.X, Hip.Y, Knee[1].X, Knee[1].Y, 0.13, 0.11, Shade);
		D.TaperLine(Knee[1].X, Knee[1].Y, Foot[1].X, Foot[1].Y, 0.11, 0.09, Shade);
		D.Ellipse(Foot[1].X + 0.03 * F, Foot[1].Y, 0.09, 0.045, Shade, 10);
		const FVector2D ShB = W(0.0, 1.06 + Bob);
		D.TaperLine(ShB.X, ShB.Y, Elbow[1].X, Elbow[1].Y, 0.1, 0.085, Shade);
		D.TaperLine(Elbow[1].X, Elbow[1].Y, Hand[1].X, Hand[1].Y, 0.085, 0.07, Shade);
		// Front leg
		D.TaperLine(Hip.X, Hip.Y, Knee[0].X, Knee[0].Y, 0.13, 0.11, Body);
		D.TaperLine(Knee[0].X, Knee[0].Y, Foot[0].X, Foot[0].Y, 0.11, 0.09, Body);
		D.Ellipse(Foot[0].X + 0.03 * F, Foot[0].Y, 0.09, 0.045, Body, 10);
		// Coat, hem, a darker back panel and a belt
		D.Poly(Coat, 6, CoatC);
		for (int32 I = 0; I < 5; ++I) { D.Tri(Teeth[I][0].X, Teeth[I][0].Y, Teeth[I][1].X, Teeth[I][1].Y, Teeth[I][2].X, Teeth[I][2].Y, CoatC); }
		const FVector2D Back[4] = { Coat[0], W(-0.05, 1.14 + Bob), W(-0.08, 0.4), Coat[4] };
		D.Poly(Back, 4, ECColor(PlayerShade, 0.45f * (float)Alpha));
		const FVector2D BeltA = W(-0.215, 0.77 + Bob), BeltB = W(0.205, 0.79 + Bob);
		D.Line(BeltA.X, BeltA.Y, BeltB.X, BeltB.Y, 0.035, ECColor(0x8FA3BF, (float)Alpha));
		// Front arm
		D.TaperLine(ShB.X, ShB.Y, Elbow[0].X, Elbow[0].Y, 0.1, 0.085, Body);
		D.TaperLine(Elbow[0].X, Elbow[0].Y, Hand[0].X, Hand[0].Y, 0.085, 0.07, Body);
		D.Circle(Hand[0].X, Hand[0].Y, 0.045, Body, 8);
		// Hood, peak, dark face
		D.Tri(Peak[0].X, Peak[0].Y, Peak[1].X, Peak[1].Y, Peak[2].X, Peak[2].Y, Body);
		D.Ellipse(HoodC.X, HoodC.Y, HoodRX, HoodRY, Body, 22);
		D.Ellipse(FaceC.X, FaceC.Y, 0.118 * SX, 0.132 * SY, ECColor(PlayerFace, (float)Alpha), 18);
	}
	else
	{
		// Glass: translucent fill, bright edges.
		const FLinearColor Fill = ECColor(ES->Color, 0.2f * (float)Alpha);
		const FLinearColor Edge = ECColor(ES->Color, 0.92f * (float)Alpha);
		const FLinearColor Limb = ECColor(ES->Color, 0.55f * (float)Alpha);
		auto Figure = [&](const FVector2D& O, const FLinearColor& FillC, const FLinearColor& EdgeC, const FLinearColor& LimbC)
		{
			for (int32 K = 1; K >= 0; --K)
			{
				D.Line(Hip.X + O.X, Hip.Y + O.Y, Knee[K].X + O.X, Knee[K].Y + O.Y, 0.07, LimbC);
				D.Line(Knee[K].X + O.X, Knee[K].Y + O.Y, Foot[K].X + O.X, Foot[K].Y + O.Y, 0.06, LimbC);
			}
			FVector2D C[6];
			for (int32 I = 0; I < 6; ++I) { C[I] = Coat[I] + O; }
			D.Poly(C, 6, FillC);
			D.PolyOutline(C, 6, 0.035, EdgeC);
			for (int32 I = 0; I < 5; ++I)
			{
				D.Line(Teeth[I][0].X + O.X, Teeth[I][0].Y + O.Y, Teeth[I][2].X + O.X, Teeth[I][2].Y + O.Y, 0.03, EdgeC);
				D.Line(Teeth[I][2].X + O.X, Teeth[I][2].Y + O.Y, Teeth[I][1].X + O.X, Teeth[I][1].Y + O.Y, 0.03, EdgeC);
			}
			const FVector2D ShB = W(0.0, 1.06 + Bob) + O;
			for (int32 K = 0; K < 2; ++K)
			{
				D.Line(ShB.X, ShB.Y, Elbow[K].X + O.X, Elbow[K].Y + O.Y, 0.06, LimbC);
				D.Line(Elbow[K].X + O.X, Elbow[K].Y + O.Y, Hand[K].X + O.X, Hand[K].Y + O.Y, 0.05, LimbC);
			}
			D.Tri(Peak[0].X + O.X, Peak[0].Y + O.Y, Peak[1].X + O.X, Peak[1].Y + O.Y, Peak[2].X + O.X, Peak[2].Y + O.Y, FillC);
			D.Ellipse(HoodC.X + O.X, HoodC.Y + O.Y, HoodRX, HoodRY, FillC, 20);
			EllipseOutline(D, HoodC.X + O.X, HoodC.Y + O.Y, HoodRX, HoodRY, 0.035, EdgeC, 20);
		};

		if (ES->Effect == 5)
		{
			// Rose gold: heavy blur - several faint offset copies.
			for (int32 I = 0; I < 4; ++I)
			{
				const double A = I * PI * 0.5 + Time * 1.3;
				Figure(FVector2D(FMath::Cos(A), FMath::Sin(A)) * 0.045, ECAlpha(Fill, 0.4), ECAlpha(Edge, 0.25), ECAlpha(Limb, 0.3));
			}
		}
		Figure(FVector2D::ZeroVector, Fill, Edge, Limb);
		if (ES->Effect == 1)
		{
			// Magenta: chromatic split, red and blue ghosts pulled apart.
			D.Additive();
			const double Off = 0.045 + FMath::Sin(Time * 5.0) * 0.012;
			Figure(FVector2D(-Off, 0.005), ECColor(0xFF2040, 0.0f), ECColor(0xFF2040, 0.4f * (float)Alpha), ECColor(0xFF2040, 0.22f * (float)Alpha));
			Figure(FVector2D(Off, -0.005), ECColor(0x2060FF, 0.0f), ECColor(0x3070FF, 0.45f * (float)Alpha), ECColor(0x3070FF, 0.24f * (float)Alpha));
			D.Translucent();
		}
		if (ES->Effect == 2)
		{
			// Violet: scanlines rolling up through the figure.
			D.Additive();
			for (int32 I = 0; I < 14; ++I)
			{
				const double V = FMath::Fmod(I * 0.12 + Time * 0.35, 1.68);
				const double A = 0.12 + 0.12 * FMath::Sin(Time * 6.0 + I);
				const FVector2D L = W(-0.24, V), R = W(0.24, V);
				D.Line(L.X, L.Y, R.X, R.Y, 0.022, ECColor(ES->Color, (float)(A * Alpha)));
			}
			D.Translucent();
		}
		D.Ellipse(FaceC.X, FaceC.Y, 0.118 * SX, 0.132 * SY, ECColor(0x05080F, 0.55f * (float)Alpha), 18);
	}

	// Eyes and the chest time-core (pulses with the loop clock).
	D.Additive();
	D.Glow(Core.X, Core.Y, 0.28 + 0.1 * Pulse, ECAlpha(Glow, (0.35 + 0.35 * Pulse) * Alpha), ECAlpha(Glow, 0), 18);
	D.Glow((Eye0.X + Eye1.X) * 0.5, Eye0.Y, 0.2, ECAlpha(Glow, 0.3 * Alpha), ECAlpha(Glow, 0), 14);
	D.Translucent();
	const FLinearColor Bright = FMath::Lerp(Glow, FLinearColor::White, 0.55f);
	D.Circle(Core.X, Core.Y, 0.042, ECAlpha(Bright, Alpha), 10);
	const double Blink = FMath::Fmod(Time + (bEcho ? Style * 1.7 : 0.0), 4.1) < 0.12 ? 0.25 : 1.0;
	D.Ellipse(Eye0.X, Eye0.Y, 0.024, 0.034 * Blink, ECAlpha(Bright, Alpha), 8);
	D.Ellipse(Eye1.X, Eye1.Y, 0.022, 0.032 * Blink, ECAlpha(Bright, Alpha), 8);
}

// =================================================================================================
// Frame
// =================================================================================================

FECViewTransform FECWorldRenderer::Draw(FECDraw& D, const FECGame& G, bool bTouchMargin)
{
	const FLevel& L = G.Sim.Level;
	FECViewTransform V;
	// Fit the level with a little breathing room; on touch screens leave space under the floor for the pads.
	const double MarginY = bTouchMargin ? 1.3 : 0.5;
	V.Scale = FMath::Min(D.ScreenH / (L.H + MarginY), D.ScreenW / (L.W + 0.4));
	const double ShakeX = FMath::Sin(G.RealTime * 71.0) * G.Shake * 0.06;
	const double ShakeY = FMath::Cos(G.RealTime * 53.0) * G.Shake * 0.05;
	V.OriginX = L.W * 0.5 - D.ScreenW * 0.5 / V.Scale + ShakeX;
	V.OriginY = L.H * 0.5 + (bTouchMargin ? MarginY * 0.5 - 0.15 : 0.0) + D.ScreenH * 0.5 / V.Scale + ShakeY;
	if (G.Fx == EECFx::Rewind || G.Fx == EECFx::Restart)
	{
		// VHS tracking wobble.
		V.OriginX += FMath::Sin(G.FxTime * 43.0) * 0.03 * (1.0 - G.FxProgress());
	}
	D.SetTransform(V.Scale, V.OriginX, V.OriginY, true);

	const double Time = G.RealTime;
	UsePalette(L.Def ? L.Def->World : 1);
	{
		const double FrameDt = FMath::Clamp(Time - LastTime, 0.0, 0.1);
		LastTime = Time;
		const double K = 1.0 - FMath::Exp(-FrameDt * 3.0);
		ParallaxX += ((G.Sim.Player.X - L.W * 0.5) - ParallaxX) * K;
		ParallaxY += ((G.Sim.Player.Y - L.H * 0.5) - ParallaxY) * K;
	}
	DrawBackground(D, G, V, Time);

	// Door and plate state: live, or scrubbed from the history while rewinding.
	const float* DoorOpen = G.Sim.DoorOpen;
	uint8 PlateBits = 0;
	for (int32 P = 0; P < L.NumPlates; ++P) { PlateBits |= G.Sim.PlatePressed[P] ? (1 << P) : 0; }
	if ((G.Fx == EECFx::Rewind || G.Fx == EECFx::Restart) && G.Sim.Current.Length > 0)
	{
		const int32 S = FMath::Clamp((int32)((G.Sim.Current.Length - 1) * (1.0 - Smooth(G.FxProgress()))), 0, G.Sim.Current.Length - 1);
		DoorOpen = G.DoorHistory[S];
		PlateBits = G.PlateHistory[S];
	}

	DrawExit(D, G, Time, G.Fx == EECFx::Solve ? G.FxProgress() : 0.0);
	DrawPlatesAndLinks(D, G, PlateBits, Time);
	DrawDoors(D, G, DoorOpen, Time);
	DrawTiles(D, G, Time);
	DrawLasers(D, G, Time);
	DrawShard(D, G, Time);
	DrawMist(D, G, Time);
	DrawHint(D, G, Time);
	DrawActors(D, G, Time);
	DrawParticles(D, G, false);
	DrawLighting(D, G, V, Time);
	DrawParticles(D, G, true);
	DrawOverlays(D, G, Time);
	D.Flush();
	return V;
}

void FECWorldRenderer::DrawBackground(FECDraw& D, const FECGame& G, const FECViewTransform& V, double Time)
{
	const FLevel& L = G.Sim.Level;
	const double Left = V.OriginX, Right = V.OriginX + D.ScreenW / V.Scale;
	const double Top = V.OriginY, Bottom = V.OriginY - D.ScreenH / V.Scale;

	D.Translucent();
	D.RectV(Left, Bottom, Right, Top, ECColor(BgBottom), ECColor(BgTop));

	// Parallax: three depths behind the play layer, each trailing the player a little less than the
	// one behind it. (The play layer itself never moves: the level is one fixed screen.)
	auto Depth = [&](double F) { D.Flush(); D.SetTransform(V.Scale, V.OriginX + ParallaxX * F, V.OriginY + ParallaxY * F * 0.6, true); };

	// Far: the silhouette of the facility's structure - columns, cross-beams, hanging cables.
	Depth(0.16);
	{
		const FLinearColor Far = ECColor(Midground, 0.22f);
		for (int32 I = -1; I < 7; ++I)
		{
			const double CX0 = I * 3.1 + 0.8 + Hash(I + 900) * 0.8, CW0 = 0.35 + Hash(I + 910) * 0.5;
			D.Rect(CX0, -2, CX0 + CW0, L.H + 2, Far);
			D.Rect(CX0 - 0.15, L.H * (0.3 + 0.4 * Hash(I + 920)), CX0 + CW0 + 0.15, L.H * (0.3 + 0.4 * Hash(I + 920)) + 0.18, Far);
		}
		for (int32 I = 0; I < 3; ++I)
		{
			const double BY = L.H * (0.25 + 0.27 * I) + Hash(I + 930);
			D.Rect(-3, BY, L.W + 3, BY + 0.22, ECColor(Midground, 0.16f));
		}
		for (int32 I = 0; I < 6; ++I)
		{
			// cables sagging between the columns
			const double X0 = I * 3.1 - 1.0, X1 = X0 + 3.1, Y0 = L.H - 0.6 - Hash(I + 940);
			FVector2D Prev(X0, Y0);
			for (int32 S = 1; S <= 8; ++S)
			{
				const double T = S / 8.0;
				const FVector2D Next(FMath::Lerp(X0, X1, T), Y0 - FMath::Sin(T * PI) * (0.6 + Hash(I + 950)));
				D.Line(Prev.X, Prev.Y, Next.X, Next.Y, 0.04, ECColor(Midground, 0.3f));
				Prev = Next;
			}
		}
	}

	// Middle: monitors (Lab) or gears and steam (Factory), and the great dial.
	Depth(0.09);
	for (int32 I = 0; I < 5 && GPaletteWorld == 1; ++I)
	{
		const double MX = 1.2 + Hash(I * 13 + 1) * (L.W - 3.5), MY = 2.2 + Hash(I * 13 + 2) * (L.H - 4.5);
		D.Rect(MX, MY, MX + 1.1, MY + 0.7, ECColor(Screen, 0.9f));
		for (int32 R = 0; R < 4; ++R)
		{
			const double W = 0.3 + 0.6 * Hash(I * 31 + R + int32(Time * 0.5 + I));
			D.Rect(MX + 0.1, MY + 0.12 + R * 0.14, MX + 0.1 + W * 0.9, MY + 0.17 + R * 0.14, ECColor(Accent, 0.12f));
		}
	}

	if (GPaletteWorld == 2)
	{
		// The factory's machinery: big gears turning slowly behind the walls, meshed in a chain.
		static const double Gears[][4] = { { 2.5, 6.2, 2.1, 12 }, { 5.6, 7.4, 1.3, 8 }, { 12.8, 5.8, 2.4, 14 }, { 10.2, 2.6, 1.5, 9 } };
		for (int32 I = 0; I < 4; ++I)
		{
			const double GX = Gears[I][0], GY = Gears[I][1], GR = Gears[I][2];
			const int32 Teeth = (int32)Gears[I][3];
			const double Spin = (I % 2 ? -1.0 : 1.0) * Time * 0.35 / GR;
			const FLinearColor GC = ECColor(Midground, 0.55f);
			D.Circle(GX, GY, GR * 0.86, GC, 40);
			for (int32 T = 0; T < Teeth; ++T)
			{
				const double A = Spin + T * 2.0 * PI / Teeth, HW = PI / Teeth * 0.45;
				const FVector2D P0(GX + FMath::Cos(A - HW) * GR * 0.84, GY + FMath::Sin(A - HW) * GR * 0.84);
				const FVector2D P1(GX + FMath::Cos(A + HW) * GR * 0.84, GY + FMath::Sin(A + HW) * GR * 0.84);
				const FVector2D P2(GX + FMath::Cos(A + HW * 0.7) * GR * 1.02, GY + FMath::Sin(A + HW * 0.7) * GR * 1.02);
				const FVector2D P3(GX + FMath::Cos(A - HW * 0.7) * GR * 1.02, GY + FMath::Sin(A - HW * 0.7) * GR * 1.02);
				D.Quad(P0, P1, P2, P3, GC);
			}
			D.Circle(GX, GY, GR * 0.62, ECColor(BgBottom, 0.9f), 32);
			for (int32 K = 0; K < 5; ++K)
			{
				const double A = Spin + K * 2.0 * PI / 5;
				D.Line(GX, GY, GX + FMath::Cos(A) * GR * 0.64, GY + FMath::Sin(A) * GR * 0.64, GR * 0.12, GC);
			}
			D.Circle(GX, GY, GR * 0.16, ECColor(Midground, 0.8f), 16);
		}
		// Steam drifting up from the floor vents.
		D.Additive();
		for (int32 I = 0; I < 10; ++I)
		{
			const double T = FMath::Fmod(Time * 0.12 + Hash(I + 300), 1.0);
			const double SX = 1.0 + Hash(I + 310) * (L.W - 2.0) + FMath::Sin(Time * 0.7 + I) * 0.3;
			D.Glow(SX, 1.0 + T * (L.H - 2.0), 0.6 + T * 1.2, ECColor(0xFFE2C8, 0.05f * (float)FMath::Sin(T * PI)), ECColor(0xFFE2C8, 0.0f), 16);
		}
		D.Translucent();
	}

	// The time machine of the facility: a huge dial, slowly turning.
	Depth(0.06);
	D.Additive();
	const double CX = L.W * 0.5, CY = L.H * 0.55, R = L.H * 0.42;
	D.Ring(CX, CY, R - 0.04, R, ECColor(Accent, 0.07f), ECColor(Accent, 0.07f), 72);
	D.Ring(CX, CY, R * 0.72 - 0.03, R * 0.72, ECColor(Accent, 0.05f), ECColor(Accent, 0.05f), 60);
	for (int32 I = 0; I < 60; ++I)
	{
		const double A = I * 2.0 * PI / 60.0 + Time * 0.02;
		const double R0 = R - (I % 5 == 0 ? 0.35 : 0.18);
		D.Line(CX + FMath::Cos(A) * R0, CY + FMath::Sin(A) * R0, CX + FMath::Cos(A) * (R - 0.06), CY + FMath::Sin(A) * (R - 0.06), 0.03, ECColor(Accent, I % 5 == 0 ? 0.1f : 0.05f));
	}
	const double Hand = PI * 0.5 - (G.Sim.Tick / double(LoopTicks)) * 2.0 * PI;   // the dial is the loop clock
	D.Arc(CX, CY, R * 0.72 + 0.1, R - 0.12, PI * 0.5, Hand, ECColor(Accent, 0.045f), 72);
	D.Line(CX, CY, CX + FMath::Cos(Hand) * (R - 0.2), CY + FMath::Sin(Hand) * (R - 0.2), 0.05, ECColor(Accent, 0.12f));

	// Near: the back wall itself, glass panels with thin seams.
	Depth(0.03);
	D.Translucent();
	{
		const FLinearColor Seam = ECColor(Midground, 0.35f);
		for (int32 X = -2; X <= L.W + 2; X += 2) { D.Line(X, -1, X, L.H + 1, 0.03, Seam); }
		for (int32 Y = 0; Y <= L.H; Y += 3) { D.Line(-2, Y + 0.5, L.W + 2, Y + 0.5, 0.03, Seam); }
	}
	Depth(0.0);
	D.Additive();

	// Ceiling lamps and their light shafts (part of the play layer: they sit on the real ceiling).
	NumLamps = 0;
	for (int32 X = 1; X < L.W - 1; ++X)
		for (int32 Y = L.H - 1; Y > 0; --Y)
		{
			if (L.TileAt(X, Y) != ETile::Solid || L.TileAt(X, Y - 1) == ETile::Solid) { continue; }
			if (Hash(X * 17 + Y * 5) > 0.3) { break; }
			int32 Floor = Y - 1;
			while (Floor > 0 && L.TileAt(X, Floor - 1) != ETile::Solid) { --Floor; }
			const double Flicker = 0.85 + 0.15 * FMath::Sin(Time * (3.0 + X) + X);
			Shaft(D, X + 0.5, Y, 0.3, Floor, 1.4, ECColor(Lamp, 0.075f * (float)Flicker), ECColor(Accent, 0.0f));
			D.Glow(X + 0.5, Y - 0.05, 0.9, ECColor(Lamp, 0.25f), ECColor(Accent, 0.0f), 16);
			if (NumLamps < MaxLamps) { Lamps[NumLamps++] = FVector(X + 0.5, Y, Floor); }
			break;
		}

	// Warm light from the exit, behind the play layer.
	D.Glow(L.Exit.X, L.Exit.Y + 1.0, 4.2, ECColor(ExitGlow, 0.16f), ECColor(ExitGlow, 0.0f), 32);
	D.Translucent();
}

void FECWorldRenderer::DrawTiles(FECDraw& D, const FECGame& G, double Time)
{
	const FLevel& L = G.Sim.Level;
	const FLinearColor Fill = ECColor(WallFill), Panel = ECColor(WallPanel), Edge = ECColor(WallEdge);
	auto Solid = [&L](int32 X, int32 Y) { return X < 0 || X >= L.W || Y < 0 || Y >= L.H || L.TileAt(X, Y) == ETile::Solid; };

	// Everything outside the level is wall.
	D.Rect(-40, -40, 0, L.H + 40, Fill);
	D.Rect(L.W, -40, L.W + 40, L.H + 40, Fill);
	D.Rect(0, -40, L.W, 0, Fill);
	D.Rect(0, L.H, L.W, L.H + 40, Fill);

	for (int32 Y = 0; Y < L.H; ++Y)
		for (int32 X = 0; X < L.W; ++X)
		{
			if (L.TileAt(X, Y) != ETile::Solid) { continue; }
			D.Rect(X, Y, X + 1, Y + 1, Fill);
			D.RectV(X + 0.05, Y + 0.05, X + 0.95, Y + 0.95, Panel, ECAlpha(Panel, 0.5));
			if (Hash(X * 91 + Y * 7) < 0.08)
			{
				D.Additive();
				D.Circle(X + 0.8, Y + 0.8, 0.035, ECColor(Accent, 0.5f + 0.4f * (float)FMath::Sin(Time * 2.0 + X)), 6);
				D.Translucent();
			}
		}

	// Exposed faces: a bevel, a crisp cyan lip on floors, and a soft glow above walkable tops.
	for (int32 Y = 0; Y < L.H; ++Y)
		for (int32 X = 0; X < L.W; ++X)
		{
			if (L.TileAt(X, Y) != ETile::Solid) { continue; }
			if (!Solid(X, Y + 1))
			{
				D.Rect(X, Y + 0.86, X + 1, Y + 1, Edge);
				D.Rect(X, Y + 0.965, X + 1, Y + 1.0, ECColor(Accent, 0.75f));
				D.Additive();
				D.RectV(X, Y + 1, X + 1, Y + 1.35, ECColor(Accent, 0.09f), ECColor(Accent, 0.0f));
				D.Translucent();
			}
			if (!Solid(X, Y - 1)) { D.Rect(X, Y, X + 1, Y + 0.06, ECAlpha(Edge, 0.8)); }
			if (!Solid(X - 1, Y)) { D.Rect(X, Y, X + 0.05, Y + 1, ECAlpha(Edge, 0.9)); }
			if (!Solid(X + 1, Y)) { D.Rect(X + 0.95, Y, X + 1, Y + 1, ECAlpha(Edge, 0.9)); }
		}

	// Spikes: hot red teeth with a slow pulse.
	for (int32 Y = 0; Y < L.H; ++Y)
		for (int32 X = 0; X < L.W; ++X)
		{
			if (L.TileAt(X, Y) != ETile::Spike) { continue; }
			const float Pulse = 0.6f + 0.4f * (float)FMath::Sin(Time * 4.0 + X);
			D.Additive();
			D.RectV(X, Y, X + 1, Y + 0.8, ECColor(Danger, 0.25f * Pulse), ECColor(Danger, 0.0f));
			D.Translucent();
			for (int32 I = 0; I < 4; ++I)
			{
				D.Tri(X + I * 0.25, Y, X + I * 0.25 + 0.25, Y, X + I * 0.25 + 0.125, Y + 0.5, ECColor(Danger));
				D.Tri(X + I * 0.25 + 0.08, Y, X + I * 0.25 + 0.17, Y, X + I * 0.25 + 0.125, Y + 0.42, ECColor(0xFFC0C6));
			}
		}
}

void FECWorldRenderer::DrawPlatesAndLinks(FECDraw& D, const FECGame& G, const uint8 PlateBits, double Time)
{
	const FLevel& L = G.Sim.Level;
	for (int32 P = 0; P < L.NumPlates; ++P)
	{
		const FPlate& Pl = L.Plates[P];
		const bool bDown = (PlateBits & (1 << P)) != 0;
		const FLinearColor C = ChannelColor(Pl.Channel);

		// Circuit line along the floor to every door on this channel; pulses travel it while pressed.
		for (int32 Di = 0; Di < L.NumDoors; ++Di)
		{
			const FDoor& Dr = L.Doors[Di];
			if (Dr.Channel != Pl.Channel) { continue; }
			const double X0 = (Pl.X0 + Pl.X1) * 0.5, X1 = (Dr.X0 + Dr.X1) * 0.5, Y = Pl.Y - 0.18;
			const double Len = FMath::Abs(X1 - X0);
			const double Dir = X1 > X0 ? 1.0 : -1.0;
			for (double S = 0; S < Len; S += 0.3)
			{
				D.Rect(X0 + Dir * S - 0.06, Y - 0.02, X0 + Dir * S + 0.06, Y + 0.02, ECAlpha(C, bDown ? 0.55 : 0.18));
			}
			if (bDown)
			{
				D.Additive();
				for (int32 K = 0; K < 3; ++K)
				{
					const double S = FMath::Fmod(Time * 5.0 + K * Len / 3.0, FMath::Max(0.1, Len));
					D.Glow(X0 + Dir * S, Y, 0.35, ECAlpha(C, 0.6), ECAlpha(C, 0), 12);
				}
				D.Translucent();
			}
		}

		// Slot, pad (sinks when pressed), and a light cone upward.
		const double Flash = G.PlateFlash[P];
		D.Rect(Pl.X0 + 0.08, Pl.Y - 0.02, Pl.X1 - 0.08, Pl.Y + 0.1, ECColor(0x050A14));
		const double PadTop = bDown ? Pl.Y + 0.06 : Pl.Y + 0.15;
		D.Rect(Pl.X0 + 0.14, Pl.Y + 0.02, Pl.X1 - 0.14, PadTop, ECAlpha(C, bDown ? 1.0 : 0.6));
		D.Rect(Pl.X0 + 0.14, PadTop - 0.025, Pl.X1 - 0.14, PadTop, FMath::Lerp(C, FLinearColor::White, 0.6f));
		D.Additive();
		const double Up = bDown ? 0.35 + 0.35 * Flash : 0.08 + 0.04 * FMath::Sin(Time * 3.0 + P);
		Shaft(D, (Pl.X0 + Pl.X1) * 0.5, PadTop, 0.36, PadTop + 1.6, 0.6, ECAlpha(C, Up * 0.5), ECAlpha(C, 0));
		D.Glow((Pl.X0 + Pl.X1) * 0.5, PadTop, 0.9 + Flash * 0.6, ECAlpha(C, Up), ECAlpha(C, 0), 18);
		D.Translucent();
	}
}

void FECWorldRenderer::DrawDoors(FECDraw& D, const FECGame& G, const float* DoorOpen, double Time)
{
	const FLevel& L = G.Sim.Level;
	for (int32 I = 0; I < L.NumDoors; ++I)
	{
		const FDoor& Dr = L.Doors[I];
		const FLinearColor C = ChannelColor(Dr.Channel);
		const double Open = DoorOpen[I];
		const double Bottom = Dr.Y0 + (Dr.Y1 - Dr.Y0) * Open;
		const double Jit = G.DoorShake * FMath::Sin(Time * 90.0) * 0.02;

		// Frame rails stay in place; they glow brighter while the door is closed (it is "armed").
		D.Rect(Dr.X0 + 0.02, Dr.Y0, Dr.X0 + 0.1, Dr.Y1, ECAlpha(C, 0.35 + 0.4 * (1.0 - Open)));
		D.Rect(Dr.X1 - 0.1, Dr.Y0, Dr.X1 - 0.02, Dr.Y1, ECAlpha(C, 0.35 + 0.4 * (1.0 - Open)));
		if (Bottom < Dr.Y1 - 0.001)
		{
			D.RectV(Dr.X0 + 0.12 + Jit, Bottom, Dr.X1 - 0.12 + Jit, Dr.Y1, ECColor(0x0D1A2E), ECColor(0x14263F));
			for (double Y = Dr.Y1 - 0.25; Y > Bottom + 0.05; Y -= 0.25)
			{
				D.Rect(Dr.X0 + 0.18 + Jit, Y - 0.015, Dr.X1 - 0.18 + Jit, Y + 0.015, ECColor(Midground, 0.7f));
			}
			D.Rect((Dr.X0 + Dr.X1) * 0.5 - 0.035 + Jit, Bottom + 0.1, (Dr.X0 + Dr.X1) * 0.5 + 0.035 + Jit, Dr.Y1 - 0.1, ECAlpha(C, 0.7));
			// The energy edge at the bottom.
			D.Rect(Dr.X0 + 0.12 + Jit, Bottom, Dr.X1 - 0.12 + Jit, Bottom + 0.05, FMath::Lerp(C, FLinearColor::White, 0.5f));
			D.Additive();
			D.RectV(Dr.X0 + 0.1, Bottom - 0.3, Dr.X1 - 0.1, Bottom, ECAlpha(C, 0.0), ECAlpha(C, 0.35 + 0.15 * FMath::Sin(Time * 8.0)));
			D.Translucent();
		}
		D.Additive();
		D.Glow((Dr.X0 + Dr.X1) * 0.5, (Dr.Y0 + Dr.Y1) * 0.5, 1.2, ECAlpha(C, 0.1 + 0.1 * (1.0 - Open)), ECAlpha(C, 0), 18);
		D.Translucent();
	}
}

void FECWorldRenderer::DrawExit(FECDraw& D, const FECGame& G, double Time, double Solve)
{
	const FLevel& L = G.Sim.Level;
	const double X = L.Exit.X, Y = L.Exit.Y + 1.0;
	const double Breathe = 0.5 + 0.5 * FMath::Sin(Time * 2.0);
	D.Additive();
	D.Glow(X, Y, 1.6 + Solve * 2.5, ECColor(ExitWarm, 0.3f + 0.1f * (float)Breathe + 0.5f * (float)Solve), ECColor(ExitGlow, 0.0f), 28);
	// Swirling arcs inside the portal
	for (int32 K = 0; K < 3; ++K)
	{
		const double A0 = Time * (1.2 + K * 0.4) + K * 2.1;
		for (int32 S = 0; S < 8; ++S)
		{
			const double T0 = A0 + S * 0.16, T1 = A0 + (S + 1) * 0.16;
			const double RX = 0.3 - K * 0.07, RY = 0.78 - K * 0.18;
			D.Line(X + FMath::Cos(T0) * RX, Y + FMath::Sin(T0) * RY, X + FMath::Cos(T1) * RX, Y + FMath::Sin(T1) * RY, 0.03, ECColor(ExitWarm, 0.25f * (1.f - S / 8.f)));
		}
	}
	D.Translucent();
	// Portal: a tall warm ring with a bright core.
	const FLinearColor Warm = ECColor(ExitWarm);
	FVector2D Outer[28], Inner[28];
	for (int32 I = 0; I < 28; ++I)
	{
		const double A = I * 2.0 * PI / 28;
		Outer[I] = FVector2D(X + FMath::Cos(A) * 0.46, Y + FMath::Sin(A) * 0.98);
		Inner[I] = FVector2D(X + FMath::Cos(A) * 0.36, Y + FMath::Sin(A) * 0.86);
	}
	D.Poly(Inner, 28, ECColor(0xFFE7C2, 0.28f + 0.2f * (float)Breathe));
	for (int32 I = 0; I < 28; ++I)
	{
		const int32 J = (I + 1) % 28;
		D.Quad(Outer[I], Outer[J], Inner[J], Inner[I], Warm);
	}
	D.Additive();
	D.Ellipse(X, Y, 0.22, 0.6, ECColor(ExitWarm, 0.35f), 20);
	// Rising motes
	for (int32 I = 0; I < 14; ++I)
	{
		const double T = FMath::Fmod(Time * (0.35 + Hash(I) * 0.3) + Hash(I + 50), 1.0);
		const double PX = X + (Hash(I + 100) - 0.5) * 0.7 + FMath::Sin(Time * 2.0 + I) * 0.06;
		const double PY = L.Exit.Y + T * 2.4;
		D.Circle(PX, PY, 0.035, ECColor(ExitWarm, (float)(0.8 * FMath::Sin(T * PI))), 6);
	}
	D.Translucent();
}

void FECWorldRenderer::DrawLasers(FECDraw& D, const FECGame& G, double Time)
{
	const FSim& S = G.Sim;
	const FLevel& L = S.Level;
	if (L.NumEmitters == 0) { return; }
	const bool bOn = S.LaserOn() && G.Fx != EECFx::Rewind && G.Fx != EECFx::Restart;
	const bool bWarn = S.LaserWarning();
	const FLinearColor Hot = ECColor(Danger);
	const FLinearColor Core = FMath::Lerp(Hot, FLinearColor::White, 0.65f);
	for (int32 I = 0; I < L.NumEmitters; ++I)
	{
		const FEmitter& E = L.Emitters[I];
		const EC::FBox B = S.BeamBox(I);
		const bool bHorizontal = E.DY == 0;
		const double CX = (B.L + B.R) * 0.5, CY = (B.B + B.T) * 0.5;
		const double X0 = bHorizontal ? (E.DX > 0 ? B.L : B.R) : CX, Y0 = bHorizontal ? CY : B.T;   // at the emitter
		const double X1 = bHorizontal ? (E.DX > 0 ? B.R : B.L) : CX, Y1 = bHorizontal ? CY : B.B;   // where it stops

		// Housing: a dark block with ribs and a lens facing the beam.
		D.Rect(E.X + 0.06, E.Y + 0.06, E.X + 0.94, E.Y + 0.94, ECColor(0x1A0E10));
		for (int32 K = 0; K < 3; ++K)
		{
			if (bHorizontal) { D.Rect(E.X + 0.2 + K * 0.22, E.Y + 0.14, E.X + 0.28 + K * 0.22, E.Y + 0.86, ECColor(Midground, 0.9f)); }
			else { D.Rect(E.X + 0.14, E.Y + 0.2 + K * 0.22, E.X + 0.86, E.Y + 0.28 + K * 0.22, ECColor(Midground, 0.9f)); }
		}
		const double Charge = bOn ? 1.0 : (bWarn ? 0.5 + 0.5 * FMath::Sin(Time * 40.0) : 0.25 + 0.1 * FMath::Sin(Time * 3.0));
		D.Circle(X0, Y0, 0.14, ECAlpha(Hot, 0.5 + 0.5 * Charge), 16);
		D.Circle(X0, Y0, 0.07, ECAlpha(Core, Charge), 12);
		D.Additive();
		D.Glow(X0, Y0, 0.6 + 0.3 * Charge, ECAlpha(Hot, 0.35 * Charge), ECAlpha(Hot, 0), 18);

		if (bOn)
		{
			// Live beam: wide soft glow, the coloured beam, a white-hot core, and sparks where it stops.
			const double Wob = 0.02 * FMath::Sin(Time * 30.0 + I);
			if (bHorizontal)
			{
				D.RectV(FMath::Min(X0, X1), CY - 0.35, FMath::Max(X0, X1), CY, ECAlpha(Hot, 0.0), ECAlpha(Hot, 0.18));
				D.RectV(FMath::Min(X0, X1), CY, FMath::Max(X0, X1), CY + 0.35, ECAlpha(Hot, 0.18), ECAlpha(Hot, 0.0));
			}
			else
			{
				D.RectH(CX - 0.35, Y1, CX, Y0, ECAlpha(Hot, 0.0), ECAlpha(Hot, 0.18));
				D.RectH(CX, Y1, CX + 0.35, Y0, ECAlpha(Hot, 0.18), ECAlpha(Hot, 0.0));
			}
			D.Line(X0, Y0, X1, Y1, 0.16 + Wob, ECAlpha(Hot, 0.85));
			D.Line(X0, Y0, X1, Y1, 0.05, ECAlpha(Core, 1.0));
			D.Glow(X1, Y1, 0.55, ECAlpha(Core, 0.6), ECAlpha(Hot, 0), 16);
			for (int32 K = 0; K < 5; ++K)
			{
				const double A = Hash(K + int32(Time * 30.0) * 7 + I * 50) * 2.0 * PI;
				const double R = 0.1 + 0.25 * Hash(K + 11 + int32(Time * 30.0));
				D.Circle(X1 + FMath::Cos(A) * R, Y1 + FMath::Sin(A) * R, 0.03, ECAlpha(Core, 0.8), 6);
			}
		}
		else if (bWarn)
		{
			// About to fire: a thin flickering line along the path.
			if (Hash(int32(Time * 25.0) + I) < 0.6) { D.Line(X0, Y0, X1, Y1, 0.035, ECAlpha(Hot, 0.55)); }
		}
		else
		{
			// Dark: a faint dotted guide, so the path can be planned.
			const double Len = FMath::Sqrt(FMath::Square(X1 - X0) + FMath::Square(Y1 - Y0));
			for (double T = 0.2; T < Len; T += 0.35)
			{
				const double F = T / FMath::Max(0.001, Len);
				D.Circle(FMath::Lerp(X0, X1, F), FMath::Lerp(Y0, Y1, F), 0.025, ECAlpha(Hot, 0.3), 6);
			}
		}
		D.Translucent();
	}
}

void FECWorldRenderer::DrawShard(FECDraw& D, const FECGame& G, double Time)
{
	const FLevel& L = G.Sim.Level;
	if (!L.HasShard || G.Sim.ShardTaken()) { return; }
	const double X = L.Shard.X, Y = L.Shard.Y + FMath::Sin(Time * 2.4) * 0.08;
	const double Spin = FMath::Sin(Time * 1.7);
	D.Additive();
	D.Glow(X, Y, 0.8, ECColor(PlayerCyan, 0.4f), ECColor(PlayerCyan, 0.0f), 20);
	D.Translucent();
	const double W = 0.2 * FMath::Max(0.25, FMath::Abs(Spin));
	D.Tri(X - W, Y, X, Y + 0.32, X, Y - 0.32, ECColor(0x9EF7FF));
	D.Tri(X + W, Y, X, Y + 0.32, X, Y - 0.32, ECColor(0xE8FFFF));
	D.Tri(X - W * 0.4, Y + 0.05, X, Y + 0.25, X + W * 0.3, Y + 0.02, ECColor(0xFFFFFF, 0.8f));
	D.Additive();
	for (int32 I = 0; I < 4; ++I)
	{
		const double A = Time * 1.5 + I * PI * 0.5;
		D.Circle(X + FMath::Cos(A) * 0.45, Y + FMath::Sin(A) * 0.3, 0.025, ECColor(0xE8FFFF, 0.7f), 6);
	}
	D.Translucent();
}

void FECWorldRenderer::DrawActors(FECDraw& D, const FECGame& G, double Time)
{
	const FSim& S = G.Sim;
	const double A = G.RenderAlpha();
	const bool bScrub = (G.Fx == EECFx::Rewind || G.Fx == EECFx::Restart) && S.Current.Length > 0;
	const int32 ScrubTick = bScrub ? FMath::Clamp((int32)((S.Current.Length - 1) * (1.0 - Smooth(G.FxProgress()))), 0, S.Current.Length - 1) : 0;
	const double TimeLeft = S.TimeLeft();
	const double Pulse = 0.5 + 0.5 * FMath::Sin(Time * (TimeLeft < 3.0 ? 14.0 : 5.0));

	auto PoseOf = [](const EC::FFrame& Fr)
	{
		FPose P;
		P.X = Fr.X; P.Y = Fr.Y; P.VX = Fr.VX; P.VY = Fr.VY; P.Facing = Fr.Facing; P.Anim = (EAnim)Fr.Anim;
		return P;
	};

	// Echo tick shown: live (interpolated), scrubbed, or slowed down after the solve.
	for (int32 I = 0; I < S.NumEchoes; ++I)
	{
		const FRecording& R = S.Echoes[I];
		const FECEchoStyle& ES = EchoStyle(I);
		int32 T = S.Tick;
		if (bScrub) { T = ScrubTick; }
		if (G.Fx == EECFx::Solve) { T = S.Tick + (int32)(G.FxTime * TickHz * 0.3); }
		FPose P = PoseOf(R.At(FMath::Max(0, T)));
		if (!bScrub && G.Fx == EECFx::None)
		{
			P.X = FMath::Lerp((double)S.EchoState[I].PrevX, (double)S.EchoState[I].X, A);
			P.Y = FMath::Lerp((double)S.EchoState[I].PrevY, (double)S.EchoState[I].Y, A);
		}
		const bool bHolding = T >= R.Length;
		if (bHolding) { P.Anim = EAnim::Idle; P.VX = P.VY = 0; }

		double Alpha = 1.0;
		if (ES.Effect == 3) { Alpha = Hash(int32(Time * 18.0) + I * 1000) < 0.18 ? 0.35 : 1.0; }
		if (G.Fx == EECFx::Paradox && S.ParadoxEcho == I) { Alpha = FMath::Fmod(Time * 12.0, 1.0) < 0.5 ? 1.0 : 0.2; }
		if (G.Fx == EECFx::Solve) { Alpha = 1.0 - 0.6 * G.FxProgress(); }

		// Afterimages from the recording (the trail of where it has just been).
		const int32 NumTrail = ES.Effect == 4 ? 5 : 3;
		const int32 Gap = ES.Effect == 4 ? 5 : (ES.Effect == 0 ? 4 : 3);
		if (!bHolding)
		{
			for (int32 K = NumTrail; K >= 1; --K)
			{
				const int32 TT = T - K * Gap;
				if (TT < 0) { continue; }
				FPose Ghost = PoseOf(R.At(TT));
				if (FMath::Abs(Ghost.X - P.X) + FMath::Abs(Ghost.Y - P.Y) < 0.05) { continue; }
				DrawCharacter(D, Ghost, I, Time, Alpha * (0.22 - K * 0.035) * (ES.Effect == 4 ? 0.8 : 1.0), 0);
			}
		}
		// Floor glow, body glow, figure.
		D.Additive();
		D.Glow(P.X, P.Y + 0.8, 1.1, ECColor(ES.Color, 0.16f * (float)Alpha), ECColor(ES.Color, 0.0f), 20);
		D.Ellipse(P.X, P.Y + 0.02, 0.45, 0.06, ECColor(ES.Color, 0.25f * (float)Alpha), 16);
		D.Translucent();
		if (bHolding)
		{
			// Its run is over: it holds this pose. A flat ring on the floor says so.
			EllipseOutline(D, P.X, P.Y + 0.03, 0.42 + 0.05 * Pulse, 0.07, 0.03, ECColor(ES.Color, 0.6f * (float)Alpha), 24);
		}
		DrawCharacter(D, P, I, Time, Alpha, bHolding ? 0.2 : 0.6);
	}

	// Subject 7
	FPose P;
	if (bScrub)
	{
		P = PoseOf(S.Current.At(ScrubTick));
	}
	else
	{
		const FBody& B = S.Player;
		P.X = FMath::Lerp((double)G.PrevPX, (double)B.X, A);
		P.Y = FMath::Lerp((double)G.PrevPY, (double)B.Y, A);
		P.VX = B.VX; P.VY = B.VY; P.Facing = B.Facing; P.Anim = B.Anim;
	}
	if (S.Phase == EPhase::Solved && G.Fx != EECFx::Solve) { return; }   // gone through the portal
	double Alpha = 1.0;
	if (G.Fx == EECFx::Solve) { Alpha = 1.0 - Smooth(G.FxProgress() * 1.6); }
	if (G.Fx == EECFx::Death) { Alpha = FMath::Fmod(Time * 16.0, 1.0) < 0.5 ? 1.0 : 0.15; }
	D.Additive();
	D.Ellipse(P.X, P.Y + 0.02, 0.5, 0.07, ECColor(PlayerCyan, 0.3f * (float)Alpha), 16);
	D.Glow(P.X, P.Y + 0.9, 1.3, ECColor(PlayerCyan, 0.08f * (float)Alpha), ECColor(PlayerCyan, 0.0f), 20);
	D.Translucent();
	if (bScrub && G.Fx == EECFx::Rewind)
	{
		// The run you just lived is turning into the next Echo: blend toward its glass colour.
		DrawCharacter(D, P, S.NumEchoes, Time, Smooth(G.FxProgress() * 1.5), 0.8);
		DrawCharacter(D, P, -1, Time, 1.0 - Smooth(G.FxProgress() * 1.5), Pulse);
	}
	else
	{
		DrawCharacter(D, P, -1, Time, Alpha, Pulse);
	}
	if (G.Fx == EECFx::Solve)
	{
		D.Additive();
		const double T = G.FxProgress();
		for (int32 I = 0; I < 24; ++I)
		{
			const double R = T * (0.6 + Hash(I) * 1.6);
			const double Ang = Hash(I + 40) * 2.0 * PI;
			D.Circle(P.X + FMath::Cos(Ang) * R, P.Y + 0.8 + FMath::Sin(Ang) * R + T * 0.8, 0.04, ECColor(PlayerCyan, (float)(1.0 - T)), 6);
		}
		D.Translucent();
	}
}

// Slow mist hugging the floor and hanging in the room.
void FECWorldRenderer::DrawMist(FECDraw& D, const FECGame& G, double Time)
{
	const FLevel& L = G.Sim.Level;
	D.Additive();
	for (int32 I = 0; I < 12; ++I)
	{
		const bool bLow = I < 8;
		const double Speed = (0.12 + 0.18 * Hash(I + 400)) * (I % 2 ? 1.0 : -1.0);
		const double Span = L.W + 8.0;
		const double X = FMath::Fmod(Hash(I + 410) * Span + Time * Speed + Span * 100.0, Span) - 4.0;
		const double Y = bLow ? 1.0 + Hash(I + 420) * 1.3 : 2.5 + Hash(I + 420) * (L.H - 4.0);
		const double R = bLow ? 2.2 + Hash(I + 430) * 1.6 : 3.0 + Hash(I + 430) * 2.0;
		const float A = (bLow ? 0.055f : 0.03f) * (0.7f + 0.3f * (float)FMath::Sin(Time * 0.4 + I));
		D.Glow(X, Y + FMath::Sin(Time * 0.25 + I) * 0.15, R, ECColor(Lamp, A), ECColor(Lamp, 0.0f), 18);
	}
	D.Translucent();
}

void FECWorldRenderer::DrawParticles(FECDraw& D, const FECGame& G, bool bGlow)
{
	if (bGlow) { D.Additive(); } else { D.Translucent(); }
	for (int32 I = 0; I < G.NumParticles; ++I)
	{
		const FECParticle& P = G.Particles[I];
		if (P.bGlow != bGlow) { continue; }
		const float T = FMath::Clamp(P.Life / P.MaxLife, 0.f, 1.f);
		if (bGlow)
		{
			D.Circle(P.X, P.Y, P.Size * (0.5 + 0.5 * T), ECColor(P.Color, T), 6);
			D.Glow(P.X, P.Y, P.Size * 4.0, ECColor(P.Color, 0.35f * T), ECColor(P.Color, 0.0f), 8);
		}
		else
		{
			// Dust puffs grow and thin out as they age.
			D.Circle(P.X, P.Y, P.Size * (1.0 + 1.4 * (1.f - T)), ECColor(P.Color, 0.45f * T), 8);
		}
	}
	D.Translucent();
}

// The hint: a pale ghost walks the next run of the par solution along a dotted path.
void FECWorldRenderer::DrawHint(FECDraw& D, const FECGame& G, double Time)
{
	const FRecording& H = G.HintTrack;
	if (!G.bHintActive || G.bAttract || H.Length <= 0 || G.Fx != EECFx::None || G.Sim.Phase != EPhase::Playing) { return; }
	D.Translucent();
	for (int32 T = 0; T < H.Length; T += 4)
	{
		const EC::FFrame& Fr = H.Frames[T];
		const float A = T < G.Sim.Tick ? 0.12f : 0.4f;
		D.Circle(Fr.X, Fr.Y + 0.8, 0.045, ECColor(0xFFFFFF, A), 6);
	}
	const EC::FFrame& Now = H.At(G.Sim.Tick);
	FPose P;
	P.X = Now.X; P.Y = Now.Y; P.VX = Now.VX; P.VY = Now.VY; P.Facing = Now.Facing; P.Anim = (EAnim)Now.Anim;
	const bool bDone = G.Sim.Tick >= H.Length;
	if (bDone) { P.Anim = EAnim::Idle; P.VX = P.VY = 0; }
	DrawCharacter(D, P, 4, Time, 0.5, 0.3);
	if (bDone && G.bHintEndsWithRewind)
	{
		// "Now rewind": the rewind mark pulses over the ghost's head.
		const double S = 0.26 + 0.05 * FMath::Sin(Time * 8.0), X = P.X, Y = P.Y + 2.15;
		const FLinearColor C = ECColor(0xFFFFFF, 0.9f);
		D.Tri(X - S * 0.05, Y - S * 0.5, X - S * 0.05, Y + S * 0.5, X - S * 0.65, Y, C);
		D.Tri(X + S * 0.55, Y - S * 0.5, X + S * 0.55, Y + S * 0.5, X - S * 0.05, Y, C);
	}
}

// Darkness with light sources: a coarse mesh over the screen whose vertices are darkened by how little
// light reaches them. Subject 7 carries the strongest light; everything that matters to the puzzle
// (exit, plates, doors, spikes, lasers, shard, Echoes) is a light too, so nothing important hides.
void FECWorldRenderer::DrawLighting(FECDraw& D, const FECGame& G, const FECViewTransform& V, double Time)
{
	const FSim& S = G.Sim;
	const FLevel& L = S.Level;
	struct FLight { float X, Y, R, I; };
	FLight Lights[96];
	int32 N = 0;
	auto Add = [&](double X, double Y, double R, double I) { if (N < 96) { Lights[N++] = { (float)X, (float)Y, (float)R, (float)I }; } };

	const double Flick = 0.96 + 0.04 * FMath::Sin(Time * 9.0);
	if (S.Phase != EPhase::Solved || G.Fx == EECFx::Solve)
	{
		const double A = G.RenderAlpha();
		Add(FMath::Lerp((double)G.PrevPX, (double)S.Player.X, A), FMath::Lerp((double)G.PrevPY, (double)S.Player.Y, A) + 0.9, 5.2, 0.95 * Flick);
	}
	for (int32 I = 0; I < S.NumEchoes; ++I) { Add(S.EchoState[I].X, S.EchoState[I].Y + 0.8, 2.9, 0.55); }
	Add(L.Exit.X, L.Exit.Y + 1.0, G.Fx == EECFx::Solve ? 4.0 + 14.0 * G.FxProgress() : 4.0, 0.8);
	if (L.HasShard && !S.ShardTaken()) { Add(L.Shard.X, L.Shard.Y, 2.2, 0.55); }
	for (int32 I = 0; I < NumLamps; ++I)
	{
		Add(Lamps[I].X, Lamps[I].Y - 0.6, 3.0, 0.5);
		Add(Lamps[I].X, (Lamps[I].Y + Lamps[I].Z) * 0.5, 2.8, 0.3);
		Add(Lamps[I].X, Lamps[I].Z + 0.5, 2.6, 0.38);
	}
	for (int32 I = 0; I < L.NumPlates; ++I) { Add((L.Plates[I].X0 + L.Plates[I].X1) * 0.5, L.Plates[I].Y + 0.3, 1.9, S.PlatePressed[I] ? 0.6 : 0.38); }
	for (int32 I = 0; I < L.NumDoors; ++I) { Add((L.Doors[I].X0 + L.Doors[I].X1) * 0.5, (L.Doors[I].Y0 + L.Doors[I].Y1) * 0.5, 2.2, 0.4); }
	for (int32 I = 0; I < L.NumEmitters; ++I)
	{
		const EC::FBox B = S.BeamBox(I);
		Add(L.Emitters[I].X + 0.5, L.Emitters[I].Y + 0.5, 1.8, 0.4);
		const double Len = FMath::Max(B.R - B.L, B.T - B.B);
		const bool bHorizontal = L.Emitters[I].DY == 0;
		const double Power = S.LaserOn() ? 0.5 : 0.16;
		for (double T = 0.5; T < Len; T += 1.4)
		{
			if (bHorizontal) { Add(B.L + T, (B.B + B.T) * 0.5, 1.9, Power); } else { Add((B.L + B.R) * 0.5, B.B + T, 1.9, Power); }
		}
	}
	for (int32 Y = 0; Y < L.H; ++Y)
		for (int32 X = 0; X < L.W; ++X)
			if (L.TileAt(X, Y) == ETile::Spike && (X % 2) == 0) { Add(X + 1.0, Y + 0.4, 1.9, 0.36); }
	if (G.bHintActive && G.HintTrack.Length > 0)
	{
		const EC::FFrame& Fr = G.HintTrack.At(S.Tick);
		Add(Fr.X, Fr.Y + 0.8, 2.4, 0.4);
	}

	// Menus sit over a brighter room; the level itself is the darkest.
	const double Ambient = G.Screen == EECScreen::Playing && !G.bAttract ? 0.30 : 0.42;
	const double MaxDark = 0.80;
	const FLinearColor Shadow = ECColor(0x02040A);
	const double Step = 0.5;
	const double Left = V.OriginX, Top = V.OriginY;
	const int32 NX = FMath::Min(95, FMath::CeilToInt(D.ScreenW / V.Scale / Step) + 1);
	const int32 NY = FMath::Min(63, FMath::CeilToInt(D.ScreenH / V.Scale / Step) + 1);
	static float Row[2][96];
	auto DarkAt = [&](double X, double Y)
	{
		double Light = Ambient;
		for (int32 I = 0; I < N; ++I)
		{
			const double DX = X - Lights[I].X, DY = Y - Lights[I].Y;
			const double D2 = DX * DX + DY * DY, R = Lights[I].R;
			if (D2 >= R * R) { continue; }
			const double K = 1.0 - FMath::Sqrt(D2) / R;
			Light += Lights[I].I * K * K * (3.0 - 2.0 * K);
		}
		return (float)(MaxDark * (1.0 - FMath::Clamp(Light, 0.0, 1.0)));
	};
	D.Translucent();
	for (int32 IX = 0; IX <= NX; ++IX) { Row[0][IX] = DarkAt(Left + IX * Step, Top); }
	for (int32 IY = 0; IY < NY; ++IY)
	{
		const float* A = Row[IY & 1];
		float* B = Row[(IY + 1) & 1];
		const double Y0 = Top - IY * Step, Y1 = Y0 - Step;
		for (int32 IX = 0; IX <= NX; ++IX) { B[IX] = DarkAt(Left + IX * Step, Y1); }
		for (int32 IX = 0; IX < NX; ++IX)
		{
			if (A[IX] + A[IX + 1] + B[IX] + B[IX + 1] < 0.01f) { continue; }
			const double X0 = Left + IX * Step, X1 = X0 + Step;
			const FLinearColor C00 = ECAlpha(Shadow, A[IX]), C10 = ECAlpha(Shadow, A[IX + 1]), C01 = ECAlpha(Shadow, B[IX]), C11 = ECAlpha(Shadow, B[IX + 1]);
			D.TriColors(FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), C00, C10, C11);
			D.TriColors(FVector2D(X0, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1), C00, C11, C01);
		}
	}
}

void FECWorldRenderer::DrawOverlays(FECDraw& D, const FECGame& G, double Time)
{
	D.Flush();
	D.SetPixels();
	const double W = D.ScreenW, H = D.ScreenH;

	// Dust in the light (screen space, cheap).
	D.Additive();
	for (int32 I = 0; I < 36; ++I)
	{
		const double X = FMath::Fmod(Hash(I) * W + FMath::Sin(Time * 0.3 + I) * 30.0 + W, W);
		const double Y = FMath::Fmod(Hash(I + 77) * H - Time * (8.0 + Hash(I + 9) * 14.0) + H * 10.0, H);
		D.Circle(X, Y, 1.2 + Hash(I + 5) * 1.4, ECColor(Dust, (float)(0.18 + 0.15 * FMath::Sin(Time * 1.7 + I))), 6);
	}
	D.Translucent();

	const double P = G.FxProgress();
	if (G.Fx == EECFx::Rewind || G.Fx == EECFx::Restart)
	{
		// VHS rewind: a cold tint, scanlines, bright tracking bands rolling up.
		const double Env = FMath::Sin(P * PI);
		D.Rect(0, 0, W, H, ECColor(0x061020, 0.35f * (float)Env));
		for (double Y = 0; Y < H; Y += 4) { D.Rect(0, Y, W, Y + 1.5, FLinearColor(0, 0, 0, 0.22f * (float)Env)); }
		D.Additive();
		for (int32 K = 0; K < 3; ++K)
		{
			const double BY = FMath::Fmod(H * (1.0 - P * 2.5) + K * H * 0.37 + H * 3, H);
			const double BH = H * (0.015 + 0.02 * Hash(K + int32(Time * 20)));
			D.RectV(0, BY, W, BY + BH, ECColor(0xAFEFFF, 0.0f), ECColor(0xAFEFFF, 0.12f * (float)Env));
			D.Rect(0, BY + BH, W, BY + BH + 2, ECColor(0xFFFFFF, 0.2f * (float)Env));
		}
		// RGB fringe on the edges
		D.RectH(0, 0, W * 0.04, H, ECColor(0xFF2050, 0.12f * (float)Env), ECColor(0xFF2050, 0.0f));
		D.RectH(W * 0.96, 0, W, H, ECColor(0x2060FF, 0.0f), ECColor(0x2060FF, 0.12f * (float)Env));
		D.Translucent();
	}
	else if (G.Fx == EECFx::Paradox)
	{
		// Glitch: broken colour bars, a hard flicker, the screen tearing.
		const double Env = 1.0 - P * 0.6;
		D.Additive();
		for (int32 K = 0; K < 8; ++K)
		{
			const int32 Seed = K * 31 + int32(Time * 20.0);
			const double Y = Hash(Seed) * H, BH = H * (0.003 + Hash(Seed + 1) * 0.018);
			const double X = Hash(Seed + 2) * W * 0.6, BW = W * (0.1 + Hash(Seed + 3) * 0.5);
			const uint32 Col = Hash(Seed + 4) < 0.5 ? 0xFF3DDC : 0x3DF2FF;
			D.Rect(X, Y, X + BW, Y + BH, ECColor(Col, (float)(0.2 * Env)));
		}
		D.Translucent();
		if (FMath::Fmod(Time * 8.0, 1.0) < 0.2) { D.Rect(0, 0, W, H, ECColor(0xFF3DDC, 0.08f * (float)Env)); }
	}
	else if (G.Fx == EECFx::Death)
	{
		D.Rect(0, 0, W, H, ECColor(Danger, 0.35f * (float)(1.0 - P)));
	}
	else if (G.Fx == EECFx::OutOfLoops)
	{
		D.Rect(0, 0, W, H, ECColor(0x05080F, 0.55f * (float)P));
	}

	if (G.Flash > 0)
	{
		D.Additive();
		D.Rect(0, 0, W, H, ECColor(0xFFF4E0, 0.55f * (float)G.Flash));
		D.Translucent();
	}

	// Vignette
	const FLinearColor Dark = ECColor(0x02050B, 0.7f), None = ECColor(0x02050B, 0.0f);
	D.RectV(0, 0, W, H * 0.22, Dark, None);
	D.RectV(0, H * 0.78, W, H, None, Dark);
	D.RectH(0, 0, W * 0.14, H, Dark, None);
	D.RectH(W * 0.86, 0, W, H, None, Dark);
	D.Flush();
}
