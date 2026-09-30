// ECHOES core: the deterministic loop simulation. CLAUDE.md: Core / Echoes.
#include "ECSim.h"
#include <cmath>

namespace EC
{
	static constexpr float Eps = 1e-4f;

	static float Approach(float V, float Target, float Step)
	{
		return V < Target ? (V + Step > Target ? Target : V + Step) : (V - Step < Target ? Target : V - Step);
	}

	bool FSim::Init(const FLevelDef& Def, char* Error, int ErrorLen)
	{
		if (!Level.Parse(Def, Error, ErrorLen)) return false;
		RestartLevel();
		return true;
	}

	void FSim::RestartLevel()
	{
		NumEchoes = 0;
		bShardCommitted = false;
		ResetWorld();
	}

	void FSim::RetryLoop() { ResetWorld(); }

	void FSim::CommitLoop()
	{
		if (Phase != EPhase::LoopEnded) return;
		Echoes[NumEchoes] = Current;   // fixed-size copy, no allocation
		++NumEchoes;
		bShardCommitted |= bShardThisLoop;
		ResetWorld();
	}

	// Every level object back to its initial state. Only Echoes carry information across loops.
	void FSim::ResetWorld()
	{
		Phase = EPhase::Playing;
		Tick = 0;
		ParadoxEcho = -1;
		bShardThisLoop = false;
		Current.Length = 0;
		Player = FBody();
		Player.X = Level.Spawn.X;
		Player.Y = Level.Spawn.Y;
		Player.Grounded = true;
		for (int i = 0; i < MaxDoors; ++i) DoorOpen[i] = 0.f;
		for (int i = 0; i < MaxPlates; ++i) PlatePressed[i] = false;
		for (int i = 0; i < NumEchoes; ++i)
		{
			const FFrame& F = Echoes[i].At(0);
			EchoState[i] = FEchoState();
			EchoState[i].X = EchoState[i].PrevX = F.X;
			EchoState[i].Y = EchoState[i].PrevY = F.Y;
		}
		ComputeBeams();
	}

	bool FSim::LaserOnAt(int T) const
	{
		const FLevelDef& D = *Level.Def;
		if (D.LaserOff <= 0) return true;
		return (T % (D.LaserOn + D.LaserOff)) < D.LaserOn;
	}

	bool FSim::LaserWarning() const
	{
		if (LaserOn() || Level.Def->LaserOff <= 0) return false;
		return LaserOnAt(Tick + LaserWarnTicks);
	}

	FBox FSim::BeamBox(int i) const
	{
		const FEmitter& E = Level.Emitters[i];
		if (E.DY == 0)
		{
			const float Y = E.Y + 0.5f, X0 = E.DX > 0 ? float(E.X + 1) : float(E.X);
			return { X0 < BeamEnd[i] ? X0 : BeamEnd[i], Y - BeamHalf, X0 < BeamEnd[i] ? BeamEnd[i] : X0, Y + BeamHalf };
		}
		const float X = E.X + 0.5f;
		return { X - BeamHalf, BeamEnd[i], X + BeamHalf, float(E.Y) };
	}

	// Beams run until a wall, the solid part of a door, or an Echo (which absorbs them).
	void FSim::ComputeBeams()
	{
		for (int i = 0; i < Level.NumEmitters; ++i)
		{
			const FEmitter& E = Level.Emitters[i];
			FBox D;
			if (E.DY == 0)
			{
				const float Y = E.Y + 0.5f, X0 = E.DX > 0 ? float(E.X + 1) : float(E.X);
				float End = X0;
				for (int X = E.X + E.DX; ; X += E.DX)
					if (Level.TileAt(X, E.Y) == ETile::Solid) { End = E.DX > 0 ? float(X) : float(X + 1); break; }
				auto Clip = [&](float L, float R)
				{
					if (E.DX > 0 && R > X0 && L < End) End = L > X0 ? L : X0;
					if (E.DX < 0 && L < X0 && R > End) End = R < X0 ? R : X0;
				};
				for (int d = 0; d < Level.NumDoors; ++d)
					if (DoorSolid(d, D) && D.B < Y + BeamHalf && D.T > Y - BeamHalf) Clip(D.L, D.R);
				for (int k = 0; k < NumEchoes; ++k)
				{
					const FBox B = BodyBox(EchoState[k].X, EchoState[k].Y);
					if (B.B < Y + BeamHalf && B.T > Y - BeamHalf) Clip(B.L, B.R);
				}
				BeamEnd[i] = End;
			}
			else
			{
				const float X = E.X + 0.5f, Y0 = float(E.Y);
				float End = -2.f;
				for (int Y = E.Y - 1; Y >= -2; --Y)
					if (Level.TileAt(E.X, Y) == ETile::Solid) { End = float(Y + 1); break; }
				auto Clip = [&](float L, float R, float T) { if (L < X + BeamHalf && R > X - BeamHalf && T <= Y0 && T > End) End = T; };
				for (int d = 0; d < Level.NumDoors; ++d)
					if (DoorSolid(d, D)) Clip(D.L, D.R, D.T);
				for (int k = 0; k < NumEchoes; ++k)
				{
					const FBox B = BodyBox(EchoState[k].X, EchoState[k].Y);
					Clip(B.L, B.R, B.T);
				}
				BeamEnd[i] = End;
			}
		}
	}

	bool FSim::DoorSolid(int i, FBox& Out) const
	{
		const FDoor& D = Level.Doors[i];
		const float Bottom = D.Y0 + (D.Y1 - D.Y0) * DoorOpen[i];
		if (Bottom >= D.Y1 - Eps) return false;
		Out = { D.X0, Bottom, D.X1, D.Y1 };
		return true;
	}

	bool FSim::SolidAt(const FBox& B) const
	{
		const int X0 = int(std::floor(B.L + Eps)), X1 = int(std::floor(B.R - Eps));
		const int Y0 = int(std::floor(B.B + Eps)), Y1 = int(std::floor(B.T - Eps));
		for (int y = Y0; y <= Y1; ++y)
			for (int x = X0; x <= X1; ++x)
				if (Level.TileAt(x, y) == ETile::Solid) return true;
		FBox D;
		for (int i = 0; i < Level.NumDoors; ++i)
			if (DoorSolid(i, D) && D.Overlaps(B)) return true;
		return false;
	}

	// Horizontal move against tiles and closed doors. Echoes never block sideways: you walk through them.
	void FSim::MoveX(float Dx)
	{
		if (Dx == 0.f) return;
		FBody& P = Player;
		P.X += Dx;
		const FBox B = BodyBox(P.X, P.Y);
		const int Y0 = int(std::floor(B.B + Eps)), Y1 = int(std::floor(B.T - Eps));
		const int X0 = int(std::floor(B.L + Eps)), X1 = int(std::floor(B.R - Eps));
		for (int y = Y0; y <= Y1; ++y)
			for (int x = X0; x <= X1; ++x)
				if (Level.TileAt(x, y) == ETile::Solid)
				{
					if (Dx > 0) P.X = std::fmin(P.X, x - BodyW * 0.5f);
					else        P.X = std::fmax(P.X, x + 1 + BodyW * 0.5f);
					P.VX = 0;
				}
		FBox D;
		for (int i = 0; i < Level.NumDoors; ++i)
			if (DoorSolid(i, D) && D.Overlaps(BodyBox(P.X, P.Y)))
			{
				if (Dx > 0) P.X = std::fmin(P.X, D.L - BodyW * 0.5f);
				else        P.X = std::fmax(P.X, D.R + BodyW * 0.5f);
				P.VX = 0;
			}
	}

	// Vertical move. Tiles and doors are solid both ways; Echo heads are one-way platforms.
	void FSim::MoveY(float Dy, uint32_t& Events)
	{
		FBody& P = Player;
		const bool bWasGrounded = P.Grounded;
		const float OldY = P.Y;
		P.Y += Dy;
		P.Grounded = false;
		P.GroundEcho = -1;

		const FBox B = BodyBox(P.X, P.Y);
		const int X0 = int(std::floor(B.L + Eps)), X1 = int(std::floor(B.R - Eps));
		const int Y0 = int(std::floor(B.B + Eps)), Y1 = int(std::floor(B.T - Eps));
		if (Dy < 0)
		{
			for (int y = Y0; y <= Y1; ++y)
				for (int x = X0; x <= X1; ++x)
					if (Level.TileAt(x, y) == ETile::Solid && y + 1 > P.Y)
					{ P.Y = float(y + 1); P.Grounded = true; }
			FBox D;
			for (int i = 0; i < Level.NumDoors; ++i)
				if (DoorSolid(i, D) && D.Overlaps(BodyBox(P.X, P.Y)) && OldY >= D.T - Eps)
				{ P.Y = D.T; P.Grounded = true; }
			for (int i = 0; i < NumEchoes; ++i)
			{
				const FEchoState& E = EchoState[i];
				const float Top = E.Y + BodyH;
				const bool bOverX = P.X - BodyW * 0.5f < E.X + BodyW * 0.5f && E.X - BodyW * 0.5f < P.X + BodyW * 0.5f;
				if (bOverX && OldY >= Top - EchoStepUp && P.Y < Top + Eps && Top >= P.Y)
				{ P.Y = Top; P.Grounded = true; P.GroundEcho = i; }
			}
			if (P.Grounded) P.VY = 0;
		}
		else if (Dy > 0)
		{
			for (int y = Y0; y <= Y1; ++y)
				for (int x = X0; x <= X1; ++x)
					if (Level.TileAt(x, y) == ETile::Solid && y < P.Y + BodyH)
					{ P.Y = std::fmin(P.Y, y - BodyH); P.VY = 0; }
			FBox D;
			for (int i = 0; i < Level.NumDoors; ++i)
				if (DoorSolid(i, D) && D.Overlaps(BodyBox(P.X, P.Y)))
				{ P.Y = std::fmin(P.Y, D.B - BodyH); P.VY = 0; }
		}
		if (P.Grounded && !bWasGrounded) { Events |= EV_Land; P.LandTicks = 6; }
	}

	void FSim::StepPlayer(const FInput& In, uint32_t& Events)
	{
		FBody& P = Player;

		// Ride the Echo under our feet (horizontal carry; vertical is handled by landing on its head).
		if (P.GroundEcho >= 0)
		{
			const FEchoState& E = EchoState[P.GroundEcho];
			MoveX(E.X - E.PrevX);
		}

		const int Dir = int(In.Right) - int(In.Left);
		if (Dir != 0)
		{
			P.Facing = Dir > 0 ? 1 : 0;
			P.VX = Approach(P.VX, Dir * RunSpeed, (P.Grounded ? GroundAccel : AirAccel) * Dt);
		}
		else
			P.VX = Approach(P.VX, 0.f, (P.Grounded ? GroundDecel : AirDecel) * Dt);

		const bool bPressed = In.Jump && !P.PrevJump;
		P.PrevJump = In.Jump;
		if (bPressed) P.JumpBuffer = JumpBufferTicks; else if (P.JumpBuffer > 0) --P.JumpBuffer;
		if (P.Grounded) P.Coyote = CoyoteTicks; else if (P.Coyote > 0) --P.Coyote;

		bool bJumped = false;
		if (P.JumpBuffer > 0 && P.Coyote > 0)
		{
			P.VY = JumpSpeed;
			P.JumpBuffer = P.Coyote = 0;
			P.Grounded = false;
			P.GroundEcho = -1;
			P.bJumpCut = false;
			bJumped = true;
			Events |= EV_Jump;
		}
		if (!In.Jump && P.VY > 0 && !P.bJumpCut) { P.VY *= JumpCut; P.bJumpCut = true; }

		P.VY = std::fmax(P.VY - Gravity * Dt, -MaxFall);
		MoveX(P.VX * Dt);
		MoveY(P.VY * Dt, Events);

		if (P.LandTicks > 0) --P.LandTicks;
		if (P.Grounded) P.Anim = P.LandTicks > 0 ? EAnim::Land : (std::fabs(P.VX) > 0.5f ? EAnim::Run : EAnim::Idle);
		else            P.Anim = P.VY > 0 ? EAnim::Jump : EAnim::Fall;

		FFrame& F = Current.Frames[Tick];
		F.X = P.X; F.Y = P.Y; F.VX = P.VX; F.VY = P.VY;
		F.Facing = P.Facing;
		F.Anim = uint8_t(P.Anim);
		F.Flags = uint8_t((P.Grounded ? FF_Grounded : 0) | (bJumped ? FF_Jumped : 0) | ((Events & EV_Land) ? FF_Landed : 0));
		F.Pad = 0;
	}

	// Something to stand on under Echo i: a tile, a closed door, or an earlier Echo (the ones it could see).
	bool FSim::HasSupport(int EchoIndex) const
	{
		const FEchoState& E = EchoState[EchoIndex];
		const FBox Probe = { E.X - BodyW * 0.5f, E.Y - 0.05f, E.X + BodyW * 0.5f, E.Y + 0.02f };
		if (SolidAt(Probe)) return true;
		for (int j = 0; j < EchoIndex; ++j)
		{
			const FEchoState& O = EchoState[j];
			const bool bOverX = Probe.L < O.X + BodyW * 0.5f && O.X - BodyW * 0.5f < Probe.R;
			if (bOverX && std::fabs(O.Y + BodyH - E.Y) < 0.06f) return true;
		}
		return false;
	}

	void FSim::EndLoop(uint32_t& Events)
	{
		Current.Length = Tick;
		Events |= EV_LoopEnd;
		Phase = NumEchoes < Level.Def->MaxEchoes ? EPhase::LoopEnded : EPhase::OutOfLoops;
	}

	uint32_t FSim::Step(const FInput& In)
	{
		if (Phase != EPhase::Playing) return 0;
		uint32_t Events = 0;
		if (In.Rewind && Tick >= MinRewindTick) { EndLoop(Events); return Events; }

		// 1. Echoes move to their recorded pose for this tick (kinematic, exact).
		for (int i = 0; i < NumEchoes; ++i)
		{
			FEchoState& E = EchoState[i];
			const FRecording& R = Echoes[i];
			const FFrame& F = R.At(Tick);
			E.PrevX = E.X; E.PrevY = E.Y;
			E.X = F.X; E.Y = F.Y;
			E.bFinished = Tick >= R.Length;
			if (!E.bFinished && (F.Flags & FF_Jumped)) Events |= EV_EchoJump;
			if (!E.bFinished && (F.Flags & FF_Landed)) Events |= EV_EchoLand;
		}

		// 2. The player.
		StepPlayer(In, Events);

		// 3. Plates: pressed by the feet of anyone standing on them (player or Echo).
		for (int p = 0; p < Level.NumPlates; ++p)
		{
			const FPlate& Pl = Level.Plates[p];
			auto OnPlate = [&Pl](float X, float Y)
			{
				return X + BodyW * 0.5f > Pl.X0 + 0.15f && X - BodyW * 0.5f < Pl.X1 - 0.15f && Y >= Pl.Y - 0.01f && Y <= Pl.Y + 0.25f;
			};
			bool bDown = OnPlate(Player.X, Player.Y);
			for (int i = 0; i < NumEchoes && !bDown; ++i) bDown = OnPlate(EchoState[i].X, EchoState[i].Y);
			if (bDown != PlatePressed[p]) Events |= bDown ? EV_PlateDown : EV_PlateUp;
			PlatePressed[p] = bDown;
		}

		// 4. Doors: open while every plate on their channel is down. A closing door stops on anyone under it.
		for (int d = 0; d < Level.NumDoors; ++d)
		{
			const FDoor& D = Level.Doors[d];
			bool bOpen = true;
			for (int p = 0; p < Level.NumPlates; ++p)
				if (Level.Plates[p].Channel == D.Channel && !PlatePressed[p]) bOpen = false;
			const float Old = DoorOpen[d];
			if (bOpen)
			{
				DoorOpen[d] = std::fmin(1.f, Old + DoorOpenRate * Dt);
				if (Old == 0.f && DoorOpen[d] > 0.f) Events |= EV_DoorOpen;
			}
			else if (Old > 0.f)
			{
				float New = std::fmax(0.f, Old - DoorCloseRate * Dt);
				const float H = D.Y1 - D.Y0;
				auto Hold = [&](float X, float Y)
				{
					if (X + BodyW * 0.5f > D.X0 + Eps && X - BodyW * 0.5f < D.X1 - Eps && Y < D.Y1 && Y + BodyH > D.Y0 + H * New)
						New = std::fmin(Old, std::fmax(New, (Y + BodyH - D.Y0) / H));
				};
				Hold(Player.X, Player.Y);
				for (int i = 0; i < NumEchoes; ++i) Hold(EchoState[i].X, EchoState[i].Y);
				DoorOpen[d] = New;
				if (New == 0.f) Events |= EV_DoorClose;
			}
		}

		// 5. Paradox: an Echo cannot do what it recorded - pushed into a closed door, or left standing on nothing.
		for (int i = 0; i < NumEchoes && Phase == EPhase::Playing; ++i)
		{
			FEchoState& E = EchoState[i];
			const FBox B = BodyBox(E.X, E.Y);
			bool bBlocked = false;
			FBox D;
			for (int d = 0; d < Level.NumDoors; ++d)
				if (DoorSolid(d, D) && D.Overlaps(B))
				{
					const float Px = std::fmin(B.R, D.R) - std::fmax(B.L, D.L);
					const float Py = std::fmin(B.T, D.T) - std::fmax(B.B, D.B);
					bBlocked |= std::fmin(Px, Py) > ParadoxDepth;
				}
			E.BlockedTicks = bBlocked ? E.BlockedTicks + 1 : 0;
			const bool bStanding = (Echoes[i].At(Tick).Flags & FF_Grounded) != 0;
			E.UnsupportedTicks = bStanding && !HasSupport(i) ? E.UnsupportedTicks + 1 : 0;
			if (E.BlockedTicks >= ParadoxTicks || E.UnsupportedTicks >= UnsupportedTicks)
			{
				Phase = EPhase::Paradox;
				ParadoxEcho = i;
				Events |= EV_Paradox;
			}
		}
		if (Phase != EPhase::Playing) return Events;

		// 6. Hazards, shard, exit (the living player only; Echoes are ghosts to spikes and pickups).
		const FBox PB = BodyBox(Player.X, Player.Y);
		ComputeBeams();
		bool bDead = Player.Y < -2.f;
		if (LaserOn())
			for (int i = 0; i < Level.NumEmitters; ++i)
				if (BeamBox(i).Overlaps(PB)) bDead = true;
		{
			const FBox Inner = { PB.L + 0.1f, PB.B, PB.R - 0.1f, PB.T - 0.2f };
			const int X0 = int(std::floor(Inner.L)), X1 = int(std::floor(Inner.R));
			const int Y0 = int(std::floor(Inner.B)), Y1 = int(std::floor(Inner.T));
			for (int y = Y0; y <= Y1; ++y)
				for (int x = X0; x <= X1; ++x)
					if (Level.TileAt(x, y) == ETile::Spike && Inner.Overlaps({ float(x) + 0.1f, float(y), float(x) + 0.9f, float(y) + 0.5f }))
						bDead = true;
		}
		if (bDead)
		{
			Phase = EPhase::Died;
			Events |= EV_Died;
			return Events;
		}
		if (Level.HasShard && !ShardTaken() && PB.Overlaps({ Level.Shard.X - 0.3f, Level.Shard.Y - 0.3f, Level.Shard.X + 0.3f, Level.Shard.Y + 0.3f }))
		{
			bShardThisLoop = true;
			Events |= EV_Shard;
		}
		if (PB.Overlaps({ Level.Exit.X - 0.2f, Level.Exit.Y, Level.Exit.X + 0.2f, Level.Exit.Y + 2.f }))
		{
			++Tick;
			Current.Length = Tick;
			Phase = EPhase::Solved;
			Events |= EV_Solved;
			return Events;
		}

		// 7. Time.
		++Tick;
		if (Tick >= LoopTicks) EndLoop(Events);
		return Events;
	}
}
