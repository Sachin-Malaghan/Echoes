// ECHOES: saved progress and settings (one slot, all platforms). (CLAUDE.md: Game flow)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ECSaveGame.generated.h"

UENUM()
enum class EECTouchMode : uint8
{
	Auto,   // shown on touch devices, or after the first touch
	On,
	Off
};

UCLASS()
class UECSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* SlotName = TEXT("Echoes");

	UPROPERTY() int32 Version = 1;
	UPROPERTY() int32 UnlockedLevels = 1;
	UPROPERTY() int32 LastLevel = 0;
	UPROPERTY() TArray<uint8> Stars;        // per level, bits: 1 solved, 2 at par, 4 shard
	UPROPERTY() TArray<int32> BestLoops;    // 0 = never solved
	UPROPERTY() TArray<float> BestTimes;    // seconds of the fastest solve, 0 = never solved

	UPROPERTY() bool bMusic = true;
	UPROPERTY() bool bSound = true;
	UPROPERTY() EECTouchMode TouchMode = EECTouchMode::Auto;
};
