// Copyright Epic Games, Inc. All Rights Reserved.

#include "DialogueSubsystem.h"
#include "DialogueWidget.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"

UDialogueSubsystem* UDialogueSubsystem::Get(const UObject* WorldContext)
{
	if (!GEngine || !WorldContext)
	{
		return nullptr;
	}

	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull))
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UDialogueSubsystem>();
		}
	}

	return nullptr;
}

void UDialogueSubsystem::StartDialogue(const TArray<FText>& InLines, const FDialogueSettings& InSettings, FSimpleDelegate OnFinished)
{
	// Ignore overlapping requests; let the current dialogue finish first.
	if (bActive)
	{
		return;
	}

	Lines = InLines;
	Settings = InSettings;
	OnFinishedDelegate = OnFinished;
	CurrentIndex = 0;

	// No lines: complete immediately so callers can chain safely.
	if (Lines.Num() == 0)
	{
		OnFinishedDelegate.ExecuteIfBound();
		OnFinishedDelegate.Unbind();
		return;
	}

	TSubclassOf<UDialogueWidget> WidgetClass = Settings.WidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UDialogueWidget::StaticClass();
	}

	// Prefer a controller-owned widget; fall back to GameInstance ownership.
	if (APlayerController* PlayerController = GetLocalPlayerController())
	{
		Widget = CreateWidget<UDialogueWidget>(PlayerController, WidgetClass);
	}
	else if (UGameInstance* GameInstance = GetGameInstance())
	{
		Widget = CreateWidget<UDialogueWidget>(GameInstance, WidgetClass);
	}

	if (!Widget)
	{
		OnFinishedDelegate.ExecuteIfBound();
		OnFinishedDelegate.Unbind();
		return;
	}

	bActive = true;
	// Apply the panel image / font / colors before the widget builds its slate.
	Widget->ApplyStyle(Settings.Style);
	Widget->AddToViewport(DialogueZOrder);
	EnableDialogueInput();
	ShowLine(0);
}

void UDialogueSubsystem::ShowLine(int32 Index)
{
	if (Widget && Lines.IsValidIndex(Index))
	{
		Widget->TypeLine(Lines[Index], Settings.CharactersPerSecond);
	}
}

void UDialogueSubsystem::HandleAdvance()
{
	if (!bActive || !Widget)
	{
		return;
	}

	// While scrolling, the first input completes the line.
	if (Widget->IsTyping())
	{
		Widget->CompleteLine();
		return;
	}

	// Otherwise, advance to the next line (or end).
	++CurrentIndex;
	if (Lines.IsValidIndex(CurrentIndex))
	{
		ShowLine(CurrentIndex);
	}
	else
	{
		EndDialogue();
	}
}

void UDialogueSubsystem::EndDialogue()
{
	bActive = false;
	DisableDialogueInput();

	if (Widget)
	{
		Widget->RemoveFromParent();
		Widget = nullptr;
	}

	// Copy then clear so a re-entrant StartDialogue from the callback is safe.
	FSimpleDelegate Callback = OnFinishedDelegate;
	OnFinishedDelegate.Unbind();
	Callback.ExecuteIfBound();
}

void UDialogueSubsystem::EnableDialogueInput()
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
	{
		return;
	}

	// Add the dialogue-only mapping context so the advance key is recognised.
	if (Settings.MappingContext)
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				InputSubsystem->AddMappingContext(Settings.MappingContext, Settings.MappingContextPriority);
			}
		}
	}

	// Bind the advance action.
	if (Settings.AdvanceAction)
	{
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
		{
			FEnhancedInputActionEventBinding& Binding =
				EnhancedInput->BindAction(Settings.AdvanceAction, ETriggerEvent::Started, this, &UDialogueSubsystem::HandleAdvance);
			AdvanceBindingHandle = Binding.GetHandle();
		}
	}

	// Suppress gameplay movement/look while dialogue is up (counter-based, composable).
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
}

void UDialogueSubsystem::DisableDialogueInput()
{
	APlayerController* PlayerController = GetLocalPlayerController();
	if (!PlayerController)
	{
		return;
	}

	if (Settings.MappingContext)
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				InputSubsystem->RemoveMappingContext(Settings.MappingContext);
			}
		}
	}

	if (AdvanceBindingHandle != 0)
	{
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
		{
			EnhancedInput->RemoveBindingByHandle(AdvanceBindingHandle);
		}
		AdvanceBindingHandle = 0;
	}

	PlayerController->SetIgnoreMoveInput(false);
	PlayerController->SetIgnoreLookInput(false);
}

APlayerController* UDialogueSubsystem::GetLocalPlayerController() const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetFirstLocalPlayerController();
	}
	return nullptr;
}
