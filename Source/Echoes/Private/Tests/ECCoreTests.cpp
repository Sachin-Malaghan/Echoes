// ECHOES automation tests: the same core the harness checks, run inside Unreal. (CLAUDE.md: Validation)
#include "Core/ECAutopilot.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FPlayResult
	{
		EC::EPhase Phase = EC::EPhase::Playing;
		int32 Loops = 0;
		bool bShard = false;
		float MaxEchoError = 0.f;
	};

	// The sim is ~80 KB: keep it off the stack.
	FPlayResult Play(const EC::FLevelDef& Def, const char* Plan)
	{
		static EC::FSim Sim;
		static EC::FAutopilot Pilot;
		static EC::FFrame Lived[EC::MaxRuns][EC::LoopTicks];
		static int32 LivedLen[EC::MaxRuns];
		FPlayResult R;
		char Err[256];
		if (!Sim.Init(Def, Err, sizeof Err) || !Pilot.Load(Plan, Err, sizeof Err)) { return R; }
		for (int32 Loop = 0; Loop < 12; ++Loop)
		{
			Pilot.BeginLoop(Sim.NumEchoes);
			const int32 Run = Sim.NumEchoes;
			while (Sim.Phase == EC::EPhase::Playing)
			{
				const int32 T = Sim.Tick;
				const uint32 Ev = Sim.Step(Pilot.Next(Sim));
				if (Sim.Tick > T) { Lived[Run][T] = Sim.Current.Frames[T]; }
				const bool bStepped = !(Ev & EC::EV_LoopEnd) || Sim.Tick == EC::LoopTicks;
				for (int32 I = 0; I < Sim.NumEchoes && bStepped; ++I)
				{
					const EC::FFrame& F = Lived[I][FMath::Min(T, LivedLen[I] - 1)];
					R.MaxEchoError = FMath::Max(R.MaxEchoError, FMath::Max(FMath::Abs(Sim.EchoState[I].X - F.X), FMath::Abs(Sim.EchoState[I].Y - F.Y)));
				}
			}
			LivedLen[Run] = Sim.Current.Length;
			if (Sim.Phase != EC::EPhase::LoopEnded) { break; }
			Sim.CommitLoop();
		}
		R.Phase = Sim.Phase;
		R.Loops = Sim.LoopsUsed();
		R.bShard = Sim.ShardTaken();
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FECLevelsSolvableTest, "Echoes.Levels.SolvableAtPar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FECLevelsSolvableTest::RunTest(const FString&)
{
	for (int32 I = 0; I < EC::NumLevels(); ++I)
	{
		const EC::FLevelDef& Def = EC::GetLevelDef(I);
		const FPlayResult Par = Play(Def, Def.Plan);
		TestEqual(FString::Printf(TEXT("%hs: par plan solves"), Def.Name), (int32)Par.Phase, (int32)EC::EPhase::Solved);
		TestEqual(FString::Printf(TEXT("%hs: loops used == par"), Def.Name), Par.Loops, Def.ParLoops);
		TestEqual(FString::Printf(TEXT("%hs: Echoes replay exactly"), Def.Name), Par.MaxEchoError, 0.f);
		const FPlayResult Shard = Play(Def, Def.ShardPlan);
		TestTrue(FString::Printf(TEXT("%hs: shard plan solves with the shard"), Def.Name), Shard.Phase == EC::EPhase::Solved && Shard.bShard);
		TestTrue(FString::Printf(TEXT("%hs: shard plan within the loop cap"), Def.Name), Shard.Loops <= Def.MaxEchoes + 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FECParadoxTest, "Echoes.Echoes.Paradox",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FECParadoxTest::RunTest(const FString&)
{
	static EC::FSim Sim;
	char Err[128];
	Sim.Init(EC::GetLevelDef(0), Err, sizeof Err);
	EC::FRecording& R = Sim.Echoes[0];
	R.Length = 100;
	for (int32 T = 0; T < R.Length; ++T) { R.Frames[T] = { 6.5f + T * 0.1f, 1.f, 5.f, 0.f, 1, 1, EC::FF_Grounded, 0 }; }
	Sim.NumEchoes = 1;
	Sim.RetryLoop();
	for (int32 T = 0; T < 200 && Sim.Phase == EC::EPhase::Playing; ++T) { Sim.Step(EC::FInput()); }
	TestEqual(TEXT("an Echo walking into a closed door is a paradox"), (int32)Sim.Phase, (int32)EC::EPhase::Paradox);
	return true;
}

#endif
