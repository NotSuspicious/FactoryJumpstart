// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SlideTransitionWidget.h"
#include "DialogueSubsystem.h"
#include "LevelFlowData.h"
#include "RatchetItUpGameJamGameMode.generated.h"

class ULevelSequence;
class ULevelSequencePlayer;
class ULevelTransitionSubsystem;

/**
 *  Game Mode that conducts the per-level flow and the sequential level list.
 *
 *  Per-level cinematics + dialogue live in a ULevelFlowData asset; the Levels
 *  list pairs each level with its data asset. On BeginPlay the current level is
 *  matched against that list so the right content plays (works even when you PIE
 *  directly into a level).
 *
 *  Scene start:  fade in  ->  Intro Sequence  ->  Intro Dialogue  ->  OnLevelBegin
 *  Scene end (OnPuzzleSolved):  Puzzle Solved Sequence  ->  End Dialogue
 *                               ->  Exit Sequence  ->  fade out + load next level
 */
UCLASS(abstract)
class ARatchetItUpGameJamGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	ARatchetItUpGameJamGameMode();

	/** Call when the level's puzzle is solved to start the scene-end flow. */
	UFUNCTION(BlueprintCallable, Category = "Level Flow")
	void OnPuzzleSolved();

	/**
	 * Slides the transition closed and loads the next level in Levels.
	 * Called automatically at the end of the scene-end flow; also callable directly.
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Flow")
	void AdvanceToNextLevel();

	/** Returns the zero-based index of the level currently being played. */
	UFUNCTION(BlueprintPure, Category = "Level Flow")
	int32 GetCurrentLevelIndex() const { return CurrentLevelIndex; }

	/** Returns true if there is another level after the current one. */
	UFUNCTION(BlueprintPure, Category = "Level Flow")
	bool HasNextLevel() const;

	/** Fired when gameplay should begin (after the intro sequence + dialogue). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Level Flow")
	void OnLevelBegin();

protected:

	virtual void BeginPlay() override;

	/**
	 * Ordered list of levels, each paired with its per-level content (ULevelFlowData).
	 * Configure once on the Blueprint GameMode. Soft level references avoid
	 * hard-loading every level at startup.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Flow|Levels")
	TArray<FLevelFlowEntry> Levels;

	/** Look & feel of the open/close slide transition between levels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Flow|Levels")
	FSlideTransitionSettings TransitionSettings;

	/** Widget class for the slide. Defaults to the C++ USlideTransitionWidget. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Flow|Levels")
	TSubclassOf<USlideTransitionWidget> SlideWidgetClass;

	/** Dialogue widget / input / style configuration (shared across levels). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level Flow|Dialogue")
	FDialogueSettings DialogueSettings;

private:

	// --- Flow steps ---

	void StartSceneIntro();
	void BeginLevel();
	void StartSceneOutro();

	/** Fades the scene in (unless the transition subsystem is already revealing). */
	void PlayOpenTransitionIfNeeded();

	/** Plays Sequence (if set) and runs OnDone when it finishes; runs OnDone immediately if Sequence is null. */
	void PlaySequenceThen(ULevelSequence* Sequence, TFunction<void()> OnDone);

	UFUNCTION()
	void HandleSequenceFinished();

	/** Plays the dialogue Lines, then runs OnDone. */
	void PlayDialogueThen(const TArray<FText>& DialogueLines, TFunction<void()> OnDone);

	/** Enables/disables cinematic mode on the local player (freezes movement & turning). */
	void FreezePlayer(bool bFreeze);

	/** Matches the current world against Levels and caches the index + flow data. */
	void ResolveCurrentLevel();

	/** Returns the Levels index whose level matches the current world, or INDEX_NONE. */
	int32 FindCurrentLevelIndex() const;

	ULevelTransitionSubsystem* GetTransitionSubsystem() const;

	/** Per-level content resolved for the level currently being played. */
	UPROPERTY(Transient)
	TObjectPtr<ULevelFlowData> ActiveFlowData;

	/** Active cinematic player; kept alive while playing. */
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> ActiveSequencePlayer;

	/** Continuation to run when the active sequence finishes. */
	TFunction<void()> SequenceFinishedCallback;

	/**
	 * Index of the level currently loaded within Levels.
	 * Static so it survives level loads (the GameMode is recreated each level).
	 */
	static int32 CurrentLevelIndex;
};
