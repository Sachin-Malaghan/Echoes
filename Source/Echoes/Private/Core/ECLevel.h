// ECHOES core: level definitions and the parsed level. CLAUDE.md: Architecture / Core, Levels.
#pragma once
#include "ECTypes.h"

namespace EC
{
	// Hand-authored level. Rows are top to bottom, one char per tile:
	//   #  wall        .  empty      ^  spikes       P  spawn (feet on this tile's floor)
	//   E  exit (bottom tile of a 1x2 portal)        *  hidden shard
	//   a-d  pressure plate on channel a-d            A-D  door on channel A-D (vertical runs merge)
	//   > < v  laser emitter (a wall tile) firing right / left / down; beams stop at walls, closed doors
	//          and Echoes (Echoes absorb them); they kill the player. All emitters share one schedule.
	// A door is open while every plate of its channel is pressed.
	//
	// Plans are the autopilot's scripts, one run per '|' (the last run must reach the exit):
	//   m<x>   move until the body centre is at x (auto-jumps walls)     j   jump, wait to land
	//   w<n>   wait n ticks          u<n>  wait until loop tick n         r   rewind now
	struct FLevelDef
	{
		const char* Name;
		int World;          // 1-based
		int MaxEchoes;      // loop cap = MaxEchoes + 1 runs
		int ParLoops;       // star 2: solve in this many loops or fewer
		const char* Rows[MaxH];
		const char* Plan;       // par solution: proves the level is solvable at par
		const char* ShardPlan;  // a solution that also collects the shard
		int LaserOn = 0, LaserOff = 0;   // ticks on / off, repeating from tick 0 (LaserOff 0 = always on)
	};

	int NumLevels();
	const FLevelDef& GetLevelDef(int Index);   // 0-based
	int NumWorlds();
	int FirstLevelOfWorld(int World);          // 1-based world -> 0-based level index
	int LevelNumberInWorld(int Index);         // 1-based position inside its world
	const char* WorldName(int World);

	struct FDoor  { float X0, X1, Y0, Y1; int Channel; };
	struct FPlate { float X0, X1, Y; int Channel; };
	struct FEmitter { int X, Y, DX, DY; };   // tile, firing direction

	// Level after parsing: immutable during play.
	struct FLevel
	{
		const FLevelDef* Def = nullptr;
		int W = 0, H = 0;
		ETile Tiles[MaxH][MaxW] = {};   // [y][x], y = 0 is the bottom row
		FDoor Doors[MaxDoors] = {};  int NumDoors = 0;
		FPlate Plates[MaxPlates] = {}; int NumPlates = 0;
		FEmitter Emitters[MaxEmitters] = {}; int NumEmitters = 0;
		FVec2 Spawn, Exit, Shard;
		bool HasShard = false;

		bool Parse(const FLevelDef& D, char* Error, int ErrorLen);
		ETile TileAt(int X, int Y) const;   // out of bounds: walls at the sides, empty above and below
	};
}
