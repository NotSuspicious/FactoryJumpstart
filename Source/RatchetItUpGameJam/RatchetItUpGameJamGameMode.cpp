// Copyright Epic Games, Inc. All Rights Reserved.

#include "RatchetItUpGameJamGameMode.h"
#include "DialogueSubsystem.h"
#include "LevelFlowData.h"
#include "LevelTransitionSubsystem.h"
#include "SlideTransitionWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"

int32 ARatchetItUpGameJamGameMode::CurrentLevelIndex = 0;

ARatchetItUpGameJamGameMode::ARatchetItUpGameJamGameMode()
{
	// Default to the C++ slide widget; can be overridden on the Blueprint GameMode.
	SlideWidgetClass = USlideTransitionWidget::StaticClass();
}

void ARatchetItUpGameJamGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Figure out which level we are on and grab its content.
	ResolveCurrentLevel();

	// Scene start: fade in, then run the intro flow.
	StartSceneIntro();
}

// ---------------------------------------------------------------------------
// Scene start flow:  fade in -> Intro Sequence -> Intro Dialogue -> Begin Level
// ---------------------------------------------------------------------------

void ARatchetItUpGameJamGameMode::StartSceneIntro()
{
	FreezePlayer(true);
	PlayOpenTransitionIfNeeded();

	ULevelSequence* IntroSequence = ActiveFlowData ? ActiveFlowData->IntroSequence.Get() : nullptr;
	PlaySequenceThen(IntroSequence, [this]()
	{
		const TArray<FText> Lines = ActiveFlowData ? ActiveFlowData->IntroDialogue : TArray<FText>();
		PlayDialogueThen(Lines, [this]()
		{
			BeginLevel();
		});
	});
}

void ARatchetItUpGameJamGameMode::BeginLevel()
{
	// Hand control back to the player and let gameplay/puzzle logic start.
	FreezePlayer(false);
	OnLevelBegin();
}

// ---------------------------------------------------------------------------
// Scene end flow:  Puzzle Solved Sequence -> End Dialogue -> Exit Sequence
//                  -> close transition + load next level
// ---------------------------------------------------------------------------

void ARatchetItUpGameJamGameMode::OnPuzzleSolved()
{
	FreezePlayer(true);

	ULevelSequence* SolvedSequence = ActiveFlowData ? ActiveFlowData->PuzzleSolvedSequence.Get() : nullptr;
	PlaySequenceThen(SolvedSequence, [this]()
	{
		const TArray<FText> Lines = ActiveFlowData ? ActiveFlowData->EndDialogue : TArray<FText>();
		PlayDialogueThen(Lines, [this]()
		{
			StartSceneOutro();
		});
	});
}

void ARatchetItUpGameJamGameMode::StartSceneOutro()
{
	ULevelSequence* ExitSequence = ActiveFlowData ? ActiveFlowData->ExitLevelSequence.Get() : nullptr;
	PlaySequenceThen(ExitSequence, [this]()
	{
		// Closes the transition and loads the next level (which reopens it).
		AdvanceToNextLevel();
	});
}

// ---------------------------------------------------------------------------
// Level list / transition
// ---------------------------------------------------------------------------

bool ARatchetItUpGameJamGameMode::HasNextLevel() const
{
	return Levels.IsValidIndex(CurrentLevelIndex + 1);
}

void ARatchetItUpGameJamGameMode::AdvanceToNextLevel()
{
	// Final level completed: head back to the main menu instead.
	if (!HasNextLevel())
	{
		ReturnToMainMenu();
		return;
	}

	ULevelTransitionSubsystem* Transition = GetTransitionSubsystem();
	if (Transition && Transition->IsTransitioning())
	{
		return;
	}

	const int32 NextIndex = CurrentLevelIndex + 1;
	const TSoftObjectPtr<UWorld>& NextLevel = Levels[NextIndex].Level;
	if (NextLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("AdvanceToNextLevel: Levels entry %d has no level set."), NextIndex);
		return;
	}

	CurrentLevelIndex = NextIndex;

	if (Transition)
	{
		Transition->TransitionToLevel(NextLevel, TransitionSettings, SlideWidgetClass);
	}
	else
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, NextLevel);
	}
}

void ARatchetItUpGameJamGameMode::ReturnToMainMenu()
{
	if (MainMenuLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("ReturnToMainMenu: MainMenuLevel is not set."));
		return;
	}

	ULevelTransitionSubsystem* Transition = GetTransitionSubsystem();
	if (Transition && Transition->IsTransitioning())
	{
		return;
	}

	// Restart progression so the next playthrough begins at the first level.
	CurrentLevelIndex = 0;

	if (Transition)
	{
		Transition->TransitionToLevel(MainMenuLevel, TransitionSettings, SlideWidgetClass);
	}
	else
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, MainMenuLevel);
	}
}

void ARatchetItUpGameJamGameMode::QuitGame()
{
	ULevelTransitionSubsystem* Transition = GetTransitionSubsystem();

	// No subsystem available, or one is already mid-transition: just quit.
	if (!Transition || Transition->IsTransitioning())
	{
		DoQuit();
		return;
	}

	// Cover the screen, then quit once it is fully covered.
	FSimpleDelegate OnCovered = FSimpleDelegate::CreateUObject(this, &ARatchetItUpGameJamGameMode::DoQuit);
	Transition->PlayCloseTransition(TransitionSettings, SlideWidgetClass, OnCovered);
}

void ARatchetItUpGameJamGameMode::DoQuit()
{
	APlayerController* PlayerController = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, /*bIgnorePlatformRestrictions*/ false);
}

// ---------------------------------------------------------------------------
// Level resolution
// ---------------------------------------------------------------------------

void ARatchetItUpGameJamGameMode::ResolveCurrentLevel()
{
	const int32 Found = FindCurrentLevelIndex();
	if (Found != INDEX_NONE)
	{
		// Sync the progression index to the level we actually loaded into.
		CurrentLevelIndex = Found;
	}

	ActiveFlowData = Levels.IsValidIndex(CurrentLevelIndex) ? Levels[CurrentLevelIndex].FlowData : nullptr;

	if (!ActiveFlowData)
	{
		UE_LOG(LogTemp, Warning, TEXT("ResolveCurrentLevel: no LevelFlowData for the current level (index %d). Sequences/dialogue will be skipped."), CurrentLevelIndex);
	}
}

int32 ARatchetItUpGameJamGameMode::FindCurrentLevelIndex() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return INDEX_NONE;
	}

	// Current map's short name, with any PIE prefix (e.g. "UEDPIE_0_") stripped.
	FString CurrentName = World->GetMapName();
	CurrentName.RemoveFromStart(World->StreamingLevelsPrefix);

	for (int32 Index = 0; Index < Levels.Num(); ++Index)
	{
		const FString EntryName = Levels[Index].Level.ToSoftObjectPath().GetAssetName();
		if (!EntryName.IsEmpty() && EntryName == CurrentName)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

ULevelTransitionSubsystem* ARatchetItUpGameJamGameMode::GetTransitionSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<ULevelTransitionSubsystem>();
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void ARatchetItUpGameJamGameMode::PlayOpenTransitionIfNeeded()
{
	ULevelTransitionSubsystem* Transition = GetTransitionSubsystem();
	if (!Transition)
	{
		return;
	}

	// If we arrived via AdvanceToNextLevel, the subsystem is already revealing,
	// so only fade in here for direct loads (e.g. pressing PIE on a level).
	if (!Transition->IsTransitioning())
	{
		Transition->PlayOpenTransition(TransitionSettings, SlideWidgetClass);
	}
}

void ARatchetItUpGameJamGameMode::PlaySequenceThen(ULevelSequence* Sequence, TFunction<void()> OnDone)
{
	// Skip gracefully when a sequence slot is left empty.
	if (!Sequence)
	{
		if (OnDone)
		{
			OnDone();
		}
		return;
	}

	FMovieSceneSequencePlaybackSettings PlaybackSettings;
	ALevelSequenceActor* SequenceActor = nullptr;
	ActiveSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, PlaybackSettings, SequenceActor);

	if (!ActiveSequencePlayer)
	{
		if (OnDone)
		{
			OnDone();
		}
		return;
	}

	SequenceFinishedCallback = MoveTemp(OnDone);
	ActiveSequencePlayer->OnFinished.AddDynamic(this, &ARatchetItUpGameJamGameMode::HandleSequenceFinished);
	ActiveSequencePlayer->Play();
}

void ARatchetItUpGameJamGameMode::HandleSequenceFinished()
{
	if (ActiveSequencePlayer)
	{
		ActiveSequencePlayer->OnFinished.RemoveDynamic(this, &ARatchetItUpGameJamGameMode::HandleSequenceFinished);
	}
	ActiveSequencePlayer = nullptr;

	// Move the continuation out before running it, so it can safely start another sequence.
	TFunction<void()> Continuation = MoveTemp(SequenceFinishedCallback);
	SequenceFinishedCallback = nullptr;

	if (Continuation)
	{
		Continuation();
	}
}

void ARatchetItUpGameJamGameMode::PlayDialogueThen(const TArray<FText>& DialogueLines, TFunction<void()> OnDone)
{
	UDialogueSubsystem* Dialogue = UDialogueSubsystem::Get(this);
	if (!Dialogue)
	{
		if (OnDone)
		{
			OnDone();
		}
		return;
	}

	FSimpleDelegate Finished = FSimpleDelegate::CreateLambda([OnDone = MoveTemp(OnDone)]()
	{
		if (OnDone)
		{
			OnDone();
		}
	});

	Dialogue->StartDialogue(DialogueLines, DialogueSettings, Finished);
}

void ARatchetItUpGameJamGameMode::FreezePlayer(bool bFreeze)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	if (APlayerController* PlayerController = GameInstance->GetFirstLocalPlayerController())
	{
		// Disable movement & turning while cinematics/dialogue play.
		PlayerController->SetCinematicMode(bFreeze, /*bHidePlayer*/ false, /*bAffectsHUD*/ false, /*bAffectsMovement*/ true, /*bAffectsTurning*/ true);
	}
}
