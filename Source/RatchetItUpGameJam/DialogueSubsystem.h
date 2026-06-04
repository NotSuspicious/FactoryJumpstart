// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DialogueWidget.h"
#include "DialogueSubsystem.generated.h"

class UInputAction;
class UInputMappingContext;
class APlayerController;

/** Editor-configurable look, feel, and input bindings for the dialogue system. */
USTRUCT(BlueprintType)
struct FDialogueSettings
{
	GENERATED_BODY()

	/** Widget class for the dialogue box. Defaults to the C++ UDialogueWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TSubclassOf<UDialogueWidget> WidgetClass;

	/** Panel image, tint, font, and text color. Swap the texture here in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FDialogueStyle Style;

	/** UI Input Action that completes the current line / advances to the next. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<UInputAction> AdvanceAction = nullptr;

	/** Mapping context (added while dialogue is active) that maps a key to AdvanceAction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;

	/** Typewriter speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (ClampMin = "1.0"))
	float CharactersPerSecond = 40.0f;

	/** Priority for the dialogue mapping context (high, so it sits above gameplay). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 MappingContextPriority = 100;
};

/**
 * Singleton dialogue system (one per GameInstance).
 *
 * Plays an array of animated text lines. The advance Input Action:
 *   - completes the current line instantly while it is still typing;
 *   - advances to the next line once the current line is fully shown.
 * While dialogue is active, player movement/look input is suppressed.
 *
 * Access from anywhere with UDialogueSubsystem::Get(this).
 */
UCLASS()
class RATCHETITUPGAMEJAM_API UDialogueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Convenience accessor for the singleton from any world context. */
	static UDialogueSubsystem* Get(const UObject* WorldContext);

	/**
	 * Shows the given lines one at a time. OnFinished fires after the last line
	 * is dismissed (or immediately if Lines is empty).
	 */
	void StartDialogue(const TArray<FText>& Lines, const FDialogueSettings& InSettings, FSimpleDelegate OnFinished);

	/** True while a dialogue is on screen. */
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsDialogueActive() const { return bActive; }

private:

	/** Bound to the advance Input Action: completes the line, or moves to the next. */
	void HandleAdvance();

	/** Displays line at Index using the typewriter. */
	void ShowLine(int32 Index);

	/** Tears down the widget/input and broadcasts completion. */
	void EndDialogue();

	/** Adds the dialogue mapping context, binds the advance action, suppresses player input. */
	void EnableDialogueInput();

	/** Reverses EnableDialogueInput(). */
	void DisableDialogueInput();

	APlayerController* GetLocalPlayerController() const;

	UPROPERTY(Transient)
	TObjectPtr<UDialogueWidget> Widget = nullptr;

	UPROPERTY(Transient)
	TArray<FText> Lines;

	FDialogueSettings Settings;
	FSimpleDelegate OnFinishedDelegate;
	int32 CurrentIndex = 0;
	bool bActive = false;

	/** Handle of the advance action binding, so it can be removed when dialogue ends. */
	uint32 AdvanceBindingHandle = 0;

	/** Z-order for the dialogue box (below the level-transition panel). */
	static constexpr int32 DialogueZOrder = 500;
};
