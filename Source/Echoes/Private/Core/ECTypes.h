// ECHOES core: engine-agnostic constants and small types (no Unreal headers). CLAUDE.md: Architecture / Core.
#pragma once
#include <cstdint>

namespace EC
{
	// ---- Time ------------------------------------------------------------------------------------
	constexpr int   TickHz    = 50;                 // fixed simulation rate
	constexpr float Dt        = 1.0f / TickHz;
	constexpr int   LoopSeconds = 10;
	constexpr int   LoopTicks = LoopSeconds * TickHz; // 500 ticks per loop
	constexpr int   MinRewindTick = 5;              // "Rewind now" ignored in the first 0.1 s

	// ---- Capacities (everything is preallocated: no allocation while playing) -------------------
	constexpr int MaxEchoes = 6;
	constexpr int MaxW = 32, MaxH = 18;
	constexpr int MaxDoors = 8, MaxPlates = 8, MaxEmitters = 8;
	constexpr int MaxPlanSteps = 64, MaxRuns = MaxEchoes + 1;

	// ---- Player / Echo body (units are tiles; y is up; position = centre of the feet) -----------
	constexpr float BodyW = 0.6f, BodyH = 1.6f;
	constexpr float RunSpeed    = 6.5f;
	constexpr float GroundAccel = 70.f, GroundDecel = 80.f;
	constexpr float AirAccel    = 45.f, AirDecel = 20.f;
	constexpr float Gravity     = 42.f;
	constexpr float JumpSpeed   = 14.2f;   // ~2.26 tiles of height at 50 Hz (discrete peak)
	constexpr float JumpCut     = 0.45f;   // vy multiplier when jump is released while rising
	constexpr float MaxFall     = 22.f;
	constexpr int   CoyoteTicks = 5, JumpBufferTicks = 6;
	constexpr float EchoStepUp  = 0.35f;   // an Echo's head catches you if your feet are this close under it

	// ---- Doors -----------------------------------------------------------------------------------
	constexpr float DoorOpenRate  = 5.f;   // fraction per second (0.2 s to open)
	constexpr float DoorCloseRate = 3.f;   // 0.33 s to close

	// ---- Lasers ----------------------------------------------------------------------------------
	constexpr float BeamHalf = 0.08f;      // beam half-thickness (tiles)
	constexpr int   LaserWarnTicks = 15;   // the beam flickers this long before it turns on

	// ---- Paradox rule (see CLAUDE.md "Paradox") ---------------------------------------------------
	constexpr float ParadoxDepth  = 0.12f; // penetration into a solid that counts as "blocked"
	constexpr int   ParadoxTicks  = 3;     // ...for this many consecutive ticks
	constexpr int   UnsupportedTicks = 5;  // recorded standing, but nothing to stand on

	struct FVec2 { float X = 0, Y = 0; };

	struct FBox
	{
		float L = 0, B = 0, R = 0, T = 0;
		bool Overlaps(const FBox& o) const { return L < o.R && o.L < R && B < o.T && o.B < T; }
	};

	inline FBox BodyBox(float x, float y) { return { x - BodyW * 0.5f, y, x + BodyW * 0.5f, y + BodyH }; }

	struct FInput
	{
		bool Left = false, Right = false;
		bool Jump = false;        // held
		bool Interact = false;    // edge (reserved for levers / boxes)
		bool Rewind = false;      // edge: end this loop now
	};

	enum class EAnim : uint8_t { Idle, Run, Jump, Fall, Land };

	// Bits in FFrame::Flags and in per-tick event masks.
	enum EFrameFlag : uint8_t { FF_Grounded = 1, FF_Jumped = 2, FF_Landed = 4, FF_Interact = 8 };

	// One recorded tick of a run. 20 bytes; a full loop is 10 KB.
	struct FFrame
	{
		float X, Y, VX, VY;
		uint8_t Facing;   // 0 = left, 1 = right
		uint8_t Anim;     // EAnim
		uint8_t Flags;    // EFrameFlag
		uint8_t Pad;
	};

	enum class ETile : uint8_t { Empty, Solid, Spike };

	enum class EPhase : uint8_t
	{
		Playing,
		LoopEnded,   // time ran out or Rewind: call CommitLoop()
		Died,        // call RetryLoop()
		Paradox,     // call RestartLevel()
		OutOfLoops,  // final loop ended without solving: RetryLoop() or RestartLevel()
		Solved,
	};

	// Per-tick events for audio / effects (bitmask).
	enum EEvent : uint32_t
	{
		EV_Jump = 1 << 0, EV_Land = 1 << 1, EV_PlateDown = 1 << 2, EV_PlateUp = 1 << 3,
		EV_DoorOpen = 1 << 4, EV_DoorClose = 1 << 5, EV_Shard = 1 << 6, EV_Died = 1 << 7,
		EV_LoopEnd = 1 << 8, EV_Paradox = 1 << 9, EV_Solved = 1 << 10, EV_EchoJump = 1 << 11,
		EV_EchoLand = 1 << 12,
	};
}
