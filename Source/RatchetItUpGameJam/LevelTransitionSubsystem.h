// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlideTransitionWidget.h"
#include "LevelTransitionSubsystem.generated.h"

class USlideTransitionWidget;

/**
 * Drives artifact-free slide transitions between levels.
 *
 * Lives on the GameInstance, so it (and the transition widget it owns) survive
 * UGameplayStatics::OpenLevel. That persistence is what removes the load-time
 * flash: the panel slides in to fully cover the screen, the level loads while
 * that covered frame is held, and the same widget slides away once the new map
 * is ready (re-added during PostLoadMapWithWorld, before the first frame renders).
 *
 * Auto-instantiated for every GameInstance; no project settings changes needed.
 */
UCLASS()
class RATCHETITUPGAMEJAM_API ULevelTransitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Slides a solid panel across to cover the screen, loads Level, and slides
	 * the panel away once the new level is ready. Safe to call from Blueprint.
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Transition")
	void TransitionToLevel(TSoftObjectPtr<UWorld> Level, FSlideTransitionSettings Settings, TSubclassOf<USlideTransitionWidget> WidgetClass);

	/**
	 * Plays only the "open" (reveal) half of a transition: snaps the panel to
	 * fully covering, then slides it away. Use at scene start to fade a level in
	 * even when it was loaded directly (e.g. pressing PIE on a level).
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Transition")
	void PlayOpenTransition(FSlideTransitionSettings Settings, TSubclassOf<USlideTransitionWidget> WidgetClass);

	/**
	 * Plays only the "close" (cover) half of a transition: slides the panel in to
	 * fully cover the screen, then runs OnCovered and leaves the screen covered.
	 * Use to fade out before an action that ends the current view (e.g. quitting).
	 */
	void PlayCloseTransition(FSlideTransitionSettings Settings, TSubclassOf<USlideTransitionWidget> WidgetClass, FSimpleDelegate OnCovered);

	/**
	 * Covers the screen with the close transition, then quits the game.
	 * Callable from any GameMode/widget (e.g. a main menu Quit button).
	 */
	UFUNCTION(BlueprintCallable, Category = "Level Transition")
	void CloseAndQuitGame(FSlideTransitionSettings Settings, TSubclassOf<USlideTransitionWidget> WidgetClass);

	/** True while a transition is in progress (cover, load, or reveal). */
	UFUNCTION(BlueprintPure, Category = "Level Transition")
	bool IsTransitioning() const { return bBusy; }

private:

	/** Lazily creates (GameInstance-owned, so it persists) and returns the transition widget. */
	USlideTransitionWidget* EnsureWidget(TSubclassOf<USlideTransitionWidget> WidgetClass);

	/** Slide-in finished: the screen is fully covered, so kick off the actual load. */
	void HandleSlideInComplete();

	/** Close slide-in finished: the screen is fully covered, so run the stored callback. */
	void HandleCloseCovered();

	/** Quits the game immediately (used as the CloseAndQuitGame callback). */
	void QuitGameNow();

	/** New map is ready (and not yet rendered): re-show the covered panel, then reveal. */
	void HandlePostLoadMap(UWorld* LoadedWorld);

	/** Reveal finished: tear down the panel and clear busy state. */
	void HandleSlideOutComplete();

	/** Persistent transition widget; UPROPERTY keeps it alive across level loads. */
	UPROPERTY(Transient)
	TObjectPtr<USlideTransitionWidget> Widget = nullptr;

	FSlideTransitionSettings PendingSettings;
	TSoftObjectPtr<UWorld> PendingLevel;
	TSubclassOf<USlideTransitionWidget> PendingWidgetClass;

	/** Callback run once a close transition has fully covered the screen. */
	FSimpleDelegate CloseCallback;

	FDelegateHandle PostLoadMapHandle;

	/** True from the first call until the reveal completes. */
	bool bBusy = false;

	/** True while waiting for the map we requested to finish loading. */
	bool bAwaitingLoad = false;

	/** Z-order for the panel; above HUD and everything else. */
	static constexpr int32 TransitionZOrder = 1000;
};
