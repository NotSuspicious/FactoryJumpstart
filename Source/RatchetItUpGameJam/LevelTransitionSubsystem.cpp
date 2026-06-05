// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelTransitionSubsystem.h"
#include "SlideTransitionWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

void ULevelTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Bound once; gated by bAwaitingLoad so only our own loads trigger a reveal.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULevelTransitionSubsystem::HandlePostLoadMap);
}

void ULevelTransitionSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	Super::Deinitialize();
}

void ULevelTransitionSubsystem::TransitionToLevel(TSoftObjectPtr<UWorld> Level, FSlideTransitionSettings Settings, TSubclassOf<USlideTransitionWidget> WidgetClass)
{
	if (Level.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("TransitionToLevel: target level is unset."));
		return;
	}

	// Ignore re-entrant requests while a transition is already running.
	if (bBusy)
	{
		return;
	}

	PendingLevel = Level;
	PendingSettings = Settings;
	PendingWidgetClass = WidgetClass;

	USlideTransitionWidget* TransitionWidget = EnsureWidget(WidgetClass);
	if (!TransitionWidget)
	{
		// No widget available; fall back to a plain load so the game still progresses.
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			UGameplayStatics::OpenLevelBySoftObjectPtr(GameInstance, Level);
		}
		return;
	}

	bBusy = true;

	TransitionWidget->ApplySettings(PendingSettings);
	TransitionWidget->AddToViewport(TransitionZOrder);

	TransitionWidget->OnSlideInComplete.BindUObject(this, &ULevelTransitionSubsystem::HandleSlideInComplete);
	TransitionWidget->PlaySlideIn(PendingSettings.ExitDuration);
}

void ULevelTransitionSubsystem::PlayOpenTransition(FSlideTransitionSettings Settings, TSubclassOf<USlideTransitionWidget> WidgetClass)
{
	if (bBusy)
	{
		return;
	}

	USlideTransitionWidget* TransitionWidget = EnsureWidget(WidgetClass);
	if (!TransitionWidget)
	{
		return;
	}

	// Taking over the reveal; cancel the post-load fallback if it was queued.
	bAwaitingReveal = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FallbackRevealHandle);
	}

	bBusy = true;
	PendingSettings = Settings;

	TransitionWidget->ApplySettings(Settings);
	TransitionWidget->SnapToCovered();

	// The panel may already be in the viewport (held covering after a transition load).
	if (!TransitionWidget->IsInViewport())
	{
		TransitionWidget->AddToViewport(TransitionZOrder);
	}

	TransitionWidget->OnSlideOutComplete.BindUObject(this, &ULevelTransitionSubsystem::HandleSlideOutComplete);
	TransitionWidget->PlaySlideOut(Settings.EntryDuration);
}

void ULevelTransitionSubsystem::PlayCloseTransition(FSlideTransitionSettings Settings, TSubclassOf<USlideTransitionWidget> WidgetClass, FSimpleDelegate OnCovered)
{
	if (bBusy)
	{
		return;
	}

	USlideTransitionWidget* TransitionWidget = EnsureWidget(WidgetClass);
	if (!TransitionWidget)
	{
		// No widget available; run the callback immediately so the action still happens.
		OnCovered.ExecuteIfBound();
		return;
	}

	bBusy = true;
	PendingSettings = Settings;
	CloseCallback = OnCovered;

	TransitionWidget->ApplySettings(Settings);
	TransitionWidget->AddToViewport(TransitionZOrder);

	TransitionWidget->OnSlideInComplete.BindUObject(this, &ULevelTransitionSubsystem::HandleCloseCovered);
	TransitionWidget->PlaySlideIn(Settings.ExitDuration);
}

void ULevelTransitionSubsystem::HandleCloseCovered()
{
	// Screen is fully covered. Run the callback (e.g. quit) and leave it covered
	// so nothing flashes back into view. bBusy intentionally stays set.
	FSimpleDelegate Callback = CloseCallback;
	CloseCallback.Unbind();
	Callback.ExecuteIfBound();
}

void ULevelTransitionSubsystem::CloseAndQuitGame(FSlideTransitionSettings Settings, TSubclassOf<USlideTransitionWidget> WidgetClass)
{
	FSimpleDelegate OnCovered = FSimpleDelegate::CreateUObject(this, &ULevelTransitionSubsystem::QuitGameNow);
	PlayCloseTransition(Settings, WidgetClass, OnCovered);
}

void ULevelTransitionSubsystem::QuitGameNow()
{
	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* PlayerController = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, /*bIgnorePlatformRestrictions*/ false);
}

void ULevelTransitionSubsystem::HandleSlideInComplete()
{
	// The panel now fully covers the screen; safe to start the (blocking) load.
	// The covered frame is held on screen for the duration of the load.
	bAwaitingLoad = true;

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(GameInstance, PendingLevel);
	}
}

void ULevelTransitionSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// Only react to the load we requested (ignores the initial map and others).
	if (!bAwaitingLoad)
	{
		return;
	}
	bAwaitingLoad = false;

	// LoadMap removed all viewport widgets, but our widget UObject persists.
	// Re-add it already covering, before the first frame of the new map renders.
	USlideTransitionWidget* TransitionWidget = EnsureWidget(PendingWidgetClass);
	if (!TransitionWidget)
	{
		bBusy = false;
		return;
	}

	TransitionWidget->ApplySettings(PendingSettings);
	TransitionWidget->SnapToCovered();
	TransitionWidget->AddToViewport(TransitionZOrder);

	// Hand the reveal to the destination level's GameMode so it uses ITS OWN
	// settings (its bounce, duration, color). Free bBusy so PlayOpenTransition runs.
	bBusy = false;
	bAwaitingReveal = true;

	// Safety net: if the destination never reveals (e.g. a level without our
	// GameMode), reveal here so we never get stuck on a covered screen.
	if (LoadedWorld)
	{
		LoadedWorld->GetTimerManager().SetTimer(FallbackRevealHandle, this, &ULevelTransitionSubsystem::HandleFallbackReveal, 2.0f, false);
	}
}

void ULevelTransitionSubsystem::HandleFallbackReveal()
{
	if (!bAwaitingReveal)
	{
		return;
	}

	// Nobody took over the reveal; do it with the transition's own settings.
	PlayOpenTransition(PendingSettings, PendingWidgetClass);
}

void ULevelTransitionSubsystem::HandleSlideOutComplete()
{
	if (Widget)
	{
		Widget->OnSlideInComplete.Unbind();
		Widget->OnSlideOutComplete.Unbind();
		Widget->RemoveFromParent();
	}

	bBusy = false;
}

USlideTransitionWidget* ULevelTransitionSubsystem::EnsureWidget(TSubclassOf<USlideTransitionWidget> WidgetClass)
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	if (!WidgetClass)
	{
		WidgetClass = USlideTransitionWidget::StaticClass();
	}

	// Recreate if missing or if the requested class changed.
	if (!Widget || Widget->GetClass() != WidgetClass)
	{
		// Owned by the GameInstance (not the world), so it survives OpenLevel.
		Widget = CreateWidget<USlideTransitionWidget>(GameInstance, WidgetClass);
	}

	return Widget;
}
