// ECHOES: paints the whole game onto the canvas each frame. (CLAUDE.md: Rendering / UI)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Render/ECWorldRenderer.h"
#include "ECHUD.generated.h"

class UFont;

UCLASS()
class AECHUD : public AHUD
{
	GENERATED_BODY()

public:
	AECHUD();
	virtual void DrawHUD() override;

private:
	UPROPERTY() TObjectPtr<UFont> Font;

	FECWorldRenderer Renderer;
};
