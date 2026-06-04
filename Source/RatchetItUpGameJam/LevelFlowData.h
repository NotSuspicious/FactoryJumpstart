// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LevelFlowData.generated.h"

class ULevelSequence;

/**
 * Per-level cinematic + dialogue content. Create one of these assets per level
 * (right-click in the Content Browser -> Miscellaneous -> Data Asset -> LevelFlowData)
 * and assign it to that level's entry in the GameMode's Levels list.
 */
UCLASS(BlueprintType)
class RATCHETITUPGAMEJAM_API ULevelFlowData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// --- Cinematic sequences ---

	/** Plays at scene start, before the intro dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequences")
	TObjectPtr<ULevelSequence> IntroSequence;

	/** Plays at scene end, after the puzzle is solved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequences")
	TObjectPtr<ULevelSequence> PuzzleSolvedSequence;

	/** Plays at scene end, after the end dialogue, before the level closes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequences")
	TObjectPtr<ULevelSequence> ExitLevelSequence;

	// --- Dialogue ---

	/** Lines shown after the intro sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	TArray<FText> IntroDialogue;

	/** Lines shown after the puzzle-solved sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	TArray<FText> EndDialogue;
};

/** Pairs a level in the sequential flow with its per-level content. */
USTRUCT(BlueprintType)
struct FLevelFlowEntry
{
	GENERATED_BODY()

	/** The level asset to load. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Flow")
	TSoftObjectPtr<UWorld> Level;

	/** Cinematics + dialogue for this level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Flow")
	TObjectPtr<ULevelFlowData> FlowData;
};
