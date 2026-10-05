// ECHOES: menus, HUD, touch pads. Everything in pixels, sized from the screen height. (CLAUDE.md: UI)
#include "UI/ECUI.h"

#include "CanvasItem.h"
#include "Core/ECLevel.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/ECGame.h"
#include "Game/ECSaveGame.h"
#include "Render/ECDraw.h"
#include "Render/ECWorldRenderer.h"
#include "Rendering/SlateRenderer.h"

namespace
{
	const uint32 Ink = 0xF4F7FF, Dim = 0x9FB2CC, Faint = 0x5D7090, Accent = 0x3DF2FF, Danger = 0xFF4D5E, Warm = 0xFFF4E0;
	const uint32 Glass = 0x0E1B30, GlassEdge = 0x24466F, Paradox = 0xFF3DDC;

	// ---- Text through the Slate font cache (crisp at any size) --------------------------------

	FSlateFontInfo FontInfo(UFont* Font, double Px, int32 Spacing, bool bBold)
	{
		FSlateFontInfo Info(Font, FMath::Max(1, FMath::RoundToInt(Px * 0.75)), bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
		Info.LetterSpacing = Spacing;
		return Info;
	}

	FVector2D Measure(const FString& S, const FSlateFontInfo& Info, double Px)
	{
		if (FSlateApplication::IsInitialized())
		{
			return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Info);
		}
		return FVector2D(S.Len() * Px * 0.55, Px * 1.2);
	}

	struct FUi
	{
		FECDraw& D;
		UCanvas* Canvas;
		FECGame& G;
		const FECUiContext& Ctx;
		double W, H;

		// Align: 0 left, 0.5 centre, 1 right. Y is the vertical centre of the line.
		FVector2D Text(const FString& S, double X, double Y, double Px, const FLinearColor& Color, double Align = 0.5, int32 Spacing = 0, bool bBold = false, bool bShadow = true)
		{
			if (!Ctx.Font || S.IsEmpty() || Color.A <= 0.001f) { return FVector2D::ZeroVector; }
			D.Flush();
			const FSlateFontInfo Info = FontInfo(Ctx.Font, Px, Spacing, bBold);
			const FVector2D Size = Measure(S, Info, Px);
			FCanvasTextItem Item(FVector2D(X - Size.X * Align, Y - Size.Y * 0.5), FText::FromString(S), Info, Color);
			Item.BlendMode = SE_BLEND_Translucent;
			if (bShadow) { Item.EnableShadow(FLinearColor(0, 0, 0, 0.55f * Color.A), FVector2D(1, 1) * FMath::Max(1.0, Px * 0.05)); }
			Canvas->DrawItem(Item);
			return Size;
		}

		void Pill(double X0, double Y0, double X1, double Y1, const FLinearColor& C)
		{
			const double R = (Y1 - Y0) * 0.5;
			D.Rect(X0 + R, Y0, X1 - R, Y1, C);
			D.Circle(X0 + R, Y0 + R, R, C, 16);
			D.Circle(X1 - R, Y0 + R, R, C, 16);
		}

		bool Focused(int32 Index) const { return !G.bPointerActive && G.Focus == Index; }

		// A menu button: registers itself for hit testing (next frame) and draws.
		void Button(const FString& Label, double CX, double CY, double BW, double BH, EECAction Action, int32 Param = 0, bool bEnabled = true, bool bPrimary = false)
		{
			FECButton B;
			B.Box = FBox2D(FVector2D(CX - BW * 0.5, CY - BH * 0.5), FVector2D(CX + BW * 0.5, CY + BH * 0.5));
			B.Action = Action;
			B.Param = Param;
			B.bEnabled = bEnabled;
			const int32 Index = G.Buttons.Add(B);
			const bool bFocus = Focused(Index) || (G.bPointerActive && G.Focus == Index);
			const double X0 = CX - BW * 0.5, Y0 = CY - BH * 0.5, X1 = CX + BW * 0.5, Y1 = CY + BH * 0.5;
			const FLinearColor Edge = bPrimary ? ECColor(Accent) : (bFocus ? ECColor(Accent, 0.9f) : ECColor(GlassEdge));
			Pill(X0 - 2, Y0 - 2, X1 + 2, Y1 + 2, ECAlpha(Edge, bEnabled ? 1.0 : 0.35));
			Pill(X0, Y0, X1, Y1, bPrimary ? ECColor(0x0F3A4A, 0.95f) : ECColor(Glass, 0.92f));
			if (bFocus && bEnabled)
			{
				D.Additive();
				D.Glow(CX, CY, BW * 0.55, ECColor(Accent, 0.12f), ECColor(Accent, 0.0f), 24);
				D.Translucent();
			}
			Text(Label, CX, CY, BH * 0.42, ECColor(bEnabled ? Ink : Faint), 0.5, (int32)(BH * 0.04), true);
		}

		// A round icon button (HUD). Returns its index.
		int32 IconButton(double CX, double CY, double R, EECAction Action, bool bPrimary)
		{
			FECButton B;
			B.Box = FBox2D(FVector2D(CX - R * 1.25, CY - R * 1.25), FVector2D(CX + R * 1.25, CY + R * 1.25));
			B.Action = Action;
			const int32 Index = G.Buttons.Add(B);
			D.Circle(CX, CY, R + 2, bPrimary ? ECColor(Accent, 0.9f) : ECColor(GlassEdge, 0.9f), 28);
			D.Circle(CX, CY, R, bPrimary ? ECColor(0x0F3A4A, 0.85f) : ECColor(Glass, 0.75f), 28);
			return Index;
		}

		void Star(double CX, double CY, double R, const FLinearColor& C)
		{
			FVector2D P[12];
			P[0] = FVector2D(CX, CY);
			for (int32 I = 0; I <= 10; ++I)
			{
				const double A = -PI * 0.5 + I * PI / 5.0;
				const double RR = (I % 2 == 0) ? R : R * 0.45;
				P[I + 1] = FVector2D(CX + FMath::Cos(A) * RR, CY + FMath::Sin(A) * RR);
			}
			D.Poly(P, 12, C);
		}

		void Diamond(double CX, double CY, double R, const FLinearColor& C)
		{
			D.Tri(CX - R * 0.62, CY, CX, CY - R, CX, CY + R, C);
			D.Tri(CX + R * 0.62, CY, CX, CY - R, CX, CY + R, C);
		}

		// ---- Icons ---------------------------------------------------------------------------
		void IconRewind(double CX, double CY, double S, const FLinearColor& C)
		{
			D.Tri(CX - S * 0.05, CY - S * 0.5, CX - S * 0.05, CY + S * 0.5, CX - S * 0.6, CY, C);
			D.Tri(CX + S * 0.5, CY - S * 0.5, CX + S * 0.5, CY + S * 0.5, CX - S * 0.05, CY, C);
		}
		void IconRestart(double CX, double CY, double S, const FLinearColor& C)
		{
			D.Arc(CX, CY, S * 0.34, S * 0.5, -PI * 0.2, PI * 1.35, C, 28);
			const double A = -PI * 0.2;
			const FVector2D Tip(CX + FMath::Cos(A) * S * 0.42, CY + FMath::Sin(A) * S * 0.42);
			D.Tri(Tip.X - S * 0.22, Tip.Y - S * 0.02, Tip.X + S * 0.22, Tip.Y - S * 0.02, Tip.X, Tip.Y + S * 0.25, C);
		}
		void IconPause(double CX, double CY, double S, const FLinearColor& C)
		{
			D.Rect(CX - S * 0.38, CY - S * 0.45, CX - S * 0.12, CY + S * 0.45, C);
			D.Rect(CX + S * 0.12, CY - S * 0.45, CX + S * 0.38, CY + S * 0.45, C);
		}
		void IconArrow(double CX, double CY, double S, bool bRight, const FLinearColor& C)
		{
			const double F = bRight ? 1 : -1;
			D.Tri(CX + F * S * 0.45, CY, CX - F * S * 0.3, CY - S * 0.5, CX - F * S * 0.3, CY + S * 0.5, C);
		}
		void IconJump(double CX, double CY, double S, const FLinearColor& C)
		{
			D.Tri(CX, CY - S * 0.55, CX - S * 0.5, CY + S * 0.05, CX + S * 0.5, CY + S * 0.05, C);
			D.Rect(CX - S * 0.16, CY + S * 0.05, CX + S * 0.16, CY + S * 0.5, C);
		}
		// A tiny silhouette of a level: open space lit in the world colour, hazards and the exit marked.
		void LevelPreview(int32 Index, double X0, double Y0, double PW, double PH, const FLinearColor& WA, double Alpha)
		{
			static TArray<EC::FLevel> Cache;
			if (Cache.Num() != EC::NumLevels())
			{
				Cache.SetNum(EC::NumLevels());
				char Err[128];
				for (int32 I = 0; I < Cache.Num(); ++I) { Cache[I].Parse(EC::GetLevelDef(I), Err, sizeof Err); }
			}
			const EC::FLevel& L = Cache[Index];
			if (L.W <= 0 || L.H <= 0) { return; }
			const double T = FMath::Min(PW / L.W, PH / L.H);
			const double OX = X0 + (PW - L.W * T) * 0.5, OY = Y0 + (PH - L.H * T) * 0.5;
			D.Rect(OX, OY, OX + L.W * T, OY + L.H * T, ECColor(0x05090F, 0.9f * (float)Alpha));
			for (int32 Y = 0; Y < L.H; ++Y)
				for (int32 X = 0; X < L.W; ++X)
				{
					const EC::ETile Tile = L.TileAt(X, Y);
					if (Tile == EC::ETile::Solid) { continue; }
					const double PX = OX + X * T, PY = OY + (L.H - 1 - Y) * T;
					D.Rect(PX, PY, PX + T + 0.5, PY + T + 0.5, ECAlpha(WA, 0.2 * Alpha));
					if (Tile == EC::ETile::Spike) { D.Tri(PX, PY + T, PX + T, PY + T, PX + T * 0.5, PY + T * 0.3, ECColor(Danger, (float)Alpha)); }
				}
			for (int32 I = 0; I < L.NumDoors; ++I)
			{
				const EC::FDoor& Dr = L.Doors[I];
				D.Rect(OX + (Dr.X0 + 0.3) * T, OY + (L.H - Dr.Y1) * T, OX + (Dr.X1 - 0.3) * T, OY + (L.H - Dr.Y0) * T, ECAlpha(FECWorldRenderer::ChannelColor(Dr.Channel), Alpha));
			}
			for (int32 I = 0; I < L.NumPlates; ++I)
			{
				const EC::FPlate& Pl = L.Plates[I];
				D.Rect(OX + Pl.X0 * T, OY + (L.H - Pl.Y) * T - T * 0.3, OX + Pl.X1 * T, OY + (L.H - Pl.Y) * T, ECAlpha(FECWorldRenderer::ChannelColor(Pl.Channel), Alpha));
			}
			for (int32 I = 0; I < L.NumEmitters; ++I)
			{
				const EC::FEmitter& E = L.Emitters[I];
				const double EX = OX + (E.X + 0.5) * T, EY = OY + (L.H - E.Y - 0.5) * T;
				D.Line(EX, EY, EX + E.DX * T * 3.0, EY - E.DY * T * 3.0, FMath::Max(1.0, T * 0.2), ECColor(0xFF2E63, 0.8f * (float)Alpha));
			}
			D.Rect(OX + (L.Exit.X - 0.3) * T, OY + (L.H - L.Exit.Y - 2.0) * T, OX + (L.Exit.X + 0.3) * T, OY + (L.H - L.Exit.Y) * T, ECColor(Warm, (float)Alpha));
			D.Circle(OX + L.Spawn.X * T, OY + (L.H - L.Spawn.Y - 0.8) * T, T * 0.45, ECColor(Accent, (float)Alpha), 8);
		}

		void IconLock(double CX, double CY, double S, const FLinearColor& C)
		{
			D.Rect(CX - S * 0.4, CY - S * 0.05, CX + S * 0.4, CY + S * 0.5, C);
			D.Arc(CX, CY - S * 0.05, S * 0.18, S * 0.3, PI, 2.0 * PI, C, 16);
		}

		// ---- Screens -------------------------------------------------------------------------
		void Darken(double A) { D.Rect(0, 0, W, H, ECColor(0x03060D, (float)A)); }

		void Title()
		{
			Darken(0.45);
			const double TY = H * 0.3;
			const double Px = H * 0.17;
			const int32 Sp = (int32)(H * 0.03);
			const double Wob = FMath::Sin(G.RealTime * 1.3) * H * 0.004;
			// The title and its Echoes: two tinted copies trailing the real one.
			Text(TEXT("ECHOES"), W * 0.5 - H * 0.016 + Wob, TY + H * 0.006, Px, ECColor(0xFFB020, 0.55f), 0.5, Sp, true, false);
			Text(TEXT("ECHOES"), W * 0.5 + H * 0.016 - Wob, TY - H * 0.004, Px, ECColor(Paradox, 0.5f), 0.5, Sp, true, false);
			Text(TEXT("ECHOES"), W * 0.5, TY, Px, ECColor(Ink), 0.5, Sp, true);
			D.Additive();
			D.Glow(W * 0.5, TY, H * 0.45, ECColor(Accent, 0.08f), ECColor(Accent, 0.0f), 32);
			D.Translucent();
			Text(TEXT("YOU ARE YOUR ONLY TEAMMATE"), W * 0.5, TY + H * 0.12, H * 0.028, ECColor(Dim), 0.5, (int32)(H * 0.006));

			const double BW = H * 0.42, BH = H * 0.085;
			double Y = H * 0.6;
			Button(TEXT("PLAY"), W * 0.5, Y, BW, BH, EECAction::Play, 0, true, true);
			Y += BH * 1.3;
			Button(TEXT("LEVELS"), W * 0.5, Y, BW, BH, EECAction::Levels);
			if (FECGame::PlatformHasQuitButton())
			{
				Y += BH * 1.3;
				Button(TEXT("QUIT"), W * 0.5, Y, BW, BH, EECAction::Quit);
			}
			Text(TEXT("BRAINROT INTERACTIVE STUDIOS"), W * 0.5, H - Ctx.Safe.W - H * 0.035, H * 0.02, ECColor(Faint), 0.5, (int32)(H * 0.005));
		}

		void LevelSelect()
		{
			Darken(0.62);
			static const uint32 WorldAccent[] = { 0x3DF2FF, 0xFF8A2B, 0xFFD166, 0xFF3DDC };
			const int32 World = FMath::Clamp(G.SelectWorld, 1, EC::NumWorlds());
			const FLinearColor WA = ECColor(WorldAccent[(World - 1) % 4]);
			Text(FString::Printf(TEXT("WORLD %d"), World), W * 0.5, H * 0.1, H * 0.028, WA, 0.5, (int32)(H * 0.008), true);
			Text(FString(EC::WorldName(World)).ToUpper(), W * 0.5, H * 0.155, H * 0.065, ECColor(Ink), 0.5, (int32)(H * 0.012), true);
			{
				bool bAll = false;
				const double Total = G.WorldTotalTime(World, bAll);
				if (Total > 0)
				{
					Text(FString::Printf(TEXT("%s  %s"), bAll ? TEXT("WORLD TIME") : TEXT("TIME SO FAR"), *FECGame::FormatTime(Total)), W * 0.5, H * 0.215, H * 0.02, bAll ? WA : ECColor(Dim), 0.5, 2, true);
				}
			}

			const int32 First = EC::FirstLevelOfWorld(World), Last = EC::FirstLevelOfWorld(World + 1);
			const int32 Cols = 5;
			const double CW = H * 0.21, CH = H * 0.255, Gap = H * 0.022;
			const double X0 = W * 0.5 - (Cols * CW + (Cols - 1) * Gap) * 0.5;
			const double Y0 = H * 0.245;
			for (int32 I = First; I < Last; ++I)
			{
				const EC::FLevelDef& Def = EC::GetLevelDef(I);
				const bool bOpen = I < G.Save->UnlockedLevels;
				const int32 K = I - First;
				const double CX = X0 + (K % Cols) * (CW + Gap) + CW * 0.5;
				const double CY = Y0 + (K / Cols) * (CH + Gap) + CH * 0.5;
				FECButton B;
				B.Box = FBox2D(FVector2D(CX - CW * 0.5, CY - CH * 0.5), FVector2D(CX + CW * 0.5, CY + CH * 0.5));
				B.Action = EECAction::SelectLevel;
				B.Param = I;
				B.bEnabled = bOpen;
				const int32 Index = G.Buttons.Add(B);
				const bool bFocus = G.Focus == Index;
				D.Rect(CX - CW * 0.5 - 2, CY - CH * 0.5 - 2, CX + CW * 0.5 + 2, CY + CH * 0.5 + 2, bFocus && bOpen ? WA : ECColor(GlassEdge, bOpen ? 1.f : 0.4f));
				D.RectV(CX - CW * 0.5, CY - CH * 0.5, CX + CW * 0.5, CY + CH * 0.5, ECColor(0x12233D, 0.95f), ECColor(Glass, 0.95f));
				// The silhouette of the level fills the top of the card; locked chapters stay in shadow.
				LevelPreview(I, CX - CW * 0.44, CY - CH * 0.45, CW * 0.88, CH * 0.5, WA, bOpen ? 1.0 : 0.3);
				if (!bOpen)
				{
					IconLock(CX, CY + CH * 0.25, CH * 0.13, ECColor(Faint));
					continue;
				}
				if (bFocus)
				{
					D.Additive();
					D.Glow(CX, CY, CW * 0.75, ECAlpha(WA, 0.14), ECAlpha(WA, 0.0), 24);
					D.Translucent();
				}
				Text(FString::Printf(TEXT("%d-%d  %s"), World, K + 1, *FString(Def.Name).ToUpper()), CX, CY + CH * 0.15, CH * 0.068, ECColor(Ink), 0.5, 1, true);
				const float BestT = G.Save->BestTimes.IsValidIndex(I) ? G.Save->BestTimes[I] : 0.f;
				if (BestT > 0) { Text(FECGame::FormatTime(BestT), CX, CY + CH * 0.4, CH * 0.06, ECColor(Dim), 0.5, 1); }
				const uint8 Stars = G.Save->Stars.IsValidIndex(I) ? G.Save->Stars[I] : 0;
				for (int32 S = 0; S < 3; ++S)
				{
					const bool bGot = (Stars & (1 << S)) != 0;
					Star(CX + (S - 1) * CH * 0.17, CY + CH * 0.28, CH * 0.062, bGot ? ECColor(0xFFD166) : ECColor(Faint, 0.6f));
				}
			}

			// World arrows
			const double AR = H * 0.05, AY = Y0 + CH + Gap * 0.5;
			if (World > 1)
			{
				const double AX = X0 - AR * 2.2;
				const int32 Index = IconButton(AX, AY, AR, EECAction::WorldPrev, false);
				if (G.Focus == Index && !G.bPointerActive) { D.Ring(AX, AY, AR + 2, AR + 5, WA, WA, 28); }
				IconArrow(AX, AY, AR * 0.8, false, ECColor(Ink));
			}
			if (World < EC::NumWorlds())
			{
				const double AX = X0 + Cols * CW + (Cols - 1) * Gap + AR * 2.2;
				const int32 Index = IconButton(AX, AY, AR, EECAction::WorldNext, false);
				if (G.Focus == Index && !G.bPointerActive) { D.Ring(AX, AY, AR + 2, AR + 5, WA, WA, 28); }
				IconArrow(AX, AY, AR * 0.8, true, ECColor(Ink));
			}
			// Page dots
			for (int32 Wd = 1; Wd <= EC::NumWorlds(); ++Wd)
			{
				const double DX = W * 0.5 + (Wd - (EC::NumWorlds() + 1) * 0.5) * H * 0.035;
				D.Circle(DX, H * 0.825, H * 0.008, Wd == World ? WA : ECColor(Faint), 12);
			}
			Button(TEXT("BACK"), W * 0.5, H * 0.9, H * 0.3, H * 0.065, EECAction::Back);
		}

		void Paused()
		{
			Darken(0.6);
			Text(TEXT("PAUSED"), W * 0.5, H * 0.16, H * 0.07, ECColor(Ink), 0.5, (int32)(H * 0.012), true);
			const double BW = H * 0.44, BH = H * 0.075, Step = BH * 1.28;
			const double XL = W * 0.5 - BW * 0.54, XR = W * 0.5 + BW * 0.54;
			double Y = H * 0.32;
			Button(TEXT("RESUME"), XL, Y, BW, BH, EECAction::Resume, 0, true, true); Y += Step;
			Button(TEXT("RESTART LEVEL"), XL, Y, BW, BH, EECAction::Restart); Y += Step;
			Button(G.bHintActive ? TEXT("HINT: ON") : TEXT("SHOW ME A HINT"), XL, Y, BW, BH, EECAction::Hint, 0, !G.bHintActive); Y += Step;
			Button(TEXT("SKIP THIS LEVEL"), XL, Y, BW, BH, EECAction::Skip, 0, G.SkipAvailable()); Y += Step;
			Button(TEXT("LEVELS"), XL, Y, BW, BH, EECAction::Levels);
			Y = H * 0.32;
			Button(G.Save->bMusic ? TEXT("MUSIC: ON") : TEXT("MUSIC: OFF"), XR, Y, BW, BH, EECAction::ToggleMusic); Y += Step;
			Button(G.Save->bSound ? TEXT("SOUND: ON") : TEXT("SOUND: OFF"), XR, Y, BW, BH, EECAction::ToggleSound); Y += Step;
			static const TCHAR* TouchNames[] = { TEXT("TOUCH PADS: AUTO"), TEXT("TOUCH PADS: ON"), TEXT("TOUCH PADS: OFF") };
			Button(TouchNames[(int32)G.Save->TouchMode % 3], XR, Y, BW, BH, EECAction::CycleTouch); Y += Step;
			Button(TEXT("TITLE"), XR, Y, BW, BH, EECAction::ToTitle);
			if (!G.SkipAvailable() && G.LevelIndex < G.NumLevels() - 1)
			{
				Text(TEXT("SKIP UNLOCKS IF A LEVEL KEEPS BEATING YOU"), W * 0.5, H * 0.86, H * 0.018, ECColor(Faint), 0.5, 2);
			}
		}

		void Complete()
		{
			Darken(0.55);
			const EC::FLevelDef& Def = EC::GetLevelDef(G.LevelIndex);
			const double T = FMath::Clamp(G.ScreenTime / 0.5, 0.0, 1.0);
			Text(TEXT("LEVEL COMPLETE"), W * 0.5, H * 0.18, H * 0.03, ECAlpha(ECColor(Accent), T), 0.5, (int32)(H * 0.01), true);
			Text(FString(Def.Name).ToUpper(), W * 0.5, H * 0.26, H * 0.08, ECAlpha(ECColor(Ink), T), 0.5, (int32)(H * 0.01), true);

			static const TCHAR* Labels[3] = { TEXT("SOLVED"), TEXT("PAR"), TEXT("SHARD") };
			for (int32 S = 0; S < 3; ++S)
			{
				const double SX = W * 0.5 + (S - 1) * H * 0.2, SY = H * 0.43;
				const double Appear = FMath::Clamp((G.ScreenTime - 0.3 - S * 0.25) / 0.25, 0.0, 1.0);
				const bool bGot = (G.ResultStars & (1 << S)) != 0;
				const double Pop = 1.0 + 0.35 * FMath::Sin(Appear * PI) * (bGot ? 1.0 : 0.0);
				Star(SX, SY, H * 0.055 * Pop, ECColor(Faint, 0.5f));
				if (bGot && Appear > 0)
				{
					D.Additive();
					D.Glow(SX, SY, H * 0.09 * Appear, ECColor(0xFFD166, 0.35f), ECColor(0xFFD166, 0.0f), 20);
					D.Translucent();
					Star(SX, SY, H * 0.055 * Pop * Appear, ECColor(0xFFD166));
				}
				const FString Label = S == 1 ? FString::Printf(TEXT("PAR %d"), Def.ParLoops) : FString(Labels[S]);
				Text(Label, SX, SY + H * 0.085, H * 0.022, ECColor(bGot ? Ink : Faint), 0.5, 2, true);
			}
			Text(FString::Printf(TEXT("SOLVED IN %d LOOP%s"), G.ResultLoops, G.ResultLoops == 1 ? TEXT("") : TEXT("S")), W * 0.5, H * 0.585, H * 0.03, ECColor(Dim), 0.5, 2);
			Text(FString::Printf(TEXT("TIME  %s"), *FECGame::FormatTime(G.ResultTime)), W * 0.5 - (G.bNewBestTime ? H * 0.07 : 0.0), H * 0.64, H * 0.03, ECColor(Ink), 0.5, 2, true);
			if (G.bNewBestTime) { Text(TEXT("NEW BEST"), W * 0.5 + H * 0.2, H * 0.64, H * 0.022, ECColor(Accent), 0.5, 2, true); }

			const bool bLast = G.LevelIndex >= G.NumLevels() - 1;
			const double BW = H * 0.3, BH = H * 0.08;
			const double Y = H * 0.76;
			if (!bLast)
			{
				Button(TEXT("NEXT"), W * 0.5 + BW * 1.1, Y, BW, BH, EECAction::NextLevel, 0, true, true);
				Button(TEXT("REPLAY"), W * 0.5, Y, BW, BH, EECAction::Replay);
				Button(TEXT("LEVELS"), W * 0.5 - BW * 1.1, Y, BW, BH, EECAction::Levels);
			}
			else
			{
				Text(TEXT("YOU ESCAPED THE FACILITY - MORE LOOPS SOON"), W * 0.5, H * 0.695, H * 0.024, ECColor(Warm), 0.5, 2);
				Button(TEXT("REPLAY"), W * 0.5 + BW * 0.56, Y, BW, BH, EECAction::Replay);
				Button(TEXT("LEVELS"), W * 0.5 - BW * 0.56, Y, BW, BH, EECAction::Levels, 0, true, true);
			}
		}

		// ---- In-game HUD -----------------------------------------------------------------------
		void Hud()
		{
			const EC::FSim& S = G.Sim;
			const EC::FLevelDef& Def = EC::GetLevelDef(G.LevelIndex);
			const double Top = Ctx.Safe.Y + H * 0.02;

			// Loop clock: remaining time as a ring, seconds inside; cyan, turning red in the last 3 seconds.
			const double CX = W * 0.5, CY = Top + H * 0.065, R = H * 0.052;
			const double Left = G.Fx == EECFx::Rewind ? 10.0 * G.FxProgress() : S.TimeLeft();
			const double Frac = FMath::Clamp(Left / EC::LoopSeconds, 0.0, 1.0);
			const double Hot = FMath::Clamp((3.0 - Left) / 3.0, 0.0, 1.0);
			const FLinearColor RingC = FMath::Lerp(ECColor(Accent), ECColor(Danger), (float)Hot);
			D.Circle(CX, CY, R * 1.28, ECColor(Glass, 0.7f), 32);
			D.Ring(CX, CY, R * 0.86, R, ECColor(0xFFFFFF, 0.1f), ECColor(0xFFFFFF, 0.1f), 48);
			D.Arc(CX, CY, R * 0.86, R, -PI * 0.5, -PI * 0.5 + 2.0 * PI * Frac, RingC, 48);
			D.Additive();
			D.Glow(CX, CY, R * 1.9, ECAlpha(RingC, 0.18 + 0.2 * Hot * (0.5 + 0.5 * FMath::Sin(G.RealTime * 14.0))), ECAlpha(RingC, 0), 24);
			D.Translucent();
			Text(FString::Printf(TEXT("%.1f"), Left), CX, CY, R * 0.8, FMath::Lerp(ECColor(Ink), ECColor(Danger), (float)Hot), 0.5, 0, true);

			// Loop pips: past loops in their Echo colours, this loop pulsing, the rest hollow.
			const int32 N = S.LoopsAllowed();
			const double PR = H * 0.011, PG = H * 0.034;
			const double PY = CY + R * 1.28 + H * 0.028;
			for (int32 I = 0; I < N; ++I)
			{
				const double PX = CX + (I - (N - 1) * 0.5) * PG;
				if (I < S.NumEchoes)
				{
					const FLinearColor EC = ECColor(FECWorldRenderer::EchoStyle(I).Color);
					D.Circle(PX, PY, PR, EC, 14);
				}
				else if (I == S.NumEchoes)
				{
					D.Ring(PX, PY, PR * 0.7, PR * (1.15 + 0.15 * FMath::Sin(G.RealTime * 6.0)), ECColor(Ink), ECColor(Ink), 20);
					D.Circle(PX, PY, PR * 0.45, ECColor(Ink), 10);
				}
				else
				{
					D.Ring(PX, PY, PR * 0.72, PR, ECColor(Faint), ECColor(Faint), 20);
				}
			}
			Text(FString::Printf(TEXT("LOOP %d / %d"), FMath::Min(S.LoopsUsed(), N), N), CX, PY + H * 0.032, H * 0.02, ECColor(Dim), 0.5, 2, true);

			// Level card (top left)
			const double LX = Ctx.Safe.X + H * 0.035;
			Text(FString::Printf(TEXT("%d-%d"), Def.World, EC::LevelNumberInWorld(G.LevelIndex)), LX, Top + H * 0.03, H * 0.024, ECColor(Def.World == 2 ? 0xFF8A2B : Accent), 0.0, 2, true);
			Text(FString(Def.Name).ToUpper(), LX, Top + H * 0.065, H * 0.036, ECColor(Ink), 0.0, 2, true);
			Text(FString::Printf(TEXT("PAR %d LOOPS"), Def.ParLoops), LX, Top + H * 0.103, H * 0.019, ECColor(Dim), 0.0, 2);
			if (S.Level.HasShard)
			{
				const bool bGot = S.ShardTaken();
				Diamond(LX + H * 0.012, Top + H * 0.14, H * 0.014, bGot ? ECColor(0xCFFBFF) : ECColor(Faint, 0.7f));
				Text(bGot ? TEXT("SHARD") : TEXT("SHARD ?"), LX + H * 0.034, Top + H * 0.14, H * 0.017, ECColor(bGot ? Ink : Faint), 0.0, 2);
			}
			{
				// Speedrun clock for this attempt, and the time to beat.
				Text(FECGame::FormatTime(G.RunTime), LX, Top + H * 0.182, H * 0.026, ECColor(Ink), 0.0, 1, true);
				const float BestT = G.Save->BestTimes.IsValidIndex(G.LevelIndex) ? G.Save->BestTimes[G.LevelIndex] : 0.f;
				if (BestT > 0) { Text(FString::Printf(TEXT("BEST %s"), *FECGame::FormatTime(BestT)), LX, Top + H * 0.213, H * 0.016, ECColor(Dim), 0.0, 1); }
			}

			if (G.Fx == EECFx::OutOfLoops) { Effects(); return; }   // the panel owns the buttons

			// Buttons (top right): rewind (primary), restart, pause.
			const double BR = H * 0.042, BY = Top + H * 0.055;
			double BX = W - Ctx.Safe.Z - H * 0.06;
			IconButton(BX, BY, BR, EECAction::Pause, false);
			IconPause(BX, BY, BR * 0.8, ECColor(Ink));
			BX -= BR * 2.6;
			IconButton(BX, BY, BR, EECAction::Restart, false);
			IconRestart(BX, BY, BR * 1.05, ECColor(Ink));
			BX -= BR * 2.9;
			const bool bCanRewind = S.Tick >= EC::MinRewindTick && G.Fx == EECFx::None;
			IconButton(BX, BY, BR * 1.2, EECAction::Rewind, bCanRewind);
			if (G.HintAvailable())
			{
				// A lifeline once the level has beaten you a couple of times.
				const double HX = BX - BR * 2.9;
				if (!G.bHintActive)
				{
					D.Additive();
					D.Glow(HX, BY, BR * 2.2, ECColor(Warm, 0.18f + 0.12f * (float)FMath::Sin(G.RealTime * 4.0)), ECColor(Warm, 0.0f), 20);
					D.Translucent();
				}
				IconButton(HX, BY, BR, EECAction::Hint, G.bHintActive);
				Text(TEXT("?"), HX, BY, BR * 1.2, ECColor(G.bHintActive ? Accent : Warm), 0.5, 0, true);
				Text(G.bHintActive ? TEXT("FOLLOW THE GHOST") : TEXT("HINT"), HX, BY + BR * 1.75, H * 0.015, ECColor(Dim), 0.5, 1, true);
			}
			IconRewind(BX + BR * 0.05, BY, BR * 0.95, ECColor(bCanRewind ? Ink : Faint));
			if (!Ctx.bShowTouch && G.LevelIndex == 0 && S.NumEchoes == 0)
			{
				Text(TEXT("R"), BX, BY + BR * 1.9, H * 0.018, ECColor(Dim), 0.5, 0, true);
			}

			// First level, first loop: show the keys once (touch players see the pads instead).
			if (G.LevelIndex == 0 && S.NumEchoes == 0 && !Ctx.bShowTouch && G.Fx == EECFx::None)
			{
				Text(TEXT("A / D  MOVE        SPACE  JUMP        R  REWIND - YOUR RUN BECOMES AN ECHO"), W * 0.5, H - Ctx.Safe.W - H * 0.04, H * 0.02, ECColor(Dim, 0.85f), 0.5, 2);
			}

			// Level intro
			if (S.NumEchoes == 0 && G.LevelTime < 3.0 && G.Fx == EECFx::None)
			{
				const double A = FMath::Clamp(3.0 - G.LevelTime, 0.0, 1.0) * FMath::Clamp(G.LevelTime * 3.0, 0.0, 1.0);
				Text(FString(Def.Name).ToUpper(), W * 0.5, H * 0.42, H * 0.075, ECColor(Ink, (float)A), 0.5, (int32)(H * 0.012), true);
				Text(FString::Printf(TEXT("%d ECHOES ALLOWED"), Def.MaxEchoes), W * 0.5, H * 0.5, H * 0.026, ECColor(Accent, (float)A), 0.5, (int32)(H * 0.006), true);
			}

			Effects();
		}

		void Effects()
		{
			const double P = G.FxProgress();
			switch (G.Fx)
			{
			case EECFx::Rewind:
			{
				const double A = FMath::Sin(P * PI);
				IconRewind(W * 0.5 - H * 0.14, H * 0.42, H * 0.05, ECColor(Ink, (float)A));
				Text(TEXT("REWIND"), W * 0.5 - H * 0.08, H * 0.42, H * 0.06, ECColor(Ink, (float)A), 0.0, (int32)(H * 0.01), true);
				const int32 Echo = G.Sim.NumEchoes;
				Text(FString::Printf(TEXT("ECHO %d RECORDED"), Echo + 1), W * 0.5, H * 0.5, H * 0.026,
					ECColor(FECWorldRenderer::EchoStyle(Echo).Color, (float)A), 0.5, (int32)(H * 0.006), true);
				break;
			}
			case EECFx::Restart:
			{
				const double A = FMath::Sin(P * PI);
				IconRestart(W * 0.5, H * 0.44, H * 0.08, ECColor(Ink, (float)A));
				break;
			}
			case EECFx::Paradox:
			{
				const double A = FMath::Clamp(P * 5.0, 0.0, 1.0);
				const double J = (FMath::Fmod(G.RealTime * 17.0, 1.0) - 0.5) * H * 0.012;
				Text(TEXT("PARADOX"), W * 0.5 - J, H * 0.42, H * 0.1, ECColor(Accent, 0.6f * (float)A), 0.5, (int32)(H * 0.015), true, false);
				Text(TEXT("PARADOX"), W * 0.5 + J, H * 0.42, H * 0.1, ECColor(Paradox, 0.7f * (float)A), 0.5, (int32)(H * 0.015), true, false);
				Text(TEXT("PARADOX"), W * 0.5, H * 0.42, H * 0.1, ECColor(Ink, (float)A), 0.5, (int32)(H * 0.015), true);
				Text(TEXT("AN ECHO COULD NOT REPEAT ITS PAST"), W * 0.5, H * 0.52, H * 0.024, ECColor(Dim, (float)A), 0.5, 2, true);
				break;
			}
			case EECFx::OutOfLoops:
			{
				const double A = P;
				Text(TEXT("OUT OF LOOPS"), W * 0.5, H * 0.36, H * 0.08, ECColor(Ink, (float)A), 0.5, (int32)(H * 0.012), true);
				Text(TEXT("EVERY ECHO IS SPENT"), W * 0.5, H * 0.44, H * 0.024, ECColor(Dim, (float)A), 0.5, 2, true);
				if (!G.bAttract && !G.bAutopilotInPlay)
				{
					const double BW = H * 0.4, BH = H * 0.08;
					Button(TEXT("RETRY LAST LOOP"), W * 0.5, H * 0.56, BW, BH, EECAction::RetryLoop, 0, true, true);
					Button(TEXT("RESTART LEVEL"), W * 0.5, H * 0.56 + BH * 1.3, BW, BH, EECAction::Restart);
					Button(TEXT("SHOW ME A HINT"), W * 0.5, H * 0.56 + BH * 2.6, BW, BH, EECAction::Hint);
				}
				break;
			}
			case EECFx::Solve:
			{
				const double A = FMath::Clamp(P * 3.0, 0.0, 1.0) * FMath::Clamp((1.0 - P) * 4.0, 0.0, 1.0);
				Text(TEXT("SOLVED"), W * 0.5, H * 0.4, H * 0.1, ECColor(Warm, (float)A), 0.5, (int32)(H * 0.02), true);
				break;
			}
			default:
				break;
			}
		}

		void TouchPads()
		{
			const FECTouchLayout L = FECTouchLayout::Compute(W, H, Ctx.Safe);
			auto Pad = [&](const FVector2D& C, double R, bool bDown)
			{
				D.Circle(C.X, C.Y, R + 2, ECColor(0xFFFFFF, bDown ? 0.5f : 0.22f), 32);
				D.Circle(C.X, C.Y, R, bDown ? ECColor(Accent, 0.35f) : ECColor(Glass, 0.35f), 32);
			};
			Pad(L.Left, L.Radius, Ctx.bLeftDown);
			IconArrow(L.Left.X, L.Left.Y, L.Radius * 0.8, false, ECColor(Ink, 0.85f));
			Pad(L.Right, L.Radius, Ctx.bRightDown);
			IconArrow(L.Right.X, L.Right.Y, L.Radius * 0.8, true, ECColor(Ink, 0.85f));
			Pad(L.Jump, L.JumpRadius, Ctx.bJumpDown);
			IconJump(L.Jump.X, L.Jump.Y, L.JumpRadius * 0.7, ECColor(Ink, 0.85f));
		}
	};
}

FECTouchLayout FECTouchLayout::Compute(double W, double H, const FVector4& Safe)
{
	FECTouchLayout L;
	L.W = W;
	L.H = H;
	L.Radius = H * 0.085;
	L.JumpRadius = H * 0.105;
	const double Bottom = H - Safe.W - H * 0.04;
	L.Left = FVector2D(Safe.X + H * 0.05 + L.Radius, Bottom - L.Radius);
	L.Right = FVector2D(L.Left.X + L.Radius * 2.5, Bottom - L.Radius);
	L.Jump = FVector2D(W - Safe.Z - H * 0.06 - L.JumpRadius, Bottom - L.JumpRadius);
	return L;
}

// Generous zones: the whole lower-left belongs to the arrows, the lower-right to jump.
bool FECTouchLayout::HitLeft(const FVector2D& P) const
{
	return P.Y > H * 0.45 && P.X < W * 0.42 && P.X < (Left.X + Right.X) * 0.5;
}

bool FECTouchLayout::HitRight(const FVector2D& P) const
{
	return P.Y > H * 0.45 && P.X < W * 0.42 && P.X >= (Left.X + Right.X) * 0.5;
}

bool FECTouchLayout::HitJump(const FVector2D& P) const
{
	return P.Y > H * 0.45 && P.X > W * 0.6;
}

void FECUI::Draw(FECDraw& D, UCanvas* Canvas, FECGame& Game, const FECUiContext& Ctx)
{
	D.Flush();
	D.SetPixels();
	D.Translucent();
	Game.Buttons.Reset();
	FUi U{ D, Canvas, Game, Ctx, D.ScreenW, D.ScreenH };

	switch (Game.Screen)
	{
	case EECScreen::Title: U.Title(); break;
	case EECScreen::LevelSelect: U.LevelSelect(); break;
	case EECScreen::Playing:
		if (!Game.bAttract)
		{
			U.Hud();
			if (Ctx.bShowTouch && Game.Fx != EECFx::OutOfLoops) { U.TouchPads(); }
		}
		break;
	case EECScreen::Paused: U.Paused(); break;
	case EECScreen::Complete: U.Complete(); break;
	}
	D.Flush();
}
