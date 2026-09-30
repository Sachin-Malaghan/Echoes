// ECHOES core: plays a level from a written plan (one script per loop). Proves levels are solvable,
// drives the attract mode and screenshot captures. CLAUDE.md: Autopilot.
#pragma once
#include "ECSim.h"

namespace EC
{
	enum class EOp : uint8_t { Move, MovePlain, Jump, Wait, WaitUntil, Rewind };

	struct FPlanStep { EOp Op; float X; int N; };

	struct FPlan
	{
		FPlanStep Steps[MaxPlanSteps];
		int RunStart[MaxRuns + 1] = {};   // steps of run r are [RunStart[r], RunStart[r+1])
		int NumRuns = 0;
		bool Parse(const char* Text, char* Error, int ErrorLen);
	};

	class FAutopilot
	{
	public:
		bool Load(const char* PlanText, char* Error, int ErrorLen);
		void BeginLoop(int RunIndex);            // call at the start of every loop (RunIndex = Sim.NumEchoes)
		FInput Next(const FSim& S);              // input for the coming tick
		int NumRuns() const { return Plan.NumRuns; }

	private:
		FPlan Plan;
		int Step = 0, End = 0, StepTicks = 0;
		bool bJumpStarted = false, bLeftGround = false, bHoldJump = false;
		void Advance() { ++Step; StepTicks = 0; bJumpStarted = bLeftGround = false; }
	};
}
