// ECHOES core: the deterministic loop simulation (player, Echoes, plates, doors, paradox). CLAUDE.md: Core / Echoes.
#pragma once
#include "ECLevel.h"

namespace EC
{
	// A recorded run: state snapshots, not inputs, so playback is exact by construction.
	struct FRecording
	{
		FFrame Frames[LoopTicks];
		int Length = 0;
		const FFrame& At(int Tick) const { return Frames[Tick < Length ? Tick : Length - 1]; }
	};

	struct FBody
	{
		float X = 0, Y = 0, VX = 0, VY = 0;
		bool Grounded = false;
		int GroundEcho = -1;       // index of the Echo we stand on, -1 if none
		uint8_t Facing = 1;
		EAnim Anim = EAnim::Idle;
		int Coyote = 0, JumpBuffer = 0, LandTicks = 0;
		bool PrevJump = false, bJumpCut = false;
	};

	struct FEchoState
	{
		float X = 0, Y = 0, PrevX = 0, PrevY = 0;
		int BlockedTicks = 0, UnsupportedTicks = 0;
		bool bFinished = false;    // its run ended early: it holds its last pose for the rest of the loop
	};

	// The whole game state of one level attempt. Plain data, copyable by value, no allocation after Init.
	class FSim
	{
	public:
		bool Init(const FLevelDef& Def, char* Error, int ErrorLen);

		// Advances one fixed tick (1/50 s). Returns an EEvent mask. Does nothing unless Phase == Playing.
		uint32_t Step(const FInput& In);

		void CommitLoop();     // after LoopEnded: the run becomes an Echo and the level resets
		void RetryLoop();      // after Died / OutOfLoops: replay this loop, Echoes kept
		void RestartLevel();   // everything from scratch

		int LoopsUsed() const { return NumEchoes + 1; }
		int LoopsAllowed() const { return Level.Def->MaxEchoes + 1; }
		float TimeLeft() const { return (LoopTicks - Tick) * Dt; }
		bool ShardTaken() const { return bShardCommitted || bShardThisLoop; }
		bool DoorSolid(int i, FBox& Out) const;   // the solid part of door i, false when fully open
		bool SolidAt(const FBox& B) const;        // any wall tile or closed door part overlapping B
		bool LaserOnAt(int T) const;               // the shared laser schedule at loop tick T
		bool LaserOn() const { return LaserOnAt(Tick); }
		bool LaserWarning() const;                 // off, but about to turn on
		FBox BeamBox(int i) const;                 // the full beam of emitter i as it stands now

		FLevel Level;
		EPhase Phase = EPhase::Playing;
		int Tick = 0;                         // 0..LoopTicks within the current loop
		int NumEchoes = 0;
		int ParadoxEcho = -1;
		FBody Player;
		FRecording Current;
		FRecording Echoes[MaxEchoes];
		FEchoState EchoState[MaxEchoes];
		float DoorOpen[MaxDoors] = {};
		bool PlatePressed[MaxPlates] = {};
		bool bShardCommitted = false, bShardThisLoop = false;
		float BeamEnd[MaxEmitters] = {};      // where each beam stops (x for horizontal, y for vertical)

	private:
		void ResetWorld();
		void StepPlayer(const FInput& In, uint32_t& Events);
		void MoveX(float Dx);
		void MoveY(float Dy, uint32_t& Events);
		bool HasSupport(int EchoIndex) const;
		void EndLoop(uint32_t& Events);
		void ComputeBeams();
	};
}
