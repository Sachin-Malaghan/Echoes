// Standalone test harness for the ECHOES core (no Unreal). Build and run: Tools/SimHarness/run.ps1
//   harness [-level N] [-trace]
#include "../../Source/Echoes/Private/Core/ECAutopilot.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// ---- Allocation counter: proves stepping never allocates -------------------------------------
static long long GAllocs = 0;
void* operator new(size_t N) { ++GAllocs; if (void* P = std::malloc(N ? N : 1)) return P; throw std::bad_alloc(); }
void operator delete(void* P) noexcept { std::free(P); }
void operator delete(void* P, size_t) noexcept { std::free(P); }

using namespace EC;

static int GFailures = 0;
static bool GTrace = false;
#define CHECK(Cond, ...) do { if (!(Cond)) { ++GFailures; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// The player's path in each loop, to compare against the Echo it becomes.
static FFrame GLived[MaxRuns][LoopTicks];
static int GLivedLen[MaxRuns];

struct FRunResult
{
	EPhase Final = EPhase::Playing;
	int Loops = 0;
	bool Shard = false;
	float MaxEchoError = 0.f;
	long long Allocs = 0;
	unsigned long long Hash = 1469598103934665603ull;
};

static void HashBytes(unsigned long long& H, const void* P, size_t N)
{
	const unsigned char* B = static_cast<const unsigned char*>(P);
	for (size_t i = 0; i < N; ++i) { H ^= B[i]; H *= 1099511628211ull; }
}

static const char* PhaseName(EPhase P)
{
	switch (P)
	{
	case EPhase::Playing: return "playing"; case EPhase::LoopEnded: return "loop ended"; case EPhase::Died: return "died";
	case EPhase::Paradox: return "PARADOX"; case EPhase::OutOfLoops: return "out of loops"; case EPhase::Solved: return "solved";
	}
	return "?";
}

// Plays a plan to the end. The sim lives in static storage (it is ~80 KB).
static FSim GSim;
static FRunResult RunPlan(const FLevelDef& Def, const char* PlanText, int MaxLoops = 99)
{
	FRunResult R;
	char Err[256];
	if (!GSim.Init(Def, Err, sizeof Err)) { CHECK(false, "%s", Err); return R; }
	static FAutopilot AP;
	if (!AP.Load(PlanText, Err, sizeof Err)) { CHECK(false, "%s: %s", Def.Name, Err); return R; }

	const long long Allocs0 = GAllocs;
	for (int Loop = 0; Loop < MaxLoops; ++Loop)
	{
		AP.BeginLoop(GSim.NumEchoes);
		const int Run = GSim.NumEchoes;
		while (GSim.Phase == EPhase::Playing)
		{
			const FInput In = AP.Next(GSim);
			const int T = GSim.Tick;
			const uint32_t Ev = GSim.Step(In);
			if (GSim.Phase == EPhase::Playing || GSim.Phase == EPhase::Solved)
				GLived[Run][T] = GSim.Current.Frames[T];
			// Every Echo sits exactly on the pose it lived at this tick (a Rewind tick moves nothing).
			const bool bStepped = !(Ev & EV_LoopEnd) || GSim.Tick == LoopTicks;
			for (int i = 0; i < GSim.NumEchoes && bStepped; ++i)
			{
				const int F = T < GLivedLen[i] ? T : GLivedLen[i] - 1;
				const float E = std::fmax(std::fabs(GSim.EchoState[i].X - GLived[i][F].X), std::fabs(GSim.EchoState[i].Y - GLived[i][F].Y));
				if (E > 0.f && GTrace) printf("    echo %d off by %f at loop %d tick %d (len %d): echo %f,%f lived %f,%f rec %f\n", i + 1, E, Run + 1, T, GLivedLen[i],
					GSim.EchoState[i].X, GSim.EchoState[i].Y, GLived[i][F].X, GLived[i][F].Y, GSim.Echoes[i].Frames[F].X);
				R.MaxEchoError = std::fmax(R.MaxEchoError, E);
			}
			if ((Ev & EV_Died) && GTrace)
				printf("    DIED loop %d tick %d at x=%.2f y=%.2f laser=%d\n", Run + 1, T, GSim.Player.X, GSim.Player.Y, (int)GSim.LaserOnAt(T));
			HashBytes(R.Hash, &GSim.Player, sizeof(GSim.Player));
			HashBytes(R.Hash, GSim.DoorOpen, sizeof(GSim.DoorOpen));
			if (GTrace && (T % 25 == 0 || Ev))
				printf("    loop %d t=%3d  x=%6.2f y=%5.2f vx=%5.2f vy=%6.2f %s%s  doors=%.2f  ev=%x\n", Run + 1, T, GSim.Player.X, GSim.Player.Y,
					GSim.Player.VX, GSim.Player.VY, GSim.Player.Grounded ? "G" : " ", GSim.Player.GroundEcho >= 0 ? "e" : " ", GSim.DoorOpen[0], Ev);
		}
		GLivedLen[Run] = GSim.Current.Length;
		if (GSim.Phase == EPhase::LoopEnded) { GSim.CommitLoop(); continue; }
		break;
	}
	R.Allocs = GAllocs - Allocs0;
	R.Final = GSim.Phase;
	R.Loops = GSim.LoopsUsed();
	R.Shard = GSim.ShardTaken();
	return R;
}

static void TestLevel(int Index)
{
	const FLevelDef& D = GetLevelDef(Index);
	printf("Level %d \"%s\" (cap %d echoes, par %d loops)\n", Index + 1, D.Name, D.MaxEchoes, D.ParLoops);

	const FRunResult Par = RunPlan(D, D.Plan);
	printf("  par plan:   %-12s loops=%d  echo error=%.6f  allocs=%lld\n", PhaseName(Par.Final), Par.Loops, Par.MaxEchoError, Par.Allocs);
	CHECK(Par.Final == EPhase::Solved, "par plan did not solve (%s)", PhaseName(Par.Final));
	CHECK(Par.Loops == D.ParLoops, "par plan used %d loops, par is %d", Par.Loops, D.ParLoops);
	CHECK(Par.MaxEchoError == 0.f, "echo drifted by %f tiles", Par.MaxEchoError);
	CHECK(Par.Allocs == 0, "%lld allocations while stepping", Par.Allocs);

	const FRunResult Again = RunPlan(D, D.Plan);
	CHECK(Again.Hash == Par.Hash, "not deterministic");

	const FRunResult Shard = RunPlan(D, D.ShardPlan);
	printf("  shard plan: %-12s loops=%d  shard=%s\n", PhaseName(Shard.Final), Shard.Loops, Shard.Shard ? "yes" : "no");
	CHECK(Shard.Final == EPhase::Solved && Shard.Shard, "shard plan failed (%s, shard %d)", PhaseName(Shard.Final), Shard.Shard);

	// The final run of the par plan alone, with no Echoes, must not solve: the level really needs its Echoes.
	FPlan P; char Err[128];
	P.Parse(D.Plan, Err, sizeof Err);
	const char* Last = D.Plan;
	for (const char* C = D.Plan; *C; ++C) if (*C == '|') Last = C + 1;
	const FRunResult Alone = RunPlan(D, Last, 1);
	CHECK(Alone.Final != EPhase::Solved, "solvable without Echoes");
}

// Shortcuts a clever player might try: none of them may solve their level.
static void TestCheats()
{
	printf("Cheat routes (must all fail)\n");
	static const struct { int Level; const char* Plan; } Cheats[] =
	{
		{ 1, "m2.5 m14.5" },                  // hold the plate, then sprint for the door
		{ 1, "m2.5 w5 m14.5" },
		{ 2, "m12.5" },                       // jump the wall unaided
		{ 3, "m1.5 r | m6.5 m14.5" },         // one Echo, hold the second plate yourself and run
		{ 3, "m6.5 r | m1.5 m14.5" },
		{ 4, "m10.5 r | m10.5 j m13.5" },                        // one Echo is not tall enough
		{ 5, "m1.5 r | m9.5 m14.5" },                            // hold the second key yourself
		{ 5, "m9.5 r | m14.5" },
		{ 6, "m2.5 r | m14.5" },                                 // an Echo that never hands off
		{ 6, "m4.5 r | m14.5" },
		{ 7, "m14.5" },
		{ 8, "m2.5 r | m7.5 j m14.5" },                          // key without a step
		{ 8, "m7.5 r | m7.5 j m14.5" },                          // step without a key
		{ 9, "m6.5 r | m14.5" },
		{ 9, "m6.5 r | m6.5 j m2.5 m14.5" },
		{ 10, "m1.5 r | m9.5 r | m9.5 j m14.5" },                // a one-Echo climb
		{ 11, "u58 m14.5" },                                     // sprint through one dark window
		{ 11, "u60 m14.5" },
		{ 12, "m14.5" },
		{ 12, "u50 m4.5 u100 m14.5" },
		{ 13, "u17 n1.4 r | u100 m2.2 m7.5" },                   // only one side covered
		{ 13, "u10 n14.4 r | u100 m2.2 m7.5" },
		{ 13, "u70 m2.2 m7.5" },
		{ 14, "u10 m10.5 r | m9.5 u170 m10.5 j m12.5" },
		{ 15, "m3.5 r | m3.5 j u60 m14.5" },                     // run the walkway in one window
		{ 15, "m3.5 r | m3.5 j u160 m14.5" },
		{ 16, "u45 m5.5 r | u40 m2.5 r | u70 m14.5" },           // two keys of three
		{ 17, "u89 m1.5" },
		{ 17, "u60 m1.5" },
		{ 18, "u140 m4.5 r | m10.5 j m13.5" },
		{ 19, "u45 m4.5 r | u70 m8.5 u150 m9.5 u160 m14.5" },
		{ 20, "m3.5 u40 m4.5 r | u48 m9.5 r | u110 m9.5 j m14.5" },
	};
	for (const auto& C : Cheats)
	{
		const FRunResult R = RunPlan(GetLevelDef(C.Level - 1), C.Plan);
		printf("  level %d  %-22s -> %s\n", C.Level, C.Plan, PhaseName(R.Final));
		CHECK(R.Final != EPhase::Solved, "cheat solved level %d", C.Level);
	}
}

// Hand-made Echo tracks that cannot happen any more must trigger a paradox.
static void TestParadox()
{
	printf("Paradox detection\n");
	char Err[128];
	// 1. An Echo that walked through door A while nobody holds its plate.
	GSim.Init(GetLevelDef(0), Err, sizeof Err);
	FRecording& R = GSim.Echoes[0];
	R.Length = 100;
	for (int t = 0; t < R.Length; ++t)
		R.Frames[t] = { 6.5f + t * 0.1f, 1.f, 5.f, 0.f, 1, uint8_t(EAnim::Run), FF_Grounded, 0 };
	GSim.NumEchoes = 1;
	GSim.RetryLoop();
	int T = 0;
	while (GSim.Phase == EPhase::Playing && T < 200) { GSim.Step(FInput()); ++T; }
	printf("  walk through closed door: %s at tick %d\n", PhaseName(GSim.Phase), T);
	CHECK(GSim.Phase == EPhase::Paradox && GSim.ParadoxEcho == 0, "no paradox for an Echo inside a closed door");

	// 2. An Echo recorded standing in mid-air.
	GSim.Init(GetLevelDef(0), Err, sizeof Err);
	for (int t = 0; t < 50; ++t) R.Frames[t] = { 4.5f, 3.f, 0.f, 0.f, 1, uint8_t(EAnim::Idle), FF_Grounded, 0 };
	R.Length = 50;
	GSim.NumEchoes = 1;
	GSim.RetryLoop();
	T = 0;
	while (GSim.Phase == EPhase::Playing && T < 50) { GSim.Step(FInput()); ++T; }
	printf("  standing on nothing:      %s at tick %d\n", PhaseName(GSim.Phase), T);
	CHECK(GSim.Phase == EPhase::Paradox, "no paradox for an unsupported Echo");

	// 3. The par plans must never raise one (checked in TestLevel); an idle run must not either.
	GSim.Init(GetLevelDef(0), Err, sizeof Err);
	for (int L = 0; L < 3; ++L)
	{
		while (GSim.Phase == EPhase::Playing) GSim.Step(FInput());
		if (GSim.Phase == EPhase::LoopEnded) GSim.CommitLoop();
	}
	printf("  three idle loops:         %s (loops used %d)\n", PhaseName(GSim.Phase), GSim.LoopsUsed());
	CHECK(GSim.Phase == EPhase::OutOfLoops, "loop cap not enforced");
}

// You can stand on an Echo's head, and an Echo on a plate keeps its door open for you.
static void TestMechanics()
{
	printf("Mechanics\n");
	char Err[128];
	static FAutopilot AP;
	GSim.Init(GetLevelDef(1), Err, sizeof Err);
	AP.Load("m9.5 r | m9.5 j w20", Err, sizeof Err);
	for (int Loop = 0; Loop < 2; ++Loop)
	{
		AP.BeginLoop(GSim.NumEchoes);
		for (int t = 0; t < 150 && GSim.Phase == EPhase::Playing; ++t) GSim.Step(AP.Next(GSim));
		if (GSim.Phase == EPhase::LoopEnded) GSim.CommitLoop();
	}
	printf("  on Echo head: y=%.3f groundEcho=%d\n", GSim.Player.Y, GSim.Player.GroundEcho);
	CHECK(GSim.Player.GroundEcho == 0 && std::fabs(GSim.Player.Y - (1.f + BodyH)) < 1e-4f, "not standing on the Echo");

	GSim.Init(GetLevelDef(0), Err, sizeof Err);
	AP.Load("m2.5 r | m7.5", Err, sizeof Err);
	float MinOpen = 1.f;
	for (int Loop = 0; Loop < 2; ++Loop)
	{
		AP.BeginLoop(GSim.NumEchoes);
		while (GSim.Phase == EPhase::Playing)
		{
			GSim.Step(AP.Next(GSim));
			if (Loop == 1 && GSim.Tick > 80) MinOpen = std::fmin(MinOpen, GSim.DoorOpen[0]);
		}
		if (GSim.Phase == EPhase::LoopEnded) GSim.CommitLoop();
	}
	printf("  door held by Echo from 1.6 s to 10 s: min open %.2f\n", MinOpen);
	CHECK(MinOpen == 1.f, "door not held open by the Echo");
}

int main(int Argc, char** Argv)
{
	int Only = 0;
	for (int i = 1; i < Argc; ++i)
	{
		if (!strcmp(Argv[i], "-level") && i + 1 < Argc) Only = atoi(Argv[++i]);
		else if (!strcmp(Argv[i], "-trace")) GTrace = true;
	}
	for (int i = 0; i < NumLevels(); ++i)
		if (!Only || Only == i + 1) TestLevel(i);
	if (!Only) { TestCheats(); TestParadox(); TestMechanics(); }
	printf(GFailures ? "\n%d FAILURE(S)\n" : "\nALL PASSED\n", GFailures);
	return GFailures ? 1 : 0;
}
