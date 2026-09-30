// ECHOES: paints the whole game onto the canvas each frame. (CLAUDE.md: Rendering / UI)
#include "Game/ECHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Game/ECPlayerController.h"
#include "Render/ECDraw.h"
#include "UI/ECUI.h"
#include "UObject/ConstructorHelpers.h"

AECHUD::AECHUD()
{
	static ConstructorHelpers::FObjectFinder<UFont> Roboto(TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	Font = Roboto.Object;
}

void AECHUD::DrawHUD()
{
	Super::DrawHUD();
	AECPlayerController* PC = Cast<AECPlayerController>(PlayerOwner);
	if (!PC || !Canvas || !PC->Save || !PC->Game || !PC->Game->Sim.Level.Def) { return; }
	FECGame& Game = *PC->Game;

	FECDraw D(Canvas);
	const bool bTouch = PC->ShouldShowTouch() && !Game.bAttract && (Game.Screen == EECScreen::Playing || Game.Screen == EECScreen::Paused);
	Renderer.Draw(D, Game, bTouch);

	if (!Game.bHideUI)
	{
		FECUiContext Ctx;
		Ctx.Font = Font;
		Ctx.bShowTouch = PC->ShouldShowTouch();
		Ctx.bTouchDevice = PC->IsTouchDevice();
		Ctx.bLeftDown = PC->bTouchLeft;
		Ctx.bRightDown = PC->bTouchRight;
		Ctx.bJumpDown = PC->bTouchJump;
		Ctx.Safe = PC->SafeArea;
		FECUI::Draw(D, Canvas, Game, Ctx);
	}
	else
	{
		Game.Buttons.Reset();
	}
	D.Flush();
}
