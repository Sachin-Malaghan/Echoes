// ECHOES core: plan-driven autopilot. CLAUDE.md: Autopilot.
#include "ECAutopilot.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace EC
{
	bool FPlan::Parse(const char* Text, char* Error, int ErrorLen)
	{
		int NumSteps = 0;
		NumRuns = 1;
		RunStart[0] = 0;
		const char* C = Text;
		while (*C)
		{
			if (*C == ' ') { ++C; continue; }
			if (*C == '|')
			{
				if (NumRuns == MaxRuns) { snprintf(Error, ErrorLen, "plan: too many runs"); return false; }
				RunStart[NumRuns++] = NumSteps;
				++C;
				continue;
			}
			if (NumSteps == MaxPlanSteps) { snprintf(Error, ErrorLen, "plan: too many steps"); return false; }
			FPlanStep& S = Steps[NumSteps++];
			S = { EOp::Wait, 0.f, 0 };
			const char Op = *C++;
			char* EndP = nullptr;
			switch (Op)
			{
			case 'm': S.Op = EOp::Move;      S.X = strtof(C, &EndP); break;
			case 'n': S.Op = EOp::MovePlain; S.X = strtof(C, &EndP); break;
			case 'w': S.Op = EOp::Wait;      S.N = int(strtol(C, &EndP, 10)); break;
			case 'u': S.Op = EOp::WaitUntil; S.N = int(strtol(C, &EndP, 10)); break;
			case 'j': S.Op = EOp::Jump; break;
			case 'r': S.Op = EOp::Rewind; break;
			default: snprintf(Error, ErrorLen, "plan: unknown op '%c'", Op); return false;
			}
			if (EndP) { if (EndP == C) { snprintf(Error, ErrorLen, "plan: '%c' needs a number", Op); return false; } C = EndP; }
		}
		RunStart[NumRuns] = NumSteps;
		return true;
	}

	bool FAutopilot::Load(const char* PlanText, char* Error, int ErrorLen)
	{
		if (!Plan.Parse(PlanText, Error, ErrorLen)) return false;
		BeginLoop(0);
		return true;
	}

	void FAutopilot::BeginLoop(int RunIndex)
	{
		const int R = RunIndex < Plan.NumRuns ? RunIndex : Plan.NumRuns - 1;
		Step = Plan.RunStart[R];
		End = Plan.RunStart[R + 1];
		StepTicks = 0;
		bJumpStarted = bLeftGround = bHoldJump = false;
	}

	static bool SupportAhead(const FSim& S, float X, float Y)
	{
		if (S.SolidAt({ X - 0.05f, Y - 0.3f, X + 0.05f, Y - 0.01f })) return true;
		for (int i = 0; i < S.NumEchoes; ++i)
		{
			const FEchoState& E = S.EchoState[i];
			if (std::fabs(E.X - X) < BodyW * 0.5f && std::fabs(E.Y + BodyH - Y) < 0.3f) return true;
		}
		return false;
	}

	FInput FAutopilot::Next(const FSim& S)
	{
		FInput In;
		const FBody& P = S.Player;

		// Keep holding a jump for full height.
		if (bHoldJump)
		{
			if (!P.Grounded) bLeftGround = true;
			if (bLeftGround && P.VY <= 0.f) bHoldJump = false;
			else In.Jump = true;
		}

		while (Step < End)
		{
			const FPlanStep& St = Plan.Steps[Step];
			switch (St.Op)
			{
			case EOp::Move:
			case EOp::MovePlain:
			{
				const float Dx = St.X - P.X;
				if (std::fabs(Dx) < 0.12f && std::fabs(P.VX) < 0.3f && P.Grounded && !bHoldJump) { Advance(); continue; }
				const int Dir = Dx > 0 ? 1 : -1;
				const float Stop = P.VX * P.VX / (2.f * GroundDecel);
				if (std::fabs(Dx) > Stop + 0.04f) { In.Right = Dir > 0; In.Left = Dir < 0; }
				if (St.Op == EOp::Move && P.Grounded && !bHoldJump && !P.PrevJump)
				{
					const float Front = P.X + Dir * BodyW * 0.5f;
					const FBox Ahead = Dir > 0 ? FBox{ Front, P.Y + 0.05f, Front + 0.25f, P.Y + BodyH - 0.05f }
					                           : FBox{ Front - 0.25f, P.Y + 0.05f, Front, P.Y + BodyH - 0.05f };
					const bool bWall = std::fabs(Dx) > 0.3f && S.SolidAt(Ahead);
					const bool bGap = std::fabs(Dx) > 1.0f && !SupportAhead(S, P.X + Dir * (BodyW * 0.5f + 0.35f), P.Y);
					if (bWall || bGap) { In.Jump = true; bHoldJump = true; bLeftGround = false; }
				}
				return In;
			}
			case EOp::Jump:
				if (!bJumpStarted)
				{
					if (P.PrevJump || !P.Grounded) return In;   // release first / wait to be standing
					In.Jump = true; bHoldJump = true; bJumpStarted = true; bLeftGround = false;
					return In;
				}
				if (!P.Grounded) bLeftGround = true;
				if (bLeftGround && P.Grounded) { Advance(); continue; }
				return In;
			case EOp::Wait:
				if (StepTicks++ >= St.N) { Advance(); continue; }
				return In;
			case EOp::WaitUntil:
				if (S.Tick >= St.N) { Advance(); continue; }
				return In;
			case EOp::Rewind:
				if (S.Tick < MinRewindTick) return In;
				Advance();
				In.Rewind = true;
				return In;
			}
		}
		return In;
	}
}
