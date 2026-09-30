// ECHOES core: the hand-authored levels and the level parser. CLAUDE.md: Levels.
#include "ECLevel.h"
#include <cstdio>
#include <cstring>

namespace EC
{
	// Each level fits one 16 x 9 screen. Walkable floor is y = 1. Jump height ~2.26 tiles, a body is 1.6
	// tiles tall, so a 3-tile wall needs one Echo as a step and a 4.2+ ledge needs two.
	static const FLevelDef GLevels[] =
	{
		{
			// A plate far from its door: nobody can hold it and get through. The first Echo holds it for you.
			"First Step", 1, 2, 2,
			{
				"################",
				"#.........#....#",
				"#.........#....#",
				"#....*....#....#",
				"#.........#....#",
				"#.........#....#",
				"#.........A....#",
				"#.a...P...A...E#",
				"################",
			},
			"m2.5 r | m14.5",
			"m2.5 r | m5.5 r | m5.5 j j m14.5",
		},
		{
			// A wall one tile higher than you can jump. Your Echo is the step.
			"Stepping Stone", 1, 2, 2,
			{
				"################",
				"#......*.......#",
				"#..............#",
				"#..............#",
				"#...........E..#",
				"#.........######",
				"#.........######",
				"#..P......######",
				"################",
			},
			"m9.5 r | m9.5 j m12.5",
			"m7.5 r | m7.5 j r | m7.5 j j j m12.5",
		},
		{
			// Two plates, one door: two Echoes hold the keys while you walk through.
			"Two Keys", 1, 3, 3,
			{
				"################",
				"#........#.....#",
				"#........#.....#",
				"#........#.*...#",
				"#........#.....#",
				"#........#.....#",
				"#........A.....#",
				"#a..P.a..A....E#",
				"################",
			},
			"m1.5 r | m6.5 r | m14.5",
			"m1.5 r | m6.5 r | m11.5 r | m11.5 j j m14.5",
		},
		{
			// Two Echoes stacked: a wall four tiles high.
			"Tower", 1, 3, 3,
			{
				"################",
				"#..............#",
				"#..............#",
				"#..*.........E.#",
				"#..........#####",
				"#..........#####",
				"#..........#####",
				"#..P.......#####",
				"################",
			},
			"m10.5 r | m10.5 j r | m10.5 j j m13.5",
			"w5 r | m10.5 r | m10.5 j r | j j m10.5 j j m13.5",
		},
		{
			// Two doors in a row: the first Echo lets the second through to the second key.
			"Relay", 1, 3, 3,
			{
				"################",
				"#.....#......#.#",
				"#.....#......#.#",
				"#.....#.*....#.#",
				"#.....#......#.#",
				"#.....#......#.#",
				"#.....A......B.#",
				"#a..P.A..b...BE#",
				"################",
			},
			"m1.5 r | m9.5 r | m14.5",
			"m1.5 r | m9.5 r | m8.5 r | m8.5 j j m14.5",
		},
		{
			// One Echo, two keys: hold the first while you pass, then walk over to the second.
			"Handoff", 1, 2, 2,
			{
				"################",
				"#.......#....#.#",
				"#.......#....#.#",
				"#.......#.*..#.#",
				"#.......#....#.#",
				"#.......#....#.#",
				"#.......A....B.#",
				"#.a.b.P.A....BE#",
				"################",
			},
			"m2.5 u150 m4.5 r | m14.5",
			"m2.5 u150 m4.5 r | m10.5 r | m10.5 j j m14.5",
		},
		{
			// Spikes too wide to jump; the walkway above is one Echo out of reach.
			"Mind the Gap", 1, 2, 2,
			{
				"################",
				"#..*...........#",
				"#..............#",
				"#..............#",
				"#..............#",
				"#....#######...#",
				"#..............#",
				"#.P.^^^^^^^^^.E#",
				"################",
			},
			"m3.5 r | m3.5 j m14.5",
			"m3.5 r | m3.5 j r | m3.5 j j j m14.5",
		},
		{
			// The door is up on the ledge: one Echo holds the key, one is the step.
			"Ledge Door", 1, 3, 3,
			{
				"################",
				"#.........#....#",
				"#.........#....#",
				"#..*......A....#",
				"#.........A...E#",
				"#.......########",
				"#.......########",
				"#.a..P..########",
				"################",
			},
			"m2.5 r | m7.5 r | m7.5 j m14.5",
			"m2.5 r | m7.5 r | m3.5 r | m3.5 j j m7.5 j m14.5",
		},
		{
			// The key downstairs is also the step up to the key upstairs.
			"Upstairs", 1, 3, 3,
			{
				"################",
				"#.......#......#",
				"#.......#......#",
				"#.......#..*...#",
				"#.a.....#......#",
				"####....#......#",
				"####....A......#",
				"####.Pa.A.....E#",
				"################",
			},
			"m6.5 r | m6.5 j m2.5 r | m14.5",
			"m6.5 r | m6.5 j m2.5 r | m11.5 r | m11.5 j j m14.5",
		},
		{
			// Everything World 1 taught: two doors, two keys, a two-Echo climb.
			"Clockwork", 1, 4, 4,
			{
				"################",
				"#......#.....#.#",
				"#..*...#.....B.#",
				"#......#.....BE#",
				"#......#...#####",
				"#......#...#####",
				"#......A...#####",
				"#a..P..A.b.#####",
				"################",
			},
			"m1.5 r | m9.5 r | m9.5 j r | m9.5 j j m14.5",
			"m1.5 r | m9.5 r | m9.5 j r | m3.5 r | m3.5 j j m9.5 j j m14.5",
		},
		// ---- World 2: The Factory. Lasers pulse on a shared clock; Echoes absorb them. ----------------
		{
			// The floor is live. Drop in while it is dark, then rewind: your Echo takes the beam for you.
			"Hot Wire", 2, 2, 2,
			{
				"################",
				"#..............#",
				"#..............#",
				"#.........*....#",
				"#..............#",
				"#.P............#",
				"####...........#",
				">.............E#",
				"################",
			},
			"u50 n4.6 r | u88 m14.5",
			"u50 n4.6 r | u88 m10.5 r | u88 m10.5 j j m14.5",
			60, 40,
		},
		{
			// The key lies under a beam. Step on it in the dark and rewind before it burns.
			"Curtain", 2, 2, 2,
			{
				"####v########v##",
				"#.........#....#",
				"#.........#....#",
				"#.........#.*..#",
				"#.........#....#",
				"#.........#....#",
				"#.........A....#",
				"#.P.a.....A...E#",
				"################",
			},
			"u50 m4.5 r | u76 m12.2 u250 m14.5",
			"u50 m4.5 r | u76 m12.5 r | u76 m12.5 j j u250 m14.5",
			50, 50,
		},
		{
			// Beams from both walls. One Echo on each side of the exit.
			"Crossfire", 2, 3, 3,
			{
				"################",
				"#......*.......#",
				"#..............#",
				"#..............#",
				"#......P.......#",
				"#..##########..#",
				"#..........*...#",
				">......E.......<",
				"################",
			},
			"u17 n1.4 r | u10 n14.4 r | u100 m2.2 m7.5",
			"u17 n1.4 r | u10 n14.4 r | u100 m13.6 m7.5",
			70, 30,
		},
		{
			// The beam falls right where you must climb. Build the stair in the dark.
			"Stairwell", 2, 3, 3,
			{
				"##########v#####",
				"#..............#",
				"#..............#",
				"#.*.........E..#",
				"#..........#####",
				"#..........#####",
				"#..........#####",
				"#.P........#####",
				"################",
			},
			"u10 m10.5 r | m10.5 j r | m9.5 u170 m10.5 j j m12.5",
			"u10 m10.5 r | m10.5 j r | w5 r | j j m9.5 u170 m10.5 j j m12.5",
			40, 90,
		},
		{
			// A beam sweeps the walkway over the spikes. Park an Echo at its start.
			"Hot Walkway", 2, 2, 3,
			{
				"################",
				"#.......*......#",
				"#..............#",
				">..............#",
				"#..............#",
				"#....#######...#",
				"#..............#",
				"#.P.^^^^^^^^^.E#",
				"################",
			},
			"m3.5 r | m3.5 j u60 m5.6 r | m3.5 j u160 m14.5",
			"m3.5 r | m3.5 j u60 m5.6 r | m3.5 j u160 m8.5 j m14.5",
			60, 40,
		},
		{
			// Three keys, three beams. The first Echo in place shields the path for the rest.
			"Assembly Line", 2, 4, 4,
			{
				"##v##v##v#######",
				"#..........#...#",
				"#..........#...#",
				"#..........#.*.#",
				"#..........#...#",
				"#..........#...#",
				"#..........A...#",
				"#.a.Pa..a..A..E#",
				"################",
			},
			"u45 m5.5 r | u40 m2.5 r | u50 m8.5 r | u70 m14.5",
			"u45 m5.5 r | u40 m2.5 r | u50 m8.5 r | u70 m13.5 r | u70 m13.5 j j m14.5",
			50, 50,
		},
		{
			// The key sits at the mouth of the laser: one Echo, two jobs.
			"Blind Corner", 2, 2, 2,
			{
				"################",
				"#..#...........#",
				"#..#...........#",
				"#..#.......P...#",
				"#..#.....####..#",
				"#..#..*........#",
				"#..A...........#",
				"#E.A..........a<",
				"################",
			},
			"u45 n14.5 r | u89 m1.5",
			"u45 n14.5 r | u89 m6.5 r | u89 m6.5 j j m1.5",
			60, 40,
		},
		{
			// The beam over the key sleeps only late in the loop. Everyone waits for overtime.
			"Overtime", 2, 3, 3,
			{
				"####v###########",
				"#........#.....#",
				"#........#.....#",
				"#......*.#.....#",
				"#........#...E.#",
				"#........A.#####",
				"#........A.#####",
				"#...a.P..A.#####",
				"################",
			},
			"u140 m4.5 r | m10.5 r | m10.5 j m13.5",
			"u140 m4.5 r | m10.5 r | m7.5 r | m7.5 j j m10.5 j m13.5",
			150, 50,
		},
		{
			// Two keys under two beams, two doors in a row.
			"Gauntlet", 2, 3, 3,
			{
				"####v####v######",
				"#......#.....#.#",
				"#......#.....#.#",
				"#......#..*..#.#",
				"#......#.....#.#",
				"#......#.....#.#",
				"#......A.....B.#",
				"#.P.a..A.b...BE#",
				"################",
			},
			"u45 m4.5 r | u70 m8.5 u150 m9.5 r | u160 m14.5",
			"u45 m4.5 r | u70 m8.5 u150 m9.5 r | u160 m10.5 r | u160 m10.5 j j m14.5",
			50, 50,
		},
		{
			// The finale: a key under a beam, a key in the laser's mouth, a two-Echo climb.
			"The Core", 2, 5, 4,
			{
				"####v###########",
				"#......#.....#.#",
				"#.*....#.....B.#",
				"#......#.....BE#",
				"#......#...#####",
				"#......#...#####",
				"#......A...#####",
				"#.P.a..A.b.<####",
				"################",
			},
			"m3.5 u40 m4.5 r | u48 m9.5 r | u109 m9.5 j r | u110 m9.5 j j m14.5",
			"m3.5 u40 m4.5 r | u48 m9.5 r | u109 m9.5 j r | w5 r | u110 j j m9.5 j j m14.5",
			40, 80,
		},
	};

	int NumLevels() { return int(sizeof(GLevels) / sizeof(GLevels[0])); }
	const FLevelDef& GetLevelDef(int Index) { return GLevels[Index]; }
	int NumWorlds() { return GLevels[NumLevels() - 1].World; }

	int FirstLevelOfWorld(int World)
	{
		for (int i = 0; i < NumLevels(); ++i) if (GLevels[i].World >= World) return i;
		return NumLevels();
	}

	int LevelNumberInWorld(int Index) { return Index - FirstLevelOfWorld(GLevels[Index].World) + 1; }

	const char* WorldName(int World)
	{
		static const char* Names[] = { "The Lab", "The Factory", "The Clocktower", "The Rift" };
		return World >= 1 && World <= 4 ? Names[World - 1] : "";
	}

	ETile FLevel::TileAt(int X, int Y) const
	{
		if (X < 0 || X >= W) return ETile::Solid;
		if (Y < 0 || Y >= H) return ETile::Empty;
		return Tiles[Y][X];
	}

	bool FLevel::Parse(const FLevelDef& D, char* Error, int ErrorLen)
	{
		*this = FLevel();
		Def = &D;
		while (H < MaxH && D.Rows[H]) ++H;
		if (H == 0) { snprintf(Error, ErrorLen, "%s: no rows", D.Name); return false; }
		W = int(strlen(D.Rows[0]));
		if (W > MaxW) { snprintf(Error, ErrorLen, "%s: too wide", D.Name); return false; }

		bool bSpawn = false, bExit = false;
		for (int Row = 0; Row < H; ++Row)
		{
			const char* S = D.Rows[Row];
			if (int(strlen(S)) != W) { snprintf(Error, ErrorLen, "%s: row %d has the wrong width", D.Name, Row); return false; }
			const int Y = H - 1 - Row;
			for (int X = 0; X < W; ++X)
			{
				const char C = S[X];
				ETile T = ETile::Empty;
				if (C == '#') T = ETile::Solid;
				else if (C == '^') T = ETile::Spike;
				else if (C == 'P') { Spawn = { X + 0.5f, float(Y) }; bSpawn = true; }
				else if (C == 'E') { Exit = { X + 0.5f, float(Y) }; bExit = true; }
				else if (C == '*') { Shard = { X + 0.5f, Y + 0.5f }; HasShard = true; }
				else if (C == '>' || C == '<' || C == 'v')
				{
					if (NumEmitters == MaxEmitters) { snprintf(Error, ErrorLen, "%s: too many lasers", D.Name); return false; }
					Emitters[NumEmitters++] = { X, Y, C == '>' ? 1 : (C == '<' ? -1 : 0), C == 'v' ? -1 : 0 };
					T = ETile::Solid;
				}
				else if (C >= 'a' && C <= 'd')
				{
					if (NumPlates == MaxPlates) { snprintf(Error, ErrorLen, "%s: too many plates", D.Name); return false; }
					Plates[NumPlates++] = { float(X), float(X + 1), float(Y), C - 'a' };
				}
				else if (C >= 'A' && C <= 'D')
				{
					// Merge with the door directly above (already parsed: rows go top to bottom).
					bool bMerged = false;
					for (int i = 0; i < NumDoors; ++i)
						if (Doors[i].Channel == C - 'A' && Doors[i].X0 == float(X) && Doors[i].Y0 == float(Y + 1))
						{ Doors[i].Y0 = float(Y); bMerged = true; break; }
					if (!bMerged)
					{
						if (NumDoors == MaxDoors) { snprintf(Error, ErrorLen, "%s: too many doors", D.Name); return false; }
						Doors[NumDoors++] = { float(X), float(X + 1), float(Y), float(Y + 1), C - 'A' };
					}
				}
				else if (C != '.') { snprintf(Error, ErrorLen, "%s: unknown tile '%c'", D.Name, C); return false; }
				Tiles[Y][X] = T;
			}
		}
		if (!bSpawn || !bExit) { snprintf(Error, ErrorLen, "%s: needs a P and an E", D.Name); return false; }
		for (int i = 0; i < NumDoors; ++i)
		{
			bool bHasPlate = false;
			for (int p = 0; p < NumPlates; ++p) bHasPlate |= Plates[p].Channel == Doors[i].Channel;
			if (!bHasPlate) { snprintf(Error, ErrorLen, "%s: door %c has no plate", D.Name, 'A' + Doors[i].Channel); return false; }
		}
		return true;
	}
}
